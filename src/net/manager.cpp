#include "manager.hpp"

#include "address.hpp"
#include "listener.hpp"
#include "game_socket.hpp"

#include <Geode/ui/GeodeUI.hpp>
#include <asp/sync.hpp>
#include <asp/thread.hpp>

#include <data/packets/all.hpp>
#include <defs/minimal_geode.hpp>
#include <managers/account.hpp>
#include <managers/admin.hpp>
#include <managers/error_queues.hpp>
#include <managers/game_server.hpp>
#include <managers/profile_cache.hpp>
#include <managers/friend_list.hpp>
#include <managers/settings.hpp>
#include <managers/room.hpp>
#include <managers/role.hpp>
#include <util/cocos.hpp>
#include <util/format.hpp>
#include <util/time.hpp>
#include <util/net.hpp>
#include <ui/notification/panel.hpp>

using namespace asp;
using namespace geode::prelude;
using ConnectionState = NetworkManager::ConnectionState;

static constexpr uint16_t MIN_PROTOCOL_VERSION = 12;
static constexpr uint16_t MAX_PROTOCOL_VERSION = 12;
static constexpr std::array SUPPORTED_PROTOCOLS = std::to_array<uint16_t>({12});

static bool isProtocolSupported(uint16_t proto) {
#ifdef GLOBED_DEBUG
    return true;
#else
    return std::find(SUPPORTED_PROTOCOLS.begin(), SUPPORTED_PROTOCOLS.end(), proto) != SUPPORTED_PROTOCOLS.end();
#endif
}

// yes, really
struct AtomicConnectionState {
    AtomicInt inner;

    AtomicConnectionState& operator=(ConnectionState state) {
        inner.store(static_cast<int>(state));
        return *this;
    }

    operator ConnectionState() {
        return static_cast<ConnectionState>(inner.load());
    }
};

static std::string formatListenerKey(packetid_t id) {
    return util::cocos::spr(fmt::format("packet-listener-{}", id));
}

struct WeakRefDummy {
    std::shared_ptr<WeakRefController> ctrl;
};

template <typename T>
static void* addrFromWeakRef(const WeakRef<T>& ref) {
    // Do not do this.
    auto dummy = reinterpret_cast<const WeakRefDummy&>(ref);
    return dummy.ctrl ? dummy.ctrl->get() : nullptr;
}

// Packet listener pool. Most of the functions must not be used on a different thread than main.
class GLOBED_DLL PacketListenerPool : public CCObject {
public:
    PacketListenerPool(const PacketListenerPool&) = delete;
    PacketListenerPool(PacketListenerPool&&) = delete;
    PacketListenerPool& operator=(const PacketListenerPool&) = delete;
    PacketListenerPool& operator=(PacketListenerPool&&) = delete;

    static PacketListenerPool& get() {
        static PacketListenerPool instance;
        return instance;
    }

    // Must be called from the main thread. Delivers packets to all listeners that are tied to an object.
    void update(float dt) {
        // this is a bit irrelevant here but who gives a shit
        if (GJAccountManager::get()->m_accountID != ProfileCacheManager::get().getOwnAccountData().accountId) {
            NetworkManager::get().disconnect();
            auto& gam = GlobedAccountManager::get();
            gam.autoInitialize();
            gam.authToken.lock()->clear();

            // clear the queue
            while (auto t = packetQueue.tryPop());

            return;
        }

        if (packetQueue.empty()) return;

        // clear any dead listeners
        this->removeDeadListeners();

        while (auto packet_ = packetQueue.tryPop()) {
            auto& packet = packet_.value();
            packetid_t id = packet->getPacketId();

            bool invoked = false;
            auto& lsm = listeners[id];

            // reorder them based on priority
            std::sort(lsm.begin(), lsm.end(), [](auto& l1, auto& l2) -> bool {
                auto r1 = l1.lock();
                auto r2 = l2.lock();

                if (!r1) return false;
                if (!r2) return true;

                return r1->priority < r2->priority;
            });

            for (auto& listener : lsm) {
                if (auto l = listener.lock()) {
                    l->invokeCallback(packet);

                    if (l->isFinal) {
                        break;
                    }
                }
            }
        }
    }

    void removeDeadListeners() {
        for (auto& [id, listeners] : listeners) {
            for (int i = listeners.size() - 1; i >= 0; i--) {
#ifdef GLOBED_DEBUG
                auto addr = addrFromWeakRef(listeners[i]);
#endif

                if (!listeners[i].valid()) {
#ifdef GLOBED_DEBUG
                    log::debug("Unregistering listener {} (id {})", addr, id);
#endif
                    listeners.erase(listeners.begin() + i);
                }
            }
        }
    }

