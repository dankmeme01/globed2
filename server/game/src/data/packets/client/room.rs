use crate::data::*;

#[derive(Packet, Decodable)]
#[packet(id = 13000)]
pub struct CreateRoomPacket {
    pub room_name: InlineString<32>,
    pub password: InlineString<16>,
    pub settings: RoomSettings,
}

#[derive(Packet, Decodable)]
#[packet(id = 13001)]
pub struct JoinRoomPacket {
    pub room_id: u32,
    pub password: InlineString<16>,
}

#[derive(Packet, Decodable)]
#[packet(id = 13002)]
pub struct LeaveRoomPacket;

#[derive(Packet, Decodable)]
#[packet(id = 13003)]
pub struct RequestRoomPlayerListPacket;

#[derive(Packet, Decodable)]
#[packet(id = 13004)]
pub struct UpdateRoomSettingsPacket {
    pub settings: RoomSettings,
}

#[derive(Packet, Decodable)]
#[packet(id = 13005)]
pub struct RoomSendInvitePacket {
    pub player: i32,
}

#[derive(Packet, Decodable)]
#[packet(id = 13006)]
pub struct RequestRoomListPacket;

#[derive(Packet, Decodable)]
#[packet(id = 13007)]
pub struct CloseRoomPacket {
    pub room_id: u32,
}

#[derive(Packet, Decodable)]
#[packet(id = 13008)]
pub struct KickRoomPlayerPacket {
    pub player: i32,
}
