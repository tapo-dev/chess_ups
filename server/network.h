#pragma once

#include <string>
#include <vector>
#include <cstddef>

const size_t MAX_LINE_LENGTH = 1024;

bool send_line(int fd, const std::string &text);

bool read_lines(int skt, std::string &inbuf, std::vector<std::string> &lines);

int create_listen_socket(const std::string &address, int port);
