use std::net::IpAddr;

use globed_shared::{
    esp::{types::FastString, ByteBuffer, ByteBufferExtWrite},
    logger::debug,
    rand::{self, Rng},
    warn, GameServerBootData, ServerUserEntry, UserLoginData, UserLoginResponse, MAX_SUPPORTED_PROTOCOL, SERVER_MAGIC,
};

use rocket::{get, post, serde::json::Json, State};
use serde::{Deserialize, Serialize};

use crate::{config::UserlistMode, db::GlobedDb, state::ServerState, web::*};

#[post("/gs/boot")]
pub async fn boot(
    state: &State<ServerState>,
    password: GameServerPasswordGuard,
    ip_address: IpAddr,
    user_agent: GameServerUserAgentGuard<'_>,
) -> WebResult<Vec<u8>> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    let state = state.state_read().await;
    let config = &state.config;

    let bdata = GameServerBootData {
        protocol: MAX_SUPPORTED_PROTOCOL,
        tps: config.tps,
        maintenance: config.maintenance,
        secret_key2: config.secret_key2.clone(),
        token_expiry: config.token_expiry,
        status_print_interval: config.status_print_interval,
        admin_key: FastString::new(&config.admin_key),
        whitelist: config.userlist_mode == UserlistMode::Whitelist,
        admin_webhook_url: config.admin_webhook_url.clone(),
        rate_suggestion_webhook_url: config.rate_suggestion_webhook_url.clone(),
        featured_webhook_url: config.featured_webhook_url.clone(),
        room_webhook_url: config.room_webhook_url.clone(),
        chat_burst_limit: config.chat_burst_limit,
        chat_burst_interval: config.chat_burst_interval,
        roles: config.roles.clone(),
    };

    debug!("boot data request from game server {} at {}", user_agent.0, ip_address);

    let mut bb = ByteBuffer::new();
    bb.write_bytes(SERVER_MAGIC);
    bb.write_value(&bdata);

    drop(state);
    Ok(bb.into_vec())
}

async fn _get_user_by_id(database: &GlobedDb, account_id: i32) -> WebResult<ServerUserEntry> {
    let data = database.get_user(account_id).await?;
    Ok(data.unwrap_or_else(|| ServerUserEntry::new(account_id)))
}

async fn _get_user(database: &GlobedDb, user: &str) -> WebResult<ServerUserEntry> {
    if let Ok(account_id) = user.parse::<i32>() {
        _get_user_by_id(database, account_id).await
    } else {
        let user = database.get_user_by_name(user).await?;

        if let Some(user) = user {
            Ok(user)
        } else {
            bad_request!("failed to find the user by name");
        }
    }
}

#[get("/gs/user/<user>")]
pub async fn get_user(
    state: &State<ServerState>,
    password: GameServerPasswordGuard,
    database: &GlobedDb,
    user: &str,
    _user_agent: GameServerUserAgentGuard<'_>,
) -> WebResult<CheckedEncodableResponder> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    Ok(CheckedEncodableResponder::new(_get_user(database, user).await?))
}

#[post("/gs/userlogin", data = "<userdata>")]
pub async fn user_login(
    state: &State<ServerState>,
    password: GameServerPasswordGuard,
    database: &GlobedDb,
    userdata: CheckedDecodableGuard<UserLoginData>,
    _user_agent: GameServerUserAgentGuard<'_>,
) -> WebResult<CheckedEncodableResponder> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    let link_code = rand::thread_rng().gen_range(1000..10_000);

    // store login attempt
    state.state_write().await.put_login(&userdata.0.name, userdata.0.account_id, link_code);

    let user = _get_user_by_id(database, userdata.0.account_id).await?;

    let resp = UserLoginResponse { user_entry: user, link_code };

    Ok(CheckedEncodableResponder::new(resp))
}

