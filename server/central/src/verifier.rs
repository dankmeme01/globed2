use std::{
    collections::HashMap,
    sync::atomic::{AtomicBool, Ordering},
    time::{Duration, SystemTime},
};

use globed_shared::{
    anyhow::bail,
    base64::{engine::general_purpose as b64e, Engine as _},
    *,
};

const MICRO_SLEEP_PERIOD: Duration = Duration::from_millis(250);

#[derive(Clone)]
struct AccountEntry {
    pub account_id: i32,
    pub user_id: i32,
    pub name: String,
    pub authcode: String,
    pub message_id: i32,
    #[cfg(debug_assertions)]
    pub age: String,
}

pub struct AccountVerifier {
    http_client: reqwest::Client,
    account_id: i32,
    account_gjp: String,
    base_api_url: String,
    message_cache: SyncMutex<Vec<AccountEntry>>,
    last_update: SyncMutex<SystemTime>,
    outdated_messages: SyncMutex<IntSet<i32>>,
    is_enabled: AtomicBool,
    ignore_name_mismatch: bool,
    flush_period: Duration,
    delete_period: Duration,
}

impl AccountVerifier {
    pub fn new(
        account_id: i32,
        account_gjp: String,
        mut base_api_url: String,
        enabled: bool,
        ignore_name_mismatch: bool,
        flush_period: Duration,
    ) -> Self {
        let http_client = reqwest::ClientBuilder::new()
            .use_rustls_tls()
            .danger_accept_invalid_certs(true)
            .user_agent("")
            .timeout(Duration::from_secs(10))
            .build()
            .unwrap();

        while base_api_url.ends_with('/') {
            base_api_url.pop();
        }

        Self {
            http_client,
            account_id,
            account_gjp,
            base_api_url,
            message_cache: SyncMutex::new(Vec::new()),
            last_update: SyncMutex::new(SystemTime::now()),
            outdated_messages: SyncMutex::new(IntSet::default()),
            is_enabled: AtomicBool::new(enabled),
            ignore_name_mismatch,
            flush_period,
            delete_period: flush_period * 15,
        }
    }

    pub fn set_enabled(&self, state: bool) {
        self.is_enabled.store(state, Ordering::Relaxed);
    }

    pub async fn verify_account(&self, account_id: i32, user_id: i32, account_name: &str, authcode: &str) -> Result<i32, String> {
        if !self.is_enabled.load(Ordering::Relaxed) {
            return Ok(0);
        }

        let request_time = SystemTime::now();

        let mut passed_time = Duration::new(0, 0);

        // wait for max 5 flush periods
        while passed_time < self.flush_period * 5 {
            // if we haven't flushed the cache yet, keep sleeping
            let last_update = *self.last_update.lock();
            if last_update < request_time {
                tokio::time::sleep(MICRO_SLEEP_PERIOD).await;
                passed_time += MICRO_SLEEP_PERIOD;
                continue;
            }

            // if we found a message with the same authcode, break out
            {
                let msg_cache = self.message_cache.lock();
                if msg_cache.iter().any(|x| x.authcode == authcode) {
                    break;
                }
            }

            // there's no message yet, wait for the next update (or for 10 seconds to elapse)
            tokio::time::sleep(MICRO_SLEEP_PERIOD).await;
            passed_time += MICRO_SLEEP_PERIOD;
        }

        // `true` if we found a message with matching authcode.
        let mut has_matching_authcode = false;
        // `true` if we found a message with matching authcode, account ID and user ID.
        let mut has_matching_ids = false;
        let mut mismatched_name: Option<String> = None;
        let mut mismatched_id: i32 = 0;

        let cache = self.message_cache.lock();
        for msg in &*cache {
            if msg.authcode == authcode {
                has_matching_authcode = true;
                if msg.account_id == account_id && msg.user_id == user_id {
                    has_matching_ids = true;
                    if self.ignore_name_mismatch || msg.name.eq_ignore_ascii_case(account_name) {
                        return Ok(msg.message_id);
                    }

                    // if the name didnt match, set the mismatched name
                    mismatched_name = Some(msg.name.clone());
                }

                mismatched_id = msg.account_id;
            }
        }

        if has_matching_ids {
            Err(format!(
                "challenge solution proof was found, but account name is mismatched: \"{}\" vs \"{}\"",
                account_name,
                mismatched_name.unwrap_or_default()
            ))
        } else if has_matching_authcode {
            Err(format!(
                "challenge solution proof was found, but it was sent by a different account: {account_id} vs {mismatched_id}"
            ))
        } else {
            Err("challenge solution proof was not found".to_owned())
        }
    }

    pub async fn run_refresher(&self) {
        let mut interval = tokio::time::interval(self.flush_period);
        interval.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Burst);
        interval.tick().await;

