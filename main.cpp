#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include <vector>
#include <map>
#include "bcblog.hpp"
#include <sstream>

#define UNUSED(expr) do { (void)(expr); } while (0)

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
        
        Vector2(float x, float y) {
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

/// Base entity
class Entity {
    public:
        Vector2 position = Vector2(0, 0);
        Vector2 velocity = Vector2(0, 0);
        unsigned int id;
        float rotation = 0;
        static constexpr float friction = 0.9f;
        float& x = position.x;
        float& y = position.y;
        string name = "Entity";

        virtual void next_tick() {
            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity;
        }

        virtual void take_census(StreamPeerBuffer& buf) {
            buf.put_u8(0); // id
            buf.put_u32(this->id); // game id
            buf.put_16(this->position.x); // position
            buf.put_16(this->position.y);
            buf.put_float(this->rotation); // rotation
        }

        virtual ~Entity() {}
};

/// A domtank, stats vary based on mockups.
class Tank: public Entity {
    public:
        struct Keys {
            bool W;
            bool A;
            bool S;
            bool D;
        };

        string name = "Unnamed";
        ws28::Client *client = nullptr;
        Keys keys = Keys {.W = false, .A = false, .S = false, .D = false};
        static constexpr float movement_speed = 4;
        static constexpr float friction = 0.8f;

        void next_tick() {
            if (this->keys.W) {
                this->velocity.y -= this->movement_speed;
            } else if (this->keys.S) {
                this->velocity.y += this->movement_speed;
            }
                
            if (this->keys.A) {
                this->velocity.x -= this->movement_speed;
            } else if (this->keys.D) {
                this->velocity.x += this->movement_speed;
            }

            this->velocity *= Vector2(this->friction, this->friction);
            this->position += this->velocity;
        }
};

class Arena {
    public:
        struct Entities {
            map<unsigned int, Entity*> entities;
            map<unsigned int, Tank*> players;
        };

    	ws28::Server server{uv_default_loop(), nullptr};
        Entities entities;
        // map<unsigned int, Entity*> entities;
        // map<unsigned int, Tank*> entities.players;

        Arena(int message_size) {
            server.SetMaxMessageSize(message_size);
        }
        
        void handle_init_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            string player_name = buf.get_utf8();
            Tank* new_player = new Tank;
            new_player->name = player_name;
            new_player->client = client;

            unsigned int player_id = get_uuid();
            client->SetUserData(reinterpret_cast<void*>(player_id));
            new_player->id = player_id;
            this->entities.players[player_id] = new_player;

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
            
            entities.players[player_id]->keys = Tank::Keys {.W = false, .A = false, .S = false, .D = false};
            
            if (0b1000 & movement_byte) {
                entities.players[player_id]->keys.W = true;
                //puts("w");
            } else if (0b0010 & movement_byte) {
                entities.players[player_id]->keys.S = true;
                //puts("s");
            }
            
            if (0b0100 & movement_byte) {
                entities.players[player_id]->keys.A = true;
                //puts("a");
            } else if (0b0001 & movement_byte) {
                entities.players[player_id]->keys.D = true;
                //puts("d");
            }
        }
        
        void update() {
            StreamPeerBuffer buf(true);
            buf.put_u8(2);
            buf.put_u16(entities.entities.size() + entities.players.size());
            for (const auto &entity : entities.entities) {
                if (entity.second == nullptr) {
                    delete entity.second;
                    entities.entities.erase(entity.first);
                    continue;
                }
                
                entity.second->next_tick();
                entity.second->take_census(buf);
            }
            
            for (const auto &player : entities.players) {
                if (player.second == nullptr) {
                    delete player.second;
                    entities.players.erase(player.first);
                    continue;
                }
                player.second->take_census(buf);
            }

            for (const auto &player : entities.players) {
                if (player.second == nullptr) {
                    delete player.second;
                    entities.players.erase(player.first);
                    continue;
                }
                if (player.second->client == nullptr) {
                    delete player.second;
                    entities.players.erase(player.first);
                    continue;
                }

                player.second->next_tick();
                
                player.second->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2); // send census!
                //INFO("A packet has been mailed! The Dombattles Postal Service will be shipping it in 2-5 buisness minutes.");
            }
        }

        void run(unsigned short port) {
            for (int i = 0; i<50; i++) {
                Entity *new_entity = new Entity;
                new_entity->id = get_uuid();
                new_entity->position = Vector2(rand() % 12000, rand() % 12000);
                entities.entities[new_entity->id] = new_entity;
            }

            uv_timer_t* timer = new uv_timer_t();
            uv_timer_init(uv_default_loop(), timer);
            timer->data = this;
            uv_timer_start(timer, [](uv_timer_t *timer) {
                auto &arena = *(Arena*)(timer->data);
                arena.update();
                uv_update_time(uv_default_loop());
            }, 0, 1000/60);
        
            assert(server.Listen(port));
            
            SUCCESS("Server listening on port " << port);
        }
};

int main(int argc, char **argv) {
    srand((unsigned) time(0));
    unsigned short port;
    if (argc >= 2) {
        port = atoi(argv[1]);
    } else {
        ERR("Please supply a port number");
        return 1;
    }
    
    static Arena arena(64000);
    
    arena.server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest &) {
        INFO("Client with ip " << client->GetIP() << " connected");
    });
    
    arena.server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
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
                if (len < 2) {
                    client->Destroy();
                    delete arena.entities.players[player_id];
                    arena.entities.players.erase(player_id);
                    return;
                }
                arena.handle_input_packet(buf, client);
                break;
        }
    });
    
    arena.server.SetClientDisconnectedCallback([](ws28::Client *client){
		INFO("Client disconnected");
		unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        delete arena.entities.players[player_id];
		arena.entities.players.erase(player_id);
	});

    arena.server.SetHTTPCallback([](ws28::HTTPRequest &req, ws28::HTTPResponse &res) {
        res.send("Please connect with an real client.");
        INFO("Got an http request...");
    });
    
    arena.run(port);
    
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    assert(uv_loop_close(uv_default_loop()) == 0);
    
    return 0;
}
