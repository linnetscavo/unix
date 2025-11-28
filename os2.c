#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h> 

static volatile sig_atomic_t got_sighup = 0;

void sighup_handler(int sig) {
    (void)sig;
    got_sighup = 1;
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345); 
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_fd);
        exit(1);
    }

    if (listen(listen_fd, 5) == -1) {
        perror("listen");
        close(listen_fd);
        exit(1);
    }

    struct sigaction sa = {0};
    sa.sa_handler = sighup_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction");
        close(listen_fd);
        exit(1);
    }

    sigset_t blocked_mask, orig_mask;
    sigemptyset(&blocked_mask);
    sigaddset(&blocked_mask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blocked_mask, &orig_mask) == -1) {
        perror("sigprocmask");
        close(listen_fd);
        exit(1);
    }

    int client_fd = -1; 
    const int MAX_BUF = 256;
    char buf[MAX_BUF];

    printf("Server started on 127.0.0.1:12345. Waiting for connections...\n");

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        int max_fd = listen_fd;

        if (client_fd != -1) {
            FD_SET(client_fd, &read_fds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        int ready = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, &orig_mask);
        if (ready == -1) {
            if (errno == EINTR) {
                if (got_sighup) {
                    printf("Received SIGHUP\n");
                    got_sighup = 0; 
                }
                continue;
            } else {
                perror("pselect");
                break;
            }
        }

        if (FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int new_fd = accept(listen_fd, (struct sockaddr*)&cli_addr, &cli_len);
            if (new_fd == -1) {
                perror("accept");
                continue;
            }

            if (client_fd == -1) {
                client_fd = new_fd;
                printf("Accepted connection (fd=%d)\n", client_fd);
            } else {
                close(new_fd);
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds)) {
            ssize_t n = recv(client_fd, buf, MAX_BUF, 0);
            if (n > 0) {
                printf("Received %zd bytes from client\n", n);
            } else if (n == 0) {
                printf("Client closed connection\n");
                close(client_fd);
                client_fd = -1;
            } else {
                    perror("recv");
                    close(client_fd);
                    client_fd = -1;
            }
        }
    }

    if (client_fd != -1) close(client_fd);
    close(listen_fd);
    return 0;
}