#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 2048
#define MAX_CLIENTS 10

static unsigned int client_count = 0;
static int uid = 10;

// Client structure
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_trim_lf(char *arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void queue_add(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buff_out[BUFFER_SIZE];
    int leave_flag = 0;

    client_count++;
    client_t *cli = (client_t *)arg;

    // Print client address
    printf("Client %d connected from %s:%d\n", cli->uid, inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));

    // Send welcome message
    sprintf(buff_out, "Welcome, Client %d!\n", cli->uid);
    write(cli->sockfd, buff_out, strlen(buff_out));

    bzero(buff_out, BUFFER_SIZE);

    while (1) {
        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buff_out) > 0) {
                str_trim_lf(buff_out, strlen(buff_out));
                printf("Client %d: %s\n", cli->uid, buff_out);

                // Get custom response from server operator
                printf("Enter custom response for Client %d: ", cli->uid);
                fgets(buff_out, BUFFER_SIZE, stdin);
                str_trim_lf(buff_out, strlen(buff_out));

                // Send the custom response to the client
                write(cli->sockfd, buff_out, strlen(buff_out));
            }
        } else if (receive == 0 || strcmp(buff_out, "exit") == 0) {
            printf("Client %d disconnected\n", cli->uid);
            leave_flag = 1;
        } else {
            perror("recv");
            leave_flag = 1;
        }

        bzero(buff_out, BUFFER_SIZE);

        if (leave_flag) {
            break;
        }
    }

    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    client_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip = "0.0.0.0";
    int port = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", port);

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        if ((client_count + 1) == MAX_CLIENTS) {
            printf("Max clients reached. Rejected: ");
            print_client_addr(cli_addr);
            printf(":%d\n", ntohs(cli_addr.sin_port));
            close(connfd);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);

        sleep(1);
    }

    return 0;
}
