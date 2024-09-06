pub mod guards;
pub mod responders;

pub use guards::*;
pub use responders::*;

use rocket::{catch, Request};

#[catch(404)]
pub fn not_found(_req: &Request) -> &'static str {
    "not found"
}

#[catch(422)]
pub fn query_string(_req: &Request) -> &'static str {
    use const_format::concatcp;
    use globed_shared::MIN_CLIENT_VERSION;

    concatcp!(
        "Outdated Globed version, minimum required is ",
        MIN_CLIENT_VERSION,
        ". Please make sure that you are on the latest versions of Geometry Dash, Geode and Globed."
    )
}

pub mod routes {
    pub mod auth;
    pub mod auth_v2;
    pub mod featured;
    pub mod game_server;
    pub mod meta;
    pub mod public;

    pub use super::*;
    pub use crate::{db::GlobedDb, state::ServerState};
    pub use rocket::{get, post, State};
    use rocket::{routes, Route};

    pub fn build_router() -> Vec<Route> {
        routes![
            meta::version,
            meta::servers,
            meta::index,
            meta::robots,
            game_server::boot,
            game_server::get_user,
            game_server::user_login,
            game_server::update_user,
            game_server::p_get_user,
            game_server::p_update_user,
            game_server::p_user_lookup,
            game_server::p_sync_roles,
            auth::totp_login,
            auth::challenge_start,
            auth::challenge_finish,
            auth_v2::totp_login,
            auth_v2::challenge_new,
            auth_v2::challenge_verify,
            featured::current,
            featured::history,
            featured::history_v2,
            featured::replace,
            featured::replace_v2,
            public::player_counts,
        ]
    }

    macro_rules! check_maintenance {
        ($ctx:expr) => {
            if $ctx.maintenance() {
                return Err(crate::web::MaintenanceResponder {
                    inner: "The server is currently under maintenance, please try connecting again later",
                }
                .into());
            }
        };
    }

    pub(crate) use check_maintenance;
}
