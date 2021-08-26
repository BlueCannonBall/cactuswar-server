#include "bcblog.hpp"
#include "entityconfig.hpp"
#include "quadtree.hpp"
#include "streampeerbuffer.hpp"
#include "ws28/src/Server.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <leveldb/db.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <uv.h>
#include <vector>

#pragma once
//#define THREADING
//#define DEBUG_MAINLOOP_SPEED
#define COLLISION_STRENGTH     5
#define BOT_ACCURACY_THRESHOLD 30
#define HOT_RELOAD_TIMEOUT     30
#define TARGET_TPS             30
#define RAND(a, b)             rand() % (b - a + 1) + a
#define ELLIPSIS               "…"

using namespace std;
using namespace spb;
using json = nlohmann::json;

leveldb::DB* db;          // NOLINT
leveldb::Options options; // NOLINT

enum class Packet {
    InboundInit = 0,
    Input = 1,
    Census = 2,
    OutboundInit = 3,
    Mockups = 3,
    Chat = 4,
    Death = 5,
    Respawn = 6
};

struct ClientInfo {
    string path;
    ws28::RequestHeaders headers;
};

unsigned uid = 0;                               // NOLINT
unordered_map<ws28::Client*, ClientInfo> paths; // NOLINT

inline unsigned int get_uid() {
    if (uid > 4294947295) {
        WARN("IDs are close to the 32-bit unsigned integer limit");
    }
    return uid++;
}

inline bool aabb(shared_ptr<qt::Rect> rect1, shared_ptr<qt::Rect> rect2) {
    return (
        rect1->x < rect2->x + rect2->width &&
        rect1->x + rect1->width > rect2->x &&
        rect1->y < rect2->y + rect2->height &&
        rect1->y + rect1->height > rect2->y);
}

inline bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

void ban(ws28::Client* client, bool destroy = false) { // NOLINT
    leveldb::Status s;
    string ip;
    if (paths[client].headers.Get("x-forwarded-for")) {
        ip = paths[client].headers.Get("x-forwarded-for");
        ip = ip.substr(0, ip.find(","));
    } else {
        ip = client->GetIP();
    }
    s = db->Put(leveldb::WriteOptions(), ip, "1");
    if (!s.ok()) {
        ERR("Failed to ban player: " << s.ToString());
    } else {
        INFO("Banned player with ip " << ip);
    }

    if (destroy)
        client->Destroy();
}

template <typename A, typename B>
std::pair<B, A> flip_pair(const std::pair<A, B>& p) {
    return std::pair<B, A>(p.second, p.first);
}

template <typename A, typename B>
std::multimap<B, A> flip_map(const std::map<A, B>& src) {
    std::multimap<B, A> dst;
    std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()), flip_pair<A, B>);
    return dst;
}

std::string truncate(std::string& str, size_t width, bool ellipsis = true) { // NOLINT
    if (str.length() + sizeof(ELLIPSIS) + 1 > width) {
        if (ellipsis) {
            return str.substr(0, width) + ELLIPSIS;
        } else {
            return str.substr(0, width);
        }
    }
    return str;
}

class Vector2 {
public:
    float x = 0;
    float y = 0;

    Vector2(float x = 0, float y = 0) {
        this->x = x;
        this->y = y;
    }

    Vector2 operator+(const Vector2& v) {
        return Vector2(this->x + v.x, this->y + v.y);
    }

    Vector2 operator-(const Vector2& v) {
        return Vector2(this->x - v.x, this->y - v.y);
    }

    Vector2 operator*(const Vector2& v) {
        return Vector2(this->x * v.x, this->y * v.y);
    }

    Vector2 operator/(const Vector2& v) {
        return Vector2(this->x / v.x, this->y / v.y);
    }

    Vector2& operator+=(const Vector2& v) {
        this->x += v.x;
        this->y += v.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& v) {
        this->x -= v.x;
        this->y -= v.y;
        return *this;
    }

    Vector2& operator*=(const Vector2& v) {
        this->x *= v.x;
        this->y *= v.y;
        return *this;
    }

    Vector2& operator/=(const Vector2& v) {
        this->x /= v.x;
        this->y /= v.y;
        return *this;
    }

    friend ostream& operator<<(ostream& output, const Vector2& v) {
        output << "(" << v.x << ", " << v.y << ")";
        return output;
    }

    float distance_to(const Vector2& v) {
        return sqrt(pow(v.x - this->x, 2) + pow(v.y - this->y, 2));
    }

    float angle_to(const Vector2& v) {
        return atan2(this->y - v.y, this->x - v.x);
    }

    float angle() {
        return this->angle_to(Vector2(0, 0));
    }

    Vector2 normalize() {
        float angle = this->angle();
        return Vector2(cos(angle), sin(angle));
    }
};

bool circle_collision(const Vector2& pos1, unsigned int radius1, const Vector2& pos2, unsigned int radius2) { // NOLINT
    float dx = pos1.x - pos2.x;
    float dy = pos1.y - pos2.y;
    float distance = sqrt(dx * dx + dy * dy);
    return distance < radius1 + radius2;
}

template <class T1, class T2>
inline bool in_map(T1& map, const T2& object) {
    return map.find(object) != map.end();
}

template <class T1, class T2>
inline bool in_vec(T1& vec, const T2& object) {
    return find(vec.begin(), vec.end(), object) != vec.end();
}

