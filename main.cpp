#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include "bcblog.hpp"
#include "core.hpp"

#define UNUSED(expr) do { (void)(expr); } while (0)

using namespace std;
using namespace spb;

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
