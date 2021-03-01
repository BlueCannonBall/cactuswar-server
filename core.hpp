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

#pragma once
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#define COLLISION_STRENGTH 5
#define ARENA_SIZE 12000

using namespace std;
using namespace spb;

unsigned int uuid = 0;

unsigned int get_uuid() {
    if (uuid >= 4294965295) {
        WARN("IDs are close to the 32-bit unsigned number limit");
    }
    return uuid++;
}

inline bool aabb(qt::Rect rect1, qt::Rect rect2) {
    return (rect1.x < rect2.x + rect2.width &&
    rect1.x + rect1.width > rect2.x &&
    rect1.y < rect2.y + rect2.height &&
    rect1.y + rect1.height > rect2.y);
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
        
        double distance_to(const Vector2& v) {
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
            // if (this->x > this->y) {
            //     return Vector2(1, this->y/this->x);
            // } else if (this->y > this->x) {
            //     return Vector2(this->x/this->y, 1);
            // } else {
            //     return Vector2(1, 1);
            // }
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
        double rotation = 0;
        static constexpr float friction = 0.9f;
        float& x = position.x;
        float& y = position.y;
        string name = "Entity";
        unsigned radius = 50;
        float health = 500;
        float damage = 0;

        virtual void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity;

            if (this->x > ARENA_SIZE) {
                this->x = ARENA_SIZE;
                this->velocity.x = 0;
            } else if (this->x < 0) {
                this->x = 0;
                this->velocity.x = 0;
            }
            if (this->y > ARENA_SIZE) {
                this->y = ARENA_SIZE;
                this->velocity.y = 0;
            } else if (this->y < 0) {
                this->y = 0;
                this->velocity.y = 0;
            }
        }

        virtual void take_census(StreamPeerBuffer& buf) {}
        virtual void collision_response(Arena *arena);

        virtual ~Entity() {
            //INFO("Entity destroyed.");
        }
};

class Shape: public Entity {
    public:
        unsigned radius = 100;

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(1); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            //buf.put_u8(7); // sides
        }

        void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity;

            if (this->x > ARENA_SIZE) {
                this->x = ARENA_SIZE;
                this->velocity.x = 0;
            } else if (this->x < 0) {
                this->x = 0;
                this->velocity.x = 0;
            }
            if (this->y > ARENA_SIZE) {
                this->y = ARENA_SIZE;
                this->velocity.y = 0;
            } else if (this->y < 0) {
                this->y = 0;
                this->velocity.y = 0;
            }
        }

        void collision_response(Arena *arena);
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
        float bullet_speed = 50;
        static constexpr float friction = 0.8f;
        unsigned full_reload = 6;
        unsigned reload = full_reload;
        float recoil = 3.5;

        void next_tick(Arena* arena);
        void collision_response(Arena *arena);

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(0); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->rotation); // rotation
        }
};

class Bullet: public Entity {
    public:
        unsigned int radius = 25;
        static constexpr float friction = 1;
        short lifetime = 100;
        float damage = 20;
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
            this->position += this->velocity;

            if (this->x > ARENA_SIZE) {
                this->x = ARENA_SIZE;
                this->velocity.x = 0;
            } else if (this->x < 0) {
                this->x = 0;
                this->velocity.x = 0;
            }
            if (this->y > ARENA_SIZE) {
                this->y = ARENA_SIZE;
                this->velocity.y = 0;
            } else if (this->y < 0) {
                this->y = 0;
                this->velocity.y = 0;
            }
        }

        void collision_response(Arena *arena);
};

class Arena {
    public:
        struct Entities {
            map<unsigned int, Entity*> entities;
            map<unsigned int, Shape*> shapes;
            map<unsigned int, Tank*> players;
            map<unsigned int, Bullet*> bullets;
        };

        Entities entities;
        qt::Quadtree tree = qt::Quadtree(qt::Rect {.x = 0, .y = 0, .width = ARENA_SIZE, .height = ARENA_SIZE}, 10, 4);
        unsigned int target_shape_count = 125;
        mutex qtmtx;
        
        void handle_init_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            string player_name = buf.get_utf8();
            Tank* new_player = new Tank;
            new_player->name = player_name;
            new_player->client = client;

            unsigned int player_id = get_uuid();
            client->SetUserData(reinterpret_cast<void*>(player_id));
            new_player->id = player_id;
            this->entities.players[player_id] = new_player;
            new_player->position = Vector2(rand() % 9000 + 3000, rand() % 9000 + 3000);

