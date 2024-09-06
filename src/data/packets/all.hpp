/*
* To add a new packet, your chores are:
* 1. find the appropriate category that suits it and make a proper packet ID
*   client/ - 1xxxx, server/ - 2xxxx
*     connection.hpp - x0xxx - connection-related (encryption, keepalive, ping, handshake)
*     general.hpp - x1xxx - general packets (rooms, level list)
*     game.hpp - x2xxx - level-related (player data, chat, account data)
*     admin.hpp - x9xxx - admin-related packets
*     misc.hpp - other misc stuff
*
* 2. in your class, inherit Packet and add GLOBED_PACKET(id, encrypt), encrypt should be true for packets that are sensitive.
* 3. add the GLOBED_ENCODE or GLOBED_DECODE method
* 4. For client packets, you may also choose to add a ::create(...) function and/or a constructor
* 5. For server packets, in `all.cpp` add the packet to the switch as PACKET(cls).
*/

#pragma once
#include "packet.hpp"

#include "client/admin.hpp"
#include "client/connection.hpp"
#include "client/game.hpp"
#include "client/general.hpp"
#include "client/misc.hpp"
#include "client/room.hpp"

#include "server/admin.hpp"
#include "server/connection.hpp"
#include "server/game.hpp"
#include "server/general.hpp"
#include "server/room.hpp"

#include "match.hpp"