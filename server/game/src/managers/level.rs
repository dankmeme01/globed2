use globed_shared::IntMap;

use crate::data::{
    types::PlayerData, AssociatedPlayerData, AssociatedPlayerMetadata, BorrowedAssociatedPlayerData, BorrowedAssociatedPlayerMetadata, LevelId,
    PlayerMetadata,
};

#[derive(Default)]
pub struct LevelManagerPlayer {
    pub account_id: i32,
    pub data: PlayerData,
    pub meta: PlayerMetadata,
    pub is_invisible: bool,
}

impl LevelManagerPlayer {
    pub fn to_associated_data(&self) -> AssociatedPlayerData {
        AssociatedPlayerData {
            account_id: self.account_id,
            data: self.data.clone(),
        }
    }

    pub fn to_borrowed_associated_data(&self) -> BorrowedAssociatedPlayerData {
        BorrowedAssociatedPlayerData {
            account_id: self.account_id,
            data: &self.data,
        }
    }

    pub fn to_associated_meta(&self) -> AssociatedPlayerMetadata {
        AssociatedPlayerMetadata {
            account_id: self.account_id,
            data: self.meta.clone(),
        }
    }

    pub fn to_borrowed_associated_meta(&self) -> BorrowedAssociatedPlayerMetadata {
        BorrowedAssociatedPlayerMetadata {
            account_id: self.account_id,
            data: &self.meta,
        }
    }
}

#[derive(Default)]
pub struct Level {
    pub players: Vec<i32>,
    pub unlisted: bool,
}

// Manages an entire room (all levels and players inside of it).
#[derive(Default)]
pub struct LevelManager {
    pub players: IntMap<i32, LevelManagerPlayer>, // player id : associated data
    pub levels: IntMap<LevelId, Level>,           // level id : [player id]
}

impl LevelManager {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn get_player_data(&self, account_id: i32) -> Option<&LevelManagerPlayer> {
        self.players.get(&account_id)
    }

    pub fn get_player_data_mut(&mut self, account_id: i32) -> Option<&mut LevelManagerPlayer> {
        self.players.get_mut(&account_id)
    }

    pub fn create_player(&mut self, account_id: i32, invisible: bool) {
        self.players.insert(
            account_id,
            LevelManagerPlayer {
                account_id,
                is_invisible: invisible,
                ..Default::default()
            },
        );
    }

    fn get_or_create_player(&mut self, account_id: i32) -> &mut LevelManagerPlayer {
        self.players.entry(account_id).or_insert_with(|| LevelManagerPlayer {
            account_id,
            ..Default::default()
        })
    }

    /// set player's data, inserting a new entry if doesn't already exist
    pub fn set_player_data(&mut self, account_id: i32, data: &PlayerData) {
        self.get_or_create_player(account_id).data.clone_from(data);
    }

    /// set player's metadata, inserting a new entry if it doesn't already exist
    pub fn set_player_meta(&mut self, account_id: i32, meta: &PlayerMetadata) {
        self.get_or_create_player(account_id).meta.clone_from(meta);
    }

    /// remove the player from the list of players
    pub fn remove_player(&mut self, account_id: i32) {
        self.players.remove(&account_id);
    }

    #[inline]
    pub fn has_player(&self, account_id: i32) -> bool {
        self.players.contains_key(&account_id)
    }

    /// get a reference to a list of account IDs of players on a level given its ID
    pub fn get_level(&self, level_id: LevelId) -> Option<&Level> {
        self.levels.get(&level_id)
    }

    /// get amount of levels in the room
    pub fn get_level_count(&self) -> usize {
        self.levels.len()
    }

    /// get the amount of players on a level given its ID
    pub fn get_player_count_on_level(&self, level_id: LevelId) -> Option<usize> {
        self.levels.get(&level_id).map(|x| x.players.len())
    }

    /// get the total amount of players
    pub fn get_total_player_count(&self) -> usize {
        self.players.len()
    }

    /// run a function `f` on each player on a level given its ID, with possibility to pass additional data
    #[inline]
    pub fn for_each_player_on_level<F: FnMut(&LevelManagerPlayer)>(&self, level_id: LevelId, f: F) {
        if let Some(level) = self.levels.get(&level_id) {
            level.players.iter().filter_map(|&key| self.players.get(&key)).for_each(f);
        }
    }

    /// run a function `f` on each player in this `PlayerManager`, with possibility to pass additional data
    pub fn for_each_player<F: FnMut(&LevelManagerPlayer)>(&self, f: F) {
        self.players.values().for_each(f);
    }

    /// run a function `f` on each level in this `PlayerManager`, with possibility to pass additional data
    pub fn for_each_level<F: FnMut(LevelId, &Level)>(&self, mut f: F) {
        self.levels.iter().for_each(|(id, level)| f(*id, level));
    }

    /// add a player to a level given a level ID and an account ID
    pub fn add_to_level(&mut self, level_id: LevelId, account_id: i32, unlisted: bool) {
        let level = self.levels.entry(level_id).or_insert_with(|| Level {
            players: Vec::with_capacity(8),
            unlisted,
        });

        if !level.players.contains(&account_id) {
            level.players.push(account_id);
        }

        level.unlisted = unlisted;
    }

    /// remove a player from a level given a level ID and an account ID
    pub fn remove_from_level(&mut self, level_id: LevelId, account_id: i32) {
        let should_remove_level = self.levels.get_mut(&level_id).is_some_and(|level| {
            if let Some(index) = level.players.iter().position(|&x| x == account_id) {
                level.players.remove(index);
            }

            level.players.is_empty()
        });

        if should_remove_level {
            self.levels.remove(&level_id);
        }
    }
}
