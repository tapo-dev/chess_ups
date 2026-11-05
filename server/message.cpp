#include "message.h"

// Helper to check whether a string contains only a number or also characters
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