class Arena;

// Base entity
class Entity {
public:
    Vector2 position;
    Vector2 velocity;
    unsigned int id;
    float rotation = 0;
    static constexpr float friction = 0.9f;
    string name = "Entity";
    unsigned short radius = 50;
    float max_health = 500;
    float health = max_health;
    float damage = 0;
    float mass = 1;
    shared_ptr<qt::Rect> qt_rect = make_shared<qt::Rect>();

    void take_census(StreamPeerBuffer&);
    void collision_response(Arena*);
    void next_tick(Arena*);
};

// A shape, includes cacti and rocks.
class Shape: public Entity {
public:
    unsigned short radius = 100;
    float mass = 5;
    float damage = 20;
    float reward = 0.075f;

    Shape() {
        this->radius = RAND(85, 115);
    }

    void take_census(StreamPeerBuffer& buf) {
        buf.put_u8(1);                // id
        buf.put_u32(this->id);        // game id
        buf.put_16(this->position.x); // position
        buf.put_16(this->position.y);
        buf.put_float(this->health / this->max_health); // health
        buf.put_u16(this->radius);                      // radius
        //buf.put_u8(7); // sides
    }

    void next_tick(Arena* arena);
    void collision_response(Arena* arena);
};

class Tank;

// Dictates current barrel timer state
enum class BarrelTarget {
    ReloadDelay,
    CoolingDown,
    None
};

// Represents a timer
struct Timer {
    BarrelTarget target = BarrelTarget::None;
    unsigned long time = 0;
};

// A tank barrel.
struct Barrel: public BarrelConfig {
    Timer target_time;
    bool cooling_down = false;
    unsigned reload = full_reload;

    void fire(Tank*, Arena*) __attribute__((hot));
};

struct ChatMessage {
    string content;
    unsigned long time = 0;
};

enum class TankType {
    Local,
    Remote
};

enum class TankState {
    Alive,
    Dead
};

// A tank, stats vary based on mockups.
class Tank: public Entity {
public:
    struct Input {
        bool W = false;
        bool A = false;
        bool S = false;
        bool D = false;
        bool mousedown = false;
        Vector2 mousepos;
    };

    string name = "Unnamed";
    ws28::Client* client = nullptr;
    Input input;
    float movement_speed = 4;
    static constexpr float friction = 0.8f;
    vector<unique_ptr<Barrel>> barrels;
    unsigned int mockup;
    unsigned char fov;
    float level = 1;
    ChatMessage message;
    TankType type = TankType::Remote;
    TankState state = TankState::Alive;
    chrono::time_point<chrono::steady_clock> spawn_time = chrono::steady_clock::now();
    shared_ptr<qt::Rect> viewport_rect = make_shared<qt::Rect>();

    void next_tick(Arena* arena);
    void collision_response(Arena* arena) __attribute__((hot));

    void take_census(StreamPeerBuffer& buf, unsigned long time) {
        buf.put_u8(0);                // id
        buf.put_u32(this->id);        // game id
        buf.put_16(this->position.x); // position
        buf.put_16(this->position.y);
        buf.put_float(this->rotation); // rotation
        buf.put_16(this->velocity.x);  // velocity
        buf.put_16(this->velocity.y);
        buf.put_u8(this->mockup);                                  // mockup id
        buf.put_float(this->health / this->max_health);            // health
        buf.put_u16(this->radius);                                 // radius
        buf.put_string(this->name);                                // tank name
        if (message.time == 0 || time - (30 * 5) > message.time) { // show chat messages for 30*5 ticks
            buf.put_string("");                                    // empty message
        } else {
            buf.put_string(message.content);
        }
    }

    void define(unsigned int index) {
        this->barrels.clear();

        const TankConfig& tank = tanksconfig[index];
        for (const auto& barrel : tank.barrels) {
            unique_ptr<Barrel> new_barrel(new Barrel);
            new_barrel->full_reload = barrel.full_reload - barrel.reload_delay;
            new_barrel->reload = barrel.full_reload;
            new_barrel->reload_delay = barrel.reload_delay;
            new_barrel->recoil = barrel.recoil;
            new_barrel->bullet_speed = barrel.bullet_speed;
            new_barrel->bullet_damage = barrel.bullet_damage;
            new_barrel->angle = barrel.angle;
            new_barrel->width = barrel.width;
            new_barrel->length = barrel.length;
            new_barrel->bullet_penetration = barrel.bullet_penetration;
            this->barrels.push_back(std::move(new_barrel));
        }

        fov = tank.fov;

        mockup = index;
    }
};

class Bullet: public Entity {
public:
    unsigned short radius = 25;
    static constexpr float friction = 1;
    short lifetime = 50;
    float damage = 20;
    float max_health = 10;
    float health = max_health;
    unsigned int owner;

    void take_census(StreamPeerBuffer& buf) {
        buf.put_u8(2);                // id
        buf.put_u32(this->id);        // game id
        buf.put_16(this->position.x); // position
        buf.put_16(this->position.y);
        buf.put_u16(this->radius);    // radius
        buf.put_16(this->velocity.x); // velocity
        buf.put_16(this->velocity.y);
        buf.put_u32(this->owner); // owner of bullet
    }

    void next_tick(Arena* arena);
    void collision_response(Arena* arena);
};

