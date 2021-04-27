#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include <vector>
#include <unordered_map>
#include "bcblog.hpp"
#include "quadtree.hpp"
#include <thread>
#include <mutex>
#include "entityconfig.hpp"

#pragma once
#define COLLISION_STRENGTH 5
#define ARENA_SIZE 12000
#define THREADING

using namespace std;
using namespace spb;

unsigned int uid = 0;

unsigned int get_uid() {
    if (uid >= 4294965295) {
        WARN("IDs are close to the 32-bit unsigned number limit");
    }
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
        unsigned radius = 50;
        float max_health = 500;
        float health = max_health;
        float damage = 0;
        float mass = 1;

        void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity / Vector2(this->mass, this->mass);

            if (this->position.x > ARENA_SIZE) {
                this->position.x = ARENA_SIZE;
                this->velocity.x = 0;
            } else if (this->position.x < 0) {
                this->position.x = 0;
                this->velocity.x = 0;
            }
            if (this->position.y > ARENA_SIZE) {
                this->position.y = ARENA_SIZE;
                this->velocity.y = 0;
            } else if (this->position.y < 0) {
                this->position.y = 0;
                this->velocity.y = 0;
            }
        }

        void take_census(StreamPeerBuffer& buf) {}
        void collision_response(Arena *arena);

        // ~Entity() {
        //     INFO("Entity destroyed.");
        // }
};

/// A shape, includes cacti and rocks.
class Shape: public Entity {
    public:
        unsigned radius = 100;
        float mass = 5;
        float damage = 20;

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(1); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->health); // health
            //buf.put_u8(7); // sides
        }

        void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity / Vector2(this->mass, this->mass);

            if (this->position.x > ARENA_SIZE) {
                this->position.x = ARENA_SIZE;
                this->velocity.x = 0;
            } else if (this->position.x < 0) {
                this->position.x = 0;
                this->velocity.x = 0;
            }
            if (this->position.y > ARENA_SIZE) {
                this->position.y = ARENA_SIZE;
                this->velocity.y = 0;
            } else if (this->position.y < 0) {
                this->position.y = 0;
                this->velocity.y = 0;
            }
        }

        void collision_response(Arena *arena);
};

class Tank;

/// Dictates current barrel timer state
enum class BarrelTarget {
    ReloadDelay
};

/// Represents a timer
struct Timer {
    BarrelTarget target;
    unsigned int time;
};

/// A tank barrel.
class Barrel {
    public:
        /* All these values get overridden */
        unsigned full_reload;
        unsigned reload = full_reload;
        Timer* targetTime;

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

/// A domtank, stats vary based on mockups.
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

        void next_tick(Arena* arena);
        void collision_response(Arena *arena);

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(0); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->rotation); // rotation
            buf.put_16(this->velocity.x); // velocity
            buf.put_16(this->velocity.y);
            buf.put_u8(this->mockup); // mockup id
            buf.put_float(this->health); // health
            buf.put_u16(this->radius); // radius
            buf.put_string(this->name); // player name
        }

        void define(unsigned int index) {
            for (const auto barrel : barrels) {
                free(barrel);
            }
            this->barrels = vector<Barrel*>();

            const TankConfig& tank = tanksconfig[index];
            for (const auto& barrel : tank.barrels) {
                Barrel* new_barrel = (Barrel*) malloc(sizeof(Barrel));
                new_barrel->full_reload = barrel.full_reload;
                new_barrel->reload = barrel.full_reload + barrel.reload_delay;
                new_barrel->recoil = barrel.recoil;
                new_barrel->bullet_speed = barrel.bullet_speed;
                new_barrel->bullet_damage = barrel.bullet_damage;
                new_barrel->angle = barrel.angle;
                new_barrel->width = barrel.width;
                new_barrel->length = barrel.length;
                new_barrel->bullet_penetration = barrel.bullet_penetration;
                this->barrels.push_back(new_barrel);
            }
            mockup = index;
        }

        ~Tank() {
            for (const auto barrel : barrels) {
                free(barrel);
            }
        }
};

