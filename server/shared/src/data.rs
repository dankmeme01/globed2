use super::*;
use argon2::{
    password_hash::{rand_core::OsRng, PasswordHash, PasswordHasher, PasswordVerifier, SaltString},
    Argon2,
};
use esp::FastString;
use serde::{Deserialize, Serialize};

#[derive(Encodable, Decodable, Clone)]
pub struct GameServerBootData {
    pub protocol: u16,
    pub tps: u32,
    pub maintenance: bool,
    pub secret_key2: String,
    pub token_expiry: u64,
    pub status_print_interval: u64,
    pub admin_key: FastString,
    pub whitelist: bool,
    pub admin_webhook_url: String,
    pub rate_suggestion_webhook_url: String,
    pub featured_webhook_url: String,
    pub room_webhook_url: String,
    pub chat_burst_limit: u32,
    pub chat_burst_interval: u32,
    pub roles: Vec<ServerRole>,
}

impl Default for GameServerBootData {
    fn default() -> Self {
        #[cfg(debug_assertions)]
        let status_print_interval = 60 * 15; // 15 min
        #[cfg(not(debug_assertions))]
        let status_print_interval = 7200; // 2 hours

        Self {
            protocol: MAX_SUPPORTED_PROTOCOL,
            tps: 30,
            maintenance: false,
            secret_key2: String::new(),
            token_expiry: 0,
            status_print_interval,
            admin_key: generate_alphanum_string(ADMIN_KEY_LENGTH).into(),
            whitelist: false,
            admin_webhook_url: String::new(),
            rate_suggestion_webhook_url: String::new(),
            featured_webhook_url: String::new(),
            room_webhook_url: String::new(),
            chat_burst_limit: 0,
            chat_burst_interval: 0,
            roles: Vec::new(),
        }
    }
}

pub fn generate_argon2_hash(password: &str) -> String {
    let salt = SaltString::generate(&mut OsRng);

    Argon2::default()
        .hash_password(password.as_bytes(), &salt)
        .map(|x| x.to_string())
        .unwrap()
}

#[derive(Encodable, Decodable, Serialize, Deserialize, DynamicSize, Clone, Default)]
pub struct ServerUserEntry {
    pub account_id: i32,
    pub user_name: Option<String>,
    pub name_color: Option<String>,
    pub user_roles: Vec<String>,
    pub is_banned: bool,
    pub is_muted: bool,
    pub is_whitelisted: bool,
    pub admin_password: Option<String>,
    pub admin_password_hash: Option<String>,
    pub violation_reason: Option<String>,
    pub violation_expiry: Option<i64>, // seconds since unix epoch
}

// this is pure laziness i was just too lazy to get derive macros n shit into the central server
#[derive(Decodable, Clone)]
pub struct UserLoginData {
    pub account_id: i32,
    pub name: String,
}

// same here
#[derive(Encodable, Decodable, Clone)]
pub struct UserLoginResponse {
    pub user_entry: ServerUserEntry,
    pub link_code: u32,
}

impl ServerUserEntry {
    pub fn new(account_id: i32) -> Self {
        Self {
            account_id,
            ..Default::default()
        }
    }

    pub fn to_user_entry(self) -> UserEntry {
        UserEntry {
            account_id: self.account_id,
            user_name: self.user_name,
            name_color: self.name_color,
            user_roles: self.user_roles,
            is_banned: self.is_banned,
            is_muted: self.is_muted,
            is_whitelisted: self.is_whitelisted,
            admin_password: None,
            violation_reason: self.violation_reason,
            violation_expiry: self.violation_expiry,
        }
    }

    pub fn from_user_entry(entry: UserEntry) -> Self {
        Self {
            account_id: entry.account_id,
            user_name: entry.user_name,
            name_color: entry.name_color,
            user_roles: entry.user_roles,
            is_banned: entry.is_banned,
            is_muted: entry.is_muted,
            is_whitelisted: entry.is_whitelisted,
            admin_password: entry.admin_password,
            admin_password_hash: None,
            violation_reason: entry.violation_reason,
            violation_expiry: entry.violation_expiry,
        }
    }

    pub fn verify_password(&self, password: &str) -> Result<bool, argon2::password_hash::Error> {
        match self.admin_password_hash.as_ref() {
            Some(x) => {
                let parsed_hash = PasswordHash::new(x)?;

                Ok(Argon2::default().verify_password(password.as_bytes(), &parsed_hash).is_ok())
            }
            None => Ok(false),
        }
    }
}

#[derive(Encodable, Decodable, Serialize, Deserialize, DynamicSize, Clone, Default)]
pub struct UserEntry {
    pub account_id: i32,
    pub user_name: Option<String>,
    pub name_color: Option<String>,
    pub user_roles: Vec<String>,
    pub is_banned: bool,
    pub is_muted: bool,
    pub is_whitelisted: bool,
    pub admin_password: Option<String>, // always None when returning
    pub violation_reason: Option<String>,
    pub violation_expiry: Option<i64>, // seconds since unix epoch
}

impl UserEntry {
    pub fn new(account_id: i32) -> Self {
        Self {
            account_id,
            ..Default::default()
        }
    }
}

#[derive(Serialize, Deserialize, Encodable, Decodable, DynamicSize, Clone, Default)]
#[allow(clippy::struct_excessive_bools)]
pub struct ServerRole {
    pub id: String,
    pub priority: i32,
    #[serde(default)]
    pub badge_icon: String,
    #[serde(default)]
    pub name_color: String,
    #[serde(default)]
    pub chat_color: String,

    // permissions
    #[serde(default)]
    pub notices: bool,
    #[serde(default)]
    pub notices_to_everyone: bool,
    #[serde(default)]
    pub kick: bool,
    #[serde(default)]
    pub kick_everyone: bool,
    #[serde(default)]
    pub mute: bool,
    #[serde(default)]
    pub ban: bool,
    #[serde(default)]
    pub edit_role: bool,
    #[serde(default)]
    pub edit_featured_levels: bool,
    #[serde(default)]
    pub admin: bool,
}
