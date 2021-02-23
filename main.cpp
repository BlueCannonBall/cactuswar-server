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

#define UNUSED(expr) do { (void)(expr); } while (0)
#define COLLISION_STRENGTH 5
#pragma clang diagnostic ignored "-Woverloaded-virtual"

using namespace std;
using namespace spb;

unsigned int uuid = 0;

unsigned int get_uuid() {
    if (uuid >= 4294965295) {
        WARN("IDs are close to the 32-bit unsigned number limit");
    }
    return uuid++;
}

bool aabb(qt::Rect rect1, qt::Rect rect2) {
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
        const float friction = 0.9f;
        float& x = position.x;
        float& y = position.y;
        string name = "Entity";
        const unsigned radius = 50;

        virtual void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity;

            if (this->x > 12000) {
                this->x = 12000;
            } else if (this->x < 0) {
                this->x = 0;
            }
            if (this->y > 12000) {
                this->y = 12000;
            } else if (this->y < 0) {
                this->y = 0;
            }
        }

        virtual ~Entity() {
            //INFO("Entity destroyed.");
        }
};

class Shape: public Entity {
    public:
        const unsigned radius = 100;
        //const float friction = ;

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(1); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_u8(7); // sides
        }

        void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity;

            if (this->x > 12000) {
                this->x = 12000;
            } else if (this->x < 0) {
                this->x = 0;
            }
            if (this->y > 12000) {
                this->y = 12000;
            } else if (this->y < 0) {
                this->y = 0;
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
        const float movement_speed = 4;
        const float bullet_speed = 10;
        const float friction = 0.8f;

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

class Arena {
    public:
        struct Entities {
            map<unsigned int, Entity*> entities;
            map<unsigned int, Tank*> players;
            map<unsigned int, Shape*> shapes;
        };

        Entities entities;
        qt::Quadtree tree = qt::Quadtree(qt::Rect {.x = 0, .y = 0, .width = 12000, .height = 12000}, 10, 4, 0);
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
            thread t1([](Arena* arena) {
                for (const auto &entity : arena->entities.shapes) {
                    if (entity.second == nullptr) {
                        delete entity.second;
                        arena->entities.shapes.erase(entity.first);
                        continue;
                    }
                    
                    entity.second->next_tick();
                    //entity.second->take_census(buf);
                    arena->qtmtx.lock();
                    arena->tree.insert(qt::Rect {
                        .x = entity.second->x - entity.second->radius, 
                        .y = entity.second->y - entity.second->radius, 
                        .width = static_cast<double>(entity.second->radius*2), 
                        .height = static_cast<double>(entity.second->radius*2), 
                        .id = entity.second->id, 
                        .radius = entity.second->radius
                    });
                    arena->qtmtx.unlock();
                }
            }, this);
            
            thread t2([](Arena* arena) {
                for (const auto &entity : arena->entities.players) {
                    if (entity.second == nullptr) {
                        delete entity.second;
                        arena->entities.players.erase(entity.first);
                        continue;
                    }
                    
                    entity.second->next_tick(arena);
                    //entity.second->take_census(buf);
                    arena->qtmtx.lock();
                    arena->tree.insert(qt::Rect {
                        .x = entity.second->x - entity.second->radius, 
                        .y = entity.second->y - entity.second->radius, 
                        .width = static_cast<double>(entity.second->radius*2), 
                        .height = static_cast<double>(entity.second->radius*2), 
                        .id = entity.second->id, 
                        .radius = entity.second->radius
                    });
                    arena->qtmtx.unlock();
                }
            }, this);

            t1.join();
            t2.join();

            thread t3([](Arena* arena) {
                for (const auto &entity : arena->entities.shapes) {
                    entity.second->collision_response(arena);
            }}, this);

            thread t4([](Arena* arena) {
                for (const auto &entity : arena->entities.players) {
                    entity.second->collision_response(arena);
            }}, this);

            t3.join();
            t4.join();
        }

        void run(ws28::Server& server, unsigned short port) {
            for (int i = 0; i<125; i++) {
                Shape *new_shape = new Shape;
                new_shape->id = get_uuid();
                new_shape->position = Vector2(rand() % 12000 + 0, rand() % 12000 + 0);
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
        }
        
        if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(this->x, this->y), this->radius)) {
            // response
            float angle = atan2((canidate.y + canidate.radius) - this->y, (canidate.x + canidate.radius) - this->x);
            Vector2 push_vec(cos(angle), sin(angle)); // heading vector
            this->velocity.x += -push_vec.x * COLLISION_STRENGTH;
            this->velocity.y += -push_vec.y * COLLISION_STRENGTH;
        }
    }

    // if (this->client == nullptr) {
    //     delete entity.second;
    //     entities.players.erase(entity.first);
    //     return;
    // }

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
            }
        }
    }

    buf.offset = 0;
    buf.put_u8(2);
    buf.put_u16(census_size);
    this->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2);
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

    if (this->input.mousedown) {
        Shape *new_shape = new Shape;
        //new_shape->position = this->position + Vector2(cos(this->rotation) * bullet_speed * 20, sin(this->rotation) * bullet_speed * 20);
        new_shape->position = this->input.mousepos;
        //new_shape->velocity = Vector2(cos(this->rotation) * bullet_speed, sin(this->rotation) * bullet_speed);
        new_shape->id = get_uuid();
        arena->entities.shapes[new_shape->id] = new_shape;
    }

    this->velocity *= Vector2(this->friction, this->friction);
    this->position += this->velocity;

    if (this->x > 12030) {
        this->x = 12030;
    } else if (this->x < -30) {
        this->x = -30;
    }
    if (this->y > 12030) {
        this->y = 12030;
    } else if (this->y < -30) {
        this->y = -30;
    }
}

int main(int argc, char **argv) {
    srand(69420);
    unsigned short port;
    if (argc >= 2) {
        port = atoi(argv[1]);
    } else {
        ERR("Please supply a port number");
        return 1;
    }
    
    static Arena arena;
    ws28::Server server{uv_default_loop(), nullptr};
    server.SetMaxMessageSize(12000);
    
    server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest &) {
        INFO("Client with ip " << client->GetIP() << " connected");
    });
    
    server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        if (opcode != 0x2) {
            client->Destroy();
            delete arena.entities.players[player_id];
            arena.entities.players.erase(player_id);
            return;
        }
        StreamPeerBuffer buf(true);
        buf.data_array = vector<unsigned char>(data, data+len);
        unsigned char packet_id = buf.get_u8();
        //INFO("Got packet with id " << +packet_id << " from player with id " << player_id);
        switch (packet_id) {
            case 0:
                if (len < 3) {
                    client->Destroy();
                    delete arena.entities.players[player_id];
                    arena.entities.players.erase(player_id);
                    return;
                }
                arena.handle_init_packet(buf, client);
                break;
            case 1:
                if (len != 6) {
                    client->Destroy();
                    delete arena.entities.players[player_id];
                    arena.entities.players.erase(player_id);
                    return;
                }
                arena.handle_input_packet(buf, client);
                break;
        }
    });
    
    server.SetClientDisconnectedCallback([](ws28::Client *client){
		INFO("Client disconnected");
		unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        delete arena.entities.players[player_id];
		arena.entities.players.erase(player_id);
	});

    server.SetHTTPCallback([](ws28::HTTPRequest &req, ws28::HTTPResponse &res) {
        res.send("Please connect with an real client.");
        INFO("Got an http request...");
    });
    
    arena.run(server, port);
    
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    assert(uv_loop_close(uv_default_loop()) == 0);
    
    return 0;
}
