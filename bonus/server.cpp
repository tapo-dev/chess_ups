#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include <arpa/inet.h>  // inet_pton, htons, struct sockaddr_in
#include <sys/socket.h> // socket, bind, listen, accept, send, recv
#include <unistd.h>     // close
#include <poll.h>       // poll

enum class State {
    WAIT_HELLO,
    WAIT_ANSWER
};

// Everything the server remembers about one connected client
struct Client {
    State state = State::WAIT_HELLO;
    std::string inbuf;  // received bytes that do not form a complete line yet
    int sent_num = 0;   // number sent in NUM:, the client must answer with its double
};

// Longest unfinished line a client may keep in its buffer
const size_t MAX_LINE_LENGTH = 256;

// Sends one line (adds the '\n') and returns false if it could not be sent whole
bool send_line(int fd, const std::string &text) {
    printf("Sent to socket %d: [%s]\n", fd, text.c_str());
    std::string text_to_send = text + "\n";
    ssize_t bytes_sent = send(fd, text_to_send.data(), text_to_send.size(), MSG_NOSIGNAL);
    if (bytes_sent != (ssize_t)text_to_send.size()) {
        return false;
    }
    return true;
}

// Reads what is waiting on the socket and moves every complete line
// from the clients buffer to lines
bool read_lines(int skt, Client &client, std::vector<std::string> &lines) {
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

    client.inbuf.append(buffer, size_received);

    size_t line_end;
    while ((line_end = client.inbuf.find('\n')) != std::string::npos) {
        lines.push_back(client.inbuf.substr(0, line_end));
        client.inbuf.erase(0, line_end + 1);
    }

    // What is left is an unfinished line; refuse to store it forever
    if (client.inbuf.size() > MAX_LINE_LENGTH) {
        std::cerr << "Line too long (socket " << skt << ")\n";
        send_line(skt, "ERROR");
        return false;
    }

    return true;
}

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

// Reacts to one complete line according to the client's state.
// Returns false when the connection should be closed.
bool handle_line(int skt, Client &client, const std::string &line) {
    switch (client.state) {
        case State::WAIT_HELLO: {
            if (line != "HELLO") {
                send_line(skt, "ERROR");
                return false;
            }
            int rand_num = rand() % 101;
            if (!send_line(skt, "NUM:" + std::to_string(rand_num))) {
                return false;
            }
            client.state = State::WAIT_ANSWER;
            client.sent_num = rand_num;
            break;
        }

        case State::WAIT_ANSWER: {
            if (!is_number(line)) {
                send_line(skt, "ERROR");
                return false;
            }
            if (std::stoi(line) != 2 * client.sent_num) {
                send_line(skt, "WRONG");
                return false;
            }
            // the exchange is over either way, so the connection is closed after OK too
            send_line(skt, "OK");
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    srand(time(nullptr));
    if (argc != 3) {
        std::cerr << "Running server requires IP address and port specified in args\n";
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[2]);
    if (port < 1 || port > 65535) {
        std::cerr << "Invalid port\n";
        exit(EXIT_FAILURE);
    }
    
    // create the socket (ask the system to create it)
    // AF_INET = IPv4, SOCK_STREAM = TCP (stream service), 0 = default protocol
    int skt = socket(AF_INET, SOCK_STREAM, 0);
    if (skt < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // prepare the structure with the address we want to listen on
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // htons = host to network short (converts the port to network byte order - big-endian)
    // inet_pton = converts an IP address from text to binary form
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
        std::cerr << "Wrong IP address format\n";
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    // socket option that frees the address and port as soon as the socket is closed (otherwise we would have to wait until the system releases them)
    if (setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // assign the socket the address we want to listen on
    // note: we are not listening yet, this only "binds" the socket to the address
    if (bind(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // start listening on the socket we created and bound to the address
    // this also creates the queue of incoming connections (second parameter, here 5) - how many connections may wait until the server accepts them
    if (listen(skt, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s:%d\n", argv[1], port);

    std::vector<pollfd> fds;
    std::unordered_map<int, Client> clients;

    // at the start there is only the server socket, which listens for incoming connections
    fds.push_back({skt, POLLIN, 0});

    while (1) {
        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == skt) {
                    // accept a connection from a client
                    int client_skt = accept(skt, NULL, NULL);
                    if (client_skt < 0) {
                        perror("accept");
                        std::cerr << "Accepting client failed\n";
                        continue;
                    }

                    printf("Client connected (socket %d)\n", client_skt);
                    clients[client_skt] = Client{};
                    fds.push_back({client_skt, POLLIN, 0});
                } else {
                    // receive data from the client
                    int client_skt = fds[i].fd;
                    std::vector<std::string> lines;
                    bool keep_client = read_lines(client_skt, clients[client_skt], lines);

                    for (const std::string &line : lines) {
                        printf("Received from socket %d: [%s]\n", client_skt, line.c_str());
                        if (!handle_line(client_skt, clients[client_skt], line)) {
                            keep_client = false;
                            break;
                        }
                    }

                    if (!keep_client) {
                        // the client closed the connection or an error occurred (outage, ...)
                        printf("Client disconnected (socket %d)\n", client_skt);
                        close(client_skt);

                        clients.erase(client_skt);
                        // remove the socket; erase shifts all the following ones one position back
                        fds.erase(fds.begin() + i);
                        i--; // step the index back so the next iteration checks the socket that moved to position i
                    }
                }
            }
        }
    }

    close(skt);

    return 0;
}
