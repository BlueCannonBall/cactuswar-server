#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include <vector>
#include <map>
#include "bcblog.hpp"
#include "quadtree.hpp"
#include <thread>
#include <mutex>
#include "entityconfig.hpp"

#pragma once
#define COLLISION_STRENGTH 5
#define BOT_COUNT 3
#define THREADING

using namespace std;
using namespace spb;

enum class Packet {
    InboundInit = 0,
    Input = 1,
    Census = 2,
    OutboundInit = 3,
    Chat = 4
};

unsigned uid = 0;

inline unsigned int get_uid() {
    return uid++;
}

inline bool aabb(qt::Rect rect1, qt::Rect rect2) {
    return (
        rect1.x < rect2.x + rect2.width &&
        rect1.x + rect1.width > rect2.x &&
        rect1.y < rect2.y + rect2.height &&
        rect1.y + rect1.height > rect2.y
    );
}

template<typename A, typename B>
std::pair<B,A> flip_pair(const std::pair<A,B>& p) {
    return std::pair<B,A>(p.second, p.first);
}

template<typename A, typename B>
std::multimap<B,A> flip_map(const std::map<A,B>& src) {
    std::multimap<B,A> dst;
    std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()),
        flip_pair<A,B>);
    return dst;
}

class Vector2 {
    public:
        float x = 0;
        float y = 0;
        
        Vector2(float x=0, float y=0) {
            this->x = x;
            this->y = y;
        }
        
        Vector2 operator+(const Vector2& v) {
            return Vector2(this->x+v.x, this->y+v.y);
        }
        
        Vector2 operator-(const Vector2& v) {
            return Vector2(this->x-v.x, this->y-v.y);
        }
        
        Vector2 operator*(const Vector2& v) {
            return Vector2(this->x*v.x, this->y*v.y);
        }
        
        Vector2 operator/(const Vector2& v) {
            return Vector2(this->x/v.x, this->y/v.y);
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
        
        friend ostream &operator<<(ostream &output, const Vector2 &v) {
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

bool circle_collision(const Vector2& pos1, unsigned int radius1, const Vector2& pos2, unsigned int radius2) {
    float dx = pos1.x - pos2.x;
    float dy = pos1.y - pos2.y;
    float distance = sqrt(dx * dx + dy * dy);
    return distance < radius1 + radius2;
}

class Arena;

/// Base entity
class Entity {
    public:
        Vector2 position = Vector2(0, 0);
        Vector2 velocity = Vector2(0, 0);
        unsigned int id;
        float rotation = 0;
        static constexpr float friction = 0.9f;
        string name = "Entity";
        unsigned short radius = 50;
        float max_health = 500;
        float health = max_health;
        float damage = 0;
        float mass = 1;

        void take_census(StreamPeerBuffer&);
        void collision_response(Arena*);
        void next_tick(Arena*);
};

/// A shape, includes cacti and rocks.
class Shape: public Entity {
    public:
        unsigned short radius = 100;
        float mass = 5;
        float damage = 20;
        float reward = 0.075f;

        Shape() {
            this->radius = rand() % 36 + 85;
        }

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(1); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->health / this->max_health); // health
            buf.put_u16(this->radius); // radius
            //buf.put_u8(7); // sides
        }

        void next_tick(Arena *arena);
        void collision_response(Arena *arena);
};

class Tank;

/// Dictates current barrel timer state
enum class BarrelTarget {
    ReloadDelay,
    CoolingDown,
    None
};

/// Represents a timer
struct Timer {
    BarrelTarget target = BarrelTarget::None;
    unsigned long time = 0;
};

/// A tank barrel.
class Barrel {
    public:
        /* All these values get overridden */
        unsigned full_reload = 6;
        unsigned reload_delay;
        unsigned reload = full_reload;
        Timer target_time;
        bool cooling_down = false;

        float recoil;
        float width;
        float length;
        float angle;

        // stats
        float bullet_speed;
        float bullet_damage;
        float bullet_penetration;

        void fire(Tank*, Arena*);
};

struct ChatMessage {
    string content;
    unsigned long time = 0;
};

enum class TankType {
    Player,
    Bot
};

/// A tank, stats vary based on mockups.
class Tank: public Entity {
    public:
        struct Input {
            bool W;
            bool A;
            bool S;
            bool D;
            bool mousedown;
            Vector2 mousepos;
        };