class Arena {
public:
    struct Entities {
        unordered_map<unsigned int, Shape*> shapes;
        unordered_map<unsigned int, Tank*> tanks;
        unordered_map<unsigned int, Bullet*> bullets;
    };

    unsigned long ticks = 0;
    Entities entities;
    unsigned short size = 5000;
    qt::Quadtree tree = qt::Quadtree(make_unique<qt::Rect>(qt::Rect {
        .x = 0,
        .y = 0,
        .width = static_cast<float>(size),
        .height = static_cast<float>(size)}));
    unsigned int target_shape_count = 50;
    unsigned short target_bot_count = 6;

    uv_fs_event_t entityconfig_event_handle;
    uv_timer_t timer;

#ifdef THREADING
    mutex qt_mtx;
    uv_rwlock_t entity_lock;
#endif

    Arena() {
#ifdef THREADING
        uv_rwlock_init(&entity_lock);
#endif
        uv_fs_event_init(uv_default_loop(), &entityconfig_event_handle);
        entityconfig_event_handle.data = this;
        uv_fs_event_start(
            &entityconfig_event_handle, [](uv_fs_event_t* handle, const char* filename, int events, int status) {
                INFO("Hot reloading entityconfig.json");
                std::this_thread::sleep_for(chrono::milliseconds(HOT_RELOAD_TIMEOUT));
                tanksconfig.clear();
                assert(load_tanks_from_json(filename) == 0);
                Arena* arena = (Arena*) handle->data;
                StreamPeerBuffer buf(true);
                for (const auto& tank : arena->entities.tanks) {
                    if (tank.second->type == TankType::Remote) {
                        buf.reset();
                        arena->send_init_packet(buf, tank.second);
                        tank.second->define(tank.second->mockup);
                    }
                }
            },
            "entityconfig.json",
            UV_FS_EVENT_STAT);
    }

    ~Arena() {
#ifdef THREADING
        uv_rwlock_wrlock(&entity_lock);
#endif

        for (auto entity = this->entities.shapes.cbegin(); entity != this->entities.shapes.cend();) {
            destroy_entity(entity++->first, this->entities.shapes);
        }
        for (auto entity = this->entities.tanks.cbegin(); entity != this->entities.tanks.cend();) {
            destroy_entity(entity++->first, this->entities.tanks);
        }
        for (auto entity = this->entities.bullets.cbegin(); entity != this->entities.bullets.cend();) {
            destroy_entity(entity++->first, this->entities.bullets);
        }
#ifdef THREADING
        uv_rwlock_wrunlock(&entity_lock);
        uv_rwlock_destroy(&entity_lock);
#endif
        uv_fs_event_stop(&entityconfig_event_handle);
        uv_timer_stop(&timer);
    }

    void set_size(unsigned short _size) {
        if (_size == this->size) {
            return;
        }
        this->size = _size;
        tree.clear();
        tree = qt::Quadtree(make_unique<qt::Rect>(qt::Rect {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(_size),
            .height = static_cast<float>(_size)}));
    }

    inline void update_size() {
        set_size(this->entities.tanks.size() * 1000 + 5000); // 1000 more per tank
        target_shape_count = size * size / 700000;
    }

    void send_init_packet(StreamPeerBuffer& buf, Tank* player) {
        buf.put_u8((unsigned char) Packet::OutboundInit); // packet id
        buf.put_u32(player->id);                          // player id
        buf.put_u8(tanksconfig.size());                   // amount of mockups
        for (const auto& tank : tanksconfig) {
            buf.put_string(tank.name);
            buf.put_u8(tank.fov);
            buf.put_u8(tank.barrels.size());
            for (const auto& barrel : tank.barrels) {
                buf.put_float(barrel.width);
                buf.put_float(barrel.length);
                buf.put_float(barrel.angle);
            }
        }

        player->client->Send(reinterpret_cast<char*>(buf.data()), buf.size(), 0x2);
    }

    void send_death_packet(StreamPeerBuffer& buf, Tank* player) {
        buf.put_u8((unsigned char) Packet::Death); // packet id
        auto death_time = chrono::steady_clock::now();
        chrono::duration<double> elapsed_seconds = death_time - player->spawn_time;
        if (elapsed_seconds.count() > 15) {
            INFO("\"" << player->name << "\" lived for " << elapsed_seconds.count() << "s before dying");
        } else {
            BRUH("Noob \"" << player->name << "\" lived for " << elapsed_seconds.count() << "s before dying");
        }
        buf.put_double(elapsed_seconds.count()); // seconds elapsed since spawn
        player->client->Send(reinterpret_cast<char*>(buf.data()), buf.size(), 0x2);
    }

