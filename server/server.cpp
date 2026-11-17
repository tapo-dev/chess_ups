#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <algorithm>
#include <string>

#include <sys/socket.h> // socket, bind, listen, accept, send, recv
#include <unistd.h>     // close
#include <poll.h>       // poll

#include "network.h"
#include "server.h"
#include "message.h"

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
        std::string printable_line = printable(line);
        printf("Received from socket %d: [%s]\n", client.fd, printable_line.c_str());
        handle_line(client, line);
        // a client that is being disconnected gets no more answers
        if (client.closing) {
            return;
        }
    }
}

void Server::remove_closing_clients() {
    for (auto it = clients.begin(); it != clients.end(); ) {
        if (it->second.closing) {
            std::cout << "Client disconnected (socket " << it->first << ")\n";
            remove_from_room(it->second);
            close(it->first);
            it = clients.erase(it);
        }
        else {
            it++;
        }
    }
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

// Name of a state for the log
static const char *state_name(State state) {
    switch (state) {
        case State::LOGIN:   return "LOGIN";
        case State::LOBBY:   return "LOBBY";
        case State::WAITING: return "WAITING";
        case State::IN_GAME: return "IN_GAME";
    }
    return "?";
}

void Server::set_state(Client &client, State state) {
    std::cout << client.nick << " (fd " << client.fd << "): "
              << state_name(client.state) << " -> " << state_name(state) << "\n";
    client.state = state;
}

bool Server::is_server_full() {
    int logged_in = std::count_if(clients.begin(), clients.end(), [](const auto& entry) {
        return entry.second.state != State::LOGIN && !entry.second.closing;
    });
    return logged_in >= config.max_players;
}

Client* Server::find_client(const std::string &nick) {
    for (auto &client : clients) {
        if (client.second.nick == nick && !client.second.closing) {
            return &client.second;
        }
    }
    return nullptr;
}

std::string Server::rooms_message() {
    std::string msg_to_send = "ROOMS " + std::to_string(config.room_count);
    for (int i = 0; i < config.room_count; i++) {
        std::string room_num = std::to_string(i);
        std::string room_white = rooms[i].white.empty() ? "-" : rooms[i].white;
        std::string room_black = rooms[i].black.empty() ? "-" : rooms[i].black;
        msg_to_send += " " + room_num + ":" + room_white + ":" + room_black;
    }
    return msg_to_send;
}

void Server::broadcast_rooms() {
    std::string rooms_msg = rooms_message();
    for (auto &[fd, client] : clients) {
        if (client.state == State::LOBBY) {
            send_to(client, rooms_msg);
        }
    }
}

void Server::start_game(Room &room) {
    Client *white = find_client(room.white);
    Client *black = find_client(room.black);
    if (white != nullptr) {
        set_state(*white, State::IN_GAME);
        send_to(*white, "GAME_START WHITE " + room.black);
    }
    if (black != nullptr) {
        set_state(*black, State::IN_GAME);
        send_to(*black, "GAME_START BLACK " + room.white);
    }
}

void Server::end_game(Room &room, const std::string &winner, const std::string &reason) {
    // find_client skips players marked closing (a player who is leaving), so either can be nullptr
    Client *white = find_client(room.white);
    Client *black = find_client(room.black);

    std::string game_end_msg = "GAME_END " + winner + " " + reason;
    if (white != nullptr) {
        send_to(*white, game_end_msg);
        white->room = -1;
        set_state(*white, State::LOBBY);
    }
    if (black != nullptr) {
        send_to(*black, game_end_msg);
        black->room = -1;
        set_state(*black, State::LOBBY);
    }

    room.white.clear();
    room.black.clear();

    broadcast_rooms(); // after the state change, so both players get the room list too
}

void Server::remove_from_room(Client &client) {
    if (client.state == State::WAITING) {
        rooms[client.room].white.clear(); // WAITING can only be white
        client.room = -1;
        set_state(client, State::LOBBY);
        broadcast_rooms();
        return;
    }
    if (client.state == State::IN_GAME) {
        Room &room = rooms[client.room];
        std::string winner = (client.nick == room.white) ? "BLACK" : "WHITE"; // the opponent wins
        end_game(room, winner, "OPPONENT_LEFT");
        // the leaver is closing, so end_game skipped it; reset it here, or a second
        // call (re-login, then the sweep) would end whatever game is in that room by then
        client.room = -1;
        set_state(client, State::LOBBY);
        return;
    }
    // nothing happens in states LOGIN and LOBBY
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