use std::sync::{atomic::Ordering, Arc};

use crate::{
    data::packets::PacketHeader,
    server_thread::{GameServerThread, PacketHandlingError, Result},
};

use super::*;
use crate::data::*;

/// max voice throughput in kb/s
pub const MAX_VOICE_THROUGHPUT: usize = 8;
/// max voice packet size in bytes
pub const MAX_VOICE_PACKET_SIZE: usize = 4096;

impl GameServerThread {
    gs_handler!(self, handle_request_profiles, RequestProfilesPacket, packet, {
        gs_needauth!(self);

        // slice it until the first zero
        let player_count = packet.ids.iter().position(|&x| x == 0).unwrap_or(packet.ids.len());
        let ids = &packet.ids[..player_count];

        let account_id = self.account_id.load(Ordering::Relaxed);

        let encoded_size = size_of_types!(PlayerAccountData) * player_count;

        gs_inline_encode!(self, encoded_size, buf, {
            buf.write_packet_header::<PlayerProfilesPacket>();
            buf.write_u32(player_count as u32);

            let written = self.game_server.for_each_player(
                ids,
                move |preview, count, buf| {
                    // we do additional length check because player count may have increased since then
                    if count < player_count && preview.account_id != account_id {
                        buf.write_value(preview);
                        true
                    } else {
                        false
                    }
                },
                &mut buf,
            );

            // if the written count is less than requested, we now lied and the client will fail decoding. re-encode the actual count.
            if written != player_count {
                buf.set_pos(PacketHeader::SIZE);
                buf.write_u32(written as u32);
            }
        });

        Ok(())
    });

    gs_handler!(self, handle_level_join, LevelJoinPacket, packet, {
        gs_needauth!(self);

        let account_id = self.account_id.load(Ordering::Relaxed);
        let old_level = self.level_id.swap(packet.level_id, Ordering::Relaxed);

        let mut pm = self.game_server.state.player_manager.lock();

        if old_level != 0 {
            pm.remove_from_level(old_level, account_id);
        }

        pm.add_to_level(packet.level_id, account_id);

        Ok(())
    });

    gs_handler!(self, handle_level_leave, LevelLeavePacket, _packet, {
        gs_needauth!(self);

        let level_id = self.level_id.swap(0, Ordering::Relaxed);
        if level_id != 0 {
            let account_id = self.account_id.load(Ordering::Relaxed);

            let mut pm = self.game_server.state.player_manager.lock();
            pm.remove_from_level(level_id, account_id);
        }

        Ok(())
    });

    gs_handler!(self, handle_player_data, PlayerDataPacket, packet, {
        gs_needauth!(self);

        let level_id = self.level_id.load(Ordering::Relaxed);
        if level_id == 0 {
            return Err(PacketHandlingError::UnexpectedPlayerData);
        }

        let account_id = self.account_id.load(Ordering::Relaxed);

        let written_players = {
            let mut pm = self.game_server.state.player_manager.lock();
            pm.set_player_data(account_id, &packet.data);
            // this unwrap should be safe and > 0 given that self.level_id != 0, but we leave a default just in case
            pm.get_player_count_on_level(level_id).unwrap_or(1) - 1
        };

        // no one else on the level, no need to send a response packet
        if written_players == 0 {
            return Ok(());
        }

        let calc_size = size_of_types!(PacketHeader, u32) + size_of_types!(AssociatedPlayerData) * written_players;

        gs_inline_encode!(self, calc_size, buf, {
            buf.write_packet_header::<LevelDataPacket>();
            buf.write_u32(written_players as u32);

            // this is scary
            let written = self.game_server.state.player_manager.lock().for_each_player_on_level(
                level_id,
                |player, count, buf| {
                    // we do additional length check because player count may have increased since 1st lock
                    if count < written_players && player.data.account_id != account_id {
                        buf.write_value(&player.data);
                        true
                    } else {
                        false
                    }
                },
                &mut buf,
            );

            // if the player count has instead decreased, we now lied and the client will fail decoding. re-encode the actual count.
            if written != written_players {
                buf.set_pos(PacketHeader::SIZE);
                buf.write_u32(written as u32);
            }
        });

        Ok(())
    });

    gs_handler!(self, handle_sync_player_metadata, SyncPlayerMetadataPacket, packet, {
        gs_needauth!(self);

        let level_id = self.level_id.load(Ordering::Relaxed);
        if level_id == 0 {
            return Err(PacketHandlingError::UnexpectedPlayerData);
        }

        let account_id = self.account_id.load(Ordering::Relaxed);

        let written_players = {
            let mut pm = self.game_server.state.player_manager.lock();
            pm.set_player_metadata(account_id, &packet.data);
            // this unwrap should be safe and > 0 given that self.level_id != 0, but we leave a default just in case
            pm.get_player_count_on_level(level_id).unwrap_or(1) - 1
        };

        // no one else on the level, no need to send a response packet
        if written_players == 0 {
            return Ok(());
        }

        let calc_size = size_of_types!(PacketHeader, u32) + size_of_types!(AssociatedPlayerMetadata) * written_players;

        gs_inline_encode!(self, calc_size, buf, {
            buf.write_packet_header::<PlayerMetadataPacket>();
            buf.write_u32(written_players as u32);

            let written = self.game_server.state.player_manager.lock().for_each_player_on_level(
                level_id,
                |player, count, buf| {
                    // we do additional length check because player count may have changed since 1st lock
                    if count < written_players && player.data.account_id != account_id {
                        buf.write_value(&player.meta);
                        true
                    } else {
                        false
                    }
                },
                &mut buf,
            );

            // if the player count has instead decreased, we now lied and the client will fail decoding. re-encode the actual count.
            if written != written_players {
                buf.set_pos(PacketHeader::SIZE);
                buf.write_u32(written as u32);
            }
        });

        Ok(())
    });

    gs_handler!(self, handle_voice, VoicePacket, packet, {
        gs_needauth!(self);

        let accid = self.account_id.load(Ordering::Relaxed);

        let vpkt = Arc::new(VoiceBroadcastPacket {
            player_id: accid,
            data: packet.data,
        });

        self.game_server
            .broadcast_voice_packet(&vpkt, self.level_id.load(Ordering::Relaxed))?;

        Ok(())
    });

    gs_handler!(self, handle_chat_message, ChatMessagePacket, packet, {
        gs_needauth!(self);

        let accid = self.account_id.load(Ordering::Relaxed);

        let cpkt = ChatMessageBroadcastPacket {
            player_id: accid,
            message: packet.message,
        };

        self.game_server
            .broadcast_chat_packet(&cpkt, self.level_id.load(Ordering::Relaxed))?;

        Ok(())
    });
}