    void registerListener(packetid_t id, PacketListener* listener) {
#ifdef GLOBED_DEBUG
        log::debug("Registering listener {} (id {}) for {}", listener, id, listener->owner);
#endif

        auto& ls = listeners[id];

        this->removeDeadListeners();

        // verify it's not a duplicate
        bool duped = false;
        for (auto& l : ls) {
            if (l.lock() == listener) {
                duped = true;
                break;
            }
        }

        if (!duped) {
            ls.push_back(WeakRef(listener));
        } else {
            log::warn("duped listener ({}, id {}, owner {}), not adding again", listener, id, listener->owner);
        }
    }

    // Push a packet to the queue. Thread safe.
    void pushPacket(std::shared_ptr<Packet> packet) {
        packetQueue.push(std::move(packet));
    }

private:
    std::unordered_map<packetid_t, std::vector<WeakRef<PacketListener>>> listeners;
    asp::Channel<std::shared_ptr<Packet>> packetQueue;

    PacketListenerPool() {
        CCScheduler::get()->scheduleSelector(schedule_selector(PacketListenerPool::update), this, 0.f, false);
    }
};

class GLOBED_DLL NetworkManager::Impl {
protected:
    friend class NetworkManager;
    friend class PacketListenerPool;

    static constexpr int BUILTIN_LISTENER_PRIORITY = 10000000;

    struct TaskPingServers {};
    struct TaskSendPacket {
        std::shared_ptr<Packet> packet;
    };
    struct TaskPingActive {};

    struct GlobalListener {
        packetid_t packetId;
        bool isFinal;
        PacketListener::CallbackFn callback;
    };

    using Task = std::variant<TaskPingServers, TaskSendPacket, TaskPingActive>;

    AtomicConnectionState state;
    GameSocket socket;
    asp::Thread<NetworkManager::Impl*> threadRecv, threadMain;
    asp::Channel<Task> taskQueue;

    // Note that we intentionally don't use Ref here,
    // as we use the destructor to know if the object owning the listener has been destroyed.
    asp::Mutex<std::unordered_map<packetid_t, GlobalListener>> listeners;
    asp::Mutex<std::unordered_map<packetid_t, util::time::system_time_point>> suppressed;

    // these fields are only used by us and in a safe manner, so they don't need a mutex
    NetworkAddress connectedAddress;
    std::string connectedServerId;
    util::time::time_point lastReceivedPacket;
    util::time::time_point lastSentKeepalive;
    util::time::time_point lastTcpExchange;

    AtomicBool suspended;
    AtomicBool standalone;
    AtomicBool recovering;
    AtomicBool handshakeDone;
    AtomicU8 recoverAttempt;
    AtomicBool ignoreProtocolMismatch;
    AtomicBool wasFromRecovery;
    AtomicBool cancellingRecovery;
    AtomicU32 secretKey;
    AtomicU32 serverTps;
    AtomicU16 serverProtocol;

    Impl() {
        // initialize winsock
        util::net::initialize();

        this->setupGlobalListeners();

        // start up the threads

        threadRecv.setLoopFunction(&NetworkManager::Impl::threadRecvFunc);
        threadRecv.setStartFunction([] { geode::utils::thread::setName("Network Thread (in)"); });
        threadRecv.start(this);

        threadMain.setLoopFunction(&NetworkManager::Impl::threadMainFunc);
        threadMain.setStartFunction([] { geode::utils::thread::setName("Network Thread (out)"); });
        threadMain.start(this);

        this->resetConnectionState();
    }

    ~Impl() {
        // remove all listeners
        this->removeAllListeners();

        log::debug("waiting for network threads to terminate..");
        threadRecv.stopAndWait();
        threadMain.stopAndWait();

        if (state != ConnectionState::Disconnected) {
            log::debug("disconnecting from the server..");
            try {
                this->disconnect(false, true);
            } catch (const std::exception& e) {
                log::warn("error trying to disconnect: {}", e.what());
            }
        }

        util::net::cleanup();

        log::info("goodbye from networkmanager!");
    }

    void resetConnectionState() {
        state = ConnectionState::Disconnected;
        standalone = false;
        recovering = false;
        handshakeDone = false;
        recoverAttempt = 0;
        wasFromRecovery = false;
        cancellingRecovery = false;
        secretKey = 0;
        serverTps = 0;
        serverProtocol = 0;
        lastReceivedPacket = {};
        lastSentKeepalive = {};
        lastTcpExchange = {};
    }

    /* connection and tasks */

    Result<> connect(const NetworkAddress& address, const std::string_view serverId, bool standalone, bool fromRecovery = false) {
        // if we are already connected, disconnect first
        if (state == ConnectionState::Established) {
            this->disconnect(false, false);
        }

        if (state != ConnectionState::Disconnected) {
            return Err("already trying to connect");
        }

        this->resetConnectionState();

        this->standalone = standalone;
        this->wasFromRecovery = fromRecovery;

        lastReceivedPacket = util::time::now();
        lastSentKeepalive = util::time::now();
        lastTcpExchange = util::time::now();

        if (!standalone) {
            GLOBED_REQUIRE_SAFE(!GlobedAccountManager::get().authToken.lock()->empty(), "attempting to connect with no authtoken set in account manager")
        }

        connectedAddress = address;
        connectedServerId = std::string(serverId);
        recovering = false;
        recoverAttempt = 0;

        state = ConnectionState::TcpConnecting;

        auto& pcm = ProfileCacheManager::get();
        pcm.setOwnDataAuto();

        // actual connection is deferred - the network thread does DNS resolution and TCP connection.

        return Ok();
    }

