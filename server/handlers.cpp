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
        send_error(client, "BAD_MESSAGE", true);
        return;
    }

    // Requests from the client, in the order of the table in docs/protocol.md
    if (msg.opcode == "PING") {
        handle_ping(client, msg);
    } else if (msg.opcode == "LOGIN") {
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
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
        //TODO
        send_error(client, "NOT_IMPLEMENTED", true);
    } else {
        std::cerr << "Unknown command from socket " << client.fd << ": [" << line << "]\n";
        send_error(client, "BAD_MESSAGE", true);
    }
}

// PING, in any state: replies PONG
void Server::handle_ping(Client &client, const Message &msg) {
    if (!msg.args.empty()) {
        send_error(client, "BAD_MESSAGE", true);
        return;
    }
    send_to(client, "PONG");
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
