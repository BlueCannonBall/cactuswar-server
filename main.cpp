#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include <vector>
#include <map>
#include <typeinfo>

#define UNUSED(expr) do { (void)(expr); } while (0)

using namespace std;
using namespace spb;

unsigned int uuid = 0;

unsigned int get_uuid() {
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
        float angular_velocity = 0;
        float rotation = 0;
        float friction = 0.9f;
        float& x = position.x;
        float& y = position.y;
        string name = "Entity";
        //virtual ~Entity() {}
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
        float movement_speed = 1;
        float friction = 0.3f;
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

            unsigned int player_id = uuid++;
            client->SetUserData(reinterpret_cast<void*>(player_id));
            this->entities.players[player_id] = new_player;

            cout << "======> [INFO] New player with name \"" << player_name << "\" and id " << player_id << " joined" << endl;
            
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
            for (const auto &entity : entities.entities) {
                if (entity.second == nullptr) {
                    entities.entities.erase(entity.first);
                    continue;
                }
                
                entity.second->velocity *= Vector2(entity.second->friction, entity.second->friction);
                entity.second->position += entity.second->velocity;
            }
            
            for (const auto &player : entities.players) {
                if (player.second == nullptr) {
                    entities.players.erase(player.first);
                    continue;
                }
                if (player.second->client == nullptr) {
                    entities.players.erase(player.first);
                    continue;
                }

                if (player.second->keys.W) {
                    player.second->velocity.y -= player.second->movement_speed;
                } else if (player.second->keys.S) {
                    player.second->velocity.y += player.second->movement_speed;
                }
                    
                if (player.second->keys.A) {
                    player.second->velocity.x -= player.second->movement_speed;
                } else if (player.second->keys.D) {
                    player.second->velocity.x += player.second->movement_speed;
                }

                player.second->velocity *= Vector2(player.second->friction, player.second->friction);
                player.second->position += player.second->velocity;

                StreamPeerBuffer buf(true);
                buf.put_u8(2);
                buf.put_u16(entities.entities.size());
                for (const auto &entity : entities.entities) {
                    if (entity.second == nullptr) {
                        entities.entities.erase(entity.first);
                        continue;
                    }
                    buf.put_u8(0); // id
                    buf.put_u32(entity.first); // game id
                    buf.put_16(entity.second->position.x); // position
                    buf.put_16(entity.second->position.y);
                    buf.put_float(entity.second->rotation); // rotation
                }
                for (const auto &player : entities.players) {
                    if (player.second == nullptr) {
                        entities.players.erase(player.first);
                        continue;
                    }
                    buf.put_u8(0); // id
                    buf.put_u32(player.first); // game id
                    buf.put_16(player.second->position.x); // position
                    buf.put_16(player.second->position.y);
                    buf.put_float(player.second->rotation); // rotation
                }
                player.second->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2); // send census!
            }
        }

        void run(int port) {
            uv_timer_t* timer = new uv_timer_t();
            uv_timer_init(uv_default_loop(), timer);
            timer->data = this;
            uv_timer_start(timer, [](uv_timer_t *timer) {
                auto &arena = *(Arena*)(timer->data);
                arena.update();
            }, 10, 1000/30);
        
            assert(server.Listen(port));
                    
            cout << "======> [INFO] Listening on port " << port << endl;
        }
};

int main(int argc, char **argv) {
    int port;
    if (argc >= 2) {
        port = atoi(argv[1]);
    } else {
        cerr << "======> [ERR] Please supply a port number" << endl;
        return 1;
    }
    
    static Arena arena(64000);
    
    arena.server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest &) {
        UNUSED(client);
        puts("======> [INFO] Client connected");
    });
    
    arena.server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
        UNUSED(opcode);
        StreamPeerBuffer buf(true);
        buf.data_array = vector<unsigned char>(data, data+len);
        unsigned char packet_id = buf.get_u8();
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        cout << "======> [INFO] Got packet with id " << +packet_id << endl;
        switch (packet_id) {
            case 0:
                if (len < 2) {
                    client->Destroy();
                    arena.entities.players.erase(player_id);
                    return;
                }
                arena.handle_init_packet(buf, client);
                break;
            case 1:
                arena.handle_input_packet(buf, client);
                break;
        }
    });
    
    arena.server.SetClientDisconnectedCallback([](ws28::Client *client){
		cout << "======> [INFO] Client disconnected" << endl;
		unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
		arena.entities.players.erase(player_id);
	});
    
    arena.run(port);
    
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    assert(uv_loop_close(uv_default_loop()) == 0);
    
    return 0;
}
