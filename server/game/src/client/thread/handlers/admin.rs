use globed_shared::{info, warn};

use crate::{
    managers::ComputedRole,
    webhook::{BanMuteStateChange, WebhookChannel, WebhookMessage},
};

use super::*;

macro_rules! admin_error {
    ($self:expr, $msg:expr) => {
        $self.send_packet_dynamic(&AdminErrorPacket { message: $msg }).await?;
        return Ok(());
    };
}

#[derive(Clone, Copy)]
enum AdminPerm {
    Any,
    Notice,
    NoticeToEveryone,
    Kick,
    KickEveryone,
    Mute,
    Ban,
    EditRoles,
    // EditFeaturedLevels,
    Admin,
}

impl ClientThread {
    // check if the user is logged in as admin, and if they have the given permission
    fn _has_perm(&self, perm: AdminPerm) -> bool {
        if !self.is_authorized_user.load(Ordering::Relaxed) {
            return false;
        }

        let role = self.user_role.lock();

        if role.admin {
            return true;
        }

        match perm {
            AdminPerm::Any => role.can_moderate(),
            AdminPerm::Notice => role.notices,
            AdminPerm::NoticeToEveryone => role.notices_to_everyone,
            AdminPerm::Ban => role.ban,
            AdminPerm::Mute => role.mute,
            AdminPerm::Kick => role.kick,
            AdminPerm::KickEveryone => role.kick_everyone,
            AdminPerm::EditRoles => role.edit_role,
            // AdminPerm::EditFeaturedLevels => role.edit_featured_levels,
            AdminPerm::Admin => false,
        }
    }

    fn _update_user_role(&self, from: &ComputedRole) {
        self.user_role.lock().clone_from(from);
    }

    gs_handler!(self, handle_admin_auth, AdminAuthPacket, packet, {
        let account_id = gs_needauth!(self);

        // test for the global password first
        if packet.key.constant_time_compare(&self.game_server.bridge.central_conf.lock().admin_key) {
            info!(
                "[{} ({}) @ {}] just logged into the admin panel (with global password)",
                self.account_data.lock().name,
                account_id,
                self.get_tcp_peer()
            );

            self.is_authorized_user.store(true, Ordering::Relaxed);
            // give super admin perms
            let role = self.game_server.state.role_manager.get_superadmin();
            self._update_user_role(&role);

            self.send_packet_dynamic(&AdminAuthSuccessPacket { role }).await?;

            return Ok(());
        }

        // test for the per-user password
        let pass_result = self.user_entry.lock().verify_password(&packet.key);

        match pass_result {
            Ok(true) => {
                info!(
                    "[{} ({}) @ {}] just logged into the admin panel",
                    self.account_data.lock().name,
                    account_id,
                    self.get_tcp_peer()
                );

                self.is_authorized_user.store(true, Ordering::Relaxed);

                let role = self.game_server.state.role_manager.compute(&self.user_entry.lock().user_roles);
                self._update_user_role(&role);

                self.send_packet_dynamic(&AdminAuthSuccessPacket { role }).await?;

                return Ok(());
            }
            Ok(false) => {}
            Err(e) => {
                warn!("argon2 password validation error: {e}");
            }
        }

        info!(
            "[{} ({}) @ {}] just failed to login to the admin panel with password: {}",
            self.account_data.lock().name,
            account_id,
            self.get_tcp_peer(),
            packet.key
        );

        // this is silly tbh
        // if self.game_server.bridge.has_webhook() {
        //     let name = self.account_data.lock().name.try_to_string();
        //
        //     if let Err(err) = self.game_server.bridge.send_webhook_message(WebhookMessage::AuthFail(name)).await {
        //         warn!("webhook error: {err}");
        //     }
        // }

        self.send_packet_static(&AdminAuthFailedPacket).await
    });

