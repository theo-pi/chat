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
char buffer[BUFFER_SIZE];
char username[BUFFER_SIZE];
/*
void *send_message(void *arg) {
    while (1) {
        //printf("%s: ", username);
        fgets(buffer, BUFFER_SIZE, stdin);

        // Send the message to the server
        if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }

    pthread_exit(NULL);
}
*/
void *send_message(void *arg) {
    while (1) {
        //printf("%s: ", username);
        fgets(buffer, BUFFER_SIZE, stdin);

        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = '\0';

        // Set the text color to white
        printf("\033[0;37m");

        // Send the message to the server
        if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }

        // Reset the text color to default
        printf("\033[0m");
    }

    pthread_exit(NULL);
}

void *receive_message(void *arg) {
    ssize_t bytes_received;

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("\033[1;34m%s\033[0m\n", buffer);
    }

    if (bytes_received == 0) {
        printf("Server disconnected.\n");
    } else {
        perror("recv");
    }

    pthread_exit(NULL);
}

int main() {
    struct sockaddr_in server_addr;

    // Create client socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set server address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server.\n");

    // Receive the username prompt from the server
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    buffer[bytes_received] = '\0';
    printf("%s", buffer);

    // Send the username to the server
    fgets(username, BUFFER_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0'; // Remove newline character

    if (send(client_socket, username, strlen(username), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    pthread_t send_thread, receive_thread;

    // Create send thread
    if (pthread_create(&send_thread, NULL, send_message, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Create receive thread
    if (pthread_create(&receive_thread, NULL, receive_message, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Wait for threads to finish
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(client_socket);
    return 0;
}
