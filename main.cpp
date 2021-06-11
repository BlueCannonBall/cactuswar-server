#include "json.hpp"
#include <iostream>
#include <uv.h>
#include "ws28/Server.h"
#include <string>
#include "streampeerbuffer.hpp"
#include <cmath>
#include "bcblog.hpp"
#include "core.hpp"
#include <map>
#include <unordered_map>
#include <fstream>

#define UNUSED(expr) do { (void)(expr); } while (0)
#define MESSAGE_SIZE 4096

using namespace std;
using namespace spb;
using json = nlohmann::json;

map<string, Arena*> arenas = {
    {"/ffa-1", new Arena},
    {"/ffa-2", new Arena},
    {"/ffa-3", new Arena}
};
json server_info;
unordered_map<ws28::Client*, string> paths; // HACK: store paths per client pointer

void kick(ws28::Client* client, bool destroy=false) {
    Arena *arena = arenas[paths[client]];
    if (client->GetUserData() != nullptr) {
        unsigned int player_id = (unsigned int) (uintptr_t) client->GetUserData();
        if (arena->entities.tanks.find(player_id) != arena->entities.tanks.end()) {
            delete arena->entities.tanks[player_id];
            arena->entities.tanks.erase(player_id);
            arena->update_size();
        }
    }
    paths.erase(client);
    if (destroy) {
        client->Destroy();
        WARN("Forcefully kicked client");

        // ban player
        fstream ip_file("ips.json");
        string data;
        ip_file.seekg(0, std::ios::end);   
        data.reserve(ip_file.tellg());
        ip_file.seekg(0, std::ios::beg);
        data.assign((std::istreambuf_iterator<char>(ip_file)), 
            std::istreambuf_iterator<char>());
        json ips = json::parse(data);
        if (ips.find(client->GetIP()) == ips.end()) {
            ips[client->GetIP()] = R"(
                {
                    "names": [],
                    "banned": true
                }
            )"_json;
        } else {
            ips[client->GetIP()]["banned"] = true;
        }
        data = ips.dump(4);
        ip_file.seekp(0);
        ip_file.write(data.c_str(), data.size());
        ip_file.close();
    }
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

    if (!file_exists("ips.json")) {
        ofstream ip_file("ips.json");
        ip_file << "{}";
        ip_file.close();
    }

    ws28::Server server{uv_default_loop(), nullptr};
    server.SetMaxMessageSize(MESSAGE_SIZE);
    
    // screw lambdas
    server.SetClientConnectedCallback([](ws28::Client *client, ws28::HTTPRequest& req) {
        INFO("Client with ip " << client->GetIP() << " connected to room \"" << req.path << "\"");
        if (arenas.find(req.path) == arenas.end()) {
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
        buf.data_array = vector<unsigned char>(data, data+len);
        
        Arena* arena = arenas[paths[client]];
        unsigned char packet_id = buf.get_u8();
        switch (packet_id) {
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
            default:
                kick(client);
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
        ifstream ip_file("ips.json");
        string data;
        ip_file.seekg(0, std::ios::end);   
        data.reserve(ip_file.tellg());
        ip_file.seekg(0, std::ios::beg);
        data.assign((std::istreambuf_iterator<char>(ip_file)), 
            std::istreambuf_iterator<char>());
        json ips = json::parse(data);
        ip_file.close();
        if (ips.find(client->GetIP()) != ips.end()) {
            if (ips[client->GetIP()]["banned"]) {
                WARN("BANNED IP TRIED TO CONNECT, REJECTING CONNECTION");
                return false;
            }
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
