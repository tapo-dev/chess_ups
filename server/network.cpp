#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include <arpa/inet.h>  // inet_pton, htons, struct sockaddr_in
#include <unistd.h>     // close
#include <sys/socket.h> // socket, bind, listen, accept, send, recv

#include "network.h"

// Sends one line (adds the '\n') and returns false if it could not be sent whole
bool send_line(int fd, const std::string &text) {
    printf("Sent to socket %d: [%s]\n", fd, text.c_str());
    std::string text_to_send = text + "\n";
    ssize_t bytes_sent = send(fd, text_to_send.data(), text_to_send.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
    if (bytes_sent != (ssize_t)text_to_send.size()) {
        return false;
    }
    return true;
}

// Reads what is waiting on the socket and moves every complete line
// from the clients buffer to lines
// Returns false if the client throws an error or closes
bool read_lines(int skt, std::string &inbuf, std::vector<std::string> &lines) {
    char buffer[1024];
    ssize_t size_received = recv(skt, buffer, sizeof(buffer), 0);
    // Either error or client closed the connection
    if (size_received < 0) {
        perror("recv");
        return false;
    }
    if (size_received == 0) {
        return false;
    }

    inbuf.append(buffer, size_received);

    size_t line_end;
    while ((line_end = inbuf.find('\n')) != std::string::npos) {
        // a complete line can be too long too: the rest of an earlier recv plus this one
        if (line_end > MAX_LINE_LENGTH) {
            std::cerr << "Line too long (socket " << skt << ")\n";
            send_line(skt, "ERR BAD_MESSAGE");
            return false;
        }
        lines.push_back(inbuf.substr(0, line_end));
        inbuf.erase(0, line_end + 1);
    }

    // What is left is an unfinished line; refuse to store it forever
    if (inbuf.size() > MAX_LINE_LENGTH) {
        std::cerr << "Line too long (socket " << skt << ")\n";
        send_line(skt, "ERR BAD_MESSAGE");
        return false;
    }

    return true;
}

int create_listen_socket(const std::string &address, int port) {
    int error_num = -1;
    // create the socket (ask the system to create it)
    // AF_INET = IPv4, SOCK_STREAM = TCP (stream service), 0 = default protocol
    int skt = socket(AF_INET, SOCK_STREAM, 0);
    if (skt < 0) {
        perror("socket");
        return error_num;
    }

    // prepare the structure with the address we want to listen on
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // htons = host to network short (converts the port to network byte order - big-endian)
    // inet_pton = converts an IP address from text to binary form
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Wrong IP address format\n";
        close(skt);
        return error_num;
    }

    int optval = 1;
    // socket option that frees the address and port as soon as the socket is closed (otherwise we would have to wait until the system releases them)
    if (setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        close(skt);
        return error_num;
    }

    // assign the socket the address we want to listen on
    // note: we are not listening yet, this only "binds" the socket to the address
    if (bind(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(skt);
        return error_num;
    }

    // start listening on the socket we created and bound to the address
    // this also creates the queue of incoming connections (second parameter, here 5) - how many connections may wait until the server accepts them
    if (listen(skt, 5) < 0) {
        perror("listen");
        close(skt);
        return error_num;
    }

    return skt;
}