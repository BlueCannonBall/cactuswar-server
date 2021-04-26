#include <vector>
#include <string>
#include <cmath>

#pragma once

using namespace std;

struct BarrelConfig {
    float angle;
    float width;
    float length;
    unsigned full_reload;
    unsigned reload_delay;
    float recoil;
    float bullet_speed;
    float bullet_damage;
};

struct TankConfig {
    string name;
    unsigned char fov;
    vector<BarrelConfig> barrels;
};

const vector<TankConfig> tanksconfig = {
    TankConfig {
        .name = "Station",
        .fov = 20,
        .barrels = {
            BarrelConfig {
                .angle = 0,
                .width = 0.5,
                .length = 2,
                .full_reload = 6,
                .reload_delay = 0,
                .recoil = 0.35f,
                .bullet_speed = 50,
                .bullet_damage = 20
            }
        }
    }
};
