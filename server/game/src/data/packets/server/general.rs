use crate::data::*;

/*
* For optimization reasons, most of those packets are encoded inline, and their structure is not present here.
*/

#[derive(Packet, Encodable)]
#[packet(id = 21000, tcp = true)]
pub struct GlobalPlayerListPacket; // definition intentionally missing

#[derive(Packet, Encodable, StaticSize)]
#[packet(id = 21001, tcp = false)]
pub struct RoomCreatedPacket {
    pub info: RoomInfo,
}

#[derive(Packet, Encodable, StaticSize)]
#[packet(id = 21002, tcp = false)]
pub struct RoomJoinedPacket;

#[derive(Packet, Encodable, DynamicSize)]
#[packet(id = 21003, tcp = false)]
pub struct RoomJoinFailedPacket<'a> {
    pub message: &'a str,
}

#[derive(Packet, Encodable)]
#[packet(id = 21004, tcp = true)]
pub struct RoomPlayerListPacket; // definition intentionally missing

#[derive(Packet, Encodable)]
#[packet(id = 21005, tcp = true)]
pub struct LevelListPacket; // definition intentionally missing

#[derive(Packet, Encodable)]
#[packet(id = 21006)]
pub struct LevelPlayerCountPacket; // definition intentionally missing

#[derive(Packet, Encodable, StaticSize, Clone)]
#[packet(id = 21007)]
pub struct RoomInfoPacket {
    pub info: RoomInfo,
}

#[derive(Packet, Encodable, StaticSize, Clone)]
#[packet(id = 21008)]
pub struct RoomInvitePacket {
    pub player_data: PlayerRoomPreviewAccountData,
    pub room_id: u32,
    pub room_token: u32,
}