        string name = "Unnamed";
        ws28::Client *client = nullptr;
        Input input = Input {.W = false, .A = false, .S = false, .D = false, .mousedown = false, .mousepos = Vector2(0, 0)};
        float movement_speed = 4;
        static constexpr float friction = 0.8f;
        vector<Barrel*> barrels;
        unsigned int mockup;
        unsigned char fov;
        float level = 1;
        ChatMessage message;
        TankType type = TankType::Player;

        void next_tick(Arena* arena);
        void collision_response(Arena *arena);

        void take_census(StreamPeerBuffer& buf, unsigned long time) {
            buf.put_u8(0); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->rotation); // rotation
            buf.put_16(this->velocity.x); // velocity
            buf.put_16(this->velocity.y);
            buf.put_u8(this->mockup); // mockup id
            buf.put_float(this->health / this->max_health); // health
            buf.put_u16(this->radius); // radius
            buf.put_string(this->name); // player name
            if (message.time == 0 || time - (30 * 7) > message.time) { // show chat messages for 7 seconds
                buf.put_string(""); // empty message
            } else {
                buf.put_string(message.content);
            }
        }

        inline void clear_barrels() {
            for (const auto barrel : barrels) {
                delete barrel;
            }
            this->barrels.clear();
        }

        void define(unsigned int index) {
            clear_barrels();

            const TankConfig& tank = tanksconfig[index];
            for (const auto& barrel : tank.barrels) {
                Barrel* new_barrel = new Barrel;
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
                this->barrels.push_back(new_barrel);
            }

            fov = tank.fov;

            mockup = index;
        }

        ~Tank() {
            clear_barrels();
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
            buf.put_u8(2); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_u16(this->radius); // radius
            buf.put_16(this->velocity.x); // velocity
            buf.put_16(this->velocity.y);
            buf.put_u32(this->owner); // owner of bullet
        }

        void next_tick(Arena *arena);
        void collision_response(Arena *arena);
};

class Arena {
    public:
        struct Entities {
            map<unsigned int, Entity*> entities;
            map<unsigned int, Shape*> shapes;
            map<unsigned int, Tank*> tanks;
            map<unsigned int, Bullet*> bullets;
        };

        unsigned long ticks = 0;
        Entities entities;
        unsigned short size = 5000;
        qt::Quadtree tree = qt::Quadtree(qt::Rect {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(size),
            .height = static_cast<float>(size)
        });
        unsigned int target_shape_count = 50;
#ifdef THREADING
        mutex qtmtx;
        mutex entitymtx;
#endif

        ~Arena() {
            WARN("Arena destroyed, not freeing entities");
        }

        void set_size(unsigned short _size) __attribute__((cold)) {
            if (_size == this->size) {
                return;
            }
            this->size = _size;
            tree.clear();
            tree = qt::Quadtree(qt::Rect {
                .x = 0,
                .y = 0,
                .width = static_cast<float>(_size),
                .height = static_cast<float>(_size)
            });
        }

        inline void update_size() {
            set_size(this->entities.tanks.size() * 500 + 5000); // 500 more per player
            target_shape_count = size / 100;
        }
        
        void handle_init_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            string player_name = buf.get_string();
            if (player_name.size() == 0) {
                player_name = "Unnamed";
            }
            Tank* new_player = new Tank;
            new_player->name = player_name;
            new_player->client = client;

            unsigned int player_id = get_uid();
            client->SetUserData(reinterpret_cast<void*>(player_id));
            new_player->id = player_id;
            this->entities.tanks[player_id] = new_player;
            new_player->position = Vector2(rand() % size, rand() % size);
            new_player->define(3);

            INFO("New player with name \"" << player_name << "\" and id " << player_id << " joined. There are currently " << entities.tanks.size() << " player(s) in game");
            
            update_size();

            buf.data_array.clear();
            buf.offset = 0;
            buf.put_u8((unsigned char) Packet::OutboundInit); // packet id
            buf.put_u32(player_id); // player id
            buf.put_u8(tanksconfig.size()); // amount of mockups
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

