#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "message.h"

enum class State {
    LOGIN,
    LOBBY, 
    WAITING,
    IN_GAME
};

// Everything the server remembers about one connected client
struct Client {
    int fd = -1;          // the client's socket
    std::string inbuf;    // received bytes that do not form a complete line yet
    bool closing = false; // marked for disconnecting; remove_closing_clients closes it after the current pass
    int strikes = 0; // 3 strikes and client gets disconnected
    State state = State::LOGIN;
    std::string nick;
    int room = -1;
};

struct Room {
    std::string white, black;
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
        std::vector<Room> rooms; // can never be resized

        // Accept a connecting client
        bool accept_client();

        // Safely reads from client and sends it to the handlers
        void read_from_client(Client &client);

        // Close and erase all clients with the closing tag
        void remove_closing_clients();

        // Sends one line to the client; if that fails, marks the client closing
        void send_to(Client &client, const std::string &text);
        
        // Logs state change in a client
        void set_state(Client &client, State state);

        // Checks whether the server is currently full
        bool is_server_full();

        // Check if a player with a specified nick already exists
        Client* find_client(const std::string &nick);

        // Builds the response for LIST request
        std::string rooms_message();

        // ALL HANDLERS

        // Reacts to one complete line from the client.
        // To disconnect the client, it marks it closing.
        void handle_line(Client &client, const std::string &line);

        // Handlers of the single requests (handlers.cpp)
        void handle_ping(Client &client, const Message &msg);
        void handle_login(Client &client, const Message &msg);
        void handle_list(Client &client, const Message &msg);
        void handle_logout(Client &client, const Message &msg);

        // Sends "ERR <code>"; with strike, counts it and marks the client closing at the 3rd one
        void send_error(Client &client, const std::string &code, bool strike);

    public: 
        // Constructor
        Server(const Config &cfg) : config(cfg), rooms(cfg.room_count) {};

        // Create the listening socket
        bool start();

        // Loop with poll
        bool run();
};