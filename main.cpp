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
        static constexpr float friction = 0.9f;
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

        virtual void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(0); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->rotation); // rotation
        }

        virtual ~Entity() {
            //INFO("Entity destroyed.");
        }
};

class Shape: public Entity {
    public:
        const unsigned radius = 100;

        void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(1); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_u8(7); // sides
        }
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
        static constexpr float movement_speed = 4;
        static constexpr float bullet_speed = 10;
        static constexpr float friction = 0.8f;

        void next_tick(Arena& arena);
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
        // map<unsigned int, Entity*> entities;
        // map<unsigned int, Tank*> entities.players;
        
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
            // this->tree.insert(qt::Rect {
            //     .x = new_player->x - new_player->radius, 
            //     .y = new_player->y - new_player->radius, 
            //     .width = static_cast<double>(new_player->radius*2), 
            //     .height = static_cast<double>(new_player->radius*2), 
            //     .id = new_player->id, 
            //     .radius = new_player->radius
            // });

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
            // StreamPeerBuffer buf(true);
            // buf.put_u8(2);
            // buf.put_u16(entities.entities.size() + entities.players.size() + entities.shapes.size());

            this->tree.clear();
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

            for (const auto &entity : entities.shapes) {
                if (entity.second == nullptr) {
                    delete entity.second;
                    entities.shapes.erase(entity.first);
                    continue;
                }
                
                entity.second->next_tick();
                //entity.second->take_census(buf);
                this->tree.insert(qt::Rect {
                    .x = entity.second->x - entity.second->radius, 
                    .y = entity.second->y - entity.second->radius, 
                    .width = static_cast<double>(entity.second->radius*2), 
                    .height = static_cast<double>(entity.second->radius*2), 
                    .id = entity.second->id, 
                    .radius = entity.second->radius
                });
            }
            
            for (const auto &entity : entities.players) {
                if (entity.second == nullptr) {
                    delete entity.second;
                    entities.players.erase(entity.first);
                    continue;
                }

                entity.second->next_tick(*this);
                //entity.second->take_census(buf);
                this->tree.insert(qt::Rect {
                    .x = entity.second->x - entity.second->radius, 
                    .y = entity.second->y - entity.second->radius, 
                    .width = static_cast<double>(entity.second->radius*2), 
                    .height = static_cast<double>(entity.second->radius*2), 
                    .id = entity.second->id, 
                    .radius = entity.second->radius
                });
            }

            for (const auto &entity : entities.shapes) {
                vector<qt::Rect> canidates = this->tree.retrieve(qt::Rect {
                    .x = entity.second->x - entity.second->radius,
                    .y = entity.second->y - entity.second->radius,
                    .width = static_cast<double>(entity.second->radius*2),
                    .height = static_cast<double>(entity.second->radius*2),
                    .id = entity.second->id,
                    .radius = entity.second->radius
                });
                for (const auto canidate : canidates) {
                    if (canidate.id == entity.second->id) {
                        continue;
                    }
                    
                    if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(entity.second->x, entity.second->y), entity.second->radius)) {
                        // response
                        float angle = atan2((canidate.y + canidate.radius) - entity.second->y, (canidate.x + canidate.radius) - entity.second->x);
                        Vector2 push_vec(cos(angle), sin(angle)); // heading vector
                        entity.second->velocity.x += -push_vec.x * COLLISION_STRENGTH;
                        entity.second->velocity.y += -push_vec.y * COLLISION_STRENGTH;
                    }
                }
            }

            for (const auto &entity : entities.players) {
                vector<qt::Rect> canidates = this->tree.retrieve(qt::Rect {
                    .x = entity.second->x - entity.second->radius, 
                    .y = entity.second->y - entity.second->radius, 
                    .width = static_cast<double>(entity.second->radius*2), 
                    .height = static_cast<double>(entity.second->radius*2), 
                    .id = entity.second->id, 
                    .radius = entity.second->radius
                });
                for (const auto canidate : canidates) {
                    if (canidate.id == entity.second->id) {
                        continue;
                    }
                    
                    if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(entity.second->x, entity.second->y), entity.second->radius)) {
                        // response
                        float angle = atan2((canidate.y + canidate.radius) - entity.second->y, (canidate.x + canidate.radius) - entity.second->x);
                        Vector2 push_vec(cos(angle), sin(angle)); // heading vector
                        entity.second->velocity.x += -push_vec.x * COLLISION_STRENGTH;
                        entity.second->velocity.y += -push_vec.y * COLLISION_STRENGTH;
                    }
                }

                if (entity.second->client == nullptr) {
                    delete entity.second;
                    entities.players.erase(entity.first);
                    continue;
                }
                canidates = this->tree.retrieve(qt::Rect {
                    .x = entity.second->x - 2000, 
                    .y = entity.second->y - 2000, 
                    .width = 4000, 
                    .height = 4000, 
                    .id = 0, 
                    .radius = 4000
                });
                StreamPeerBuffer buf(true);
                unsigned short census_size = 0;

                for (const auto canidate : canidates) {
                    if (circle_collision(Vector2(canidate.x + canidate.radius, canidate.y + canidate.radius), canidate.radius, Vector2(entity.second->x, entity.second->y), 2000)) {
                        if (entities.players.find(canidate.id) != entities.players.end()) {
                            entities.players[canidate.id]->take_census(buf);
                            census_size++;
                        } else if (entities.shapes.find(canidate.id) != entities.shapes.end()) {
                            entities.shapes[canidate.id]->take_census(buf);
                            census_size++;
                        }
                    }
                }

                buf.offset = 0;
                buf.put_u8(2);
                buf.put_u16(census_size);
                entity.second->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2);
            }

            // for (const auto &player : entities.players) {
            //     if (player.second == nullptr) {
            //         delete player.second;
            //         entities.players.erase(player.first);
            //         continue;
            //     }
            //     if (player.second->client == nullptr) {
            //         delete player.second;
            //         entities.players.erase(player.first);
            //         continue;
            //     }
                
            //     player.second->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2); // send census!
            //     //INFO("A packet has been mailed! The Dombattles Postal Service will be shipping it in 2-5 buisness minutes.");
            // }
        }

        void run(ws28::Server& server, unsigned short port) {
            for (int i = 0; i<100; i++) {
                Shape *new_shape = new Shape;
                new_shape->id = get_uuid();
                new_shape->position = Vector2(rand() % 12000 + 0, rand() % 12000 + 0);
                // this->tree.insert(qt::Rect {
                //     .x = new_shape->x - new_shape->radius, 
                //     .y = new_shape->y - new_shape->radius, 
                //     .width = static_cast<double>(new_shape->radius*2), 
                //     .height = static_cast<double>(new_shape->radius*2), 
                //     .id = new_shape->id, 
                //     .radius = new_shape->radius
                // });
                entities.shapes[new_shape->id] = new_shape;
            }

            uv_timer_t* timer = new uv_timer_t();
            uv_timer_init(uv_default_loop(), timer);
            timer->data = this;
            uv_timer_start(timer, [](uv_timer_t *timer) {
                auto &arena = *(Arena*)(timer->data);
                arena.update();
                uv_update_time(uv_default_loop());
            }, 0, 1000/30);
        
            assert(server.Listen(port));
            
            SUCCESS("Server listening on port " << port);
        }
};

void Tank::next_tick(Arena &arena) {
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
        new_shape->position = this->position + Vector2(cos(this->rotation) * bullet_speed * 20, sin(this->rotation) * bullet_speed * 20);
        new_shape->velocity = Vector2(cos(this->rotation) * bullet_speed, sin(this->rotation) * bullet_speed);
        new_shape->id = get_uuid();
        arena.entities.shapes[new_shape->id] = new_shape;
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
