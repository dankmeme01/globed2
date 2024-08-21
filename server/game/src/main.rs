#![feature(sync_unsafe_cell, duration_constructors, async_closure, iter_collect_into, let_chains)]
#![allow(
    clippy::must_use_candidate,
    clippy::module_name_repetitions,
    clippy::cast_possible_truncation,
    clippy::missing_errors_doc,
    clippy::missing_panics_doc,
    clippy::missing_safety_doc,
    clippy::wildcard_imports,
    clippy::redundant_closure_for_method_calls
)]

#[cfg(feature = "use_tokio_tracing")]
use tokio_tracing as tokio;

#[cfg(not(feature = "use_tokio_tracing"))]
#[allow(clippy::single_component_path_imports)]
use tokio;

use std::{
    error::Error,
    net::{IpAddr, Ipv4Addr, SocketAddr},
};

use bridge::{CentralBridge, CentralBridgeError};
use globed_shared::{log::Log, *};
use reqwest::StatusCode;
use state::ServerState;
use tokio::{
    fs::File,
    io::AsyncReadExt,
    net::{TcpListener, UdpSocket},
};

use server::GameServer;

pub mod bridge;
pub mod client;
pub mod data;
pub mod managers;
pub mod server;
pub mod state;
pub mod util;
use globed_shared::webhook;

struct StartupConfiguration {
    bind_address: SocketAddr,
    central_data: Option<(String, String)>,
}

fn abort_misconfig() -> ! {
    error!("aborting launch due to misconfiguration.");
    std::process::exit(1);
}

fn censor_key(key: &str, keep_first_n_chars: usize) -> String {
    if key.len() <= keep_first_n_chars {
        return "*".repeat(key.len());
    }

    format!("{}{}", &key[..keep_first_n_chars], "*".repeat(key.len() - keep_first_n_chars))
}

fn parse_configuration() -> StartupConfiguration {
    let mut args = std::env::args();

    let exe_name = args.next().unwrap(); // skip executable
    let arg = args.next();

    let env_addr = std::env::var("GLOBED_GS_ADDRESS");
    let using_env_variables: bool = env_addr.is_ok();

    if arg.is_none() && !using_env_variables {
        // standalone with default params
        return StartupConfiguration {
            bind_address: format!("0.0.0.0:{DEFAULT_GAME_SERVER_PORT}").parse().unwrap(),
            central_data: None,
        };
    }

    // env variable takes precedence, otherwise grab the 1st arg from the command line
    let bind_address = env_addr.ok().or(arg).unwrap();

    let bind_address = match bind_address.parse::<SocketAddr>() {
        Ok(x) => x,
        Err(_) => {
            // try to parse it as an ip addr and use a default port
            match bind_address.parse::<Ipv4Addr>() {
                Ok(x) => SocketAddr::new(IpAddr::V4(x), DEFAULT_GAME_SERVER_PORT),
                Err(e) => {
                    error!("failed to parse the given IP address ({bind_address}): {e}");
                    warn!("hint: you have to provide a valid IPv4 address with an optional port number");
                    warn!("hint: for example \"0.0.0.0\" or \"0.0.0.0:{DEFAULT_GAME_SERVER_PORT}\"");
                    abort_misconfig();
                }
            }
        }
    };

    let arg = if using_env_variables {
        std::env::var("GLOBED_GS_CENTRAL_URL").ok()
    } else {
        args.next()
    };

    if arg.is_none() {
        // standalone with a specified bind addr
        return StartupConfiguration {
            bind_address,
            central_data: None,
        };
    }

    let mut central_url = arg.unwrap();
    if !central_url.ends_with('/') {
        central_url += "/";
    }

    let arg = if using_env_variables {
        std::env::var("GLOBED_GS_CENTRAL_PASSWORD").ok()
    } else {
        args.next()
    };

    if arg.is_none() {
        if using_env_variables {
            error!("expected the environment variable 'GLOBED_GS_CENTRAL_PASSWORD', couldn't find it");
        } else {
            error!("not enough arguments, expected the password of the central server");
            error!("correct usage: \"{exe_name} <address> <central-url> <central-password>\"");
        }
        warn!("hint: you must specify the password for connecting to the central server, see the server readme.");
        abort_misconfig();
    }

    let central_pw = arg.unwrap();

    // full configuration with a central server
    StartupConfiguration {
        bind_address,
        central_data: Some((central_url, central_pw)),
    }
}

