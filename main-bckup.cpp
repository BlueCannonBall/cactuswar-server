#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.h"
#include <cmath>
#include <vector>
#include <map>

using namespace std;

static volatile sig_atomic_t quit = false;
unsigned int uuid = 0;

unsigned int get_uuid() {
    uuid++;
    return uuid - 1;
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
        
        friend ostream &operator<<(ostream &output, const Vector2 &v) {
            output << "(" << v.x << ", " << v.y << ")";
            return output;
        }
        
        double distance_to(const Vector2& v) {
            return sqrt(pow(v.x - this->x, 2) + pow(v.y - this->y, 2) * 1.0); 
        }
};

class Entity {
    public:
        Vector2 position = Vector2(0, 0);
        float& x = position.x;
        float& y = position.y;
        string name = "Entity";
};

class Tank: public Entity {
    public:
        string name = "Unnamed";
        ws28::Client *client = nullptr;
};

class Arena {
    public:
        vector<Entity*> entities;
        vector<Tank*> players;
        
};

class ArenaManager {
    public:
        ws28::Server server{uv_default_loop()};
        map<string, Arena> arenas = {
            {"ffa-1", Arena()}
        };
        
        ArenaManager(int message_size) {
            server.SetMaxMessageSize(message_size);
            
            server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest &) {
                puts("======> [INFO] Client connected");
                client->SetUserData(reinterpret_cast<void*>(get_uuid()));
            });
            
            server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
                StreamPeerBuffer buf(true);
                buf.data_array = vector<unsigned char>(data, data+len);
                unsigned char packet_id = buf.get_u8();
                switch (packet_id) {
                    case 0:
                        string player_name = buf.get_utf8();
                        string arena = buf.get_utf8();
                        Tank new_player;
                        new_player.name = player_name;
                        new_player.client = client;
                        arenas[arena].players.push_back(&new_player);
                        arenas[arena].entities.push_back(&new_player);
                        cout << "======> [INFO] Player named " << player_name << " joined";
                        break;
                }
            });
            puts("======> [INFO] Server created");
        }
        
        // static void on_connect(ws28::Client *client, ws28::HTTPRequest &) {
            // puts("======> [INFO] Client connected");
            // client->SetUserData(reinterpret_cast<void*>(get_uuid()));
        // }
        
        // void on_data(ws28::Client *client, char *data, size_t len, int opcode) {
            // StreamPeerBuffer buf(true);
            // buf.data_array = vector<unsigned char>(data, data+len);
            // unsigned char packet_id = buf.get_u8();
            // switch (packet_id) {
                // case 0:
                    // string player_name = buf.get_utf8();
                    // string arena = buf.get_utf8();
                    // Tank new_player();
                    // new_player.name = player_name;
                    // new_player.client = client;
                    // arenas[arena].players.push_back(new_player);
                    // arenas[arena].entities.push_back(new_player);
                    // cout << "======> [INFO] Player named " << player_name << " joined";
                    // break;
            // }
        // }
        
        void listen(int port) {
            uv_timer_t timer;
            uv_timer_init(uv_default_loop(), &timer);
            timer.data = &server;
            uv_timer_start(&timer, [](uv_timer_t *timer) {
                if (quit) {
                    puts("======> [INFO] Waiting for clients to disconnect, send another SIGINT to force quit");
                    auto &s = *(ws28::Server*)(timer->data);
                    s.StopListening();
                    uv_timer_stop(timer);
                    uv_close((uv_handle_t*) timer, nullptr);
                }
            }, 10, 10);

            assert(server.Listen(port));
            
            cout << "======> [INFO] Listening on port " << port << endl;
            uv_run(uv_default_loop(), UV_RUN_DEFAULT);
            assert(uv_loop_close(uv_default_loop()) == 0);
        }
};

int main() {
    signal(SIGINT, [](int) {
        if (quit) {
            exit(1);
        } else {
            quit = true;
        }
    });
    

    //cout << Vector2(0, 0).distance_to(Vector2(100, 60)) << endl;
    ArenaManager arenas(64000);
    arenas.listen(3000);
	
    return 0;
}