#[post("/gs/user/update", data = "<userdata>")]
pub async fn update_user(
    state: &State<ServerState>,
    password: GameServerPasswordGuard,
    database: &GlobedDb,
    userdata: CheckedDecodableGuard<ServerUserEntry>,
) -> WebResult<()> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    database.update_user_server(userdata.0.account_id, &userdata.0).await?;

    Ok(())
}

/* /gsp/ apis are the same except they use JSON instead of binary encoding */

#[get("/gsp/user/<user>")]
pub async fn p_get_user(
    state: &State<ServerState>,
    password: GameServerPasswordGuard,
    database: &GlobedDb,
    user: &str,
    _user_agent: GameServerUserAgentGuard<'_>,
) -> WebResult<Json<ServerUserEntry>> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    Ok(Json(_get_user(database, user).await?))
}

#[post("/gsp/user/update", data = "<userdata>")]
pub async fn p_update_user(
    state: &State<ServerState>,
    password: GameServerPasswordGuard,
    database: &GlobedDb,
    userdata: Json<ServerUserEntry>,
) -> WebResult<()> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    database.update_user_server(userdata.0.account_id, &userdata.0).await?;

    Ok(())
}

#[derive(Serialize)]
pub struct UserLookupResponse {
    pub account_id: i32,
    pub name: String,
}

#[get("/gsp/lookup?<username>&<link_code>&<bypass>")]
pub async fn p_user_lookup(
    s_state: &State<ServerState>,
    password: GameServerPasswordGuard,
    username: &str,
    link_code: u32,
    bypass: Option<bool>,
) -> WebResult<Json<UserLookupResponse>> {
    let state = s_state.state_read().await;

    if !password.verify(&state.config.game_server_password) {
        unauthorized!("invalid gameserver credentials");
    }

    debug!("link code: {link_code}, bypass: {bypass:?}");

    let response = if let Some(login) = state.get_login(username, if bypass.unwrap_or(false) { None } else { Some(link_code) }) {
        UserLookupResponse {
            account_id: login.account_id,
            name: login.name.clone(),
        }
    } else {
        not_found!("Failed to find user by given username");
    };

    drop(state);

    s_state.state_write().await.remove_login_code(username);

    Ok(Json(response))
}

#[derive(Deserialize)]
pub struct RoleSyncRequest {
    pub account_id: i32,
    pub keep: Vec<String>,
    pub remove: Vec<String>,
}

#[derive(Deserialize)]
pub struct RoleSyncRequestData {
    pub users: Vec<RoleSyncRequest>,
}

#[post("/gsp/sync_roles", data = "<userdata>")]
pub async fn p_sync_roles(
    state: &State<ServerState>,
    database: &GlobedDb,
    password: GameServerPasswordGuard,
    userdata: Json<RoleSyncRequestData>,
) -> WebResult<()> {
    let correct = state.state_read().await.config.game_server_password.clone();

    if !password.verify(&correct) {
        unauthorized!("invalid gameserver credentials");
    }

    let state = state.state_read().await;

    for r_user in &userdata.users {
        let mut user = _get_user_by_id(database, r_user.account_id).await?;

        if user.user_name.is_none() {
            if let Some(name) = state.get_username_from_login(r_user.account_id) {
                user.user_name = Some(name.to_owned());
            }
        }

        // add roles in case user doesn't have them
        for role_id in &r_user.keep {
            // check if the role is valid

            if state.config.roles.iter().any(|x| x.id == *role_id) {
                // add the role to the user
                if !user.user_roles.contains(role_id) {
                    user.user_roles.push(role_id.clone());
                }

                // // if this was a single user query, return back the names of the roles the user now received
                // if userdata.users.len() == 1 {
                //     return_val.push(server_role.)
                // }
            } else {
                warn!("attempting to sync invalid role id: {role_id}");
            }
        }

        // remove roles the user shouldn't have
        user.user_roles.retain(|r| !r_user.remove.contains(r));

        // update the user
        database.update_user_server(user.account_id, &user).await?;
    }

    Ok(())
}
