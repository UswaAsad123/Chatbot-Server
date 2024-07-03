// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(1);
    }

    printf("Connected to server on port %d\n", portno);

    bzero(buffer, BUFFER_SIZE);
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("Error reading from socket");
        close(sockfd);
        exit(1);
    }
    printf("%s\n", buffer);

    while (1) {
        printf("Enter message: ");
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0) {
            perror("Error writing to socket");
            close(sockfd);
            exit(1);
        }

        bzero(buffer, BUFFER_SIZE);
        n = read(sockfd, buffer, BUFFER_SIZE);
        if (n < 0) {
            perror("Error reading from socket");
            close(sockfd);
            exit(1);
        }
        printf("%s\n", buffer);

        if (strstr(buffer, "exit") != NULL) {
            printf("Disconnected from server.\n");
            break;
        }
    }

    close(sockfd);
    return 0;
}