    gs_handler!(self, handle_admin_send_notice, AdminSendNoticePacket, packet, {
        let account_id = gs_needauth!(self);

        if !self._has_perm(AdminPerm::Notice) {
            return Ok(());
        }

        if packet.message.len() > MAX_NOTICE_SIZE {
            admin_error!(self, "message is too long");
        }

        if packet.message.is_empty() {
            return Ok(());
        }

        let notice_packet = ServerNoticePacket { message: packet.message };

        // i am not proud of this code
        match packet.notice_type {
            AdminSendNoticeType::Everyone => {
                if !self._has_perm(AdminPerm::NoticeToEveryone) {
                    admin_error!(self, "no permission");
                }

                let threads = self
                    .game_server
                    .clients
                    .lock()
                    .values()
                    .filter(|thr| thr.authenticated())
                    .cloned()
                    .collect::<Vec<_>>();

                let name = self.account_data.lock().name.try_to_string();

                info!(
                    "[{name} ({account_id}) @ {}] sending a notice to all {} people on the server: {}",
                    self.get_tcp_peer(),
                    threads.len(),
                    notice_packet.message,
                );

                if self.game_server.bridge.has_admin_webhook() {
                    if let Err(err) = self
                        .game_server
                        .bridge
                        .send_admin_webhook_message(WebhookMessage::NoticeToEveryone(
                            name,
                            threads.len(),
                            notice_packet.message.try_to_string(),
                        ))
                        .await
                    {
                        warn!("webhook error during notice to everyone: {err}");
                    }
                }

                self.send_packet_dynamic(&AdminSuccessMessagePacket {
                    message: &format!("Sent to {} people", threads.len()),
                })
                .await?;

                for thread in threads {
                    thread.push_new_message(ServerThreadMessage::BroadcastNotice(notice_packet.clone())).await;
                }
            }

            AdminSendNoticeType::Person => {
                let thread = self.game_server.find_user(&packet.player);

                let player_name = thread.as_ref().map_or_else(
                    || "<invalid player>".to_owned(),
                    |thr| thr.account_data.lock().name.try_to_str().to_owned(),
                );

                let self_name = self.account_data.lock().name.try_to_string();
                let notice_msg = notice_packet.message.try_to_string();

                info!(
                    "[{self_name} ({account_id}) @ {}] sending a notice to {player_name}: {notice_msg}",
                    self.get_tcp_peer()
                );

                if self.game_server.bridge.has_admin_webhook() {
                    if let Err(err) = self
                        .game_server
                        .bridge
                        .send_admin_webhook_message(WebhookMessage::NoticeToPerson(self_name, player_name, notice_msg))
                        .await
                    {
                        warn!("webhook error during notice to person: {err}");
                    }
                }

                if let Some(thread) = thread {
                    thread.push_new_message(ServerThreadMessage::BroadcastNotice(notice_packet.clone())).await;

                    self.send_packet_dynamic(&AdminSuccessMessagePacket {
                        message: &format!("Sent notice to {}", thread.account_data.lock().name),
                    })
                    .await?;
                } else {
                    admin_error!(self, "failed to find the user");
                }
            }

            AdminSendNoticeType::RoomOrLevel => {
                if packet.room_id != 0 && !self.game_server.state.room_manager.is_valid_room(packet.room_id) {
                    admin_error!(self, "unable to send notice, invalid room ID");
                }

                // if this is a global room, also require the notice to everyone perm
                if packet.room_id == 0 && !self._has_perm(AdminPerm::NoticeToEveryone) {
                    admin_error!(self, "no permission");
                }

                let player_ids = self.game_server.state.room_manager.with_any(packet.room_id, |pm| {
                    let mut player_ids = Vec::with_capacity(128);
                    if packet.level_id == 0 {
                        pm.manager.for_each_player(|player| {
                            player_ids.push(player.account_id);
                        });
                    } else {
                        pm.manager.for_each_player_on_level(packet.level_id, |player| {
                            player_ids.push(player.account_id);
                        });
                    }

                    player_ids
                });

                let threads = self
                    .game_server
                    .clients
                    .lock()
                    .values()
                    .filter(|thr| player_ids.contains(&thr.account_id.load(Ordering::Relaxed)))
                    .cloned()
                    .collect::<Vec<_>>();

                let self_name = self.account_data.lock().name.try_to_string();
                let notice_msg = notice_packet.message.try_to_string();

                info!(
                    "[{self_name} ({account_id}) @ {}] sending a notice to {} people: {notice_msg}",
                    self.get_tcp_peer(),
                    threads.len()
                );

                if self.game_server.bridge.has_admin_webhook() {
                    if let Err(err) = self
                        .game_server
                        .bridge
                        .send_admin_webhook_message(WebhookMessage::NoticeToSelection(self_name, threads.len(), notice_msg))
                        .await
                    {
                        warn!("webhook error during notice to selection: {err}");
                    }
                }

                self.send_packet_dynamic(&AdminSuccessMessagePacket {
                    message: &format!("Sent to {} people", threads.len()),
                })
                .await?;

                for thread in threads {
                    thread.push_new_message(ServerThreadMessage::BroadcastNotice(notice_packet.clone())).await;
                }
            }
        }

        Ok(())
    });