            client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2);
        }
        
        void handle_input_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            unsigned char movement_byte = buf.get_u8();
            unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
            
            if (this->entities.tanks.find(player_id) == this->entities.tanks.end()) {
                WARN("Player without id tried to send input packet");
                client->Destroy();
                return;
            }
            entities.tanks[player_id]->input = {.W = false, .A = false, .S = false, .D = false, .mousedown = false, .mousepos = Vector2(0, 0)};
            
            if (0b10000 & movement_byte) {
                entities.tanks[player_id]->input.W = true;
                //puts("w");
            } else if (0b00100 & movement_byte) {
                entities.tanks[player_id]->input.S = true;
                //puts("s");
            }
            
            if (0b01000 & movement_byte) {
                entities.tanks[player_id]->input.A = true;
                //puts("a");
            } else if (0b00010 & movement_byte) {
                entities.tanks[player_id]->input.D = true;
                //puts("d");
            }

            if (0b00001 & movement_byte) {
                entities.tanks[player_id]->input.mousedown = true;
            }

            short mousex = buf.get_16();
            short mousey = buf.get_16();
            entities.tanks[player_id]->input.mousepos = Vector2(mousex, mousey);
            entities.tanks[player_id]->rotation = atan2(
                entities.tanks[player_id]->input.mousepos.y - entities.tanks[player_id]->position.y,
                entities.tanks[player_id]->input.mousepos.x - entities.tanks[player_id]->position.x
            );
        }

        void handle_chat_packet(StreamPeerBuffer& buf, ws28::Client* client) {
            unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
            
            if (this->entities.tanks.find(player_id) == this->entities.tanks.end()) {
                WARN("Player without id tried to send chat packet");
                client->Destroy();
                return;
            }

            entities.tanks[player_id]->message.time = ticks;
            entities.tanks[player_id]->message.content = buf.get_string();
            INFO("\"" << entities.tanks[player_id]->name << "\" says: " << entities.tanks[player_id]->message.content);
        }
        
        template<typename T>
        void destroy_entity(unsigned int entity_id, map<unsigned int, T>& entity_map) {
            T entity_ptr = entity_map[entity_id];
#ifdef THREADING
            entitymtx.lock();
#endif
            entity_map.erase(entity_id);
#ifdef THREADING
            entitymtx.unlock();
#endif
            if (entity_ptr != nullptr) {
                delete entity_ptr;
            } else {
                WARN("Tried to destroy null entity");
            }
        }

        void update() __attribute__((hot)) {
            bool found_player = false;
            for (const auto& tank : entities.tanks) {
                if (tank.second->type == TankType::Player) {
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
                INFO("Replenishing shapes");
                #pragma omp for simd
                for (unsigned int i = 0; i<(target_shape_count - entities.shapes.size()); i++) {
                    Shape *new_shape = new Shape;
                    new_shape->id = get_uid();
                    new_shape->position = Vector2(rand() % size, rand() % size);
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
                    entity->second->next_tick(this);
#ifdef THREADING
                    this->qtmtx.lock();
#endif
                    this->tree.insert(qt::Rect {
                        .x = entity->second->position.x - entity->second->radius, 
                        .y = entity->second->position.y - entity->second->radius, 
                        .width = static_cast<float>(entity->second->radius*2), 
                        .height = static_cast<float>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
#ifdef THREADING
                    this->qtmtx.unlock();
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
                        this->entities.tanks.erase(entity++);
                        continue;
                    }
                    
                    if (entity->second->health <= 0) {
                        entity->second->position = Vector2(rand() % this->size-3000 + 3000, rand() % this->size-3000 + 3000);
                        entity->second->health = entity->second->max_health;
                        if (entity->second->level / 2 >= 1) {
                            entity->second->level = entity->second->level / 2;
                        } else {
                            entity->second->level = 1;
                        }

                    }
                    entity->second->next_tick(this);
#ifdef THREADING
                    this->qtmtx.lock();
#endif
                    this->tree.insert(qt::Rect {
                        .x = entity->second->position.x - entity->second->radius, 
                        .y = entity->second->position.y - entity->second->radius, 
                        .width = static_cast<float>(entity->second->radius*2), 
                        .height = static_cast<float>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
#ifdef THREADING
                    this->qtmtx.unlock();
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
                    entity->second->next_tick(this);
#ifdef THREADING
                    this->qtmtx.lock();
#endif
                    this->tree.insert(qt::Rect {
                        .x = entity->second->position.x - entity->second->radius, 
                        .y = entity->second->position.y - entity->second->radius, 
                        .width = static_cast<float>(entity->second->radius*2), 
                        .height = static_cast<float>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
#ifdef THREADING
                    this->qtmtx.unlock();
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

        void run() __attribute__((cold)) {
            #pragma omp for simd
            for (unsigned int i = 0; i<target_shape_count; i++) {
                Shape *new_shape = new Shape;
                new_shape->id = get_uid();
                new_shape->position = Vector2(rand() % size, rand() % size);
                entities.shapes[new_shape->id] = new_shape;
            }

            #pragma omp for simd
            for (unsigned int i = 0; i<BOT_COUNT; i++) {
                Tank* new_tank = new Tank;
                new_tank->type = TankType::Bot;
                new_tank->id = get_uid();
                new_tank->position = Vector2(rand() % size, rand() % size);
                new_tank->define(rand() % tanksconfig.size());
                entities.tanks[new_tank->id] = new_tank;
            }
            this->update_size();

            uv_timer_t* timer = (uv_timer_t*) malloc(sizeof(uv_timer_t));
            uv_timer_init(uv_default_loop(), timer);
            timer->data = this;
            uv_timer_start(timer, [](uv_timer_t *timer) {
                auto &arena = *(Arena*)(timer->data);
                arena.update();
                //uv_update_time(uv_default_loop());
            }, 0, 1000/30);
        }
};


/* OVERLOADS */

void Barrel::fire(Tank* player, Arena* arena) {
    Bullet *new_bullet = new Bullet;
    new_bullet->position = player->position + (Vector2(cos(player->rotation + angle), sin(player->rotation + angle)).normalize() * Vector2(player->radius + new_bullet->radius + 1, player->radius + new_bullet->radius + 1));
    // new_bullet->position = player->input.mousepos;
    new_bullet->velocity = Vector2(cos(player->rotation + this->angle) * bullet_speed, sin(player->rotation + this->angle) * bullet_speed);
    player->velocity -= Vector2(cos(player->rotation + angle) * this->recoil, sin(player->rotation + angle) * this->recoil);
    new_bullet->id = get_uid();
    new_bullet->owner = player->id;
    new_bullet->radius = this->width * player->radius;

    // set stats
    new_bullet->damage = this->bullet_damage;
    new_bullet->max_health = this->bullet_penetration;
    new_bullet->health = new_bullet->max_health;

#ifdef THREADING
    arena->entitymtx.lock();
#endif
    arena->entities.bullets[new_bullet->id] = new_bullet;
#ifdef THREADING
    arena->entitymtx.unlock();
#endif
}

void Entity::collision_response(Arena* arena) {
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->position.x - this->radius,
        .y = this->position.y - this->radius,
        .width = static_cast<float>(this->radius*2),
        .height = static_cast<float>(this->radius*2),
        .id = this->id,
        .radius = this->radius
    });

    for (const auto& canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->position.x, this->position.y), this->radius)) {
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->position.y, (canidate.x + canidate.radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Shape::collision_response(Arena* arena) {
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->position.x - this->radius,
        .y = this->position.y - this->radius,
        .width = static_cast<float>(this->radius*2),
        .height = static_cast<float>(this->radius*2),
        .id = this->id,
        .radius = this->radius
    });

    for (const auto& canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->position.x, this->position.y), this->radius)) {
            if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
                this->health -= arena->entities.bullets[canidate.id]->damage; // damage
                if (this->health <= 0) {
                    arena->entities.tanks[arena->entities.bullets[canidate.id]->owner]->level += this->reward;
                    return; // death
                }
            }
            
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->position.y, (canidate.x + canidate.radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Tank::collision_response(Arena *arena) {
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->position.x - this->radius, 
        .y = this->position.y - this->radius, 
        .width = static_cast<float>(this->radius*2), 
        .height = static_cast<float>(this->radius*2), 
        .id = this->id, 
        .radius = this->radius
    });

    for (const auto& canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        } else if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
            if (arena->entities.bullets[canidate.id]->owner == this->id) {
                continue;
            }
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->position.x, this->position.y), this->radius)) {
            if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
                this->health -= arena->entities.bullets[canidate.id]->damage; // damage
                if (this->health <= 0) {
                    arena->entities.tanks[arena->entities.bullets[canidate.id]->owner]->level += this->level / 2;
                    return; // death
                }
            } else if (arena->entities.shapes.find(canidate.id) != arena->entities.shapes.end()) {
                this->health -= arena->entities.shapes[canidate.id]->damage; // damage
            }
            
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->position.y, (canidate.x + canidate.radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }

    float dr = 112.5 * this->fov * 1.8;

    qt::Rect viewport = {
        .x = this->position.x - dr/2,
        .y = this->position.y - dr/2,
        .width = dr,
        .height = dr,
        .id = 0,
        .radius = static_cast<unsigned int>(dr/2)
    };

    canidates = arena->tree.retrieve(viewport);

    if (this->type == TankType::Player) {
        StreamPeerBuffer buf(true);
        unsigned short census_size = 0;

        for (const auto& canidate : canidates) {
            if (aabb(viewport, canidate)) {
                if (arena->entities.tanks.find(canidate.id) != arena->entities.tanks.end()) {
                    arena->entities.tanks[canidate.id]->take_census(buf, arena->ticks);
                    census_size++;
                } else if (arena->entities.shapes.find(canidate.id) != arena->entities.shapes.end()) {
                    arena->entities.shapes[canidate.id]->take_census(buf);
                    census_size++;
                } else if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
                    arena->entities.bullets[canidate.id]->take_census(buf);
                    census_size++;
                }
            }
        }

        buf.offset = 0;
        buf.put_u8((unsigned char) Packet::Census);
        buf.put_u16(census_size);
        buf.put_u16(arena->size);
        buf.put_float(this->level);
        this->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2);
    } else if (this->type == TankType::Bot) {
        map<unsigned int, unsigned int> nearby_tanks;
        map<unsigned int, unsigned int> nearby_shapes;

        for (const auto& canidate : canidates) {
            if (canidate.id == this->id) {
                continue;
            }

            if (aabb(viewport, canidate)) {
                if (arena->entities.tanks.find(canidate.id) != arena->entities.tanks.end()) {
                    nearby_tanks[canidate.id] = arena->entities.tanks[canidate.id]->position.distance_to(this->position);
                } else if (arena->entities.shapes.find(canidate.id) != arena->entities.shapes.end()) {
                    nearby_shapes[canidate.id] = arena->entities.shapes[canidate.id]->position.distance_to(this->position);
                }
            }
        }

        input = {.W = false, .A = false, .S = false, .D = false, .mousedown = false, .mousepos = Vector2(0, 0)};
        
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
            this->input.mousepos.x - this->position.x
        );
        this->input.mousedown = true;
        if (dist > 400) {
            if (position.x > input.mousepos.x) input.A = true;
            else input.D = true;
            if (position.y > input.mousepos.y) input.W = true;
            else input.S = true;
        }
    }
}


void Bullet::collision_response(Arena *arena) {
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->position.x - this->radius,
        .y = this->position.y - this->radius,
        .width = static_cast<float>(this->radius*2),
        .height = static_cast<float>(this->radius*2),
        .id = this->id,
        .radius = this->radius
    });

    for (const auto& canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        } else if (canidate.id == this->owner) {
            continue;
        } else if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
            if (arena->entities.bullets[canidate.id]->owner == this->owner) {
                continue;
            }
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->position.x, this->position.y), this->radius)) {
            if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
                this->health -= arena->entities.bullets[canidate.id]->damage; // damage
            } else if (arena->entities.shapes.find(canidate.id) != arena->entities.shapes.end()) {
                this->health -= arena->entities.shapes[canidate.id]->damage; // damage
            }

            // response
            float angle = atan2((canidate.y + canidate.radius) - this->position.y, (canidate.x + canidate.radius) - this->position.x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}


void Tank::next_tick(Arena *arena) {
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

    for (auto barrel : barrels) {
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

    this->radius = 50 + level * 2;

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
}

void Bullet::next_tick(Arena* arena) {
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
}

void Shape::next_tick(Arena* arena) {
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
}

///////////