    void disconnect(bool quiet = false, bool noclear = false) {
        ConnectionState prevState = state;

        this->resetConnectionState();

        if (!quiet && prevState == ConnectionState::Established) {
            // send it directly instead of pushing to the queue
            (void) socket.sendPacket(DisconnectPacket::create());
        }

        socket.disconnect();

        // singletons could have been destructed before NetworkManager, so this could be UB. Additionally will break autoconnect.
        if (!noclear) {
            RoomManager::get().setGlobal();
            GameServerManager::get().clearActive();
            AdminManager::get().deauthorize();
        }
    }

    void disconnectWithMessage(const std::string_view message, bool quiet = true) {
        ErrorQueues::get().error(fmt::format("You have been disconnected from the active server.\n\nReason: <cy>{}</c>", message));
        this->disconnect(quiet);
    }

    void cancelReconnect() {
        cancellingRecovery = true;
    }

    void onConnectionError(const std::string_view reason) {
        ErrorQueues::get().debugWarn(reason);
    }

    void send(std::shared_ptr<Packet> packet) {
        taskQueue.push(TaskSendPacket {
            .packet = std::move(packet)
        });
    }

    void pingServers() {
        taskQueue.push(TaskPingServers {});
    }

    void updateServerPing() {
        taskQueue.push(TaskPingActive {});
    }

    ConnectionState getConnectionState() {
        return state;
    }

    bool established() {
        return state == ConnectionState::Established;
    }

    /* listeners */

    void addListener(CCNode* target, PacketListener* listener) {
        if (target) {
            target->setUserObject(formatListenerKey(listener->packetId), listener);
        } else {
            // if target is nullptr we leak the listener,
            // essentially saying it should live for the entire duration of the program.
            listener->retain();
        }

        this->registerPacketListener(listener->packetId, listener);
    }

    void registerPacketListener(packetid_t packet, PacketListener* listener) {
        PacketListenerPool::get().registerListener(packet, listener);
    }

    void removeListener(cocos2d::CCNode* target, packetid_t id) {
        auto key = util::cocos::spr(fmt::format("packet-listener-{}", id));

        target->setUserObject(key, nullptr);

        // the listener will unregister itself in the destructor.
    }

    void unregisterPacketListener(packetid_t packet, PacketListener* listener, bool suppressUnhandled) {
    }

    void suppressUnhandledUntil(packetid_t id, util::time::system_time_point point) {
        (*suppressed.lock())[id] = point;
    }

    void removeAllListeners() {
        listeners.lock()->clear();
    }

    // adds a global listener, with same fairness as all other listeners
    void addGlobalListener(packetid_t id, PacketCallback&& callback) {
        auto listener = PacketListener::create(id, std::move(callback), nullptr, BUILTIN_LISTENER_PRIORITY, false);
        this->addListener(nullptr, listener);
    }

    template <HasPacketID Pty>
    void addGlobalListener(PacketCallbackSpecific<Pty>&& callback) {
        this->addGlobalListener(Pty::PACKET_ID, [callback = std::move(callback)](std::shared_ptr<Packet> pkt) {
            callback(std::static_pointer_cast<Pty>(pkt));
        });
    }

    // adds a global listener, which always runs NOT on the main thread and always before other listeners
    void addInternalListener(packetid_t id, PacketCallback&& callback) {
        GlobalListener listener {
            .packetId = id,
            .isFinal = false,
            .callback = std::move(callback),
        };

        (*listeners.lock())[id] = std::move(listener);
#ifdef GLOBED_DEBUG
        log::debug("Registered internal listener (id = {})", id);
#endif
    }

    template <HasPacketID Pty>
    void addInternalListener(PacketCallbackSpecific<Pty>&& callback) {
        this->addInternalListener(Pty::PACKET_ID, [cb = std::move(callback)](std::shared_ptr<Packet> packet) {
            cb(std::static_pointer_cast<Pty>(std::move(packet)));
        });
    }

    // same as `addInternalListener` but runs the callback on the main thread
    template <HasPacketID Pty>
    void addInternalListenerSync(PacketCallbackSpecific<Pty>&& callback) {
        this->addInternalListener<Pty>([cb = std::move(callback)](std::shared_ptr<Pty> packet) {
            Loader::get()->queueInMainThread([cb = std::move(cb), packet = std::move(packet)] {
                cb(std::move(packet));
            });
        });
    }

    /* global listeners */

