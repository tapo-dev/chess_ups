#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "message.h"
#include "server.h"

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
    Server server(config);

    bool listening_socket = server.start();
    if (!listening_socket) {
        exit(EXIT_FAILURE);
    }
    printf("Listening on %s:%d\n", config.address.c_str(), config.port);

    return server.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
