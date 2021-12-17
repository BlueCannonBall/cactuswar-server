#include "bcblog.hpp"
#include "json.hpp"
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include "logger.hpp"

#pragma once
#define PI M_PI

using json = nlohmann::json;

struct BarrelConfig {
    float angle;
    float width;
    float length;
    unsigned full_reload;
    unsigned reload_delay;
    float recoil;

    // stats
    float bullet_speed;
    float bullet_damage;
    float bullet_penetration;
};

struct TankConfig {
    std::string name;
    unsigned char fov;
    std::vector<BarrelConfig> barrels;
};

std::vector<TankConfig> tanksconfig; // NOLINT

int load_tanks_from_json(const std::string& filename) { // NOLINT
    std::ifstream tanks_file(filename);
    if (!tanks_file.is_open()) {
        perror(std::string("Failed to open " + filename).c_str());
        return -1;
    }
    std::string data;
    tanks_file.seekg(0, std::ios::end);
    data.reserve(tanks_file.tellg());
    tanks_file.seekg(0, std::ios::beg);
    data.assign((std::istreambuf_iterator<char>(tanks_file)),
        std::istreambuf_iterator<char>());

    json tanks;
    try {
        tanks = json::parse(data);
    } catch (std::exception& e) {
        ERR("Failed to parse " << filename << ": " << e.what());
        return -1;
    }

    for (const auto& tank : tanks) {
        tanksconfig.push_back(TankConfig {
            .name = tank["name"],
            .fov = tank["fov"]});
        int index = tanksconfig.size() - 1;
        for (const auto& barrel : tank["barrels"]) {
            tanksconfig[index].barrels.push_back(BarrelConfig {
                .angle = barrel["angle"],
                .width = barrel["width"],
                .length = barrel["length"],
                .full_reload = barrel["full_reload"],
                .reload_delay = barrel["reload_delay"],
                .recoil = barrel["recoil"],
                .bullet_speed = barrel["bullet_speed"],
                .bullet_damage = barrel["bullet_damage"],
                .bullet_penetration = barrel["bullet_penetration"]});
        }
    }

    tanks_file.close();
    return 0;
}
