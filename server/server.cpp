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
    // the new client gets into the poll list at the next pass of run()
    Client client;
    client.fd = client_skt;
    clients[client_skt] = client;
    return true;
}

void Server::read_from_client(Client &client) {
    std::vector<std::string> lines;
    // false = connection closed, broken, or a line too long
    if (!read_lines(client.fd, client.inbuf, lines)) {
        client.closing = true;
        return;
    }

    for (const std::string &line : lines) {
        printf("Received from socket %d: [%s]\n", client.fd, line.c_str());
        handle_line(client, line);
        // a client that is being disconnected gets no more answers
        if (client.closing) {
            return;
        }
    }
}

void Server::handle_line(Client &client, const std::string &line) {
    // No commands exist yet, so every line is an unknown command
    std::cerr << "Unknown command from socket " << client.fd << ": [" << line << "]\n";
    send_to(client, "ERROR");
    client.closing = true;
}

void Server::send_to(Client &client, const std::string &text) {
    // a client that is being disconnected gets nothing more
    if (client.closing) {
        return;
    }
    // send_line fails when the connection is broken or the client does not read
    // what we send; either way the client cannot be used anymore
    if (!send_line(client.fd, text)) {
        std::cerr << "Sending to socket " << client.fd << " failed\n";
        client.closing = true;
    }
}

// Close and erase all clients with the closing tag
void Server::remove_closing_clients() {
    for (auto it = clients.begin(); it != clients.end(); ) {
        if (it->second.closing) {
            std::cout << "Client disconnected (socket " << it->first << ")\n";
            close(it->first);
            it = clients.erase(it);
        }
        else {
            it++;
        }
    }
}

bool Server::start() {
    listen_fd = create_listen_socket(config.address, config.port);
    if (listen_fd == -1) {
        return false;
    }
    return true;
}

bool Server::run() {
    std::vector<pollfd> fds;

    while (1) {
        // Clear fds and then refill
        fds.clear();
        fds.push_back({listen_fd, POLLIN, 0}); // listening socket
        for (auto &[fd, client] : clients) {
            fds.push_back({fd, POLLIN, 0});
        }

        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0) {
            perror("poll");
            return false;
        }

        // fds is not changed in this loop, and clients are only marked, never erased,
        // so every client socket in fds is still in clients
        for (const pollfd &entry : fds) {
            // nothing happened on this socket
            if (entry.revents == 0) {
                continue;
            }

            if (entry.fd == listen_fd) {
                if (entry.revents & POLLIN) {
                    accept_client();
                } else {
                    // the server cannot work without its listening socket
                    std::cerr << "Listening socket failed\n";
                    return false;
                }
                continue;
            }

            Client &client = clients.at(entry.fd);
            if (client.closing) {
                continue;
            }
            
            if (entry.revents & POLLIN) {
                read_from_client(client);
            } else if (entry.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                client.closing = true;
            }
        }

        remove_closing_clients();
    }
}