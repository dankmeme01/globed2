use crate::data::*;

/*
* For optimization reasons, most of those packets are encoded inline, and their structure is not present here.
*/

#[derive(Packet, Encodable, DynamicSize)]
#[packet(id = 21000, tcp = true)]
pub struct GlobalPlayerListPacket {
    pub players: Vec<PlayerPreviewAccountData>,
}

#[derive(Packet, Encodable, DynamicSize)]
#[packet(id = 21001, tcp = true)]
pub struct LevelListPacket {
    pub levels: Vec<GlobedLevel>,
}

#[derive(Packet, Encodable, DynamicSize)]
#[packet(id = 21002)]
pub struct LevelPlayerCountPacket {
    pub levels: Vec<(LevelId, u16)>,
}

#[derive(Packet, Encodable, StaticSize, Clone)]
#[packet(id = 21003)]
pub struct RolesUpdatedPacket {
    pub special_user_data: SpecialUserData,
}

#[derive(Packet, Encodable, StaticSize)]
#[packet(id = 21004)]
pub struct LinkCodeResponsePacket {
    pub link_code: u32,
}
