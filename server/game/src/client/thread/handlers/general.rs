use super::*;

impl ClientThread {
    gs_handler!(self, handle_sync_icons, SyncIconsPacket, packet, {
        let _ = gs_needauth!(self);

        self.account_data.lock().icons.clone_from(&packet.icons);
        Ok(())
    });

    gs_handler!(self, handle_request_global_list, RequestGlobalPlayerListPacket, _packet, {
        let _ = gs_needauth!(self);

        self.send_packet_dynamic(&GlobalPlayerListPacket {
            players: self.game_server.get_player_previews_in_room(0, self.can_moderate()),
        })
        .await
    });

    gs_handler!(self, handle_request_level_list, RequestLevelListPacket, _packet, {
        let _ = gs_needauth!(self);

        let room_id = self.room_id.load(Ordering::Relaxed);

        let levels = self.game_server.state.room_manager.with_any(room_id, |pm| {
            let mut vec = Vec::with_capacity(pm.manager.get_level_count());

            pm.manager.for_each_level(|level_id, level| {
                if !level.unlisted && !is_editorcollab_level(level_id) {
                    vec.push(GlobedLevel {
                        level_id,
                        player_count: level.players.len() as u16,
                    });
                }
            });

            vec
        });

        self.send_packet_dynamic(&LevelListPacket { levels }).await
    });

    gs_handler!(self, handle_request_player_count, RequestPlayerCountPacket, packet, {
        let _ = gs_needauth!(self);

        let room_id = self.room_id.load(Ordering::Relaxed);

        let levels = self.game_server.state.room_manager.with_any(room_id, |pm| {
            let mut levels = Vec::with_capacity(0);

            for &level_id in &*packet.level_ids {
                levels.push((level_id, pm.manager.get_player_count_on_level(level_id).unwrap_or(0) as u16));
            }

            levels
        });

        self.send_packet_dynamic(&LevelPlayerCountPacket { levels }).await
    });

    gs_handler!(self, handle_set_player_status, UpdatePlayerStatusPacket, packet, {
        let account_id = gs_needauth!(self);

        let mut p = self.privacy_settings.lock();
        p.clone_from(&packet.flags);

        // only mods can hide roles
        let is_mod = self.is_authorized_user.load(Ordering::Relaxed);

        if !is_mod {
            p.set_hide_roles(false);
        }

        let room_id = self.room_id.load(Ordering::Relaxed);

        self.game_server.state.room_manager.with_any(room_id, |pm| {
            if let Some(player) = pm.manager.get_player_data_mut(account_id) {
                player.is_invisible = packet.flags.get_hide_in_game();
            }
        });

        Ok(())
    });

    gs_handler!(self, handle_link_code_request, LinkCodeRequestPacket, _packet, {
        let _ = gs_needauth!(self);

        let link_code = self.link_code.load(Ordering::Relaxed);

        self.send_packet_static(&LinkCodeResponsePacket { link_code }).await
    });
}