    void setupGlobalListeners() {
        // Connection packets

        addInternalListenerSync<CryptoHandshakeResponsePacket>([this](auto packet) {
            this->onCryptoHandshakeResponse(std::move(packet));
        });

        addInternalListener<KeepaliveResponsePacket>([](auto packet) {
            GameServerManager::get().finishKeepalive(packet->playerCount);
        });

        addInternalListener<KeepaliveTCPResponsePacket>([](auto) {});

        addInternalListener<ServerDisconnectPacket>([this](auto packet) {
            this->disconnectWithMessage(packet->message);
        });

        addInternalListener<LoggedInPacket>([this](auto packet) {
            this->onLoggedIn(std::move(packet));
        });

        addInternalListener<LoginFailedPacket>([this](auto packet) {
            ErrorQueues::get().error(fmt::format("<cr>Authentication failed!</c> The server rejected the login attempt.\n\nReason: <cy>{}</c>", packet->message));
            GlobedAccountManager::get().authToken.lock()->clear();
            this->disconnect(true);
        });

        addInternalListener<ProtocolMismatchPacket>([this](auto packet) {
            this->onProtocolMismatch(std::move(packet));
        });

        addInternalListener<ClaimThreadFailedPacket>([this](auto packet) {
            this->disconnectWithMessage("failed to claim udp thread");
        });

        addInternalListener<LoginRecoveryFailecPacket>([this](auto packet) {
            log::debug("Login recovery failed, retrying regular connection");

            // failed to recover login, try regular connection
            this->disconnect(true);
            auto result = this->connect(connectedAddress, connectedServerId, standalone, true);

            if (!result) {
                log::warn("failed to reconnect: {}", result.unwrapErr());
                this->onConnectionError("[Globed] failed to reconnect");
            }
        });

        addGlobalListener<ServerNoticePacket>([](auto packet) {
            ErrorQueues::get().notice(packet->message);
        });

        addGlobalListener<ServerBannedPacket>([this](auto packet) {
            using namespace std::chrono;

            std::string reason = packet->message;
            if (reason.empty()) {
                reason = "No reason given";
            }

            auto msg = fmt::format(
                "<cy>You have been</c> <cr>Banned:</c>\n{}\n<cy>Expires at:</c>\n{}\n<cy>Question/Appeals? Join the </c><cb>Discord.</c>",
                reason,
                packet->timestamp == 0 ? "Permanent" : util::format::formatDateTime(sys_seconds(seconds(packet->timestamp)), false)
            );

            this->disconnectWithMessage(msg);
        });

        addGlobalListener<ServerMutedPacket>([](auto packet) {
            using namespace std::chrono;

            std::string reason = packet->reason;
            if (reason.empty()) {
                reason = "No reason given";
            }

            auto msg = fmt::format(
                "<cy>You have been</c> <cr>Muted:</c>\n{}\n<cy>Expires at:</c>\n{}\n<cy>Question/Appeals? Join the </c><cb>Discord.</c>",
                reason,
                packet->timestamp == 0 ? "Permanent" : util::format::formatDateTime(sys_seconds(seconds(packet->timestamp)), false)
            );

            ErrorQueues::get().notice(msg);
        });

        // General packets

        addGlobalListener<RolesUpdatedPacket>([](auto packet) {
            auto& pcm = ProfileCacheManager::get();
            pcm.setOwnSpecialData(packet->specialUserData);
        });

        // Room packets

        addGlobalListener<RoomInvitePacket>([](auto packet) {
            using InvitesFrom = GlobedSettings::InvitesFrom;

            // check if allowed
            int inviter = packet->playerData.accountId;
            InvitesFrom setting = static_cast<InvitesFrom>((int)GlobedSettings::get().globed.invitesFrom);

            auto& flm = FriendListManager::get();

            // block the invite if either..
            if (
                // a. invites are disabled
                setting == InvitesFrom::Nobody
                // b. invites are friend-only and the user is not a friend
                || (setting == InvitesFrom::Friends && !flm.isFriend(inviter))
                // c. user is blocked
                || flm.isBlocked(inviter)
            ) {
                return;
            }

            GlobedNotificationPanel::get()->addInviteNotification(packet->roomID, packet->password, packet->playerData);
        });

        addGlobalListener<RoomInfoPacket>([](auto packet) {
            ErrorQueues::get().success("Room configuration updated");

            RoomManager::get().setInfo(packet->info);
        });

        addGlobalListener<RoomJoinedPacket>([](auto packet) {});

        addGlobalListener<RoomJoinFailedPacket>([](auto packet) {
            std::string reason = "N/A";
            if (packet->wasInvalid) reason = "Room doesn't exist";
            if (packet->wasProtected) reason = "Room password is wrong";
            if (packet->wasFull) reason = "Room is full";

            ErrorQueues::get().error(fmt::format("Failed to join room: {}", reason));
        });

        // Admin packets

        addGlobalListener<AdminAuthSuccessPacket>([this](auto packet) {
            AdminManager::get().setAuthorized(std::move(packet->role));
            ErrorQueues::get().success("Successfully authorized");
        });

        addGlobalListener<AdminAuthFailedPacket>([this](auto packet) {
            ErrorQueues::get().warn("Login failed");

            auto& am = GlobedAccountManager::get();
            am.clearAdminPassword();
        });

        addGlobalListener<AdminSuccessMessagePacket>([](auto packet) {
            ErrorQueues::get().success(packet->message);
        });

        addGlobalListener<AdminErrorPacket>([](auto packet) {
            ErrorQueues::get().warn(packet->message);
        });
    }

