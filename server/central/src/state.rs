use std::{
    collections::HashMap,
    hash::{DefaultHasher, Hash, Hasher},
    net::IpAddr,
    path::PathBuf,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    time::{Duration, SystemTime},
};

use globed_shared::{
    anyhow,
    base64::{engine::general_purpose::STANDARD as b64e, Engine},
    crypto_box::aead::{generic_array::GenericArray, AeadMutInPlace},
    crypto_secretbox::{KeyInit, XSalsa20Poly1305},
    hmac::Hmac,
    rand::{self, distributions::Alphanumeric, rngs::OsRng, Rng, RngCore},
    reqwest,
    sha2::Sha256,
    TokenIssuer,
};
use tokio::sync::{RwLock, RwLockReadGuard, RwLockWriteGuard};

use crate::{
    config::{ServerConfig, UserlistMode},
    db::GlobedDb,
    game_pinger::GameServerPinger,
    verifier::AccountVerifier,
};
use blake2::{Blake2b, Digest};
use digest::consts::U32;

#[derive(Clone)]
pub struct ActiveChallenge {
    pub account_id: i32,
    pub user_id: i32,
    pub name: String,
    pub value: String,
    pub answer: String,
    pub started: SystemTime,
}

#[derive(Clone)]
pub struct LoginEntry {
    pub account_id: i32,
    pub name: String,
    pub time: SystemTime,
    pub link_code: u32,
}

pub struct ServerStateData {
    pub config_path: PathBuf,
    pub config: ServerConfig,
    pub hmac: Hmac<Sha256>,
    pub token_issuer: TokenIssuer,
    pub active_challenges: HashMap<IpAddr, ActiveChallenge>,
    pub challenge_pubkey: GenericArray<u8, U32>,
    pub challenge_box: XSalsa20Poly1305,
    pub http_client: reqwest::Client,
    pub last_logins: HashMap<u64, LoginEntry>, // { hash of lowercase username : entry }
}

impl ServerStateData {
    pub fn new(config_path: PathBuf, config: ServerConfig, secret_key: &str, token_secret_key: &str) -> Self {
        let skey_bytes = secret_key.as_bytes();

        let hmac = Hmac::<Sha256>::new_from_slice(skey_bytes).unwrap();
        let mut challenge_pubkey = GenericArray::<u8, U32>::default();
        OsRng.fill_bytes(&mut challenge_pubkey);
        let challenge_box = XSalsa20Poly1305::new(&challenge_pubkey);

        let token_expiry = Duration::from_secs(config.token_expiry);
        let http_client = reqwest::Client::builder()
            .use_rustls_tls()
            .danger_accept_invalid_certs(true)
            .timeout(Duration::from_secs(5))
            .user_agent(format!("globed-central-server/{}", env!("CARGO_PKG_VERSION")))
            .build()
            .unwrap();

        Self {
            config_path,
            config,
            hmac,
            token_issuer: TokenIssuer::new(token_secret_key, token_expiry),
            active_challenges: HashMap::new(),
            challenge_pubkey,
            challenge_box,
            http_client,
            last_logins: HashMap::new(),
        }
    }

    /// create a new challenge, return the rand string the user must use to solve the challenge
    pub fn create_challenge(
        &mut self,
        account_id: i32,
        user_id: i32,
        account_name: &str,
        ip_address: IpAddr,
        current_time: SystemTime,
    ) -> anyhow::Result<String> {
        let answer: String = rand::thread_rng().sample_iter(&Alphanumeric).take(16).map(char::from).collect();

        let mut nonce = [0u8; XSalsa20Poly1305::NONCE_SIZE];
        OsRng.fill_bytes(&mut nonce);

        let nonce_start = 0usize;
        let mac_start = nonce_start + XSalsa20Poly1305::NONCE_SIZE;
        let data_start = mac_start + XSalsa20Poly1305::TAG_SIZE;
        let data_end = data_start + answer.len();

        let mut out_vec = vec![0; data_end];

        out_vec[nonce_start..mac_start].copy_from_slice(&nonce);
        out_vec[data_start..data_end].copy_from_slice(answer.as_bytes());

        let tag = self
            .challenge_box
            .encrypt_in_place_detached((&nonce).into(), b"", &mut out_vec[data_start..data_end])?;

        out_vec[mac_start..data_start].copy_from_slice(&tag);

        let challenge_str = b64e.encode(&out_vec);

        let challenge = ActiveChallenge {
            started: current_time,
            account_id,
            user_id,
            value: challenge_str.clone(),
            answer,
            name: account_name.to_owned(),
        };

        self.active_challenges.insert(ip_address, challenge);

        Ok(challenge_str)
    }

    // uses hmac-sha256 to derive an auth key from user's account ID and name
    pub fn generate_authkey(&self, account_id: i32, user_id: i32, account_name: &str) -> Vec<u8> {
        use globed_shared::hmac::Mac;

        let val = format!("{account_id}:{user_id}:{account_name}");

        let mut hmac: Hmac<Sha256> = self.hmac.clone();
        hmac.update(val.as_bytes());
        let res = hmac.finalize();
        res.into_bytes().to_vec()
    }

    /// `generate_authkey` plus blake2b on top
    pub fn generate_hashed_authkey(&self, account_id: i32, user_id: i32, account_name: &str) -> [u8; 32] {
        let ak = self.generate_authkey(account_id, user_id, account_name);

        let mut hasher = Blake2b::<U32>::new();
        hasher.update(&ak);
        let output = hasher.finalize();

        output.into()
    }