    void handle_init_packet(StreamPeerBuffer& buf, ws28::Client* client) {
        if (client->GetUserData() != nullptr) {
            unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
            if (in_map(this->entities.tanks, player_id)) {
                WARN("Existing player tried to send init packet");
                destroy_entity(player_id, this->entities.tanks);
                ban(client, true);
                return;
            }
            WARN("Non-existent player with non-null id tried to send init packet");
            destroy_entity(player_id, entities.tanks);
            ban(client, true);
            return;
        }

        string player_name;
        if (buf.get_string(player_name) != 0) {
            WARN("Client tried to send invalid init packet");
            ban(client, true);
            return;
        } else if (buf.size() - buf.offset != 0) {
            WARN("Client tried to send invalid init packet");
            ban(client, true);
            return;
        }
        if (player_name.size() == 0) {
            player_name = "Unnamed";
        }
        player_name = truncate(player_name, 14, false);
        Tank* new_player = new Tank;
        new_player->name = player_name;
        new_player->client = client;

        unsigned int player_id = get_uid();
        client->SetUserData(reinterpret_cast<void*>(player_id));
        new_player->id = player_id;
        this->entities.tanks[player_id] = new_player;
        new_player->position = Vector2(RAND(0, size), RAND(0, size));
        new_player->define(RAND(0, tanksconfig.size() - 1));

        INFO("New player with name \"" << player_name << "\" and id " << player_id << " joined. There are currently " << entities.tanks.size() << " player(s) in game");

        update_size();

        buf.reset();
        send_init_packet(buf, new_player);

        // tracking
        std::string value;
        string ip;
        if (paths[client].headers.Get("x-forwarded-for")) {
            ip = paths[client].headers.Get("x-forwarded-for");
            ip = ip.substr(0, ip.find(","));
        } else {
            ip = client->GetIP();
        }
        if (db->Get(leveldb::ReadOptions(), ip, &value).IsNotFound()) {
            db->Put(leveldb::WriteOptions(), ip, "0");
        }
    }

    void handle_input_packet(StreamPeerBuffer& buf, ws28::Client* client) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        if (!in_map(this->entities.tanks, player_id)) {
            WARN("Player without id tried to send input packet");
            destroy_entity(player_id, this->entities.tanks);
            ban(client, true);
            return;
        } else if (entities.tanks[player_id]->state == TankState::Dead) {
            WARN("Dead player tried to send input packet");
            return;
        }
        Tank* player = entities.tanks[player_id];

        unsigned char movement_byte = buf.get_u8();
        player->input = {.W = false, .A = false, .S = false, .D = false, .mousedown = false};

        if (0b10000 & movement_byte) {
            player->input.W = true;
            //puts("w");
        } else if (0b00100 & movement_byte) {
            player->input.S = true;
            //puts("s");
        }

        if (0b01000 & movement_byte) {
            player->input.A = true;
            //puts("a");
        } else if (0b00010 & movement_byte) {
            player->input.D = true;
            //puts("d");
        }

        if (0b00001 & movement_byte) {
            player->input.mousedown = true;
        }

