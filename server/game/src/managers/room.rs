use std::sync::OnceLock;

use esp::InlineString;
use globed_shared::{
    rand::{self, Rng},
    IntMap, SyncMutex, SyncMutexGuard,
};

use crate::{
    data::{LevelId, PlayerPreviewAccountData, RoomInfo, RoomListingInfo, RoomSettings, ROOM_ID_LENGTH},
    server::GameServer,
};

use super::LevelManager;

#[derive(Default)]
pub struct Room {
    pub orig_owner: i32,
    pub orig_owner_data: Option<PlayerPreviewAccountData>,
    pub owner: i32,
    pub owner_data: Option<PlayerPreviewAccountData>,
    pub name: InlineString<32>,
    pub password: InlineString<16>,
    pub manager: LevelManager,
    pub settings: RoomSettings,
}

#[derive(Default)]
pub struct RoomManager {
    rooms: SyncMutex<IntMap<u32, Room>>,
    global: SyncMutex<Room>,
    game_server: OnceLock<&'static GameServer>,
}

// i.e. if ROOM_ID_LENGTH is 6 we should have a range 100_000..1_000_000
const ROOM_ID_START: u32 = 10_u32.pow(ROOM_ID_LENGTH as u32 - 1);
const ROOM_ID_END: u32 = 10_u32.pow(ROOM_ID_LENGTH as u32);

impl Room {
    pub fn new(
        owner: i32,
        owner_data: Option<PlayerPreviewAccountData>,
        name: InlineString<32>,
        password: InlineString<16>,
        settings: RoomSettings,
        manager: LevelManager,
    ) -> Self {
        Self {
            orig_owner: owner,
            orig_owner_data: owner_data.clone(),
            owner,
            owner_data,
            name,
            password,
            manager,
            settings,
        }
    }

    // Removes a player, if the player was the owner, rotates the owner and returns `true`.
    pub fn remove_player(&mut self, player: i32) -> bool {
        let was_owner = self.owner == player;

        if was_owner {
            // rotate the owner
            let mut rotate_to: i32 = 0;
            self.manager.for_each_player(|rp| {
                if rotate_to == 0 && rp.account_id != player {
                    rotate_to = rp.account_id;
                }
            });

            self.owner = rotate_to;
        }

        self.manager.remove_player(player);

        was_owner
    }

    #[inline]
    pub fn has_player(&self, player: i32) -> bool {
        self.manager.has_player(player)
    }

    #[inline]
    pub fn set_settings(&mut self, settings: RoomSettings) {
        self.settings = settings;
    }

    #[inline]
    pub fn get_room_info(&self, id: u32) -> RoomInfo {
        RoomInfo {
            id,
            name: self.name.clone(),
            password: self.password.clone(),
            owner: self.owner_data.clone().unwrap_or_default(),
            settings: self.settings,
        }
    }

    #[inline]
    pub fn get_room_listing_info(&self, id: u32) -> RoomListingInfo {
        RoomListingInfo {
            id,
            player_count: self.manager.get_total_player_count() as u16,
            name: self.name.clone(),
            has_password: !self.password.is_empty(),
            owner: self.owner_data.clone().unwrap_or_default(),
            settings: self.settings,
        }
    }

    pub fn is_hidden(&self) -> bool {
        self.settings.flags.is_hidden
    }

    pub fn is_public_invites(&self) -> bool {
        self.settings.flags.public_invites
    }

    pub fn is_two_player_mode(&self) -> bool {
        self.settings.flags.two_player
    }

    pub fn is_protected(&self) -> bool {
        !self.password.is_empty()
    }

    pub fn verify_password(&self, pwd: &InlineString<16>) -> bool {
        self.password.is_empty() || self.password == *pwd
    }

    pub fn is_full(&self) -> bool {
        let player_count = self.manager.get_total_player_count();

        if self.settings.flags.two_player {
            player_count >= 2
        } else if self.settings.player_limit == 0 {
            false
        } else {
            player_count >= (self.settings.player_limit as usize)
        }
    }
}

impl RoomManager {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn set_game_server(&self, game_server: &'static GameServer) {
        self.game_server.set(game_server).ok().expect("set_game_server failed");
    }