    void onCryptoHandshakeResponse(std::shared_ptr<CryptoHandshakeResponsePacket> packet) {
        log::debug("handshake successful, logging in");
        handshakeDone = true;

        auto key = packet->data.key;

        socket.cryptoBox->setPeerKey(key.data());
        auto& am = GlobedAccountManager::get();
        std::string authtoken;

        if (!standalone) {
            authtoken = *am.authToken.lock();
        }

        auto& pcm = ProfileCacheManager::get();
        pcm.setOwnDataAuto();
        pcm.pendingChanges = false;

        auto& settings = GlobedSettings::get();

        if (settings.globed.fragmentationLimit == 0) {
            settings.globed.fragmentationLimit = 65000;
        }

        auto gddata = am.gdData.lock();
        auto pkt = LoginPacket::create(
            gddata->accountId,
            gddata->userId,
            gddata->accountName,
            authtoken,
            pcm.getOwnData(),
            settings.globed.fragmentationLimit,
            util::net::loginPlatformString(),
            settings.getPrivacyFlags()
        );

        this->send(pkt);
    }

    void onLoggedIn(std::shared_ptr<LoggedInPacket> packet) {
        // validate protocol version
        if (!::isProtocolSupported(packet->serverProtocol)) {
            auto mismatch = std::make_shared<ProtocolMismatchPacket>();
            mismatch->serverProtocol = packet->serverProtocol;
            mismatch->minClientVersion = "unknown";
            this->onProtocolMismatch(std::move(mismatch));
            return;
        }

        log::info("Successfully logged into the server!");
        serverTps = packet->tps;
        secretKey = packet->secretKey;
        serverProtocol = packet->serverProtocol;

        state = ConnectionState::Established;

        if (recovering || wasFromRecovery) {
            recovering = false;
            recoverAttempt = 0;

            log::info("connection recovered successfully!");
            ErrorQueues::get().success("[Globed] Connection recovered");
        }

        GameServerManager::get().setActive(connectedServerId);

        auto& flm = FriendListManager::get();
        flm.maybeLoad();

        // these are not thread-safe, so delay it
        Loader::get()->queueInMainThread([specialUserData = std::move(packet->specialUserData), allRoles = std::move(packet->allRoles)] {
            auto& pcm = ProfileCacheManager::get();
            pcm.setOwnSpecialData(specialUserData);

            RoomManager::get().setGlobal();
            RoleManager::get().setAllRoles(allRoles);
        });

        // claim the tcp thread to allow udp packets through
        this->send(ClaimThreadPacket::create(this->secretKey));

        // try to login as an admin if we can
        auto& am = GlobedAccountManager::get();
        if (am.hasAdminPassword()) {
            auto password = am.getAdminPassword();
            if (password.has_value()) {
                GlobedAccountManager::get().storeTempAdminPassword(password.value());
                this->send(AdminAuthPacket::create(password.value()));
            }
        }
    }

    void onProtocolMismatch(std::shared_ptr<ProtocolMismatchPacket> packet) {
        log::warn("Failed to connect because of protocol mismatch. Server: {}, client: {}", packet->serverProtocol, this->getUsedProtocol());

        // show an error telling the user to update the mod

        if (packet->serverProtocol < this->getUsedProtocol()) {
            std::string message = "Your Globed version is <cy>too new</c> for this server. Downgrade the mod to an older version or ask the server owner to update their server.";
            ErrorQueues::get().error(message);
        } else {
            Loader::get()->queueInMainThread([packet = std::move(packet)] {
                std::string message = fmt::format(
                    "Your Globed version is <cr>outdated</c>, please <cg>update</c> Globed in order to connect."
                    " Installed version: <cy>{}</c>, required: <cy>{}</c> (or newer)",
                    Mod::get()->getVersion().toString(),
                    packet->minClientVersion
                );

                geode::createQuickPopup("Globed Error", message, "Cancel", "Update", [](FLAlertLayer*, bool update) {
                    if (!update) return;

                    geode::openModsList();
                });
            });
        }

        this->disconnect(true);
    }

    /* misc */

    void togglePacketLogging(bool enabled) {
        socket.togglePacketLogging(enabled);
    }

