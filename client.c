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

int client_socket;

void *receive_message(void *arg) {
    while (1) {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("\033[0;34m%s\033[0m\n", buffer);
        } else if (bytes_received == 0) {
            // Server has disconnected
            printf("Server has disconnected.\n");
            break;
        } else {
            perror("recv");
            exit(EXIT_FAILURE);
        }
    }

    pthread_exit(NULL);
}

int main() {
    struct sockaddr_in server_address;

    // Create the client socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_address.sin_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Get the username from the user
    char username[BUFFER_SIZE];
    printf("Enter your username: ");
    fgets(username, BUFFER_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0';

    // Send the username to the server
    if (send(client_socket, username, strlen(username), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Create a new thread to receive messages from the server
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_message, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while (1) {
        char message[BUFFER_SIZE];
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = '\0';

        if (strcmp(message, "quit") == 0) {
            printf("Déconnecté...\n");
            exit(EXIT_SUCCESS);
        }

        // Send the message to the server
        if (send(client_socket, message, strlen(message), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }

    // Close the client socket
    close(client_socket);

    return 0;
}