    gs_handler!(self, handle_admin_disconnect, AdminDisconnectPacket, packet, {
        let _ = gs_needauth!(self);

        if !self._has_perm(AdminPerm::Kick) {
            return Ok(());
        }

        // to kick everyone, require admin
        if &*packet.player == "@everyone" && self._has_perm(AdminPerm::KickEveryone) {
            let threads: Vec<_> = self.game_server.clients.lock().values().cloned().collect();
            for thread in threads {
                thread
                    .push_new_message(ServerThreadMessage::TerminationNotice(packet.message.clone()))
                    .await;
            }

            let self_name = self.account_data.lock().name.try_to_string();

            if self.game_server.bridge.has_admin_webhook() {
                if let Err(err) = self
                    .game_server
                    .bridge
                    .send_admin_webhook_message(WebhookMessage::KickEveryone(self_name, packet.message.try_to_string()))
                    .await
                {
                    warn!("webhook error during kick everyone: {err}");
                }
            }

            return Ok(());
        }

        if let Some(thread) = self.game_server.find_user(&packet.player) {
            let reason_string = packet.message.try_to_string();

            thread.push_new_message(ServerThreadMessage::TerminationNotice(packet.message)).await;

            if self.game_server.bridge.has_admin_webhook() {
                let own_name = self.account_data.lock().name.try_to_string();
                let target_name = thread.account_data.lock().name.try_to_string();

                if let Err(err) = self
                    .game_server
                    .bridge
                    .send_admin_webhook_message(WebhookMessage::KickPerson(
                        own_name,
                        target_name,
                        thread.account_id.load(Ordering::Relaxed),
                        reason_string,
                    ))
                    .await
                {
                    warn!("webhook error during kick person: {err}");
                }
            }

            self.send_packet_dynamic(&AdminSuccessMessagePacket {
                message: &format!("Successfully kicked {}", thread.account_data.lock().name),
            })
            .await
        } else {
            admin_error!(self, "failed to find the user");
        }
    });

    gs_handler!(self, handle_admin_get_user_state, AdminGetUserStatePacket, packet, {
        let _ = gs_needauth!(self);

        if !self._has_perm(AdminPerm::Any) {
            return Ok(());
        }

        let user = self.game_server.find_user(&packet.player);
        let mut packet = if let Some(user) = user {
            let entry = user.user_entry.lock().clone();
            let account_data = user.account_data.lock().make_room_preview(0, true);

            AdminUserDataPacket {
                entry: entry.to_user_entry(),
                account_data: Some(account_data),
            }
        } else {
            // on a standalone server, if the user is not online we are kinda out of luck
            if self.game_server.standalone {
                admin_error!(self, "This cannot be done on a standalone server");
            }

            // they are not on the server right now, request data via the bridge
            let user_entry = match self.game_server.bridge.get_user_data(&packet.player).await {
                Ok(x) => x,
                Err(err) => {
                    warn!("error fetching data from the bridge: {err}");
                    admin_error!(self, &err.to_string());
                }
            };

            AdminUserDataPacket {
                entry: user_entry.to_user_entry(),
                account_data: None,
            }
        };

        // only admins can see/change passwords of others
        if !self._has_perm(AdminPerm::Admin) {
            packet.entry.admin_password = None;
        }

        self.send_packet_dynamic(&packet).await
    });

