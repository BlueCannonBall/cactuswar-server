#include <vector>
#include <string>
#include <cmath>

using namespace std;

struct BarrelConfig {
    float angle;
    float width;
    float length;
    unsigned full_reload;
    unsigned reload_delay;
    float recoil;
    float bullet_speed;
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
                .width = 1,
                .length = 3,
                .full_reload = 6,
                .reload_delay = 0,
                .recoil = 0.35f,
                .bullet_speed = 50,
            }
        }
    }
};
