/*
 * KIV/UPS - priklady sitoveho programovani
 *
 * Zakladni TCP server
 * Ve vsech prikladech na TCP v teto slozce je fungovani serveru identicke:
 *   - vytvori socket
 *   - binduje na 127.0.0.1:10000
 *   - prijima spojeni
 *   - prijima znaky a odesila je invertovane (A -> Z, B -> Y, ...)
 *   - zavre sockety po ukonceni komunikace
 * 
 * V teto variante je i paralelni odbaveni klientu:
 *   Kazdy klient je obslouzen pomoci poll(), tj. server muze obsluhovat vice klientu soucasne bez vytvareni novych vlaken
 * 
 * poll je modernejsi varianta nez select(), podporuje vice socketu a nema omezeni na nejvyssi cislo socketu
 */

#include <stdlib.h>
#include <stdio.h>

#include <arpa/inet.h>  // inet_pton, htons, struct sockaddr_in
#include <sys/socket.h> // socket, bind, listen, accept, send, recv
#include <unistd.h>     // close
#include <string.h>     // memset
#include <poll.h>      // poll

#define BIND_ADDRESS "127.0.0.1"
#define BIND_PORT 10000

int main(int, char**) {

    // vytvorime socket (pozadame system o jeho vytvreni)
    // AF_INET = IPv4, SOCK_STREAM = TCP (streamovane sluzby), 0 = vychozi protokol
    int skt = socket(AF_INET, SOCK_STREAM, 0);
    if (skt < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // pripravime strukturu s adresou, na ktere chceme naslouchat
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BIND_PORT); // htons = host to network short (prevedeni portu do sitove endianity - big-endian)
    // inet_pton = prevedeni IP adresy z textove podoby do binarni podoby
    if (inet_pton(AF_INET, BIND_ADDRESS, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    // nastaveni socketu, aby se po zavreni socketu uvolnila adresa a port, na kterych naslouchal (jinak by se muselo pockat, nez by je system uvolnil)
    if (setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // priradime socketu adresu, na ktere chceme naslouchat
    // pozor, jeste neposlouchame, tady jen "svazeme" socket s adresou
    if (bind(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // zacneme naslouchat na socketu, ktery jsme si vytvorili a svazali s adresou
    // toto vytvori i frontu prichozich spojeni (druhy parametr, zde 5) - kolik spojeni muze cekat, nez je server ochoten je prijmout (accept)
    if (listen(skt, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s:%d\n", BIND_ADDRESS, BIND_PORT);

    struct pollfd fds[16];

    for (int i = 0; i < 16; i++) {
        fds[i].fd = -1; // oznacime jako nepouzite
    }

    // na zacatku mame jen server socket, ktery naslouchá na prichozí spojeni
    int nfds = 1;
    fds[0].fd = skt;
    fds[0].events = POLLIN;

    while (1) {

        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == skt) {
                    // prijmeme spojeni od klienta
                    int client_skt = accept(skt, NULL, NULL);
                    if (client_skt < 0) {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    fds[nfds].fd = client_skt;
                    fds[nfds].events = POLLIN;
                    nfds++;
                } else {
                    // prijmeme data od klienta
                    char buffer;
                    int result = recv(fds[i].fd, &buffer, 1, 0);

                    if (result <= 0) {
                        // klient uzavrel spojeni nebo nastala chyba (vypadek, ...)
                        printf("Client disconnected (socket %d)\n", fds[i].fd);
                        close(fds[i].fd);
                        fds[i].fd = -1; // oznacime jako nepouzite

                        // posuneme vsechny sockety v poli o jednu pozici zpet, aby se nepouzite sockety dostaly na konec pole
                        for (int j = i; j < nfds - 1; j++) {
                            fds[j] = fds[j + 1];
                        }
                        nfds--;
                        i--; // zmenime index, aby se v dalsim pruchodu cyklu znovu zkontroloval socket, ktery jsme posunuli na misto i

                    } else {
                        printf("Received: %c\n", buffer);
                        buffer = 'Z' - (buffer - 'A');
                        send(fds[i].fd, &buffer, 1, 0);
                    }
                }
            }
        }
    }

    close(skt);

    return 0;
}