    pub fn verify_challenge(&self, orig_value: &ActiveChallenge, answer: &str) -> bool {
        orig_value.answer == answer
    }

    pub async fn is_banned(&self, db: &GlobedDb, account_id: i32) -> anyhow::Result<Option<String>> {
        if self.config.userlist_mode == UserlistMode::Whitelist {
            return Ok(None);
        }

        let user = db.get_user(account_id).await?;
        if let Some(user) = user {
            if user.is_banned {
                Ok(user.violation_reason.clone())
            } else {
                Ok(None)
            }
        } else {
            Ok(None)
        }
    }

    pub fn clear_outdated_challenges(&mut self) {
        let now = SystemTime::now();

        // remove all challenges older than 2 hours
        self.active_challenges
            .retain(|_, v| now.duration_since(v.started).unwrap_or_default().as_secs() < 60 * 60 * 2);

        // remove logins older than 24 hours
        self.last_logins
            .retain(|_, v| now.duration_since(v.time).unwrap_or_default().as_secs() < 60 * 60 * 24);
    }

    // None to bypass verification
    pub fn get_login(&self, name: &str, link_code: Option<u32>) -> Option<&LoginEntry> {
        let lowercase = name.trim_start().to_lowercase();

        let mut hasher = DefaultHasher::new();
        lowercase.hash(&mut hasher);
        let hash = hasher.finish();

        let login = self.last_logins.get(&hash);

        login.and_then(|user| {
            // if link_code is None, or it matches, return the entry
            if link_code.map(|code| code == user.link_code && user.link_code != 0).unwrap_or(true) {
                Some(user)
            } else {
                None
            }
        })
    }

    pub fn get_username_from_login(&self, account_id: i32) -> Option<&str> {
        self.last_logins
            .iter()
            .find(|(_k, v)| v.account_id == account_id)
            .map(|x| x.1.name.as_str())
    }

    pub fn put_login(&mut self, name: &str, account_id: i32, link_code: u32) {
        let lowercase = name.trim_start().to_lowercase();

        let mut hasher = DefaultHasher::new();
        lowercase.hash(&mut hasher);
        let hash = hasher.finish();

        self.last_logins.insert(
            hash,
            LoginEntry {
                account_id,
                name: name.to_owned(),
                time: SystemTime::now(),
                link_code,
            },
        );
    }

    pub fn remove_login_code(&mut self, name: &str) {
        let lowercase = name.trim_start().to_lowercase();

        let mut hasher = DefaultHasher::new();
        lowercase.hash(&mut hasher);
        let hash = hasher.finish();

        let data = self.last_logins.get_mut(&hash);

        // reset so people cant relog again, just in case
        if let Some(data) = data {
            data.link_code = 0;
        }
    }
}

// both roa::Context and RwLock have the methods read() and write()
// so doing Context<RwLock<..>> will break some things, hence we make a wrapper

pub struct InnerServerState {
    pub data: RwLock<ServerStateData>,
    pub maintenance: AtomicBool,
    pub verifier: AccountVerifier,
    pub pinger: GameServerPinger,
}

impl InnerServerState {
    pub fn new(ssd: ServerStateData, pinger: GameServerPinger, maintenance: bool) -> Self {
        let gd_api_account = ssd.config.gd_api_account;
        let gd_api_gjp = ssd.config.gd_api_gjp.clone();
        let base_api_url = ssd.config.gd_api_url.clone();
        let use_gd_api = ssd.config.use_gd_api;
        let ignore_name_mismatch = ssd.config.skip_name_check;
        let flush_period = Duration::from_millis(ssd.config.refresh_interval);

        let verifier = AccountVerifier::new(gd_api_account, gd_api_gjp, base_api_url, use_gd_api, ignore_name_mismatch, flush_period);

        Self {
            data: RwLock::new(ssd),
            maintenance: AtomicBool::new(maintenance),
            verifier,
            pinger,
        }
    }

    pub async fn state_read(&self) -> RwLockReadGuard<'_, ServerStateData> {
        self.data.read().await
    }

    pub async fn state_write(&self) -> RwLockWriteGuard<'_, ServerStateData> {
        self.data.write().await
    }
}

#[derive(Clone)]
pub struct ServerState {
    pub inner: Arc<InnerServerState>,
}

impl ServerState {
    pub fn new(ssd: ServerStateData, pinger: GameServerPinger) -> Self {
        let maintenance = ssd.config.maintenance;
        let inner = InnerServerState::new(ssd, pinger, maintenance);
        Self { inner: Arc::new(inner) }
    }

    pub async fn state_read(&self) -> RwLockReadGuard<'_, ServerStateData> {
        self.inner.state_read().await
    }

    pub async fn state_write(&self) -> RwLockWriteGuard<'_, ServerStateData> {
        self.inner.state_write().await
    }

    pub fn maintenance(&self) -> bool {
        self.inner.maintenance.load(Ordering::SeqCst)
    }

    pub fn set_maintenance(&self, state: bool) {
        self.inner.maintenance.store(state, Ordering::SeqCst);
    }

    pub fn get_verifier(&self) -> &AccountVerifier {
        &self.inner.verifier
    }
}
