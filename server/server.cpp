#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

#include <sys/socket.h> // socket, bind, listen, accept, send, recv
#include <unistd.h>     // close
#include <poll.h>       // poll

#include "network.h"
#include "server.h"

bool Server::accept_client() {
    // accept a connection from a client
    int client_skt = accept(listen_fd, NULL, NULL);
    if (client_skt < 0) {
        perror("accept");
        std::cerr << "Accepting client failed\n";
        return false;
    }

    printf("Client connected (socket %d)\n", client_skt);
    clients[client_skt] = Client{};
    fds.push_back({client_skt, POLLIN, 0});
    return true;
}

bool Server::read_from_client(int client_skt) {
    std::vector<std::string> lines;
    bool keep_client = read_lines(client_skt, clients[client_skt].inbuf, lines);

    for (const std::string &line : lines) {
        printf("Received from socket %d: [%s]\n", client_skt, line.c_str());
        if (!handle_line(client_skt, clients[client_skt], line)) {
            keep_client = false;
            break;
        }
    }

    return keep_client;
}

bool Server::handle_line(int skt, [[maybe_unused]] Client &client, const std::string &line) {
    // No commands exist yet, so every line is an unknown command
    std::cerr << "Unknown command from socket " << skt << ": [" << line << "]\n";
    send_line(skt, "ERROR");
    return false;
}

bool Server::start() {
    listen_fd = create_listen_socket(config.address, config.port);
    if (listen_fd == -1) {
        return false;
    }
    return true;
}

bool Server::run() {
    // at the start there is only the server socket, which listens for incoming connections
    fds.push_back({listen_fd, POLLIN, 0});

    while (1) {
        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0) {
            perror("poll");
            return false;
        }

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == listen_fd) {
                    accept_client();
                } else {
                    int client_skt = fds[i].fd;
                    bool keep_client = read_from_client(client_skt);

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