    void setIgnoreProtocolMismatch(bool state) {
        ignoreProtocolMismatch = state;
    }

    uint16_t getUsedProtocol() {
        // 0xffff is a special value that the server doesn't check
#ifdef GLOBED_DEBUG
        return 0xffff;
#else
        return ignoreProtocolMismatch ? 0xffff : MAX_PROTOCOL_VERSION;
#endif
    }

    uint32_t getServerTps() {
        return established() ? serverTps.load() : 0;
    }

    uint16_t getServerProtocol() {
        return established() ? serverProtocol.load() : 0;
    }

    bool isStandalone() {
        return standalone;
    }

    bool isReconnecting() {
        return recovering;
    }

    /* suspension */

    void suspend() {
        suspended = true;
    }

    void resume() {
        suspended = false;
    }

    /* worker threads */

    void threadRecvFunc(decltype(threadRecv)::StopToken&) {
        if (this->suspended || state == ConnectionState::TcpConnecting) {
            std::this_thread::sleep_for(util::time::millis(100));
            return;
        }

        auto packet_ = socket.recvPacket(100);
        if (packet_.isErr()) {
            auto error = std::move(packet_.unwrapErr());
            if (error != "timed out") {
                this->onConnectionError(error);
            }

            return;
        }

        // this is awesome
        auto packet__ = packet_.unwrap();
        auto packet = std::move(packet__.packet);
        bool fromServer = packet__.fromConnected;

        packetid_t id = packet->getPacketId();

        if (id == PingResponsePacket::PACKET_ID) {
            this->handlePingResponse(std::move(packet));
            return;
        }

        // if it's not a ping packet, and it's NOT from the currently connected server, we reject it
        if (!fromServer) {
            return;
        }

        lastReceivedPacket = util::time::now();

        this->callListener(std::move(packet));
    }

    void callListener(std::shared_ptr<Packet>&& packet) {
        packetid_t packetId = packet->getPacketId();

        // go through internal listeners
        auto ls = listeners.lock();
        if (ls->contains(packetId)) {
            auto listener = ls->at(packetId);
            listener.callback(packet);
            if (listener.isFinal) return;
        }

        ls.unlock();

        // call other listeners
        PacketListenerPool::get().pushPacket(std::move(packet));
    }

    void handlePingResponse(std::shared_ptr<Packet>&& packet) {
        if (auto* pingr = packet->tryDowncast<PingResponsePacket>()) {
            GameServerManager::get().finishPing(pingr->id, pingr->playerCount);
        }
    }

    void threadMainFunc(decltype(threadMain)::StopToken&) {
        if (this->suspended) {
            std::this_thread::sleep_for(util::time::millis(100));
            return;
        }

        // Initial tcp connection.
        if (state == ConnectionState::TcpConnecting && !recovering) {
            // try to connect
            auto result = socket.connect(connectedAddress, recovering);

            if (!result) {
                this->disconnect(true);

                auto reason = result.unwrapErr();
                log::warn("TCP connection failed: <cy>{}</c>", reason);

                ErrorQueues::get().error(fmt::format("Failed to connect to the server.\n\nReason: <cy>{}</c>", reason));
                return;
            } else {
                log::debug("tcp connection successful, sending the handshake");
                state = ConnectionState::Authenticating;

                socket.createBox();

                uint16_t proto = this->getUsedProtocol();

                this->send(CryptoHandshakeStartPacket::create(
                    proto,
                    CryptoPublicKey(socket.cryptoBox->extractPublicKey())
                ));
            }
        }
        // Connection recovery loop itself
        else if (state == ConnectionState::TcpConnecting && recovering) {
            log::debug("recovery attempt {}", recoverAttempt.load());

            // initiate TCP connection
            auto result = socket.connect(connectedAddress, recovering);

            bool failed = false;

            if (!result) {
                failed = true;
            } else {
                log::debug("tcp connection successful, sending recovery data");
                state = ConnectionState::Authenticating;

                // if we are recovering, next steps are slightly different
                // first send our account ID and secret key (handled on the lowest level by the server)
                auto result = socket.sendRecoveryData(GJAccountManager::get()->m_accountID, secretKey);

                if (!result) {
                    failed = true;
                } else {
                    // the rest is done in a global listener
                    return;
                }
            }

            if (failed) {
                auto attemptNumber = recoverAttempt.load() + 1;
                recoverAttempt = attemptNumber;

                if (attemptNumber > 3) {
                    // give up
                    this->failedRecovery();
                    return;
                }

                auto sleepPeriod = util::time::millis(10000) * attemptNumber;

                log::debug("tcp connect failed, sleeping for {} before trying again", util::format::formatDuration(sleepPeriod));

                // wait for a bit before trying again

                // we sleep 50 times because we want to check if the user cancelled the recovery
                for (size_t i = 0; i < 50; i++) {
                    std::this_thread::sleep_for(sleepPeriod / 50);

                    if (cancellingRecovery) {
                        log::debug("recovery attempts were cancelled.");
                        recovering = false;
                        recoverAttempt = 0;
                        state = ConnectionState::Disconnected;
                        return;
                    }
                }

                return;
            }
        }
        // Detect if the tcp socket has unexpectedly disconnected and start recovering the connection
        else if (state == ConnectionState::Established && !socket.isConnected()) {
            ErrorQueues::get().warn("[Globed] connection lost, reconnecting..");

            state = ConnectionState::TcpConnecting;
            recovering = true;
            cancellingRecovery = false;
            recoverAttempt = 0;
            return;
        }
        // Detect if we disconnected while authenticating, likely the server doesn't expect us
        else if (state == ConnectionState::Authenticating && !socket.isConnected()) {
            this->disconnect(true);

            ErrorQueues::get().error(fmt::format(
                "Failed to connect to the server.\n\nReason: <cy>server abruptly disconnected during the {}</c>",
                handshakeDone ? "login attempt" : "handshake"
            ));
            return;
        }
        // Detect if authentication is taking too long
        else if (state == ConnectionState::Authenticating && !recovering && (util::time::now() - lastReceivedPacket) > util::time::seconds(5)) {
            this->disconnect(true);

            ErrorQueues::get().error(fmt::format(
                "Failed to connect to the server.\n\nReason: <cy>server took too long to respond to the {}</c>",
                handshakeDone ? "login attempt" : "handshake"
            ));
            return;
        }

        if (this->established()) {
            this->maybeSendKeepalive();
        }

        // poll for any incoming packets

        while (auto task_ = taskQueue.popTimeout(util::time::millis(50))) {
            auto task = std::move(task_.value());

            if (std::holds_alternative<TaskPingServers>(task)) {
                this->handlePingTask();
            } else if (std::holds_alternative<TaskSendPacket>(task)) {
                this->handleSendPacketTask(std::move(std::get<TaskSendPacket>(task)));
            } else if (std::holds_alternative<TaskPingActive>(task)) {
                this->handlePingActive();
            }
        }

        std::this_thread::yield();
    }