            INFO("New player with name \"" << player_name << "\" and id " << player_id << " joined. There are currently " << entities.players.size() << " player(s) in game");
            
            buf.data_array = vector<unsigned char>();
            buf.offset = 0;
            buf.put_u8(3);
            buf.put_u32(player_id);
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
            entities.players[player_id]->input = Tank::Input {.W = false, .A = false, .S = false, .D = false, .mousedown = false, .mousepos = Vector2(0, 0)};
            
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
            entities.players[player_id]->rotation = atan2(entities.players[player_id]->input.mousepos.y - entities.players[player_id]->position.y, entities.players[player_id]->input.mousepos.x - entities.players[player_id]->position.x);
            // INFO("Player rotation: " << entities.players[player_id]->rotation);
            // INFO("Player position: " << entities.players[player_id]->position);
            // INFO("Mouse position: " << entities.players[player_id]->input.mousepos);
        }
        
        void update() {
            // for (const auto &entity : entities.entities) {
            //     if (entity.second == nullptr) {
            //         delete entity.second;
            //         entities.entities.erase(entity.first);
            //         continue;
            //     }
                
            //     entity.second->next_tick();
            //     entity.second->take_census(buf);
            //     //this->tree.insert(qt::Rect {.x = entity.second->x - 50, .y = entity.second->y - 50, .width = 100, .height = 100, .id = entity.second->id});
            // }

            this->tree.clear();

            if (entities.shapes.size() <= target_shape_count - 12) {
                INFO("Replenishing shapes");
                for (unsigned int i = 0; i<(target_shape_count - entities.shapes.size()); i++) {
                    Shape *new_shape = new Shape;
                    new_shape->id = get_uuid();
                    new_shape->position = Vector2(rand() % ARENA_SIZE + 0, rand() % ARENA_SIZE + 0);
                    entities.shapes[new_shape->id] = new_shape;
                }
            }
            
            thread shape_move([](Arena* arena) {
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
                    arena->qtmtx.lock();
                    arena->tree.insert(qt::Rect {
                        .x = entity->second->x - entity->second->radius, 
                        .y = entity->second->y - entity->second->radius, 
                        .width = static_cast<double>(entity->second->radius*2), 
                        .height = static_cast<double>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
                    arena->qtmtx.unlock();
                    ++entity;
                }
            }, this);
            
            thread player_move([](Arena* arena) {
                for (auto entity = arena->entities.players.cbegin(); entity != arena->entities.players.cend();) {
                    if (entity->second == nullptr) {
                        Tank* entity_ptr = entity->second;
                        arena->entities.players.erase(entity++);
                        delete entity_ptr;
                        continue;
                    }
                    
                    entity->second->next_tick(arena);
                    arena->qtmtx.lock();
                    arena->tree.insert(qt::Rect {
                        .x = entity->second->x - entity->second->radius, 
                        .y = entity->second->y - entity->second->radius, 
                        .width = static_cast<double>(entity->second->radius*2), 
                        .height = static_cast<double>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
                    arena->qtmtx.unlock();
                    ++entity;
                }
            }, this);

            thread bullet_move([](Arena* arena) {
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
                    }
                    entity->second->next_tick();
                    arena->qtmtx.lock();
                    arena->tree.insert(qt::Rect {
                        .x = entity->second->x - entity->second->radius, 
                        .y = entity->second->y - entity->second->radius, 
                        .width = static_cast<double>(entity->second->radius*2), 
                        .height = static_cast<double>(entity->second->radius*2), 
                        .id = entity->second->id, 
                        .radius = entity->second->radius
                    });
                    arena->qtmtx.unlock();
                    ++entity;
                }
            }, this);

            shape_move.join();
            player_move.join();
            bullet_move.join();

            thread shape_collide([](Arena* arena) {
                for (auto entity = arena->entities.shapes.cbegin(); entity != arena->entities.shapes.cend();) {
                    entity->second->collision_response(arena);
                    ++entity;
            }}, this);

            thread player_collide([](Arena* arena) {
                for (auto entity = arena->entities.players.cbegin(); entity != arena->entities.players.cend();) {
                    entity->second->collision_response(arena);
                    ++entity;
            }}, this);

            thread bullet_collide([](Arena* arena) {
                for (auto entity = arena->entities.bullets.cbegin(); entity != arena->entities.bullets.cend();) {
                    entity->second->collision_response(arena);
                    ++entity;
            }}, this);

            shape_collide.join();
            player_collide.join();
            bullet_collide.join();
        }

        void run(ws28::Server& server, unsigned short port) {
            for (unsigned int i = 0; i<target_shape_count; i++) {
                Shape *new_shape = new Shape;
                new_shape->id = get_uuid();
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
void Entity::collision_response(Arena* arena) {
    arena->qtmtx.lock();
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->x - this->radius,
        .y = this->y - this->radius,
        .width = static_cast<double>(this->radius*2),
        .height = static_cast<double>(this->radius*2),
        .id = this->id,
        .radius = this->radius
    });
    arena->qtmtx.unlock();
    for (const auto canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->x, this->y), this->radius)) {
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->y, (canidate.x + canidate.radius) - this->x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Shape::collision_response(Arena* arena) {
    arena->qtmtx.lock();
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->x - this->radius,
        .y = this->y - this->radius,
        .width = static_cast<double>(this->radius*2),
        .height = static_cast<double>(this->radius*2),
        .id = this->id,
        .radius = this->radius
    });
    arena->qtmtx.unlock();
    for (const auto canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->x, this->y), this->radius)) {
            if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
                this->health -= arena->entities.bullets[canidate.id]->damage;
            }
            
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->y, (canidate.x + canidate.radius) - this->x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}

