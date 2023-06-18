#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3

int server_socket;
int client_sockets[MAX_CLIENTS];
pthread_mutex_t client_sockets_mutex;

void send_message_to_all_clients(const char *message, int sender_socket) {
    pthread_mutex_lock(&client_sockets_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = client_sockets[i];

        if (client_socket != -1 && client_socket != sender_socket) {
            if (send(client_socket, message, strlen(message), 0) == -1) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }

    pthread_mutex_unlock(&client_sockets_mutex);
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char username[BUFFER_SIZE];

    // Send "Entrer votre pseudo :" message to the client
    char prompt_message[] = "Entrer votre pseudo : ";
    if (send(client_socket, prompt_message, strlen(prompt_message), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Receive the username from the client
    ssize_t bytes_received = recv(client_socket, username, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        for (int i = 0; i < bytes_received; ++i) {
            if (username[i] == '\n' || username[i] == '\r') {
                username[i] = '\0';
                break;
            }
        }
        printf("\033[1;34m%s a rejoint le chat\033[0m\n", username);

        char welcome_message[BUFFER_SIZE];
        snprintf(welcome_message, BUFFER_SIZE, "Bienvenue, %s\n", username);
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
            printf("\033[1;34m%s a quitté(e) le chat\033[0m\n", username);

            char leave_message[BUFFER_SIZE];
            snprintf(leave_message, BUFFER_SIZE, "%s a quitté(e) le chat\n", username);
            send_message_to_all_clients(leave_message, client_socket);

            // Close the client socket
            close(client_socket);

            pthread_mutex_lock(&client_sockets_mutex);

            // Remove the client socket from the array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == client_socket) {
                    client_sockets[i] = -1;
                    break;
                }
            }

            pthread_mutex_unlock(&client_sockets_mutex);

            break;
        } else {
            perror("recv");
            exit(EXIT_FAILURE);
        }
    }

    pthread_exit(NULL);
}

void handle_signal(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        printf("Server terminated.\n");

        // Close the server socket
        close(server_socket);

        // Close all client sockets
        pthread_mutex_lock(&client_sockets_mutex);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = client_sockets[i];
            if (client_socket != -1) {
                close(client_socket);
            }
        }

        pthread_mutex_unlock(&client_sockets_mutex);

        pthread_mutex_destroy(&client_sockets_mutex);

        exit(EXIT_SUCCESS);
    }
}

int main() {
    // Register signal handlers
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_address.sin_port = htons(SERVER_PORT);

    // Bind the server socket to the specified address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Le serveur est prêt à recevoir des connexions...\n");

    // Initialize the client sockets array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    pthread_mutex_init(&client_sockets_mutex, NULL);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);

        // Accept a new client connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&client_sockets_mutex);

        // Add the client socket to the array
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == -1) {
                client_sockets[i] = client_socket;
                break;
            }
        }

        pthread_mutex_unlock(&client_sockets_mutex);

        // Create a new thread to handle the client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, &client_socket) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&client_sockets_mutex);

    return 0;
}
