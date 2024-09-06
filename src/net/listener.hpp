#pragma once

#include <defs/geode.hpp>
#include <data/packets/packet.hpp>

class GLOBED_DLL PacketListener : public cocos2d::CCObject {
public:
    using CallbackFn = std::function<void(std::shared_ptr<Packet>)>;

    ~PacketListener();

    // lower priority - runs earlier
    static PacketListener* create(packetid_t packetId, CallbackFn&& fn, cocos2d::CCObject* owner, int priority, bool isFinal);

    void invokeCallback(std::shared_ptr<Packet> packet);

    packetid_t packetId;
    cocos2d::CCObject* owner;
    int priority;
    bool isFinal;

private:
    CallbackFn callback;

    bool init(packetid_t packetId, CallbackFn&& fn, cocos2d::CCObject* owner, int priority, bool isFinal);
};
