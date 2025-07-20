#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH   "/tmp/flexraylogd_unix_socket"
#define READ_BUF_SIZE (0x4000U)

int connect_to_server() {
    int sockfd;
    struct sockaddr_un addr;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int main()
{
    char buf[READ_BUF_SIZE];
    ssize_t bytes_read;

    int sockfd = connect_to_server();
    char cmd[3];

    if (sockfd < 0) {
         exit(1);
    }

    cmd[0] = 0x1F;

    write(sockfd, cmd, 1);
    read(sockfd, buf, 2);
    close(sockfd);

    while (1) {
        sockfd = connect_to_server();

        if (sockfd < 0) {
            printf("retry connect to server\n");
            continue;
        }

        cmd[0] = 0x80;
        cmd[1] = 0x40;
        cmd[2] = 0x00;
        write(sockfd, cmd, 3);

        bytes_read = read(sockfd, buf, sizeof(buf));
        if (bytes_read < 0) {
            perror("read");
        } else if (bytes_read == 0) {
            //printf("close sever, retry.\n");
        } else {
            printf("recv %zd bytes:\n", bytes_read);
            #if 1
            for (ssize_t i = 0; i < bytes_read; i++) {
                printf("%02X ", (unsigned char)buf[i]);
                if ((i + 1) % 16 == 0) printf("\n");
            }
            printf("\n");
            #endif
        }
        close(sockfd);
    }
    return 0;
}

