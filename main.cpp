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
        Vector2 velocity = Vector2(0, 0);
        ws28::Client *client = nullptr;
};

class Arena {
    public:
    	ws28::Server server{uv_default_loop()};
        vector<Entity*> entities;
        vector<Tank*> players;

        Arena(int message_size) {
            server.SetMaxMessageSize(message_size);
        }
        
        void handle_init_packet(StreamPeerBuffer& buf, ws28::Client *client) {
            string player_name = buf.get_utf8();
            Tank new_player;
            new_player.name = player_name;
            new_player.client = client;
            this->players.push_back(&new_player);
            this->entities.push_back(&new_player);
        }

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

int main(int argc, char **argv) {
    signal(SIGINT, [](int) {
        if (quit) {
            exit(1);
        } else {
            quit = true;
        }
    });

    static Arena arena(64000);
    
    arena.server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest &) {
        puts("======> [INFO] Client connected");
        client->SetUserData(reinterpret_cast<void*>(get_uuid()));
    });
    
    arena.server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
        StreamPeerBuffer buf(false);
        buf.data_array = vector<unsigned char>(data, data+len);
        unsigned char packet_id = buf.get_u8();
        switch (packet_id) {
            case 0:
                arena.handle_init_packet(buf, client);
                break;
        }
    });
    
    if (argc >= 2) {
        arena.listen(atoi(argv[1]));
    } else {
        cerr << "======> [ERR] Please supply a port number" << endl;
        return 1;
    }
    
    return 0;
}
