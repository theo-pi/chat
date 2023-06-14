#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2

int client_sockets[MAX_CLIENTS];

void send_message_to_all_clients(const char *message, int sender_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = client_sockets[i];

        if (client_socket != -1 && client_socket != sender_socket) {
            if (send(client_socket, message, strlen(message), 0) == -1) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char username[BUFFER_SIZE];

    // Receive the username from the client
    ssize_t bytes_received = recv(client_socket, username, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        username[bytes_received] = '\0';
        printf("\033[1;34m%s has joined the chat.\033[0m\n", username);

        char welcome_message[BUFFER_SIZE];
        snprintf(welcome_message, BUFFER_SIZE, "Welcome, %s!\n", username);
        send(client_socket, welcome_message, strlen(welcome_message), 0);
    } else {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    while (1) {
        char message[BUFFER_SIZE];
        bytes_received = recv(client_socket, message, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            message[bytes_received] = '\0';

            // Send the message to all clients
            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, BUFFER_SIZE, "\033[1;34m%s:\033[0m %s", username, message);
            send_message_to_all_clients(formatted_message, client_socket);
        } else if (bytes_received == 0) {
            // Client has disconnected
            printf("\033[1;34m%s has left the chat.\033[0m\n", username);

            char leave_message[BUFFER_SIZE];
            snprintf(leave_message, BUFFER_SIZE, "%s has left the chat.\n", username);
            send_message_to_all_clients(leave_message, client_socket);

            // Close the client socket
            close(client_socket);

            // Remove the client socket from the array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == client_socket) {
                    client_sockets[i] = -1;
                    break;
                }
            }

            break;
        } else {
            perror("recv");
            exit(EXIT_FAILURE);
        }
    }

    pthread_exit(NULL);
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    // Bind the server socket to the specified address and port
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Waiting for clients...\n");

    pthread_t client_threads[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    int client_count = 0;

    while (1) {
        // Accept a new client connection
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Add the client socket to the array
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == -1) {
                client_sockets[i] = client_socket;
                break;
            }
        }

        // Create a new thread to handle the client
        if (pthread_create(&client_threads[client_count], NULL, client_handler, &client_socket) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        client_count++;

        if (client_count == MAX_CLIENTS) {
            break;
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(client_threads[i], NULL);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