void Tank::collision_response(Arena *arena) {
    arena->qtmtx.lock();
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->x - this->radius, 
        .y = this->y - this->radius, 
        .width = static_cast<double>(this->radius*2), 
        .height = static_cast<double>(this->radius*2), 
        .id = this->id, 
        .radius = this->radius
    });
    arena->qtmtx.unlock();
    for (const auto canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        } else if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
            if (arena->entities.bullets[canidate.id]->owner == this->id) {
                continue;
            }
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->x, this->y), this->radius)) {
            if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
                if (arena->entities.bullets[canidate.id]->owner != this->id) {
                    this->health -= arena->entities.bullets[canidate.id]->damage;
                }
            }
            
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->y, (canidate.x + canidate.radius) - this->x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }

    qt::Rect viewport = qt::Rect {
        .x = this->x - 2000, 
        .y = this->y - 2000, 
        .width = 4000, 
        .height = 4000, 
        .id = 0, 
        .radius = 4000
    };
    arena->qtmtx.lock();
    canidates = arena->tree.retrieve(viewport);
    arena->qtmtx.unlock();
    StreamPeerBuffer buf(true);
    unsigned short census_size = 0;

    for (const auto canidate : canidates) {
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
    arena->qtmtx.lock();
    vector<qt::Rect> canidates = arena->tree.retrieve(qt::Rect {
        .x = this->x - this->radius,
        .y = this->y - this->radius,
        .width = static_cast<double>(this->radius*2),
        .height = static_cast<double>(this->radius*2),
        .id = this->id,
        .radius = this->radius
    });
    arena->qtmtx.unlock();
    for (const auto canidate : canidates) {
        if (canidate.id == this->id) {
            continue;
        } else if (canidate.id == this->owner) {
            continue;
        } else if (arena->entities.bullets.find(canidate.id) != arena->entities.bullets.end()) {
            if (arena->entities.bullets[canidate.id]->owner == this->owner) {
                continue;
            }
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->x, this->y), this->radius)) {
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->y, (canidate.x + canidate.radius) - this->x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }
}


void Tank::next_tick(Arena *arena) {
    if (reload != 0) {
        reload--;
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
        if (reload == 0) {
            Bullet *new_bullet = new Bullet;
            new_bullet->position = this->position + (Vector2(cos(this->rotation), sin(this->rotation)).normalize() * Vector2(this->radius + new_bullet->radius + 1, this->radius + new_bullet->radius + 1));
            // new_bullet->position = this->input.mousepos;
            new_bullet->velocity = Vector2(cos(this->rotation) * bullet_speed, sin(this->rotation) * bullet_speed);
            this->velocity -= Vector2(cos(this->rotation) * recoil, sin(this->rotation) * recoil);
            new_bullet->id = get_uuid();
            new_bullet->owner = id;
            arena->entities.bullets[new_bullet->id] = new_bullet;
            reload = full_reload;
        }
    }

    this->velocity *= Vector2(this->friction, this->friction);
    this->position += this->velocity;

    if (this->x > ARENA_SIZE) {
        this->x = ARENA_SIZE;
        this->velocity.x = 0;
    } else if (this->x < 0) {
        this->x = 0;
        this->velocity.x = 0;
    }
    if (this->y > ARENA_SIZE) {
        this->y = ARENA_SIZE;
        this->velocity.y = 0;
    } else if (this->y < 0) {
        this->y = 0;
        this->velocity.y = 0;
    }
}
///////////