class Bullet: public Entity {
    public:
        unsigned int radius = 25;
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
        }

        void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity / Vector2(this->mass, this->mass);

            if (this->position.x > ARENA_SIZE) {
                this->position.x = ARENA_SIZE;
                this->velocity.x = 0;
            } else if (this->position.x < 0) {
                this->position.x = 0;
                this->velocity.x = 0;
            }
            if (this->position.y > ARENA_SIZE) {
                this->position.y = ARENA_SIZE;
                this->velocity.y = 0;
            } else if (this->position.y < 0) {
                this->position.y = 0;
                this->velocity.y = 0;
            }
        }

        void collision_response(Arena *arena);
};

class Arena {
    public:
        struct Entities {
            unordered_map<unsigned int, Entity*> entities;
            unordered_map<unsigned int, Shape*> shapes;
            unordered_map<unsigned int, Tank*> players;
            unordered_map<unsigned int, Bullet*> bullets;
        };

        Entities entities;
        qt::Quadtree tree = qt::Quadtree(qt::Rect {.x = 0, .y = 0, .width = ARENA_SIZE, .height = ARENA_SIZE}, 10, 4);
        unsigned int target_shape_count = 125;
#ifdef THREADING
        mutex qtmtx;
#endif
        
        void handle_init_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            string player_name = buf.get_string();
            Tank* new_player = new Tank;
            new_player->name = player_name;
            new_player->client = client;

            unsigned int player_id = get_uid();
            client->SetUserData(reinterpret_cast<void*>(player_id));
            new_player->id = player_id;
            this->entities.players[player_id] = new_player;
            new_player->position = Vector2(rand() % ARENA_SIZE-3000 + 3000, rand() % ARENA_SIZE-3000 + 3000);
            //new_player->barrels.push_back(new Barrel);
            new_player->define(3);

            INFO("New player with name \"" << player_name << "\" and id " << player_id << " joined. There are currently " << entities.players.size() << " player(s) in game");
            
            buf.data_array = vector<unsigned char>();
            buf.offset = 0;
            buf.put_u8(3); // packet id
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
            
            if (this->entities.players.find(player_id) == this->entities.players.end()) {
                WARN("Player without id tried to send input packet");
                client->Destroy();
                return;
            }
            entities.players[player_id]->input = {.W = false, .A = false, .S = false, .D = false, .mousedown = false, .mousepos = Vector2(0, 0)};
            
            if (0b10000 & movement_byte) {
                entities.players[player_id]->input.W = true;
                //puts("w");
            } else if (0b00100 & movement_byte) {
                entities.players[player_id]->input.S = true;
                //puts("s");
            }
            
            if (0b01000 & movement_byte) {
                entities.players[player_id]->input.A = true;
                //puts("a");
            } else if (0b00010 & movement_byte) {
                entities.players[player_id]->input.D = true;
                //puts("d");
            }

            if (0b00001 & movement_byte) {
                entities.players[player_id]->input.mousedown = true;
            }

