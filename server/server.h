#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <poll.h>

// Everything the server remembers about one connected client
struct Client {
    std::string inbuf;  // received bytes that do not form a complete line yet
};

struct Config {
    std::string address;
    int port;
    int room_count;
    int max_players;
};

class Server {
    private: 
        Config config;
        int listen_fd = -1; // start sets the socket
        std::vector<pollfd> fds;
        std::unordered_map<int, Client> clients;

        bool accept_client();
        bool read_from_client(int client_skt);

        // Reacts to one complete line from the client.
        // Returns false when the connection should be closed.
        bool handle_line(int skt, Client &client, const std::string &line);

    public: 
        // Constructor
        Server(const Config &cfg) : config(cfg) {};

        // Create the listening socket
        bool start();

        // Loop with poll
        bool run();
};