        short mousex = buf.get_16();
        short mousey = buf.get_16();
        player->input.mousepos = Vector2(mousex, mousey);
        player->rotation = atan2(
            player->input.mousepos.y - player->position.y,
            player->input.mousepos.x - player->position.x);
    }

    void handle_chat_packet(StreamPeerBuffer& buf, ws28::Client* client) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        if (!in_map(this->entities.tanks, player_id)) {
            WARN("Player without id tried to send chat packet");
            destroy_entity(player_id, this->entities.tanks);
            ban(client, true);
            return;
        } else if (entities.tanks[player_id]->state == TankState::Dead) {
            WARN("Dead player tried to send chat packet");
            return;
        }
        Tank* player = entities.tanks[player_id];

        string message;
        if (buf.get_string(message) != 0) {
            WARN("Player tried to send invalid chat packet");
            destroy_entity(player_id, this->entities.tanks);
            ban(client, true);
            return;
        }
        if (message.size() == 0) {
            player->message.time = 0;
            return;
        }

        player->message.time = ticks;
        player->message.content = truncate(message, 100, true);
        INFO("\"" << player->name << "\" says: " << player->message.content);
    }

    void handle_respawn_packet(StreamPeerBuffer& buf, ws28::Client* client) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        if (!in_map(this->entities.tanks, player_id)) {
            WARN("Player without id tried to send respawn packet");
            destroy_entity(player_id, this->entities.tanks);
            ban(client, true);
            return;
        } else if (entities.tanks[player_id]->state == TankState::Alive) {
            WARN("Living player tried to send respawn packet");
            destroy_entity(player_id, this->entities.tanks);
            ban(client, true);
            return;
        }
        Tank* player = entities.tanks[player_id];

        player->position = Vector2(RAND(0, size), RAND(0, size));
        player->health = player->max_health;
        if (player->level / 2 >= 1) {
            player->level = player->level / 2;
        } else {
            player->level = 1;
        }
        player->state = TankState::Alive;
        player->spawn_time = chrono::steady_clock::now();
    }

    template <typename T>
    void destroy_entity(unsigned int entity_id, unordered_map<unsigned int, T*>& entity_map) {
        T* entity_ptr = entity_map[entity_id];
#ifdef THREADING
        uv_rwlock_wrlock(&entity_lock);
#endif
        entity_map.erase(entity_id);
#ifdef THREADING
        uv_rwlock_wrunlock(&entity_lock);
#endif
        if (entity_ptr != nullptr) {
            delete entity_ptr;
        }
        if (typeid(T) == typeid(Tank)) {
            update_size();
        }
    }

    void update() __attribute__((hot)) {
        bool found_player = false;
        for (const auto& tank : entities.tanks) {
            if (tank.second->type == TankType::Remote) {
                found_player = true;
                break;
            }
        }
        if (!found_player) {
            return;
        }

        ticks++;

        this->tree.clear();

        if (entities.shapes.size() <= target_shape_count - 12) {
#pragma omp simd
            for (unsigned int i = 0; i < (target_shape_count - entities.shapes.size()); i++) {
                Shape* new_shape = new Shape;
                new_shape->id = get_uid();
                new_shape->position = Vector2(RAND(0, size), RAND(0, size));
                entities.shapes[new_shape->id] = new_shape;
            }
        }

#ifdef THREADING
        thread shape_tick([this]() {
#endif
            for (auto entity = this->entities.shapes.cbegin(); entity != this->entities.shapes.cend();) {
                if (entity->second == nullptr) {
                    this->destroy_entity(entity++->first, this->entities.shapes);
                    continue;
                }

                if (entity->second->health <= 0) {
                    this->destroy_entity(entity++->first, this->entities.shapes);
                    continue;
                }
#ifdef THREADING
                uv_rwlock_rdlock(&entity_lock);
#endif
                entity->second->next_tick(this);
#ifdef THREADING
                this->qt_mtx.lock();
#endif
                this->tree.insert(entity->second->qt_rect);
#ifdef THREADING
                this->qt_mtx.unlock();
                uv_rwlock_rdunlock(&entity_lock);
#endif
                ++entity;
            }
#ifdef THREADING
        });
#endif

#ifdef THREADING
        thread tank_tick([this]() {
#endif
            for (auto entity = this->entities.tanks.cbegin(); entity != this->entities.tanks.cend();) {
                if (entity->second == nullptr) {
                    this->destroy_entity(entity++->first, this->entities.tanks);
                    continue;
                }
#ifdef THREADING
                uv_rwlock_rdlock(&entity_lock);
#endif
                if (entity->second->state == TankState::Dead) {
#ifdef THREADING
                    uv_rwlock_rdunlock(&entity_lock);
#endif
                    ++entity;
                    continue;
                }

                if (entity->second->health <= 0) {
                    entity->second->input = {.W = false, .A = false, .S = false, .D = false, .mousedown = false, .mousepos = Vector2(0, 0)};
                    entity->second->health = entity->second->max_health;
                    if (entity->second->type == TankType::Local) {
                        entity->second->position = Vector2(RAND(0, size), RAND(0, size));
                        entity->second->health = entity->second->max_health;
                        if (entity->second->level / 2 >= 1) {
                            entity->second->level = entity->second->level / 2;
                        } else {
                            entity->second->level = 1;
                        }
                    } else {
                        entity->second->state = TankState::Dead;
                        StreamPeerBuffer buf(true);
                        send_death_packet(buf, entity++->second);
#ifdef THREADING
                        uv_rwlock_rdunlock(&entity_lock);
#endif
                        continue;
                    }
                }
#ifdef THREADING
                uv_rwlock_rdunlock(&entity_lock);
#endif
                entity->second->next_tick(this);
#ifdef THREADING
                uv_rwlock_rdlock(&entity_lock);
                this->qt_mtx.lock();
#endif
                this->tree.insert(entity->second->qt_rect);
#ifdef THREADING
                this->qt_mtx.unlock();
                uv_rwlock_rdunlock(&entity_lock);
#endif
                ++entity;
            }
#ifdef THREADING
        });
#endif

#ifdef THREADING
        thread bullet_tick([this]() {
#endif
            for (auto entity = this->entities.bullets.cbegin(); entity != this->entities.bullets.cend();) {
                if (entity->second == nullptr) {
                    this->destroy_entity(entity++->first, this->entities.bullets);
                    continue;
                }

                entity->second->lifetime--;
                if (entity->second->lifetime <= 0) {
                    this->destroy_entity(entity++->first, this->entities.bullets);
                    continue;
                } else if (entity->second->health <= 0) {
                    this->destroy_entity(entity++->first, this->entities.bullets);
                    continue;
                }
#ifdef THREADING
                uv_rwlock_rdlock(&entity_lock);
#endif
                entity->second->next_tick(this);
#ifdef THREADING
                this->qt_mtx.lock();
#endif
                this->tree.insert(entity->second->qt_rect);
#ifdef THREADING
                this->qt_mtx.unlock();
                uv_rwlock_rdunlock(&entity_lock);
#endif
                ++entity;
            }
#ifdef THREADING
        });
#endif

#ifdef THREADING
        shape_tick.join();
        tank_tick.join();
        bullet_tick.join();
#endif

#ifdef THREADING
        thread shape_collide([this]() {
#endif
            for (auto entity = this->entities.shapes.cbegin(); entity != this->entities.shapes.cend();) {
                entity->second->collision_response(this);
                ++entity;
            }
#ifdef THREADING
        });
#endif

#ifdef THREADING
        thread tank_collide([this]() {
#endif
            for (auto entity = this->entities.tanks.cbegin(); entity != this->entities.tanks.cend();) {
                entity->second->collision_response(this);
                ++entity;
            }
#ifdef THREADING
        });
#endif

#ifdef THREADING
        thread bullet_collide([this]() {
#endif
            for (auto entity = this->entities.bullets.cbegin(); entity != this->entities.bullets.cend();) {
                entity->second->collision_response(this);
                ++entity;
            }
#ifdef THREADING
        });
#endif

#ifdef THREADING
        shape_collide.join();
        tank_collide.join();
        bullet_collide.join();
#endif
    }

    void run() {
#pragma omp simd
        for (unsigned int i = 0; i < target_shape_count; i++) {
            Shape* new_shape = new Shape;
            new_shape->id = get_uid();
            new_shape->position = Vector2(RAND(0, size), RAND(0, size));
            entities.shapes[new_shape->id] = new_shape;
        }

#pragma omp simd
        for (unsigned int i = 0; i < target_bot_count; i++) {
            Tank* new_tank = new Tank;
            new_tank->type = TankType::Local;
            new_tank->id = get_uid();
            new_tank->position = Vector2(RAND(0, size), RAND(0, size));
            new_tank->define(RAND(0, tanksconfig.size() - 1));
            entities.tanks[new_tank->id] = new_tank;
        }
        this->update_size();

        uv_timer_init(uv_default_loop(), &timer);
        timer.data = this;
        uv_timer_start(
            &timer, [](uv_timer_t* timer) {
#ifdef DEBUG_MAINLOOP_SPEED
                auto t0 = chrono::high_resolution_clock::now();
#endif
                Arena* arena = (Arena*) timer->data;
                arena->update();
#ifdef DEBUG_MAINLOOP_SPEED
                auto t1 = chrono::high_resolution_clock::now();
                INFO("Mainloop took " << chrono::duration_cast<chrono::microseconds>(t1 - t0).count() << "μs");
#endif
            },
            0,
            1000 / TARGET_TPS);
    }
};

