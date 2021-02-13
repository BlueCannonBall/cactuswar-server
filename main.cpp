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
    	ws28::Server server{uv_default_loop()};
        vector<Entity*> entities;
        vector<Tank*> players;

        Arena(int message_size) {
            server.SetMaxMessageSize(message_size);
            server.SetClientDataCallback
        }

        static void on_connect(ws28::Client *client, ws28::HTTPRequest ) {
            puts("======> [INFO] Client connected");
            client->SetUserData(reinterpret_cast<void*>(get_uuid()));
        }

        static void on_data(ws28::Client *client, char *data, size_t len, int opcode) {
            StreamPeerBuffer buf(false);
            buf.data_array = vector<unsigned char>(data, data+len);
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


    
    return 0;
}
