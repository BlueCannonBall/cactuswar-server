#include "json.hpp"
#include <iostream>
#include <uv.h>
#include "ws28/src/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include "bcblog.hpp"
#include "core.hpp"
#include <map>
#include <unordered_map>
#include <fstream>
#include <leveldb/db.h>

#define UNUSED(expr) do { (void)(expr); } while (0)
#define MESSAGE_SIZE 1024

using namespace std;
using namespace spb;
using json = nlohmann::json;

map<string, Arena*> arenas = {
    {"/ffa-1", new Arena},
    {"/ffa-2", new Arena}
};
json server_info;
unordered_map<ws28::Client*, string> paths; // HACK: store paths per client pointer

void kick(ws28::Client* client, bool destroy=false) {
    Arena *arena = arenas[paths[client]];
    if (client->GetUserData() != nullptr) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        if (in_map(arena->entities.tanks, player_id)) {
            arena->destroy_entity(player_id, arena->entities.tanks);
        }
    }
    paths.erase(client);
    if (destroy) {
        client->Destroy();
        BRUH("Forcefully kicked client");

        // ban player
        leveldb::Status s = db->Put(leveldb::WriteOptions(), client->GetIP(), "1");
        if (!s.ok()) {
            ERR("Failed to ban player: " << s.ToString());
        }
    }
}

int main(int argc, char **argv) {
    srand(time(NULL));
    unsigned short port;
    if (argc >= 2) {
        port = atoi(argv[1]);
    } else {
        cout << "Usage: " << argv[0] << " <PORT>\n";
        ERR("Please supply a port number");
        return 1;
    }

    options.create_if_missing = true;
    leveldb::Status s = leveldb::DB::Open(options, "./bans", &db);
    if (!s.ok()) {
        ERR("Failed to open ban database: " << s.ToString());
    }
    assert(s.ok());
    assert(load_tanks_from_json("entityconfig.json") == 0);

    atexit([]() {
        delete db;
    });

    ws28::Server server{uv_default_loop(), nullptr};
    server.SetMaxMessageSize(MESSAGE_SIZE);
    
    // screw lambdas
    server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest& req) {
        INFO("Client with ip " << client->GetIP() << " connected to room \"" << req.path << "\"");
        if (!in_map(arenas, req.path)) {
            client->Destroy();
            return;
        }
        paths[client] = req.path;
    });
    
    server.SetClientDataCallback([](ws28::Client *client, char *data, size_t len, int opcode) {
        if (opcode != 0x2) {
            kick(client);
            return;
        }

        StreamPeerBuffer buf(true);
        buf.data_array = vector<uint8_t>(data, data+len);
        
        Arena* arena = arenas[paths[client]];
        unsigned char packet_id = buf.get_u8();
        switch (packet_id) {
            default:
                kick(client);
                break;

            case (int) Packet::InboundInit:
                if (len < 3) {
                    kick(client);
                    return;
                }
                arena->handle_init_packet(buf, client);
                break;

            case (int) Packet::Input:
                if (len != 6) {
                    kick(client);
                    return;
                }
                arena->handle_input_packet(buf, client);
                break;

            case (int) Packet::Chat:
                if (len < 3) {
                    kick(client);
                    return;
                }
                arena->handle_chat_packet(buf, client);
                break;

            case (int) Packet::Respawn:
                if (len > 1) {
                    kick(client);
                    return;
                }
                arena->handle_respawn_packet(buf, client);
                break;
        }
    });
    
    server.SetClientDisconnectedCallback([](ws28::Client *client) {
		INFO("Client disconnected");
        kick(client, false);
        paths.erase(client);
	});

    server.SetHTTPCallback([](ws28::HTTPRequest& req, ws28::HTTPResponse& res) {
        if (strcmp(req.path, "/serverinfo") == 0) {
            res.header("Access-Control-Allow-Origin", "*");
            res.header("Content-Type", "application/json");
            res.send(server_info.dump());
            return;
        } else {
            res.send("Please connect with an real client.");
            INFO("Got an http request");
        }
    });

    // check if player banned
    server.SetCheckConnectionCallback([](ws28::Client *client, ws28::HTTPRequest&) {
        std::string value;
        leveldb::Status s = db->Get(leveldb::ReadOptions(), client->GetIP(), &value);
        if (s.ok()) {
            if (value != "0") {
                BRUH("BANNED PLAYER TRIED TO CONNECT, REJECTING CONNECTION");
                return false;
            }
        } else if (!s.IsNotFound()) {
            ERR("Failed to check if player is banned: " << s.ToString());
        }
        return true;
    });
    
    for (const auto& arena : arenas) {
        arena.second->run();
        server_info.push_back(arena.first);
    }
    
    assert(server.Listen(port));
    SUCCESS("Server listening on port " << port);

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    assert(uv_loop_close(uv_default_loop()) == 0);
}
