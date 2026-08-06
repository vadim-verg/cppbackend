#pragma once
#include <boost/json.hpp>
#include <unordered_map>
#include <string>

namespace app {

class LootInfoProvider {
public:

    void AddLootTypesForMap(std::string map_id, boost::json::array loot_types) {
        map_to_loot_types_[std::move(map_id)] = std::move(loot_types);
    }

    const boost::json::array* GetLootTypesForMap(const std::string& map_id) const {
        auto it = map_to_loot_types_.find(map_id);
        if (it != map_to_loot_types_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, boost::json::array> map_to_loot_types_;
};

} // namespace app
