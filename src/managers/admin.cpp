#include "admin.hpp"

#include <data/packets/client/admin.hpp>
#include <data/packets/server/admin.hpp>
#include <net/manager.hpp>
#include <ui/menu/admin/user_popup.hpp>
#include <ui/general/audio_visualizer.hpp>
#include <ui/general/intermediary_loading_popup.hpp>

bool AdminManager::authorized() {
    return authorized_;
}

void AdminManager::setAuthorized(ComputedRole&& role) {
    authorized_ = true;
    this->role = std::move(role);
}

void AdminManager::deauthorize() {
    authorized_ = false;
    role = {};
}

ComputedRole& AdminManager::getRole() {
    return role;
}

void AdminManager::openUserPopup(const PlayerRoomPreviewAccountData& rpdata) {
    // load the data from the server
    auto& nm = NetworkManager::get();
    IntermediaryLoadingPopup::create([&nm, rpdata = rpdata](auto popup) {
        nm.send(AdminGetUserStatePacket::create(std::to_string(rpdata.accountId)));
        nm.addListener<AdminUserDataPacket>(popup, [popup, rpdata = std::move(rpdata)](auto packet) {
            // delay the cration to avoid deadlock
            Loader::get()->queueInMainThread([popup, userEntry = std::move(packet->userEntry), accountData = std::move(rpdata)] {
                AdminUserPopup::create(userEntry, accountData)->show();
                popup->onClose(popup);
            });
        });
    }, [](auto) {})->show();
}

bool AdminManager::canModerate() {
    return this->authorized() && role.canModerate();
}
