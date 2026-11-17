#include <iostream>
#include <string>

#include "message.h"
#include "server.h"

// Parses one complete line from the client and passes it to the handler of its opcode
void Server::handle_line(Client &client, const std::string &line) {
    Message msg;
    if (!parse_message(line, msg)) {
        std::string printable_line = printable(line);
        std::cerr << "Invalid message from socket " << client.fd << ": [" << printable_line << "]\n";
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }

    // Requests from the client, in the order of the table in docs/protocol.md
    if (msg.opcode == "PING") {
        handle_ping(client, msg);
    } else if (msg.opcode == "LOGIN") {
        handle_login(client, msg);
    } else if (msg.opcode == "LIST") {
        handle_list(client, msg);
    } else if (msg.opcode == "JOIN") {
        handle_join(client, msg);
    } else if (msg.opcode == "LEAVE") {
        handle_leave(client, msg);
    } else if (msg.opcode == "MOVE") {
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
    } else if (msg.opcode == "RESIGN") {
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
    } else if (msg.opcode == "LOGOUT") {
        handle_logout(client, msg);
    } else {
        std::cerr << "Unknown command from socket " << client.fd << ": [" << line << "]\n";
        send_error(client, ERR_BAD_MESSAGE, true);
    }
}

// PING, in any state: replies PONG
void Server::handle_ping(Client &client, const Message &msg) {
    if (!msg.args.empty()) {
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }
    send_to(client, "PONG");
}

// LOGIN <nick>; in LOGIN state checks for relogin; if everything ok, change state to LOBBY
void Server::handle_login(Client &client, const Message &msg) {
    if (msg.args.size() != 1) { 
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }
    if (client.state != State::LOGIN) {
        send_error(client, ERR_WRONG_STATE, true);
        return;
    }

    std::string nick = msg.args[0];
    if (!is_valid_nick(nick)) {
        send_error(client, ERR_INVALID_NICK, true);
        return;
    }
    
    // a re-login replaces a player, so only a new nick can hit the player limit
    Client *old = find_client(nick);
    if (old == nullptr && is_server_full()) {
        send_error(client, ERR_SERVER_FULL, false);
        client.closing = true;
        return;
    }

    if (old != nullptr) { // the same nick is online: the new connection replaces the old one
        std::cout << nick << " (fd " << old->fd << ") replaced by fd " << client.fd << "\n";
        old->closing = true;
        remove_from_room(*old);
    }
    send_to(client, "LOGIN_OK LOBBY");
    client.nick = nick;
    set_state(client, State::LOBBY);
}

// LIST, in the lobby: replies with the room list
void Server::handle_list(Client &client, const Message &msg) {
    if (!msg.args.empty()) {
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }
    if (client.state != State::LOBBY) {
        send_error(client, ERR_WRONG_STATE, true);
        return;
    }

    send_to(client, rooms_message());
}

// JOIN n, in the lobby: takes a seat in room n; the second player starts the game
void Server::handle_join(Client &client, const Message &msg) {
    if (msg.args.size() != 1 || !is_number(msg.args[0])) {
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }
    if (client.state != State::LOBBY) {
        send_error(client, ERR_WRONG_STATE, true);
        return;
    }

    int room_num = std::stoi(msg.args[0]);
    if (room_num >= config.room_count) {
        send_error(client, ERR_INVALID_ROOM, true);
        return;
    }
    Room &room = rooms[room_num];
    if (!room.black.empty()) { // if black is not empty, the game must have started
        send_error(client, ERR_ROOM_FULL, false);
        return;
    }

    // the reply first, then the events it causes
    client.room = room_num;
    send_to(client, "JOIN_OK " + std::to_string(room_num));
    if (room.white.empty()) {
        room.white = client.nick;
        set_state(client, State::WAITING);
    } else {
        room.black = client.nick;
        start_game(room);
    }
    broadcast_rooms();
}

// LEAVE, while waiting alone in a room: frees the seat and goes back to the lobby
void Server::handle_leave(Client &client, const Message &msg) {
    if (!msg.args.empty()) {
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }
    if (client.state != State::WAITING) {
        send_error(client, ERR_WRONG_STATE, true);
        return;
    }

    send_to(client, "LEAVE_OK");
    remove_from_room(client);
}

// LOGOUT, in any state except LOGIN: replies LOGOUT_OK and closes the connection
void Server::handle_logout(Client &client, const Message &msg) {
    if (!msg.args.empty()) {
        send_error(client, ERR_BAD_MESSAGE, true);
        return;
    }
    if (client.state == State::LOGIN) {
        send_error(client, ERR_WRONG_STATE, true);
        return;
    }

    send_to(client, "LOGOUT_OK");
    client.closing = true;
}

void Server::send_error(Client &client, const std::string &code, bool strike) {
    send_to(client, "ERR " + code);
    if (!strike) {
        return;
    }
    client.strikes++;
    if (client.strikes >= 3) {
        client.closing = true;
    }
}
