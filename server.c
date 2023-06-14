#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    char username[BUFFER_SIZE];
} Client;

Client clients[MAX_CLIENTS];
pthread_t client_threads[MAX_CLIENTS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    int client_socket = client->socket;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s sent: %s\n", client->username, buffer);

        pthread_mutex_lock(&mutex);

        // Broadcast the received message to all other clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1 && clients[i].socket != client_socket) {
                char message[BUFFER_SIZE];
                snprintf(message, BUFFER_SIZE, "%s: %s", client->username, buffer);
                send(clients[i].socket, message, strlen(message), 0);
            }
        }

        pthread_mutex_unlock(&mutex);
    }

    if (bytes_read == 0) {
        printf("%s disconnected.\n", client->username);
    } else {
        perror("recv");
    }

    pthread_mutex_lock(&mutex);

    // Remove client from the array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_socket) {
            clients[i].socket = -1;
            break;
        }
    }

    pthread_mutex_unlock(&mutex);

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Initialize client socket array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
    }

    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set server address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345);

    // Bind server socket to the specified address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 12345...\n");

    while (1) {
        // Accept a new connection
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_mutex_lock(&mutex);

        // Find an empty slot in the client sockets array
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == -1) {
                clients[i].socket = client_socket;
                break;
            }
        }

        pthread_mutex_unlock(&mutex);

        if (i == MAX_CLIENTS) {
            printf("Max clients reached. Connection rejected.\n");
            close(client_socket);
        } else {
            // Prompt the client for their username
            char username[BUFFER_SIZE];
            snprintf(username, BUFFER_SIZE, "Enter your username: ");
            send(client_socket, username, strlen(username), 0);

            // Receive the username from the client
            ssize_t bytes_received = recv(client_socket, username, BUFFER_SIZE - 1, 0);
            username[bytes_received] = '\0';

            // Save the username in the client struct
            strncpy(clients[i].username, username, BUFFER_SIZE);

            // Create a new thread to handle the client
            if (pthread_create(&client_threads[i], NULL, handle_client, &clients[i]) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        }
    }

    close(server_socket);
    pthread_mutex_destroy(&mutex);
    return 0;
}
