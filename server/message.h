#pragma once

#include <string>
#include <vector>

constexpr const char *ERR_BAD_MESSAGE = "BAD_MESSAGE";
constexpr const char *ERR_WRONG_STATE = "WRONG_STATE";
constexpr const char *ERR_INVALID_NICK = "INVALID_NICK";
constexpr const char *ERR_INVALID_ROOM = "INVALID_ROOM";
constexpr const char *ERR_ILLEGAL_MOVE = "ILLEGAL_MOVE";
constexpr const char *ERR_ROOM_FULL = "ROOM_FULL";
constexpr const char *ERR_SERVER_FULL = "SERVER_FULL";

struct Message {
    std::string opcode;
    std::vector<std::string> args;
};

// helper to check whether a string contains only a number or also characters
bool is_number(const std::string &s);

// parse message from client
// returns false for invalid messages
bool parse_message(const std::string &line, Message &msg);

// returns a printable line from a line that could contain
// /dev/urandom stuff that would print awkwardly in terminal
std::string printable(const std::string &line);

// checks a valid nick based on the protocol
// nick should only contain [A-Za-z0-9_] and have 3-16 chars
bool is_valid_nick(const std::string &nick);