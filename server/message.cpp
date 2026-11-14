#include <vector>
#include <string>
#include <regex>

#include "message.h"

bool is_number(const std::string &s) {
    if (s == "") {
        return false;
    }
    if (s.size() > 9) {
        return false;
    }
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool parse_message(const std::string &line, Message &msg) {
    std::string current = "";
    std::vector<std::string> parts;
    for (auto c : line) {
        if (c == 32) { // 32 is space in ascii
            if (current.empty()) {
                // The line either starts with space or there is a double space
                // Both are not allowed
                return false;
            }
            parts.push_back(current);
            current.clear(); // clears the string for next iter
            continue;
        }
        else if (c < 33 || c > 126) {
            // not a valid character
            return false;
        }
        current.push_back(c);
    }

    if (current.empty()) {
        return false;
    }
    parts.push_back(current);

    msg.opcode = parts[0];
    for (size_t i = 1; i < parts.size(); i++) {
        msg.args.push_back(parts[i]);
    }

    return true;
}

std::string printable(const std::string &line) {
    std::string printable_line;
    for (auto c : line) {
        if (c < 32 || c > 126) { // from 32 to 126 its normal characters
            printable_line.push_back('?');
        }
        else {
            printable_line.push_back(c);
        }
    }

    return printable_line;
}

bool is_valid_nick(const std::string &nick) {
    // regex_match succeeds only if the whole nick matches
    std::regex allowed("[A-Za-z0-9_]{3,16}");
    return std::regex_match(nick, allowed);
}