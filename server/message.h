#pragma once

#include <string>
#include <vector>

struct Message {
    std::string opcode;
    std::vector<std::string> args;
};

bool is_number(const std::string &s);

bool parse_message(const std::string &line, Message &msg);

std::string printable(const std::string &line);