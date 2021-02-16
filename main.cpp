#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.h"
#include <cmath>
#include <vector>
#include <map>
#include <typeinfo>

#define UNUSED(expr) do { (void)(expr); } while (0)

using namespace std;
using namespace spb;

static volatile sig_atomic_t quit = false;
unsigned int uuid = 0;

// unsigned int get_uuid() {
    // uuid++;
    // return uuid - 1;
// }

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

class Entity {
    public:
        Vector2 position = Vector2(0, 0);
        Vector2 velocity = Vector2(0, 0);
        float friction = 0.9f;
        float& x = position.x;
        float& y = position.y;
        string name = "Entity";
        virtual ~Entity() {}
};

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
        Keys keys = Keys {W: false, A: false, S: false, D: false};
        float movement_speed = 1;
        
};

class Arena {
    public:
    	ws28::Server server{uv_default_loop(), nullptr};
    	bool server_listening = true;
        map<unsigned int, Entity*> entities;
        map<unsigned int, Tank*> players;

        Arena(int message_size) {
            server.SetMaxMessageSize(message_size);
        }
        
        void handle_init_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            string player_name = buf.get_utf8();
            Tank new_player;
            new_player.name = player_name;
            new_player.client = client;
            client->SetUserData(reinterpret_cast<void*>(uuid++));
            this->players[(unsigned int) (uintptr_t) client->GetUserData()] = &new_player;
            this->entities[(unsigned int) (uintptr_t) client->GetUserData()] = &new_player;
            cout << "======> [INFO] New player with name \"" << player_name << "\" and id " << (unsigned int) (uintptr_t) client->GetUserData() << " joined" << endl;
        }
        
        void handle_input_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            unsigned char movement_byte = buf.get_u8();
            unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
            
            players[player_id]->keys = Tank::Keys {W: false, A: false, S: false, D: false};
            
            if (0b1000 & movement_byte) {
                players[player_id]->keys.W = true;
                puts("w");
            } else if (0b0010 & movement_byte) {
                players[player_id]->keys.S = true;
                puts("s");
            }
            
            if (0b0100 & movement_byte) {
                players[player_id]->keys.A = true;
                puts("a");
            } else if (0b0001 & movement_byte) {
                players[player_id]->keys.D = true;
                puts("d");
            }
        }
        
        void update() {
            for (const auto &entity : entities) {
                if (entity.second == nullptr) {
                    entities.erase(entity.first);
                    continue;
                }
                if (typeid(entity.second) == typeid(Tank*)) {
                    Tank *tank = nullptr;
                    if ((tank = dynamic_cast<Tank* const>(entity.second))) {
                        if (tank->keys.W) {
                            tank->velocity.y -= tank->movement_speed;
                        } else if (tank->keys.S) {
                            tank->velocity.y += tank->movement_speed;
                        }
                        
                        if (tank->keys.A) {
                            tank->velocity.x -= tank->movement_speed;
                        } else if (tank->keys.D) {
                            tank->velocity.x += tank->movement_speed;
                        }
                    }
                }
                
                entity.second->velocity *= Vector2(entity.second->friction, entity.second->friction);
                entity.second->position += entity.second->velocity;
            }
            
            for (const auto &player : players) {
                if (player.second == nullptr || player.second->client == nullptr) {
                    players.erase(player.first);
                    continue;
                }
                StreamPeerBuffer buf(true);
                buf.put_u8(2);
                buf.put_u16(entities.size());
                for (const auto &entity : entities) {
                    if (entity.second == nullptr) {
                        entities.erase(entity.first);
                        continue;
                    }
                    buf.put_u8(0);
                    buf.put_u8(entity.first);
                    buf.put_16(entity.second->x);
                    buf.put_16(entity.second->y);
                    buf.put_16(0);
                    player.second->client->Send(reinterpret_cast<char*>(buf.data_array.data()), buf.data_array.size(), 0x2);
                }
            }
        }
        
        // void mainloop() {
            // static uv_timer_t timer;
            // uv_timer_init(uv_default_loop(), &timer);
            // timer.data = this;
            // uv_timer_start(&timer, [](uv_timer_t *timer) {
                // auto &arena = *(Arena*)(timer->data);
                // if (!arena.server_listening) {
                    // puts("======> [INFO] Mainloop closing");
                    // uv_timer_stop(timer);
                    // uv_close((uv_handle_t*) timer, nullptr);
                // }
                // arena.update();
            // }, 10, 1000/30);
        // }

        void listen(int port) {
            static uv_timer_t timer;
            uv_timer_init(uv_default_loop(), &timer);
            timer.data = this;
            uv_timer_start(&timer, [](uv_timer_t *timer) {
                auto &arena = *(Arena*)(timer->data);
                arena.update();
                if (quit) {
                    puts("======> [INFO] Waiting for clients to disconnect, send another SIGINT to force quit");
                    auto &s = arena.server;
                    s.StopListening();
                    uv_timer_stop(timer);
                    uv_close((uv_handle_t*) timer, nullptr);
                    arena.server_listening = false;
                }
            }, 10, 1000/30);
        
            assert(server.Listen(port));
                    
            cout << "======> [INFO] Listening on port " << port << endl;
        }
};

int main(int argc, char **argv) {
    signal(SIGINT, [](int) {
        if (quit) {
            exit(1);
        } else {
            quit = true;
        }
    });

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
        cout << "======> [INFO] Got packet with id " << +packet_id << endl;
        switch (packet_id) {
            case 0:
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
		arena.players.erase(player_id);
		arena.entities.erase(player_id);
	});
    
    arena.listen(port);
    //arena.mainloop();
    
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    assert(uv_loop_close(uv_default_loop()) == 0);
    //arena.mainloop();
    
    return 0;
}