    void maybeSendKeepalive() {
        if (!this->established()) return;

        auto now = util::time::now();

        auto sinceLastPacket = now - lastReceivedPacket;
        auto sinceLastKeepalive = now - lastSentKeepalive;
        auto sinceLastTcpExchange = now - lastTcpExchange;

        if (sinceLastPacket > util::time::seconds(20)) {
            // timed out, disconnect the tcp socket but allow to reconnect
            log::warn("timed out, time since last received packet: {}", util::format::formatDuration(sinceLastPacket));
            socket.disconnect();
        } else if (sinceLastPacket > util::time::seconds(10) && sinceLastKeepalive > util::time::seconds(3)) {
            this->sendKeepalive();
        }

        // send a tcp keepalive to keep the nat hole open
        if (sinceLastTcpExchange > util::time::seconds(60)) {
            this->send(KeepaliveTCPPacket::create());
        }
    }

    void sendKeepalive() {
        // send a keepalive
        this->send(KeepalivePacket::create());
        lastSentKeepalive = util::time::now();
        GameServerManager::get().startKeepalive();
    }

    void failedRecovery() {
        recovering = false;
        recoverAttempt = 0;
        state = ConnectionState::Disconnected;

        ErrorQueues::get().error("Connection to the server was lost. Failed to reconnect after 3 attempts.");
    }

    void handlePingTask() {
        auto& gsm = GameServerManager::get();
        auto active = gsm.getActiveId();

        for (auto& [serverId, server] : gsm.getAllServers()) {
            if (serverId == active) continue;

            NetworkAddress addr(server.address);

#ifdef GLOBED_DEBUG
            auto addrString = addr.resolveToString().value_or("<unresolved>");
            log::debug("sending ping to {}", addrString);
#endif

            auto pingId = gsm.startPing(serverId);
            auto result = socket.sendPacketTo(PingPacket::create(pingId), addr);

            if (result.isErr()) {
                log::debug("failed to send ping: {}", result.unwrapErr());
                ErrorQueues::get().warn(result.unwrapErr());
            }
        }
    }

    void handleSendPacketTask(TaskSendPacket task) {
        if (task.packet->getUseTcp()) {
            lastTcpExchange = util::time::now();
        }

        try {
            auto result = socket.sendPacket(task.packet);
            if (!result) {
                auto error = result.unwrapErr();
                log::debug("failed to send packet {}: {}", task.packet->getPacketId(), error);
                this->onConnectionError(error);
                return;
            }
        } catch (const std::exception& e) {
            this->onConnectionError(e.what());
        }
    }

    void handlePingActive() {
        if (!this->established()) return;

        auto now = util::time::now();
        auto sinceLastKeepalive = now - lastSentKeepalive;

        bool isPingUnknown = GameServerManager::get().getActivePing() == -1;

        if (sinceLastKeepalive > util::time::seconds(5) || isPingUnknown) {
            this->sendKeepalive();
        }
    }
};