/* OVERLOADS */

void Barrel::fire(Tank* tank, Arena* arena) { // NOLINT
    Bullet* new_bullet = new Bullet;
    new_bullet->position = tank->position + (Vector2(cos(tank->rotation + angle), sin(tank->rotation + angle)).normalize() * Vector2(tank->radius + new_bullet->radius + 1, tank->radius + new_bullet->radius + 1));
    new_bullet->velocity = Vector2(cos(tank->rotation + this->angle) * bullet_speed, sin(tank->rotation + this->angle) * bullet_speed);
    tank->velocity -= Vector2(cos(tank->rotation + angle) * this->recoil, sin(tank->rotation + angle) * this->recoil);
    new_bullet->id = get_uid();
    new_bullet->owner = tank->id;
    new_bullet->radius = this->width * tank->radius;

    // set stats
    new_bullet->damage = this->bullet_damage;
    new_bullet->max_health = this->bullet_penetration;
    new_bullet->health = new_bullet->max_health;

#ifdef THREADING
    uv_rwlock_rdunlock(&arena->entity_lock);
    uv_rwlock_wrlock(&arena->entity_lock);
#endif
    arena->entities.bullets[new_bullet->id] = new_bullet;
#ifdef THREADING
    uv_rwlock_wrunlock(&arena->entity_lock);
    uv_rwlock_rdlock(&arena->entity_lock);
#endif
}