#[allow(clippy::too_many_lines)]
#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    // Setup logger

    let write_to_file = std::env::var("GLOBED_GS_NO_FILE_LOG").map(|p| p.parse::<i32>().unwrap()).unwrap_or(0) == 0;

    log::set_logger(Logger::instance("globed_game_server", write_to_file)).unwrap();

    if let Some(log_level) = get_log_level("GLOBED_GS_LOG_LEVEL") {
        log::set_max_level(log_level);
    } else {
        log::set_max_level(LogLevelFilter::Warn); // we have to print these logs somehow lol
        error!("invalid value for the log level environment varaible");
        warn!("hint: possible values are 'trace', 'debug', 'info', 'warn', 'error', and 'none'.");
        abort_misconfig();
    }

    // set the interrupt handler to flush the logfile and exit

    if let Err(e) = ctrlc::set_handler(move || {
        warn!("Interrupt signal received, terminating the server");
        Logger::instance("globed_game_server", true).flush();
        std::process::exit(1);
    }) {
        warn!("error setting up interrupt handler: {e}");
    }

    // setup tokio-console in debug builds

    if cfg!(all(tokio_unstable, feature = "use_tokio_tracing")) {
        info!("Initializing tokio-console subscriber");
        console_subscriber::init();
    }

    // parse the configuration from environment variables or command line

    let startup_config = parse_configuration();
    let standalone = startup_config.central_data.is_none();

    // check if there's a word filter
    let word_filter_path = std::env::current_exe()
        .expect("failed to get current executable")
        .parent()
        .unwrap()
        .join("word-filter.txt");

    let word_filter_path2 = std::env::current_dir().expect("failed to get current dir").join("word-filter.txt");

    let chosen = match (word_filter_path.exists(), word_filter_path2.exists()) {
        (_, true) => Some(word_filter_path2),
        (true, false) => Some(word_filter_path),
        (false, false) => None,
    };

    let mut filter_words = Vec::new();
    if let Some(chosen) = chosen {
        let mut content = String::new();

        match File::open(chosen).await {
            Ok(mut file) => {
                file.read_to_string(&mut content).await?;
            }
            Err(e) => warn!("failed to open word-filter.txt: {e}"),
        }

        content.lines().map(|x| x.to_owned()).collect_into(&mut filter_words);
    }

    let filter_words_count = filter_words.len();

    let state = ServerState::new(&filter_words);
    let bridge = if standalone {
        warn!("Starting in standalone mode, authentication is disabled");
        warn!("Note: use Direct Connection option in-game to connect, Add Server cannot be used.");
        CentralBridge::new("", "")
    } else {
        let (central_url, central_pw) = startup_config.central_data.unwrap();

        // check if the user put a wrong url
        if central_url.contains("http://0.0.0.0") || central_url.contains("https://0.0.0.0") {
            error!("invalid central server URL was provided");
            warn!("hint: 0.0.0.0 is an address that is only valid for *listening*, not *connecting*");
            warn!("hint: try 127.0.0.1 if the server is on your local machine");
            abort_misconfig();
        }

        let bridge = CentralBridge::new(&central_url, &central_pw);

        info!("Retrieving config from the central server..");

        let central_conf = match bridge.request_boot_data().await {
            Ok(x) => x,
            Err(CentralBridgeError::RequestError(err)) => {
                error!("failed to make a request to the central server: {err}");
                warn!("hint: make sure the URL you passed is a valid Globed central server URL.");
                abort_misconfig();
            }
            Err(CentralBridgeError::CentralError((code, response))) => {
                error!("the central server returned an error: {response}");
                if code == StatusCode::UNAUTHORIZED {
                    warn!("hint: there is a high chance that you have supplied a wrong password");
                    warn!("hint: see the server readme if you don't know what password you need to use");
                }
                abort_misconfig();
            }
            Err(CentralBridgeError::WebhookError((_code, _response))) => {
                unreachable!("webhook error");
            }
            Err(CentralBridgeError::InvalidMagic(response)) => {
                error!("got unexpected response from the specified central server");
                warn!("hint: make sure the URL you passed is a valid Global central server URL");
                warn!("hint: here is the response that was received from the specified URL:");
                warn!("{}", &response[..response.len().min(512)]);
                abort_misconfig();
            }
            Err(CentralBridgeError::MalformedData(err)) => {
                error!("failed to parse the data sent by the central server: {err}");
                warn!("hint: this is supposedly a valid Globed central server, as the sent magic string was correct, but data decoding still failed");
                warn!("hint: make sure that both the central and the game servers are on the latest version");
                abort_misconfig();
            }
            Err(CentralBridgeError::ProtocolMismatch(protocol)) => {
                let our = MAX_SUPPORTED_PROTOCOL;
                error!("incompatible protocol versions!");
                error!("this game server is on v{}, while the central server uses v{}", our, protocol);
                if protocol > our {
                    warn!(
                        "hint: you are running an old version of the Globed game server (v{}), please update to the latest one.",
                        env!("CARGO_PKG_VERSION")
                    );
                } else {
                    warn!("hint: the central server you are using is outdated, or the game server is using a development build that is too new.");
                }
                abort_misconfig();
            }
            Err(CentralBridgeError::Other(msg)) => {
                error!("unknown error occurred");
                error!("{msg}");

                abort_misconfig();
            }
        };

        bridge.set_boot_data(central_conf);
        bridge
    };

    {
        // output useful information

        let gsbd = bridge.central_conf.lock();

        debug!("Configuration:");
        debug!("* TPS: {}", gsbd.tps);
        debug!("* Token expiry: {} seconds", gsbd.token_expiry);
        debug!("* Maintenance: {}", if gsbd.maintenance { "yes" } else { "no" });

        debug!("* Token secret key: '{}'", censor_key(&gsbd.secret_key2, 4));

        if standalone {
            debug!("* Admin key: '{}'", gsbd.admin_key);
        } else {
            // print first 4 chars, rest is censored
            debug!("* Admin key: '{}'", censor_key(&gsbd.admin_key, 4));
        }

        if gsbd.chat_burst_limit == 0 || gsbd.chat_burst_interval == 0 {
            debug!("* Text chat ratelimit: disabled");
        } else {
            debug!(
                "* Text chat ratelimit: {} messages per {}ms",
                gsbd.chat_burst_limit, gsbd.chat_burst_interval
            );
        }

        if filter_words_count != 0 {
            debug!("Filtered words: {filter_words_count}");
        }

        state.role_manager.refresh_from(&gsbd);
    }

    // bind the UDP socket

    let udp_socket = match UdpSocket::bind(&startup_config.bind_address).await {
        Ok(x) => x,
        Err(err) => {
            error!("Failed to bind the UDP socket with address {}: {err}", startup_config.bind_address);
            if startup_config.bind_address.port() < 1024 {
                warn!("hint: ports below 1024 are commonly privileged and you can't use them as a regular user");
                warn!("hint: pick a higher port number or leave it out completely to use the default port number ({DEFAULT_GAME_SERVER_PORT})");
            }
            abort_misconfig();
        }
    };

    // bind the TCP socket

    let tcp_socket = match TcpListener::bind(&startup_config.bind_address).await {
        Ok(x) => x,
        Err(err) => {
            error!("Failed to bind the TCP socket with address {}: {err}", startup_config.bind_address);

            abort_misconfig();
        }
    };

    // create and run the server

    let server = GameServer::new(tcp_socket, udp_socket, state, bridge, standalone);
    let server = Box::leak(Box::new(server));

    Box::pin(server.run()).await;

    Ok(())
}
