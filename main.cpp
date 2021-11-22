#include "bcblog.hpp"
#include "core.hpp"
#include "json.hpp"
#include "streampeerbuffer.hpp"
#include "ws28/src/Server.h"
#include <cmath>
#include <csignal>
#include <fstream>
#include <iostream>
#include <leveldb/db.h>
#include <map>
#include <string>
#include <unordered_map>
#include <uv.h>

#define UNUSED(expr) \
    do { (void) (expr); } while (0)
#define MESSAGE_SIZE 1024

using namespace std;
using namespace spb;
using json = nlohmann::json;

map<string, Arena*> arenas = {
    {"/ffa-1", new Arena},
    {"/ffa-2", new Arena}};
json server_info;

void kick(ws28::Client* client, bool destroy = true) {
    auto client_info = (ClientInfo*) client->GetUserData();
    Arena* arena = arenas[client_info->path];

    if (client_info->authenticated) {
        const unsigned int player_id = client_info->id;
        if (in_map(arena->entities.tanks, player_id)) {
            arena->destroy_entity(player_id, arena->entities.tanks);
        }
    }

    if (destroy) {
        ban(client, destroy);
    }
}

inline void atexit_handler() {
    delete db;
}

inline void signal_handler(int signal_num) {
    exit(signal_num);
}

int main(int argc, char** argv) {
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

    uv_fs_event_t entityconfig_event_handle;
    uv_fs_event_init(uv_default_loop(), &entityconfig_event_handle);
    entityconfig_event_handle.data = &arenas;
    uv_fs_event_start(
        &entityconfig_event_handle, [](uv_fs_event_t* handle, const char* filename, int events, int status) {
            INFO("Hot reloading entityconfig.json");
            sync();
            tanksconfig.clear();
            assert(load_tanks_from_json(filename) == 0);
            auto arenas = (map<std::string, Arena*>*) handle->data;
            StreamPeerBuffer buf(true);
            for (const auto& arena : *arenas) {
                for (const auto& tank : arena.second->entities.tanks) {
                    if (tank.second->type == TankType::Remote) {
                        buf.reset();
                        arena.second->send_init_packet(buf, tank.second);
                        tank.second->define(tank.second->mockup);
                    }
                }
            }
        },
        "entityconfig.json", 0);

    atexit(atexit_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    ws28::Server server {uv_default_loop(), nullptr};
    server.SetMaxMessageSize(MESSAGE_SIZE);

    server.SetClientConnectedCallback([](ws28::Client* client, ws28::HTTPRequest& req) {
        INFO("Client with ip " << client->GetIP() << " connected to room \"" << req.path << "\"");
    });

    server.SetClientDataCallback([](ws28::Client* client, char* data, size_t len, int opcode) {
        if (opcode != 0x2 || len == 0) {
            kick(client);
            return;
        }

        StreamPeerBuffer buf(true);
        buf.data_array = vector<uint8_t>(data, data + len);

        Arena* arena = arenas[paths[client].path];
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

    server.SetClientDisconnectedCallback([](ws28::Client* client) {
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

    // check player
    server.SetCheckConnectionCallback([](ws28::Client* client, ws28::HTTPRequest& req) {
        // INFO("New client's HTTP headers:");
        // req.headers.ForEach([](const char* key, const char* value) {
        //     INFO("    " << key << ": " << value);
        // });

        if (!in_map(arenas, req.path)) {
            WARN("Player tried to connect to non-existent room \"" << req.path << "\"");
            return false;
        }

        string value;
        leveldb::Status s;
        string ip;
        if (req.headers.Get("x-forwarded-for")) {
            ip = req.headers.Get("x-forwarded-for");
            ip = ip.substr(0, ip.find(","));
        } else {
            ip = client->GetIP();
        }
        db->Get(leveldb::ReadOptions(), ip, &value);
        if (s.ok()) {
            if (value == "1") {
                BRUH("BANNED PLAYER TRIED TO CONNECT, REJECTING CONNECTION");
                return false;
            }
        } else if (!s.IsNotFound()) {
            ERR("Failed to check if player is banned: " << s.ToString());
        }

        paths[client] = ClientInfo {};
        paths[client].path = req.path;
        req.headers.ForEach([client](const char* key, const char* value) {
            paths[client].headers[key] = value;
        });
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
