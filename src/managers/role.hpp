#pragma once

#include <data/types/user.hpp>

#include <util/singleton.hpp>

class RoleManager : public SingletonBase<RoleManager> {
    friend class SingletonBase;

public:
    void setAllRoles(std::vector<GameServerRole>&& allRoles);
    void setAllRoles(const std::vector<GameServerRole>& allRoles);
    void clearAllRoles();
    std::vector<GameServerRole>& getAllRoles();
    ComputedRole compute(const std::vector<uint8_t>& roles);
    ComputedRole compute(const std::vector<std::string>& roles);
    std::vector<std::string> getBadgeList(const std::vector<uint8_t>& roles);
    std::vector<std::string> getBadgeList(const std::vector<std::string>& roles);

private:
    std::vector<GameServerRole> allRoles;
};