        loop {
            interval.tick().await;

            if !self.is_enabled.load(Ordering::Relaxed) {
                continue;
            }

            match self.flush_cache().await {
                Ok(()) => {
                    if cfg!(debug_assertions) {
                        trace!("refreshed account verification cache");
                        let cache = self.message_cache.lock();
                        for message in &*cache {
                            #[cfg(not(debug_assertions))]
                            trace!(
                                "{} ({} / userid {}): {}",
                                message.name,
                                message.account_id,
                                message.user_id,
                                message.authcode
                            );

                            #[cfg(debug_assertions)]
                            trace!(
                                "{} ({} / userid {}): {} ({})",
                                message.name,
                                message.account_id,
                                message.user_id,
                                message.authcode,
                                message.age
                            );
                        }
                        trace!("------------------------------------");
                    }
                }
                Err(err) => {
                    warn!("failed to refresh account verification cache: {err}");
                }
            }
        }
    }

    // run the task that periodically deletes the stale messages
    pub async fn run_deleter(&self) {
        let mut interval = tokio::time::interval(self.delete_period);
        interval.tick().await;

        loop {
            interval.tick().await;

            if !self.is_enabled.load(Ordering::Relaxed) {
                continue;
            }

            match self.delete_outdated().await {
                Ok(()) => {}
                Err(err) => {
                    warn!("failed to delete outdated messages: {err}");
                }
            }
        }
    }

    async fn delete_outdated(&self) -> anyhow::Result<()> {
        let outdated = {
            let mut mtx = self.outdated_messages.lock();
            if mtx.is_empty() {
                return Ok(());
            }

            std::mem::take(&mut *mtx)
        };

        // at most 100 messages, join them to a string
        let outdated_str = outdated.into_iter().take(100).map(|x| x.to_string()).collect::<Vec<String>>().join(",");

        let result = self
            .http_client
            .post(format!("{}/deleteGJMessages20.php", self.base_api_url))
            .form(&[
                ("accountID", self.account_id.to_string()),
                ("gjp2", self.account_gjp.clone()),
                ("secret", "Wmfd2893gb7".to_owned()),
                ("messages", outdated_str),
            ])
            .send()
            .await;

        let response = match result {
            Err(err) => {
                bail!("boomlings error: {err}");
            }
            Ok(x) => x,
        }
        .text()
        .await?;

        if response == "-1" {
            bail!("boomlings returned -1!");
        }

        Ok(())
    }

    async fn flush_cache(&self) -> anyhow::Result<()> {
        let result = self
            .http_client
            .post(format!("{}/getGJMessages20.php", self.base_api_url))
            .form(&[
                ("accountID", self.account_id.to_string()),
                ("gjp2", self.account_gjp.clone()),
                ("secret", "Wmfd2893gb7".to_string()),
                ("page", "0".to_string()),
            ])
            .send()
            .await;

        let mut response = match result {
            Err(err) => {
                warn!("Failed to make a request to GD servers: {}", err.to_string());
                bail!("server error: {err}");
            }
            Ok(x) => x,
        }
        .text()
        .await?;

        if response == "-1" {
            bail!("boomlings returned -1!");
        }

        if response.starts_with("error code:") {
            if response == "error code: 1006" {
                error!("received error 1006, your IP address is likely blocked by the GD servers.");
            }

            bail!("server error: {response}");
        }

        let mut msg_cache = self.message_cache.lock();
        msg_cache.clear();

        *self.last_update.lock() = SystemTime::now();

        if response == "-2" {
            // -2 means no messages
            return Ok(());
        }

        let octothorpe = response.find('#');

        if let Some(octothorpe) = octothorpe {
            response = response.split_at(octothorpe).0.to_string();
        }

        let message_strings = response.split('|');
        for string in message_strings {
            // example responses:
            // 6:Lxwnar:3:153455117:2:25517837:1:78431572:4:bWVzc2FnZQ==:8:0:9:0:7:10 hours
            // 6:xponder:3:224655218:2:25222473:1:78431560:4:aGkgZ3V5eg==:8:0:9:0:7:10 hours
            let values = parse_robtop_string(string, ':');

            let message_id = values.get("1");
            let title = values.get("4");
            let author_name = values.get("6");
            let author_id = values.get("2");
            let author_user_id = values.get("3");
            let age = values.get("7");

            if message_id.is_none() || title.is_none() || author_name.is_none() || author_id.is_none() || age.is_none() || author_user_id.is_none() {
                warn!("ignoring invalid message: one of the attrs is none ({string})");
                continue;
            }

            let title = b64e::URL_SAFE.decode(title.unwrap())?;
            let title = String::from_utf8(title)?;

            let message_id = message_id.unwrap().parse::<i32>()?;
            let author_name = author_name.unwrap().trim_end();
            let author_id = author_id.unwrap().parse::<i32>()?;
            let author_user_id = author_user_id.unwrap().parse::<i32>()?;
            let age = age.unwrap();

            // if the message is old, queue it for deletion
            if age.contains("minutes") || age.contains("hours") || age.contains("days") || age.contains("months") || age.contains("years") {
                self.outdated_messages.lock().insert(message_id);
                continue;
            }

            let title = title.split_once("##c## ");
            let authcode = if let Some((_void, authcode)) = title {
                authcode.to_owned()
            } else {
                continue;
            };

            let age = (*age).to_owned();

            // info!("adding message to cache: {author_id}, {author_name}, {authcode}");

            msg_cache.push(AccountEntry {
                account_id: author_id,
                user_id: author_user_id,
                name: (*author_name).to_string(),
                authcode,
                message_id,
                #[cfg(debug_assertions)]
                age,
            });
        }

        Ok(())
    }
}

fn parse_robtop_string(data: &str, separator: char) -> HashMap<&str, &str> {
    let pairs: Vec<&str> = data.split(separator).collect();
    let mut map = HashMap::new();

    for i in (0..pairs.len()).step_by(2) {
        if let Some(key) = pairs.get(i) {
            if let Some(value) = pairs.get(i + 1) {
                map.insert(*key, *value);
            }
        }
    }

    map
}
