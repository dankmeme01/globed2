#pragma once
#include "packet.hpp"

// Matches a packet by packet ID, returns nullptr if not found. Otherwise returns an Packet pointer with uninitialized data
std::shared_ptr<Packet> matchPacket(packetid_t packetId);