    #[inline]
    fn get_game_server(&self) -> &'static GameServer {
        self.game_server
            .get()
            .expect("get_game_server called without a previous call to set_game_server")
    }

    /// Try to find a room by given ID, if equal to 0 or not found, runs the provided closure with the global room
    pub fn with_any<F: FnOnce(&mut Room) -> R, R>(&self, room_id: u32, f: F) -> R {
        if room_id == 0 {
            f(&mut self.get_global())
        } else if let Some(room) = self.get_rooms().get_mut(&room_id) {
            f(room)
        } else {
            f(&mut self.get_global())
        }
    }

    /// Try to find a room by given ID and run the provided function on it. If not found, calls `default`.
    pub fn try_with_any<F: FnOnce(&mut Room) -> R, D: FnOnce() -> R, R>(&self, room_id: u32, f: F, default: D) -> R {
        if room_id == 0 {
            f(&mut self.get_global())
        } else if let Some(room) = self.get_rooms().get_mut(&room_id) {
            f(room)
        } else {
            default()
        }
    }

    pub fn get_global(&self) -> SyncMutexGuard<'_, Room> {
        self.global.lock()
    }

    pub fn get_rooms(&self) -> SyncMutexGuard<'_, IntMap<u32, Room>> {
        self.rooms.lock()
    }

    /// Creates a new room, adds the given player, removes them from the global room, and returns the room ID
    pub fn create_room(&self, account_id: i32, name: InlineString<32>, password: InlineString<16>, settings: RoomSettings) -> RoomInfo {
        let rooms = self.rooms.lock();

        // in case we accidentally generate an existing room id, keep looping until we find a suitable id
        let room_id = loop {
            let room_id = rand::thread_rng().gen_range(ROOM_ID_START..ROOM_ID_END);
            if !rooms.contains_key(&room_id) {
                break room_id;
            }
        };

        drop(rooms);

        let room = self._create_room(room_id, account_id, name, password, settings);

        self.get_global().remove_player(account_id); // dont worry about setting owner data here

        room
    }

    pub fn is_valid_room(&self, room_id: u32) -> bool {
        self.rooms.lock().contains_key(&room_id)
    }

    /// Deletes a room if there are no players in it
    pub fn maybe_remove_room(&self, room_id: u32) {
        let mut rooms = self.rooms.lock();

        let to_remove = rooms.get(&room_id).is_some_and(|room| room.manager.get_total_player_count() == 0);

        if to_remove {
            rooms.remove(&room_id);
        }
    }

    // Removes the player from the given room, returns `true` if the player was the owner of the room,
    // and either a new owner has now been chosen, or the room has been deleted.
    pub fn remove_with_any(&self, room_id: u32, account_id: i32, level_id: LevelId) -> bool {
        let was_owner = self.with_any(room_id, |pm| {
            let was_owner = pm.remove_player(account_id);

            // refresh owner data to newest
            pm.owner_data = self.get_game_server().get_player_preview_data(pm.owner);

            if level_id != 0 {
                pm.manager.remove_from_level(level_id, account_id);
            }

            was_owner
        });

        // delete the room if there are no more players there
        if room_id != 0 {
            self.maybe_remove_room(room_id);
        }

        was_owner
    }

    pub fn get_room_info(&self, room_id: u32) -> Option<RoomInfo> {
        self.try_with_any(room_id, |room| Some(room.get_room_info(room_id)), || None)
    }

    fn _create_room(&self, room_id: u32, owner: i32, name: InlineString<32>, password: InlineString<16>, settings: RoomSettings) -> RoomInfo {
        let mut rooms = self.rooms.lock();

        let mut pm = LevelManager::new();
        if owner != 0 {
            let owner_thread = self.get_game_server().get_user_by_id(owner);
            let is_invisible = owner_thread.map(|thr| thr.privacy_settings.lock().get_hide_in_game()).unwrap_or(false);

            pm.create_player(owner, is_invisible);
        }

        let owner_data = self.get_game_server().get_player_preview_data(owner);

        let room = Room::new(owner, owner_data, name, password, settings, pm);
        let room_info = room.get_room_info(room_id);

        rooms.insert(room_id, room);

        room_info
    }
}