    gs_handler!(self, handle_admin_update_user, AdminUpdateUserPacket, packet, {
        let self_account_id = gs_needauth!(self);

        if !self._has_perm(AdminPerm::Any) {
            admin_error!(self, "No permission (not a mod)");
        }

        // we cant use bridge in standalone so do nothing
        if self.game_server.standalone {
            admin_error!(self, "This cannot be done on a standalone server");
        }

        let mut new_user_entry = packet.user_entry;

        let target_account_id = new_user_entry.account_id;

        // if they are online, update them live, else get their old data from the bridge

        let thread = self.game_server.get_user_by_id(target_account_id);
        let user_entry = if let Some(thread) = thread.as_ref() {
            thread.user_entry.lock().clone()
        } else {
            match self.game_server.bridge.get_user_data(&target_account_id.to_string()).await {
                Ok(x) => x,
                Err(err) => {
                    admin_error!(self, &format!("failed to get user: {err}"));
                }
            }
        };

        let editing_self = self_account_id == target_account_id;

        // if this user has a higher priority, don't allow editing their roles

        new_user_entry.user_roles.retain(|x| !x.is_empty());
        let my_priority = self.game_server.state.role_manager.compute_priority(&self.user_entry.lock().user_roles);
        let user_priority = self.game_server.state.role_manager.compute_priority(&user_entry.user_roles);
        let new_user_priority = self.game_server.state.role_manager.compute_priority(&new_user_entry.user_roles);

        // if not admin, cant update others passwords
        if !self._has_perm(AdminPerm::Admin) {
            new_user_entry.admin_password = None;
        }

        // if no edit role perm or the user is higher than us, cant update their roles
        if !self._has_perm(AdminPerm::EditRoles) || (user_priority >= my_priority && !self._has_perm(AdminPerm::Admin) && !editing_self) {
            new_user_entry.user_roles.clone_from(&user_entry.user_roles);
        }

        // check what changed
        let c_user_roles = new_user_entry.user_roles != user_entry.user_roles;
        let c_is_banned = new_user_entry.is_banned != user_entry.is_banned;
        let c_is_muted = new_user_entry.is_muted != user_entry.is_muted;
        let c_is_whitelisted = new_user_entry.is_whitelisted != user_entry.is_whitelisted;
        let c_violation_reason = new_user_entry.violation_reason != user_entry.violation_reason;
        let c_violation_expiry = new_user_entry.violation_expiry != user_entry.violation_expiry;
        let c_name_color = new_user_entry.name_color != user_entry.name_color;
        let c_admin_password = new_user_entry.admin_password.is_some();
        // user_name intentionally left unchecked.

        if c_user_roles && new_user_priority >= my_priority && !self._has_perm(AdminPerm::Admin) {
            // if we are editing ourselves, allow to assign lower roles
            if !(editing_self && new_user_priority == my_priority) {
                admin_error!(self, "cannot promote a user to your role or higher");
            }
        }

        if (c_is_banned || c_is_whitelisted) && !self._has_perm(AdminPerm::Ban) {
            admin_error!(self, "no permission to ban/whitelist");
        }

        if c_is_muted && !self._has_perm(AdminPerm::Mute) {
            admin_error!(self, "no permission to mute");
        }

        // role validation
        if !self.game_server.state.role_manager.all_valid(&new_user_entry.user_roles) {
            admin_error!(self, "attempting to assign an invalid role");
        }

        if let Some(color) = new_user_entry.name_color.as_ref() {
            if color.parse::<Color3B>().is_err() {
                admin_error!(self, &format!("attempting to assign an invalid name color: {color}"));
            }
        }

        if !(c_user_roles
            || c_is_banned
            || c_is_muted
            || c_is_whitelisted
            || c_violation_reason
            || c_violation_expiry
            || c_name_color
            || c_admin_password)
        {
            // no changes
            return self.send_packet_dynamic(&AdminSuccessMessagePacket { message: "No changes" }).await;
        }

        // if not banned and not muted, clear the violation reason and duration
        if !new_user_entry.is_banned && !new_user_entry.is_muted {
            new_user_entry.violation_expiry = None;
            new_user_entry.violation_reason = None;
        }

        let target_user_name = new_user_entry.user_name.clone().unwrap_or_else(|| "<unknown>".to_owned());
        let mut server_entry = ServerUserEntry::from_user_entry(new_user_entry.clone());
        server_entry.admin_password_hash.clone_from(&user_entry.admin_password_hash);

        // if online, update live
        let result = if let Some(thread) = thread.as_ref() {
            let is_banned = new_user_entry.is_banned;
            let is_muted = new_user_entry.is_muted;

            // update the role
            if c_user_roles {
                let special_data = SpecialUserData::from_roles(&new_user_entry.user_roles, &self.game_server.state.role_manager);
                thread.account_data.lock().special_user_data.clone_from(&special_data);

                // tell the user that their roles changed
                thread
                    .push_new_message(ServerThreadMessage::BroadcastRoleChange(RolesUpdatedPacket {
                        special_user_data: special_data,
                    }))
                    .await;

                let new_role = self.game_server.state.role_manager.compute(&new_user_entry.user_roles);
                *thread.user_role.lock() = new_role;
            }

            let res = self
                .game_server
                .update_user(thread, |user| {
                    user.clone_from(&server_entry);
                    true
                })
                .await;

            // if they just got banned, disconnect them
            if c_is_banned && is_banned && res.is_ok() {
                thread
                    .push_new_message(ServerThreadMessage::BroadcastBan(ServerBannedPacket {
                        message: FastString::new(&new_user_entry.violation_reason.clone().unwrap_or_default()),
                        timestamp: new_user_entry.violation_expiry.unwrap_or(0),
                    }))
                    .await;
            }

            if c_is_muted && is_muted && res.is_ok() {
                thread
                    .push_new_message(ServerThreadMessage::BroadcastMute(ServerMutedPacket {
                        reason: FastString::new(&new_user_entry.violation_reason.clone().unwrap_or_default()),
                        timestamp: new_user_entry.violation_expiry.unwrap_or(0),
                    }))
                    .await;
            }

            res
        } else {
            // otherwise just make a manual bridge request
            self.game_server.bridge.update_user_data(&server_entry).await.map_err(Into::into)
        };

        match result {
            Ok(()) => {
                let own_name = self.account_data.lock().name.try_to_string();

                info!(
                    "[{} @ {}] just updated the profile of {} ({})",
                    own_name,
                    self.get_tcp_peer(),
                    target_user_name,
                    target_account_id
                );

                if self.game_server.bridge.has_admin_webhook() {
                    // this is crazy
                    let mut messages = FastVec::<WebhookMessage, 4>::new();

                    if c_is_banned {
                        let bmsc = BanMuteStateChange {
                            mod_name: own_name.clone(),
                            target_name: target_user_name.clone(),
                            target_id: new_user_entry.account_id,
                            new_state: new_user_entry.is_banned,
                            expiry: new_user_entry.violation_expiry,
                            reason: new_user_entry.violation_reason.clone(),
                        };

                        messages.push(WebhookMessage::UserBanChanged(bmsc));
                    } else if c_is_muted {
                        let bmsc = BanMuteStateChange {
                            mod_name: own_name.clone(),
                            target_name: target_user_name.clone(),
                            target_id: new_user_entry.account_id,
                            new_state: new_user_entry.is_muted,
                            expiry: new_user_entry.violation_expiry,
                            reason: new_user_entry.violation_reason.clone(),
                        };

                        messages.push(WebhookMessage::UserMuteChanged(bmsc));
                    } else if c_violation_expiry || c_violation_reason {
                        messages.push(WebhookMessage::UserViolationMetaChanged(
                            own_name.clone(),
                            target_user_name.clone(),
                            new_user_entry.is_banned,
                            new_user_entry.is_muted,
                            new_user_entry.violation_expiry,
                            new_user_entry.violation_reason.clone(),
                        ));
                    }

                    if c_user_roles {
                        messages.push(WebhookMessage::UserRolesChanged(
                            own_name.clone(),
                            target_user_name.clone(),
                            user_entry.user_roles.clone(),
                            new_user_entry.user_roles.clone(),
                        ));
                    }

                    if c_name_color {
                        messages.push(WebhookMessage::UserNameColorChanged(
                            own_name.clone(),
                            target_user_name.clone(),
                            user_entry.name_color.clone(),
                            new_user_entry.name_color.clone(),
                        ));
                    }

                    if let Err(err) = self.game_server.bridge.send_webhook_messages(&messages, WebhookChannel::Admin).await {
                        warn!("webhook error during roles changed: {err}");
                    }
                }

                self.send_packet_dynamic(&AdminSuccessMessagePacket {
                    message: "Successfully updated the user",
                })
                .await
            }
            Err(err) => {
                warn!("error from bridge: {err}");
                admin_error!(self, &err.to_string());
            }
        }
    });

    gs_handler!(self, handle_admin_send_featured_level, AdminSendFeaturedLevelPacket, packet, {
        let account_id = gs_needauth!(self);
        let self_name = self.account_data.lock().name.try_to_string();

        if !self._has_perm(AdminPerm::Any) {
            return Ok(());
        }

        if let Err(err) = self
            .game_server
            .bridge
            .send_rate_suggestion_webhook_message(WebhookMessage::FeaturedLevelSend(
                account_id,
                self_name,
                packet.level_name.to_string(),
                packet.level_id,
                packet.level_author.to_string(),
                packet.difficulty,
                packet.rate_tier,
                packet.notes.map(|x| x.to_string()),
            ))
            .await
        {
            warn!("webhook error during send featured: {err}");
        }

        return Ok(());
    });
}
