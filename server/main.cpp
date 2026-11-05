#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include <sys/socket.h> // socket, bind, listen, accept, send, recv
#include <unistd.h>     // close
#include <poll.h>       // poll

#include "network.h"
#include "message.h"

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

// Allowed values of the start arguments based on protocol.md
const int MIN_PORT = 1;
const int MAX_PORT = 65535;
const int MIN_ROOMS = 1;
const int MAX_ROOMS = 20;
const int MIN_PLAYER_LIMIT = 2;
const int MAX_PLAYER_LIMIT = 100;

// Checks that a start argument lies between min and max (inclusive);
// prints what is wrong and returns false if it does not
static bool check_range(const char *name, int value, int min, int max) {
    if (value < min || value > max) {
        std::cerr << "The " << name << " must be between " << min << " and " << max << "\n";
        return false;
    }
    return true;
}

// Reacts to one complete line from the client.
// Returns false when the connection should be closed.
bool handle_line(int skt, [[maybe_unused]] Client &client, const std::string &line) {
    // No commands exist yet, so every line is an unknown command
    std::cerr << "Unknown command from socket " << skt << ": [" << line << "]\n";
    send_line(skt, "ERROR");
    return false;
}

int main(int argc, char *argv[]) {
    std::string usage_msg = "Usage: ./server <address> <port> <rooms> <max_players>\n"
                            "Example: ./server 127.0.0.1 10000 5 20\n";
    // sanity check
    if (argc != 5) {
        std::cerr << "Running server requires IP address, port, room count and max players specified in args\n";
        std::cerr << usage_msg;
        exit(EXIT_FAILURE);
    }
    for (int i = 2; i < argc; i++) {
        bool num_check = is_number(argv[i]);
        if (!num_check) {
            std::cerr << "One or more invalid argument formats\n";
            std::cerr << usage_msg;
            exit(EXIT_FAILURE);
        }
    }
    
    // init a config; is_number above guarantees atoi gets only digits
    int port = atoi(argv[2]);
    int room_count = atoi(argv[3]);
    int max_players = atoi(argv[4]);
    if (!check_range("port", port, MIN_PORT, MAX_PORT) ||
        !check_range("room count", room_count, MIN_ROOMS, MAX_ROOMS) ||
        !check_range("player limit", max_players, MIN_PLAYER_LIMIT, MAX_PLAYER_LIMIT)) {
        std::cerr << usage_msg;
        exit(EXIT_FAILURE);
    }
    Config config = Config{argv[1], port, room_count, max_players};

    int skt = create_listen_socket(config.address, config.port);

    if (skt == -1) {
        std::cerr << usage_msg;
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s:%d\n", config.address.c_str(), config.port);

    std::vector<pollfd> fds;
    std::unordered_map<int, Client> clients;

    // at the start there is only the server socket, which listens for incoming connections
    fds.push_back({skt, POLLIN, 0});

    while (1) {
        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == skt) {
                    // accept a connection from a client
                    int client_skt = accept(skt, NULL, NULL);
                    if (client_skt < 0) {
                        perror("accept");
                        std::cerr << "Accepting client failed\n";
                        continue;
                    }

                    printf("Client connected (socket %d)\n", client_skt);
                    clients[client_skt] = Client{};
                    fds.push_back({client_skt, POLLIN, 0});
                } else {
                    // receive data from the client
                    int client_skt = fds[i].fd;
                    std::vector<std::string> lines;
                    bool keep_client = read_lines(client_skt, clients[client_skt].inbuf, lines);

                    for (const std::string &line : lines) {
                        printf("Received from socket %d: [%s]\n", client_skt, line.c_str());
                        if (!handle_line(client_skt, clients[client_skt], line)) {
                            keep_client = false;
                            break;
                        }
                    }

                    if (!keep_client) {
                        // the client closed the connection or an error occurred (outage, ...)
                        printf("Client disconnected (socket %d)\n", client_skt);
                        close(client_skt);

                        clients.erase(client_skt);
                        // remove the socket; erase shifts all the following ones one position back
                        fds.erase(fds.begin() + i);
                        i--; // step the index back so the next iteration checks the socket that moved to position i
                    }
                }
            }
        }
    }
}
