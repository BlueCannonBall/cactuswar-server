#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include "bcblog.hpp"
#include "core.hpp"
#include <map>

#define UNUSED(expr) do { (void)(expr); } while (0)
#define MESSAGE_SIZE 4096

using namespace std;
using namespace spb;

map<string, Arena*> arenas = {
    {"/ffa-1", new Arena},
    {"/ffa-2", new Arena}
};
map<ws28::Client*, string> paths; // HACK: store paths per client pointer

enum class Packet {
    Init = 0,
    Input = 1
};

void kick(ws28::Client* client, bool destroy=false) {
    if (client->GetUserData() != nullptr) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        Arena *arena = arenas[paths[client]];
        if (arena->entities.players.find(player_id) != arena->entities.players.end()) {
            delete arena->entities.players[player_id];
            arena->entities.players.erase(player_id);
        }
    }
    paths.erase(client);
    if (destroy)
        client->Destroy();
}

int main(int argc, char **argv) {
    srand(time(NULL));
    unsigned short port;
    if (argc >= 2) {
        port = atoi(argv[1]);
    } else {
        ERR("Please supply a port number");
        return 1;
    }
    
    ws28::Server server{uv_default_loop(), nullptr};
    server.SetMaxMessageSize(MESSAGE_SIZE);
    
    server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest& req) {
        INFO("Client with ip " << client->GetIP() << " connected");
        if (arenas.find(req.path) == arenas.end()) {
            client->Destroy();
            return;
        }
        paths[client] = req.path;
    });
    
    server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
        if (opcode != 0x2) {
            kick(client);
            WARN("Kicked misbehaving client");
            return;
        }

        StreamPeerBuffer buf(true);
        buf.data_array = vector<unsigned char>(data, data+len);
        
        Arena* arena = arenas[paths[client]];
        unsigned char packet_id = buf.get_u8();
        switch (packet_id) {
            case (int) Packet::Init:
                if (len < 3) {
                    kick(client);
                    WARN("Kicked misbehaving client");
                    return;
                }
                arena->handle_init_packet(buf, client);
                break;
            case (int) Packet::Input:
                if (len != 6) {
                    kick(client);
                    WARN("Kicked misbehaving client");
                    return;
                }
                arena->handle_input_packet(buf, client);
                break;
        }
    });
    
    server.SetClientDisconnectedCallback([](ws28::Client *client) {
		INFO("Client disconnected");
        kick(client, false);
        paths.erase(client);
	});

    server.SetHTTPCallback([](ws28::HTTPRequest& req, ws28::HTTPResponse& res) {
        res.send("Please connect with an real client.");
        INFO("Got an http request...");
    });

    server.SetCheckConnectionCallback([](ws28::Client *client, ws28::HTTPRequest&) {
        return true;
    });
    
    for (const auto& arena : arenas) {
        arena.second->run();
    }
    
    assert(server.Listen(port));
    SUCCESS("Server listening on port " << port);

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    assert(uv_loop_close(uv_default_loop()) == 0);
    
    return 0;
}
