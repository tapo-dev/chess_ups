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
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
    } else if (msg.opcode == "JOIN") {
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
    } else if (msg.opcode == "LEAVE") {
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
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

// Checks args count, state, validity of nick, if a server is full
// if everything is ok, reply LOGIN_OK LOBBY
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
    
    Client *old = find_client(nick);
    if (old != nullptr) { // if existing nick, reconnect
        std::cout << nick << " (fd " << old->fd << ") replaced by fd " << client.fd << "\n";
        old->closing = true;
    } else if (is_server_full()) {
        send_error(client, ERR_SERVER_FULL, false);
        client.closing = true;
        return;
    } 

    send_to(client, "LOGIN_OK LOBBY");
    client.nick = nick;
    set_state(client, State::LOBBY);
}

// Logout shouldnt have any arguments
// works on any state except LOGIN
void Server::handle_logout(Client &client, const Message &msg) {
    if (msg.args.size() > 0) {
        send_error(client, ERR_BAD_MESSAGE, true);
    } else if (client.state == State::LOGIN) {
        send_error(client, ERR_WRONG_STATE, true);
    } else {
        send_to(client, "LOGOUT_OK");
        client.closing = true;
    }
}

void Server::send_error(Client &client, const std::string &code, bool strike) {
    send_to(client, "ERR " + code);
    if (strike) {
        client.strikes++;
    }
    if (client.strikes >= 3) {
        client.closing = true;
    }
}
