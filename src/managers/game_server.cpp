#include "game_server.hpp"

#include <util/net.hpp>
#include <util/rng.hpp>
#include <util/collections.hpp>
#include <net/manager.hpp>
#include <data/types/misc.hpp>

using namespace geode::prelude;

GameServerManager::GameServerManager() {
    this->updateCache(Mod::get()->getSavedValue<std::string>(SERVER_RESPONSE_CACHE_KEY));
}

Result<> GameServerManager::addServer(const std::string_view serverId, const std::string_view name, const std::string_view address, const std::string_view region) {
    _data.lock()->servers.erase(std::string(serverId));
    GLOBED_UNWRAP(this->addOrUpdateServer(serverId, name, address, region));
    return Ok();
}

Result<bool> GameServerManager::addOrUpdateServer(const std::string_view serverId_, const std::string_view name, const std::string_view address, const std::string_view region) {
    auto serverId = std::string(serverId_);

    int ping = -1;
    uint16_t playerCount = 0;

    auto data = _data.lock();
    if (data->servers.contains(serverId)) {
        auto& server = data->servers.at(serverId).server;

        ping = server.ping;
        playerCount = server.playerCount;

        // check if the server changed
        if (
            server.id == serverId_
            && server.name == name
            && server.region == region
            && server.address == address
        ) {
            return Ok(false);
        }
    }

    GameServer server = {
        .id = std::string(serverId),
        .name = std::string(name),
        .region = std::string(region),
        .address = std::string(address),
        .ping = ping,
        .playerCount = playerCount,
    };

    GameServerManager::GameServerData gsdata = {
        .server = server
    };

    data->servers[serverId] = gsdata;

    return Ok(true);
}

void GameServerManager::clear() {
    _data.lock()->servers.clear();
}

size_t GameServerManager::count() {
    return _data.lock()->servers.size();
}

void GameServerManager::setActive(const std::string_view id) {
    auto idstr = std::string(id);
    auto data = _data.lock();
    if (!data->servers.contains(idstr)) return;

    data->active = id;

    this->saveLastConnected(id);
}

void GameServerManager::clearActive() {
    auto data = _data.lock();
    data->active.clear();

    this->saveLastConnected("");
}

void GameServerManager::clearAllExcept(const std::string_view id) {
    auto data = _data.lock();

    auto key = std::string(id);
    auto value = data->servers.at(key);
    data->servers.clear();
    data->servers.emplace(std::move(key), std::move(value));
}

std::string GameServerManager::getActiveId() {
    return _data.lock()->active;
}

std::optional<GameServer> GameServerManager::getActiveServer() {
    auto active = this->getActiveId();
    if (active.empty()) {
        return std::nullopt;
    }

    return this->getServer(active);
}

std::optional<GameServer> GameServerManager::getServer(const std::string_view id_) {
    std::string id(id_);

    auto data = _data.lock();
    if (!data->servers.contains(id)) {
        return std::nullopt;
    }

    return data->servers.at(id).server;
}

std::unordered_map<std::string, GameServer> GameServerManager::getAllServers() {
    auto data = _data.lock();

    std::unordered_map<std::string, GameServer> out;

    for (const auto& [_, gsd] : data->servers) {
        out.insert(std::make_pair(gsd.server.id, gsd.server));
    }

    return out;
}

int GameServerManager::getActivePing() {
    auto server = this->getActiveServer();
    GLOBED_REQUIRE(server.has_value(), "tried to request ping of the active server when not connected to any")

    return server.value().ping;
}

void GameServerManager::saveStandalone(const std::string_view addr) {
    Mod::get()->setSavedValue(STANDALONE_SETTING_KEY, std::string(addr));
}

std::string GameServerManager::loadStandalone() {
    return Mod::get()->getSavedValue<std::string>(STANDALONE_SETTING_KEY);
}

void GameServerManager::saveLastConnected(const std::string_view addr) {
    Mod::get()->setSavedValue(LAST_CONNECTED_SETTING_KEY, std::string(addr));
}

std::string GameServerManager::loadLastConnected() {
    return Mod::get()->getSavedValue<std::string>(LAST_CONNECTED_SETTING_KEY);
}

void GameServerManager::updateCache(const std::string_view response) {
    _data.lock()->cachedServerResponse = response;
    Mod::get()->setSavedValue(SERVER_RESPONSE_CACHE_KEY, std::string(response));
}

void GameServerManager::clearCache() {
    this->updateCache("");
}

Result<> GameServerManager::loadFromCache() {
    const size_t MAGIC_LEN = sizeof(NetworkManager::SERVER_MAGIC);

    std::string response = _data.lock()->cachedServerResponse;

    GLOBED_UNWRAP_INTO(util::crypto::base64Decode(response), auto decoded)

    GLOBED_REQUIRE_SAFE(decoded.size() >= MAGIC_LEN, fmt::format("invalid response sent by the server (missing magic)"));

    ByteBuffer buf(std::move(decoded));
    auto magicResult = buf.readValue<util::data::bytearray<MAGIC_LEN>>();

    if (magicResult.isErr()) {
        return Err(ByteBuffer::strerror(magicResult.unwrapErr()));
    }

    auto magic = magicResult.unwrap();

    // compare it with the needed magic
    bool correct = true;
    for (size_t i = 0; i < MAGIC_LEN; i++) {
        if (magic[i] != NetworkManager::SERVER_MAGIC[i]) {
            correct = false;
            break;
        }
    }

    GLOBED_REQUIRE_SAFE(correct, "invalid response sent by the server (invalid magic)");

    auto serverListResult = buf.readValue<std::vector<GameServerEntry>>();
    if (serverListResult.isErr()) {
        return Err(ByteBuffer::strerror(serverListResult.unwrapErr()));
    }

    auto serverList = serverListResult.unwrap();

    for (const auto& server : serverList) {
        auto result = this->addOrUpdateServer(
            server.id,
            server.name,
            server.address,
            server.region
        );

        if (result.isErr()) {
            this->clear();
            return Err("invalid game server found when parsing server response");
        }

        if (result.unwrap()) {
            this->pendingChanges = true;
        }
    }

    return Ok();
}

uint32_t GameServerManager::startPing(const std::string_view serverId) {
    auto pingId = util::rng::Random::get().generate<uint32_t>();

    auto data = _data.lock();
    auto& gsdata = data->servers.at(std::string(serverId));

    if (gsdata.pendingPings.size() > 50) {
        log::warn("over 50 pending pings for the game server {}, clearing", serverId);
        gsdata.pendingPings.clear();
    }

    auto now = util::time::now();
    gsdata.pendingPings[pingId] = now;

    return pingId;
}

void GameServerManager::finishPing(uint32_t pingId, uint32_t playerCount) {
    auto now = util::time::now();

    auto data = _data.lock();

    for (auto& [_, server] : data->servers) {
        if (server.pendingPings.contains(pingId)) {
            auto start = server.pendingPings.at(pingId);
            auto timeTook = util::time::asMillis(now - start);

            server.server.ping = timeTook;
            server.server.playerCount = playerCount;
            server.pendingPings.erase(pingId);
            return;
        }
    }
}

void GameServerManager::startKeepalive() {
    std::string active = _data.lock()->active;

    if (!active.empty()) {
        auto pingId = this->startPing(active);
        _data.lock()->activePingId = pingId;
    }
}

void GameServerManager::finishKeepalive(uint32_t playerCount) {
    uint32_t activePingId = _data.lock()->activePingId;
    this->finishPing(activePingId, playerCount);
}
