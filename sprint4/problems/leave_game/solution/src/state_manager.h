#pragma once
#include <fstream>
#include <filesystem>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <iostream>

#include "serialization.h"
#include "players.h"

class StateManager {
public:
    explicit StateManager(std::string file_path) : file_path_(std::move(file_path)) {}

    // Сохранить состояние в файл
    void SaveState(const app::Application& app) {
        try {
            // Создаем временный файл, чтобы запись была атомарной (на случай внезапного отключения)
            std::string tmp_file = file_path_ + ".tmp";
            std::ofstream ofs(tmp_file, std::ios::binary);
            if (!ofs.is_open()) return;

            serialization::GameStateRepr repr;
            const auto& game = app.GetGame();

            // 1. Сохраняем потерянные предметы
            for (const auto& [map_id, loot_map] : game.GetEntireLoot()) {
                std::vector<serialization::LostObjectRepr> loot_vector;
                for (const auto& [obj_id, loot_obj] : loot_map) {
                    loot_vector.emplace_back(loot_obj);
                }
                repr.map_loot[*map_id] = std::move(loot_vector);
            }

            // 2. Сохраняем счетчики ID лута
            for (const auto& [map_id, loot_id] : game.GetEntireNextLootId()) {
                repr.next_loot_id[*map_id] = loot_id;
            }

            // Нам понадобятся промежуточные геттеры приложения в app.h
            // Сначала получим доступ к PlayerManager через временный метод или сделав StateManager другом
            // Для простоты добавим геттеры в Application:
            // const app::PlayerManager& GetPlayerManager() const { return player_manager_; }
            // const app::PlayerTokens& GetPlayerTokens() const { return player_tokens_; }

            // 3. Сохраняем игроков и их собак
            const auto& pm = app.GetPlayerManager();
            repr.next_player_id = pm.GetNextIdInternal();

            for (const auto& [player_id, player_ptr] : pm.GetPlayers()) {
                serialization::PlayerRepr p_repr;
                p_repr.id = player_ptr->GetId();
                p_repr.name = player_ptr->GetName();
                p_repr.map_id = player_ptr->GetMapId();

                if (auto dog_ptr = player_ptr->GetDog()) {
                    p_repr.has_dog = true;                      // Собака есть
                    p_repr.dog = serialization::DogRepr(*dog_ptr); // Делаем её снимок
                } else {
                    p_repr.has_dog = false;                     // Собаки нет
                }
                repr.players.push_back(p_repr);
            }

            // 4. Сохраняем токены
            const auto& pt = app.GetPlayerTokens();
            for (const auto& [token_str, player_ptr] : pt.GetTokenMap()) {
                repr.token_to_player_id[token_str] = player_ptr->GetId();
            }

            // Записываем всё через Boost.Archive
            boost::archive::text_oarchive oa(ofs);
            oa << repr;
            ofs.close();

            // Переименовываем временный файл в целевой
            std::filesystem::rename(tmp_file, file_path_);
        } catch (...) {
            // Ошибки сохранения по ТЗ логировать не обязательно, но полезно
        }
    }

    // Восстановить состояние из файла
    bool LoadState(app::Application& app, model::Game& game) {
        if (!std::filesystem::exists(file_path_)) {
            return true; // Файла нет — стартуем с чистого листа по ТЗ
        }

        try {
            std::ifstream ifs(file_path_, std::ios::binary);
            if (!ifs.is_open()) return false;

            serialization::GameStateRepr repr;
            boost::archive::text_iarchive ia(ifs);
            ia >> repr;

            // Восстанавливаем лут
            for (const auto& [map_id_str, loot_vector] : repr.map_loot) {
                model::Map::Id map_id{map_id_str};
                for (const auto& loot_repr : loot_vector) {
                    game.RestoreLostObject(map_id, loot_repr.Restore());
                }
            }

            // Восстанавливаем счетчики ID лута
            for (const auto& [map_id_str, loot_id] : repr.next_loot_id) {
                game.RestoreNextLootId(model::Map::Id{map_id_str}, loot_id);
            }

            // Восстанавливаем игроков и собак
            auto& pm = app.GetPlayerManagerMutable(); // Добавьте неконстантный геттер в Application
            auto& pt = app.GetPlayerTokensMutable();

            pm.SetNextId(repr.next_player_id);

            std::unordered_map<uint32_t, std::shared_ptr<app::Player>> restored_players;

            for (const auto& p_repr : repr.players) {
                std::shared_ptr<model::Dog> dog_ptr = nullptr;

                // Проверяем флаг вместо std::optional
                if (p_repr.has_dog) {
                    // Восстанавливаем собаку
                    dog_ptr = std::make_shared<model::Dog>(p_repr.dog.Restore());

                    // Обновляем счетчик собак на карте в модели
                    game.SetDogCount(model::Map::Id{p_repr.map_id}, game.GetDogCount(model::Map::Id{p_repr.map_id}) + 1);
                }

                auto player = std::make_shared<app::Player>(p_repr.id, p_repr.name, p_repr.map_id, dog_ptr);
                pm.RestorePlayer(player);
                restored_players[p_repr.id] = player;
            }

            // Восстанавливаем токены
            for (const auto& [token_str, player_id] : repr.token_to_player_id) {
                if (restored_players.contains(player_id)) {
                    pt.RestoreToken(token_str, restored_players[player_id]);
                }
            }

            return true;
        } catch (const std::exception& e) {
            // Будет выведено в лог в main.cpp по ТЗ
            return false;
        }
    }

private:
    std::string file_path_;
};
