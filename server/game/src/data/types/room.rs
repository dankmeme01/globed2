use crate::data::*;

#[derive(Clone, Copy, Default, Encodable, Decodable, StaticSize, DynamicSize, Debug)]
#[bitfield(on = true, size = 2)]
#[allow(clippy::struct_excessive_bools)]
pub struct RoomSettingsFlags {
    pub is_hidden: bool,
    pub public_invites: bool,
    pub collision: bool,
    pub two_player: bool,
    pub deathlink: bool,
}

#[derive(Clone, Copy, Default, Encodable, Decodable, StaticSize, DynamicSize, Debug)]
#[dynamic_size(as_static = true)]
pub struct RoomSettings {
    pub flags: RoomSettingsFlags,
    pub player_limit: u16,
    pub level_id: LevelId,
    pub faster_reset: bool,
}

#[derive(Clone, Encodable, Decodable, StaticSize, DynamicSize)]
#[dynamic_size(as_static = true)]
pub struct RoomInfo {
    pub id: u32,
    pub owner: PlayerPreviewAccountData,
    pub name: InlineString<32>,
    pub password: InlineString<16>,
    pub settings: RoomSettings,
}

#[derive(Clone, Encodable, Decodable, StaticSize, DynamicSize)]
#[dynamic_size(as_static = true)]
pub struct RoomListingInfo {
    pub id: u32,
    pub player_count: u16,
    pub owner: PlayerPreviewAccountData,
    pub name: InlineString<32>,
    pub has_password: bool,
    pub settings: RoomSettings,
}