/* NetworkManager (public API) */

NetworkManager::NetworkManager() : impl(new NetworkManager::Impl()) {}

NetworkManager::~NetworkManager() {
    delete impl;
}

Result<> NetworkManager::connect(const NetworkAddress& address, const std::string_view serverId, bool standalone) {
    return impl->connect(address, serverId, standalone);
}

Result<> NetworkManager::connect(const GameServer& gsview) {
    return impl->connect(NetworkAddress(gsview.address), gsview.id, false);
}

Result<> NetworkManager::connectStandalone() {
    auto _server = GameServerManager::get().getServer(GameServerManager::STANDALONE_ID);
    if (!_server.has_value()) {
        return Err(fmt::format("failed to find server by standalone ID"));
    }

    auto server = _server.value();
    return impl->connect(NetworkAddress(server.address), GameServerManager::STANDALONE_ID, true);
}

void NetworkManager::disconnect(bool quiet, bool noclear) {
    impl->disconnect(quiet, noclear);
}

void NetworkManager::disconnectWithMessage(const std::string_view message, bool quiet) {
    impl->disconnectWithMessage(message, quiet);
}

void NetworkManager::cancelReconnect() {
    impl->cancelReconnect();
}

void NetworkManager::send(std::shared_ptr<Packet> packet) {
    impl->send(std::move(packet));
}

void NetworkManager::pingServers() {
    impl->pingServers();
}

void NetworkManager::updateServerPing() {
    impl->updateServerPing();
}

void NetworkManager::addListener(CCNode* target, packetid_t id, PacketListener* listener) {
    impl->addListener(target, listener);
}

void NetworkManager::addListener(CCNode* target, packetid_t id, PacketCallback&& callback, int priority, bool isFinal) {
    auto* listener = PacketListener::create(id, std::move(callback), target, priority, isFinal);
    impl->addListener(target, listener);
}

void NetworkManager::removeListener(CCNode* target, packetid_t id) {
    impl->removeListener(target, id);
}

void NetworkManager::removeAllListeners() {
    impl->removeAllListeners();
}

void NetworkManager::togglePacketLogging(bool enabled) {
    impl->togglePacketLogging(enabled);
}

uint16_t NetworkManager::getUsedProtocol() {
    return impl->getUsedProtocol();
}

uint32_t NetworkManager::getServerTps() {
    return impl->getServerTps();
}

uint16_t NetworkManager::getServerProtocol() {
    return impl->getServerProtocol();
}

bool NetworkManager::standalone() {
    return impl->isStandalone();
}

ConnectionState NetworkManager::getConnectionState() {
    return impl->getConnectionState();
}

bool NetworkManager::established() {
    return impl->established();
}

bool NetworkManager::reconnecting() {
    return impl->isReconnecting();
}

void NetworkManager::suspend() {
    impl->suspend();
}

void NetworkManager::resume() {
    impl->resume();
}

uint16_t NetworkManager::getMinProtocol() {
#ifdef GLOBED_DEBUG
    return 0xffff;
#else
    return MIN_PROTOCOL_VERSION;
#endif
}

uint16_t NetworkManager::getMaxProtocol() {
#ifdef GLOBED_DEBUG
    return 0xffff;
#else
    return MAX_PROTOCOL_VERSION;
#endif
}

bool NetworkManager::isProtocolSupported(uint16_t version) {
    return ::isProtocolSupported(version);
}

void NetworkManager::unregisterPacketListener(packetid_t packet, PacketListener* listener, bool suppressUnhandled) {
    impl->unregisterPacketListener(packet, listener, suppressUnhandled);
}

/* packet sending */

#define MAKE_SENDER(name, packet, arglist, arglist2) \
    void NetworkManager::name arglist { \
        impl->send(packet::create arglist2); \
    }

#define MAKE_SENDER2(name, arglist, arglist2) \
    MAKE_SENDER(send##name, name##Packet, arglist, arglist2)

// two choices:
// MAKE_SENDER(sendLeaveRoom, LeaveRoomPacket, (), ())
// MAKE_SENDER2(LeaveRoom, (), ())

MAKE_SENDER2(UpdatePlayerStatus, (const UserPrivacyFlags& flags), (flags))
MAKE_SENDER2(RequestRoomPlayerList, (), ())
MAKE_SENDER2(LeaveRoom, (), ())
MAKE_SENDER2(CloseRoom, (uint32_t roomId), (roomId))
MAKE_SENDER2(LinkCodeRequest, (), ())

void NetworkManager::sendRequestPlayerCount(LevelId id) {
    impl->send(RequestPlayerCountPacket::create(std::vector({id})));
}

void NetworkManager::sendRequestPlayerCount(std::vector<LevelId> ids) {
    impl->send(RequestPlayerCountPacket::create(std::move(ids)));
}
