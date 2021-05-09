#include <vector>
#include <string>
#include <cmath>

#pragma once
#define PI M_PI

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

const std::vector<TankConfig> tanksconfig = {
    TankConfig {
        .name = "Station",
        .fov = 20,
        .barrels = {
            BarrelConfig {
                .angle = 0,
                .width = 0.5f,
                .length = 1,
                .full_reload = 6,
                .reload_delay = 0,
                .recoil = 0.35f,
                .bullet_speed = 50,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            }
        }
    },
    TankConfig {
        .name = "Obliterator",
        .fov = 20,
        .barrels = {
            BarrelConfig {
                .angle = 0,
                .width = 1,
                .length = 1,
                .full_reload = 40,
                .reload_delay = 0,
                .recoil = 100,
                .bullet_speed = 30,
                .bullet_damage = 50000,
                .bullet_penetration = 300,
            }
        }
    },
    TankConfig {
        .name = "Escort",
        .fov = 20,
        .barrels = {
            BarrelConfig {
                .angle = 0,
                .width = 0.5f,
                .length = 1,
                .full_reload = 9,
                .reload_delay = 7,
                .recoil = 100,
                .bullet_speed = 30,
                .bullet_damage = 100,
                .bullet_penetration = 450,
            },
            BarrelConfig {
                .angle = PI/2,
                .width = 0.5,
                .length = 1,
                .full_reload = 9,
                .reload_delay = 0,
                .recoil = 100,
                .bullet_speed = 30,
                .bullet_damage = 100,
                .bullet_penetration = 450,
            },
            BarrelConfig {
                .angle = PI,
                .width = 0.5,
                .length = 1,
                .full_reload = 9,
                .reload_delay = 7,
                .recoil = 100,
                .bullet_speed = 30,
                .bullet_damage = 100,
                .bullet_penetration = 450,
            },
            BarrelConfig {
                .angle = (PI/2) * 3,
                .width = 0.5,
                .length = 1,
                .full_reload = 9,
                .reload_delay = 0,
                .recoil = 100,
                .bullet_speed = 30,
                .bullet_damage = 100,
                .bullet_penetration = 450,
            }
        },
    },
    TankConfig {
        .name = "Auxillary",
        .fov = 20,
        .barrels = {
            BarrelConfig {
                .angle = -0.2,
                .width = 0.3f,
                .length = 1,
                .full_reload = 6,
                .reload_delay = 3,
                .recoil = 1,
                .bullet_speed = 50,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            },
            BarrelConfig {
                .angle = 0.2,
                .width = 0.3f,
                .length = 1,
                .full_reload = 6,
                .reload_delay = 3,
                .recoil = 1,
                .bullet_speed = 50,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            },
            BarrelConfig {
                .angle = 0,
                .width = 0.5f,
                .length = 1.0,
                .full_reload = 6,
                .reload_delay = 0,
                .recoil = 3,
                .bullet_speed = 50,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            },
        }
    },
    TankConfig {
        .name = "Assailant",
        .fov = 20,
        .barrels = {
            BarrelConfig {
                .angle = PI + 0.45,
                .width = 0.5f,
                .length = 0.75,
                .full_reload = 6,
                .reload_delay = 3,
                .recoil = 10,
                .bullet_speed = 10,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            },
            BarrelConfig {
                .angle = PI - 0.45,
                .width = 0.5f,
                .length = 0.75,
                .full_reload = 6,
                .reload_delay = 3,
                .recoil = 10,
                .bullet_speed = 10,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            },
            BarrelConfig {
                .angle = 0,
                .width = 0.5f,
                .length = 1.0,
                .full_reload = 6,
                .reload_delay = 0,
                .recoil = 0.35f,
                .bullet_speed = 50,
                .bullet_damage = 20,
                .bullet_penetration = 30,
            },
        }
    },
};