            short mousex = buf.get_16();
            short mousey = buf.get_16();
            entities.players[player_id]->input.mousepos = Vector2(mousex, mousey);
            entities.players[player_id]->rotation = atan2(
                entities.players[player_id]->input.mousepos.y - entities.players[player_id]->position.y,
                entities.players[player_id]->input.mousepos.x - entities.players[player_id]->position.x
            );
            // INFO("Player rotation: " << entities.players[player_id]->rotation);
            // INFO("Player position: " << entities.players[player_id]->position);
            // INFO("Mouse position: " << entities.players[player_id]->input.mousepos);
        }
        
        void update() __attribute__((hot)) {
            if (entities.players.size() == 0) {
                return;
            }

            this->tree.clear();

            if (entities.shapes.size() <= target_shape_count - 12) {
                INFO("Replenishing shapes");
                for (unsigned int i = 0; i<(target_shape_count - entities.shapes.size()); i++) {
                    Shape *new_shape = new Shape;
                    new_shape->id = get_uid();
                    new_shape->position = Vector2(rand() % ARENA_SIZE + 0, rand() % ARENA_SIZE + 0);
                    entities.shapes[new_shape->id] = new_shape;
                }
            }
#ifndef THREADING
            Arena* arena = this;
#endif
            
#ifdef THREADING
            thread shape_tick([](Arena* arena) {
#endif
                for (auto entity = arena->entities.shapes.cbegin(); entity != arena->entities.shapes.cend();) {
                    if (entity->second == nullptr) {
                        Shape* entity_ptr = entity->second;
                        arena->entities.shapes.erase(entity++);
                        delete entity_ptr;
                        continue;
                    }
                    
                    if (entity->second->health <= 0) {
                        Shape* entity_ptr = entity->second;
                        arena->entities.shapes.erase(entity++);
                        delete entity_ptr;
                        continue;
                    }
                    entity->second->next_tick();
                    //entity.second->take_census(buf);
#ifdef THREADING
                    arena->qtmtx.lock();
#endif
                    arena->tree.insert(qt::Rect {
                        .x = entity->second->position.x - entity->second->radius, 
                        .y = entity->second->position.y - entity->second->radius, 
                        .width = static_cast<float>(entity->second->radius*2), 
                        .height = static_cast<float>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
#ifdef THREADING
                    arena->qtmtx.unlock();
#endif
                    ++entity;
                }
#ifdef THREADING
            }, this);
#endif
            
#ifdef THREADING
            thread player_tick([](Arena* arena) {
#endif
                for (auto entity = arena->entities.players.cbegin(); entity != arena->entities.players.cend();) {
                    if (entity->second == nullptr) {
                        Tank* entity_ptr = entity->second;
                        arena->entities.players.erase(entity++);
                        delete entity_ptr;
                        continue;
                    }
                    
                    if (entity->second->health <= 0) {
                        entity->second->position = Vector2(rand() % ARENA_SIZE-3000 + 3000, rand() % ARENA_SIZE-3000 + 3000);
                        entity->second->health = entity->second->max_health;
                    }
                    entity->second->next_tick(arena);
#ifdef THREADING
                    arena->qtmtx.lock();
#endif
                    arena->tree.insert(qt::Rect {
                        .x = entity->second->position.x - entity->second->radius, 
                        .y = entity->second->position.y - entity->second->radius, 
                        .width = static_cast<float>(entity->second->radius*2), 
                        .height = static_cast<float>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
#ifdef THREADING
                    arena->qtmtx.unlock();
#endif
                    ++entity;
                }
#ifdef THREADING
            }, this);
#endif

#ifdef THREADING
            thread bullet_tick([](Arena* arena) {
#endif
                for (auto entity = arena->entities.bullets.cbegin(); entity != arena->entities.bullets.cend();) {
                    if (entity->second == nullptr) {
                        Bullet* entity_ptr = entity->second;
                        arena->entities.bullets.erase(entity++);
                        delete entity_ptr;
                        continue;
                    }
                    
                    entity->second->lifetime--;
                    if (entity->second->lifetime <= 0) {
                        Bullet* entity_ptr = entity->second;
                        arena->entities.bullets.erase(entity++);
                        delete entity_ptr;
                        continue;
                    } else if (entity->second->health <= 0) {
                        Bullet* entity_ptr = entity->second;
                        arena->entities.bullets.erase(entity++);
                        delete entity_ptr;
                        continue;
                    }
                    entity->second->next_tick();
#ifdef THREADING
                    arena->qtmtx.lock();
#endif
                    arena->tree.insert(qt::Rect {
                        .x = entity->second->position.x - entity->second->radius, 
                        .y = entity->second->position.y - entity->second->radius, 
                        .width = static_cast<float>(entity->second->radius*2), 
                        .height = static_cast<float>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
#ifdef THREADING
                    arena->qtmtx.unlock();
#endif
                    ++entity;
                }
#ifdef THREADING
            }, this);
#endif

#ifdef THREADING
            shape_tick.join();
            player_tick.join();
            bullet_tick.join();
#endif

#ifdef THREADING
            thread shape_collide([](Arena* arena) {
#endif
                for (auto entity = arena->entities.shapes.cbegin(); entity != arena->entities.shapes.cend();) {
                    entity->second->collision_response(arena);
                    ++entity;
                }
#ifdef THREADING
            }, this);
#endif

#ifdef THREADING
            thread player_collide([](Arena* arena) {
#endif
                for (auto entity = arena->entities.players.cbegin(); entity != arena->entities.players.cend();) {
                    entity->second->collision_response(arena);
                    ++entity;
                }
#ifdef THREADING
            }, this);
#endif

#ifdef THREADING
            thread bullet_collide([](Arena* arena) {
#endif
                for (auto entity = arena->entities.bullets.cbegin(); entity != arena->entities.bullets.cend();) {
                    entity->second->collision_response(arena);
                    ++entity;
                }
#ifdef THREADING
            }, this);
#endif

#ifdef THREADING
            shape_collide.join();
            player_collide.join();
            bullet_collide.join();
#endif
        }

        void run(ws28::Server& server, unsigned short port) __attribute__((cold)) {
            for (unsigned int i = 0; i<target_shape_count; i++) {
                Shape *new_shape = new Shape;
                new_shape->id = get_uid();
                new_shape->position = Vector2(rand() % ARENA_SIZE + 0, rand() % ARENA_SIZE + 0);
                entities.shapes[new_shape->id] = new_shape;
            }

            uv_timer_t* timer = new uv_timer_t();
            uv_timer_init(uv_default_loop(), timer);
            timer->data = this;
            uv_timer_start(timer, [](uv_timer_t *timer) {
                auto &arena = *(Arena*)(timer->data);
                arena.update();
                //uv_update_time(uv_default_loop());
            }, 0, 1000/30);
        
            assert(server.Listen(port));
            
            SUCCESS("Server listening on port " << port);
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

    arena->entities.bullets[new_bullet->id] = new_bullet;
    reload = full_reload;
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

    qt::Rect viewport = {
        .x = this->position.x - 3250/2, 
        .y = this->position.y - 2500/2, 
        .width = 3250, 
        .height = 2500, 
        .id = 0,
        .radius = 3250/2
    };

    canidates = arena->tree.retrieve(viewport);

    StreamPeerBuffer buf(true);
    unsigned short census_size = 0;

    for (const auto& canidate : canidates) {
        if (aabb(viewport, canidate)) {
            if (arena->entities.players.find(canidate.id) != arena->entities.players.end()) {
                arena->entities.players[canidate.id]->take_census(buf);
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
    buf.put_u8(2);
    buf.put_u16(census_size);
    this->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2);
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
    for (const auto& barrel : this->barrels) {
        if (barrel->reload != 0) {
            barrel->reload--;
        }
    }

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

    if (this->input.mousedown) {
        for (auto const& barrel : this->barrels) {
            if (barrel->reload == 0) {
                barrel->fire(this, arena);
            }
        }
        // if (reload == 0) {
        //     Bullet *new_bullet = new Bullet;
        //     new_bullet->position = this->position + (Vector2(cos(this->rotation), sin(this->rotation)).normalize() * Vector2(this->radius + new_bullet->radius + 1, this->radius + new_bullet->radius + 1));
        //     // new_bullet->position = this->input.mousepos;
        //     new_bullet->velocity = Vector2(cos(this->rotation) * bullet_speed, sin(this->rotation) * bullet_speed);
        //     this->velocity -= Vector2(cos(this->rotation) * recoil, sin(this->rotation) * recoil);
        //     new_bullet->id = get_uid();
        //     new_bullet->owner = id;
        //     arena->entities.bullets[new_bullet->id] = new_bullet;
        //     reload = full_reload;
        // }
    }

    this->velocity *= Vector2(this->friction, this->friction);
    this->position += this->velocity / Vector2(this->mass, this->mass);

    if (this->position.x > ARENA_SIZE) {
        this->position.x = ARENA_SIZE;
        this->velocity.x = 0;
    } else if (this->position.x < 0) {
        this->position.x = 0;
        this->velocity.x = 0;
    }
    if (this->position.y > ARENA_SIZE) {
        this->position.y = ARENA_SIZE;
        this->velocity.y = 0;
    } else if (this->position.y < 0) {
        this->position.y = 0;
        this->velocity.y = 0;
    }
}
///////////
