#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <poll.h>

// Everything the server remembers about one connected client
struct Client {
    int fd = -1;          // the client's socket
    std::string inbuf;    // received bytes that do not form a complete line yet
    bool closing = false; // marked for disconnecting; remove_closing_clients closes it after the current pass
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
        std::unordered_map<int, Client> clients;

        bool accept_client();
        void read_from_client(Client &client);
        void remove_closing_clients();

        // Reacts to one complete line from the client.
        // To disconnect the client, it marks it closing.
        void handle_line(Client &client, const std::string &line);

        // Sends one line to the client; if that fails, marks the client closing
        void send_to(Client &client, const std::string &text);

    public: 
        // Constructor
        Server(const Config &cfg) : config(cfg) {};

        // Create the listening socket
        bool start();

        // Loop with poll
        bool run();
};