// Example collision response 👇
void Entity::collision_response(Arena* arena) { // NOLINT
    vector<shared_ptr<qt::Rect>> candidates;
    arena->tree.retrieve(qt_rect, candidates);

    for (const auto& candidate : candidates) {
        if (candidate->id == this->id) {
            continue;
        } else if (in_map(arena->entities.tanks, candidate->id)) {
            if (arena->entities.tanks[candidate->id]->state == TankState::Dead) {
                continue;
            }
        }

        if (circle_collision(Vector2(candidate->x + candidate->radius, candidate->y + candidate->radius), candidate->radius, Vector2(this->position.x, this->position.y), this->radius)) {
            // response
            float angle = atan2((candidate->y + candidate->radius) - this->position.y, (candidate->x + candidate->radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Shape::collision_response(Arena* arena) { // NOLINT
    vector<shared_ptr<qt::Rect>> candidates;
    arena->tree.retrieve(qt_rect, candidates);

    for (const auto& candidate : candidates) {
        if (candidate->id == this->id) {
            continue;
        } else if (in_map(arena->entities.tanks, candidate->id)) {
            if (arena->entities.tanks[candidate->id]->state == TankState::Dead) {
                continue;
            }
        }

        if (circle_collision(Vector2(candidate->x + candidate->radius, candidate->y + candidate->radius), candidate->radius, Vector2(this->position.x, this->position.y), this->radius)) {
            if (in_map(arena->entities.bullets, candidate->id)) {
                this->health -= arena->entities.bullets[candidate->id]->damage; // damage
                if (this->health <= 0) {
                    if (in_map(arena->entities.tanks, arena->entities.bullets[candidate->id]->owner)) {
                        arena->entities.tanks[arena->entities.bullets[candidate->id]->owner]->level += this->reward;
                    } else {
                        BRUH("The bullet of a non-existent player hit and killed a shape");
                    }
                    return; // death
                }
            }

            // response
            float angle = atan2((candidate->y + candidate->radius) - this->position.y, (candidate->x + candidate->radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Tank::collision_response(Arena* arena) { // NOLINT
    vector<shared_ptr<qt::Rect>> candidates;
    arena->tree.retrieve(qt_rect, candidates);

    for (const auto& candidate : candidates) {
        if (candidate->id == this->id) {
            continue;
        } else if (in_map(arena->entities.bullets, candidate->id)) {
            if (arena->entities.bullets[candidate->id]->owner == this->id) {
                continue;
            }
        } else if (in_map(arena->entities.tanks, candidate->id)) {
            if (arena->entities.tanks[candidate->id]->state == TankState::Dead) {
                continue;
            }
        }

        if (circle_collision(Vector2(candidate->x + candidate->radius, candidate->y + candidate->radius), candidate->radius, Vector2(this->position.x, this->position.y), this->radius)) {
            if (in_map(arena->entities.bullets, candidate->id)) {
                this->health -= arena->entities.bullets[candidate->id]->damage; // damage
                if (this->health <= 0) {
                    if (in_map(arena->entities.tanks, arena->entities.bullets[candidate->id]->owner)) {
                        arena->entities.tanks[arena->entities.bullets[candidate->id]->owner]->level += this->level / 2;
                    } else {
                        BRUH("The bullet of a non-existent player hit and killed another tank");
                    }
                    return; // death
                }
            } else if (in_map(arena->entities.shapes, candidate->id)) {
                this->health -= arena->entities.shapes[candidate->id]->damage; // damage
            }

            // response
            float angle = atan2((candidate->y + candidate->radius) - this->position.y, (candidate->x + candidate->radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }

    float dr = 112.5 * this->fov * 1.6;

    this->viewport_rect->x = this->position.x - dr / 2;
    this->viewport_rect->y = this->position.y - dr / 2;
    this->viewport_rect->width = dr;
    this->viewport_rect->height = dr;
    this->viewport_rect->radius = dr / 2;

    // auto viewport = make_shared<qt::Rect>(qt::Rect {
    //     .x = this->position.x - dr / 2,
    //     .y = this->position.y - dr / 2,
    //     .width = dr,
    //     .height = dr,
    //     .id = 0,
    //     .radius = static_cast<unsigned int>(dr / 2)});

    arena->tree.retrieve(viewport_rect, candidates);

    if (this->type == TankType::Remote) {
        StreamPeerBuffer buf(true);
        unsigned short census_size = 0;

        for (const auto& candidate : candidates) {
            if (aabb(viewport_rect, candidate)) {
                if (in_map(arena->entities.tanks, candidate->id)) {
                    if (arena->entities.tanks[candidate->id]->state == TankState::Alive) {
                        arena->entities.tanks[candidate->id]->take_census(buf, arena->ticks);
                        census_size++;
                    }
                } else if (in_map(arena->entities.shapes, candidate->id)) {
                    arena->entities.shapes[candidate->id]->take_census(buf);
                    census_size++;
                } else if (in_map(arena->entities.bullets, candidate->id)) {
                    arena->entities.bullets[candidate->id]->take_census(buf);
                    census_size++;
                }
            }
        }

        buf.offset = 0;
        buf.put_u8((unsigned char) Packet::Census);
        buf.put_u16(census_size);
        buf.put_u16(arena->size);
        buf.put_float(this->level);
        this->client->Send(reinterpret_cast<char*>(buf.data()), buf.size(), 0x2);
    } else if (arena->ticks % 2 == 0) {
        map<unsigned int, unsigned int> nearby_tanks;
        map<unsigned int, unsigned int> nearby_shapes;

        for (const auto& candidate : candidates) {
            if (candidate->id == this->id) {
                continue;
            }

            if (aabb(viewport_rect, candidate)) {
                if (in_map(arena->entities.tanks, candidate->id)) {
                    if (arena->entities.tanks[candidate->id]->state == TankState::Alive) {
                        nearby_tanks[candidate->id] = arena->entities.tanks[candidate->id]->position.distance_to(this->position);
                    }
                } else if (in_map(arena->entities.shapes, candidate->id)) {
                    nearby_shapes[candidate->id] = arena->entities.shapes[candidate->id]->position.distance_to(this->position);
                }
            }
        }

        input = {.W = false, .A = false, .S = false, .D = false, .mousedown = true, .mousepos = Vector2(0, 0)};

        unsigned int dist;
        if (nearby_tanks.size() > 0) {
            auto sorted_nearby_tanks = flip_map(nearby_tanks);
            this->input.mousepos = arena->entities.tanks[sorted_nearby_tanks.begin()->second]->position;
            dist = sorted_nearby_tanks.begin()->first;
        } else if (nearby_shapes.size() > 0) {
            auto sorted_nearby_shapes = flip_map(nearby_shapes);
            this->input.mousepos = arena->entities.shapes[sorted_nearby_shapes.begin()->second]->position;
            dist = sorted_nearby_shapes.begin()->first;
        } else {
            return;
        }

        this->rotation = atan2(
            this->input.mousepos.y - this->position.y,
            this->input.mousepos.x - this->position.x);
        if (dist > static_cast<unsigned>(400 + this->radius)) {
            if (position.x > input.mousepos.x &&
                abs(position.x - input.mousepos.x) > BOT_ACCURACY_THRESHOLD)
                input.A = true;
            else if (abs(position.x - input.mousepos.x) > BOT_ACCURACY_THRESHOLD)
                input.D = true;

            if (position.y > input.mousepos.y &&
                abs(position.y - input.mousepos.y) > BOT_ACCURACY_THRESHOLD)
                input.W = true;
            else if (abs(position.y - input.mousepos.y) > BOT_ACCURACY_THRESHOLD)
                input.S = true;
        }
    }
}

void Bullet::collision_response(Arena* arena) { // NOLINT
    vector<shared_ptr<qt::Rect>> candidates;
    arena->tree.retrieve(qt_rect, candidates);

    for (const auto& candidate : candidates) {
        if (candidate->id == this->id) {
            continue;
        } else if (candidate->id == this->owner) {
            continue;
        } else if (in_map(arena->entities.bullets, candidate->id)) {
            if (arena->entities.bullets[candidate->id]->owner == this->owner) {
                continue;
            }
        } else if (in_map(arena->entities.tanks, candidate->id)) {
            if (arena->entities.tanks[candidate->id]->state == TankState::Dead) {
                continue;
            }
        }

        if (circle_collision(Vector2(candidate->x + candidate->radius, candidate->y + candidate->radius), candidate->radius, Vector2(this->position.x, this->position.y), this->radius)) {
            if (in_map(arena->entities.bullets, candidate->id)) {
                this->health -= arena->entities.bullets[candidate->id]->damage; // damage
            } else if (in_map(arena->entities.shapes, candidate->id)) {
                this->health -= arena->entities.shapes[candidate->id]->damage; // damage
            }

            // response
            float angle = atan2((candidate->y + candidate->radius) - this->position.y, (candidate->x + candidate->radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Tank::next_tick(Arena* arena) { // NOLINT
#ifdef THREADING
    uv_rwlock_rdlock(&arena->entity_lock);
#endif
    if (this->input.W) {
        this->velocity.y -= this->movement_speed;
    } else if (this->input.S) {
        this->velocity.y += this->movement_speed;
    }

    if (this->input.A) {
        this->velocity.x -= this->movement_speed;
    } else if (this->input.D) {
        this->velocity.x += this->movement_speed;
    }

    for (const auto& barrel : barrels) {
        if (this->input.mousedown) {
            if (!barrel->cooling_down) {
                barrel->cooling_down = true;

                barrel->target_time.time = arena->ticks + barrel->reload_delay;
                barrel->target_time.target = BarrelTarget::ReloadDelay;
            }
        }
        if (barrel->target_time.target != BarrelTarget::None) {
            if (barrel->target_time.time <= arena->ticks) {
                switch (barrel->target_time.target) {
                    case BarrelTarget::ReloadDelay: {
                        barrel->fire(this, arena); // SAFETY: `this` and `arena` are supposedly always valid.
                        barrel->cooling_down = true;
                        barrel->target_time.time = arena->ticks + barrel->full_reload;
                        barrel->target_time.target = BarrelTarget::CoolingDown;
                        break;
                    }
                    case BarrelTarget::CoolingDown: {
                        barrel->cooling_down = false;
                        barrel->target_time.target = BarrelTarget::None;
                        break;
                    }
                    case BarrelTarget::None: break;
                };
            }
        }
    }

    if (health != max_health) {
        health += max_health * 0.0013;
        if (health > max_health)
            health = max_health;
    }

    this->radius = 50 + (level * 0.25);

    this->velocity *= Vector2(this->friction, this->friction);
    this->position += this->velocity / Vector2(this->mass, this->mass);

    if (this->position.x > arena->size) {
        this->position.x = arena->size;
        this->velocity.x = 0;
    } else if (this->position.x < 0) {
        this->position.x = 0;
        this->velocity.x = 0;
    }
    if (this->position.y > arena->size) {
        this->position.y = arena->size;
        this->velocity.y = 0;
    } else if (this->position.y < 0) {
        this->position.y = 0;
        this->velocity.y = 0;
    }

    this->qt_rect->x = this->position.x - this->radius;
    this->qt_rect->y = this->position.y - this->radius;
    this->qt_rect->width = this->radius * 2;
    this->qt_rect->height = this->radius * 2;
    this->qt_rect->radius = this->radius;
    this->qt_rect->id = this->id;
#ifdef THREADING
    uv_rwlock_rdunlock(&arena->entity_lock);
#endif
}

void Bullet::next_tick(Arena* arena) { // NOLINT
    this->velocity *= Vector2(this->friction, this->friction);
    this->position += this->velocity / Vector2(this->mass, this->mass);

    if (this->position.x > arena->size) {
        this->position.x = arena->size;
        this->velocity.x = 0;
    } else if (this->position.x < 0) {
        this->position.x = 0;
        this->velocity.x = 0;
    }
    if (this->position.y > arena->size) {
        this->position.y = arena->size;
        this->velocity.y = 0;
    } else if (this->position.y < 0) {
        this->position.y = 0;
        this->velocity.y = 0;
    }

    this->qt_rect->x = this->position.x - this->radius;
    this->qt_rect->y = this->position.y - this->radius;
    this->qt_rect->width = this->radius * 2;
    this->qt_rect->height = this->radius * 2;
    this->qt_rect->radius = this->radius;
    this->qt_rect->id = this->id;
}

void Shape::next_tick(Arena* arena) { // NOLINT
    this->velocity *= Vector2(this->friction, this->friction);
    this->position += this->velocity / Vector2(this->mass, this->mass);

    if (this->position.x > arena->size) {
        this->position.x = arena->size;
        this->velocity.x = 0;
    } else if (this->position.x < 0) {
        this->position.x = 0;
        this->velocity.x = 0;
    }
    if (this->position.y > arena->size) {
        this->position.y = arena->size;
        this->velocity.y = 0;
    } else if (this->position.y < 0) {
        this->position.y = 0;
        this->velocity.y = 0;
    }

    this->qt_rect->x = this->position.x - this->radius;
    this->qt_rect->y = this->position.y - this->radius;
    this->qt_rect->width = this->radius * 2;
    this->qt_rect->height = this->radius * 2;
    this->qt_rect->radius = this->radius;
    this->qt_rect->id = this->id;
}

///////////
