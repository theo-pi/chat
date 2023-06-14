#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int client_count = 0;
int client_sockets[MAX_CLIENTS];
pthread_t client_threads[MAX_CLIENTS];

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            // Erreur de réception ou client déconnecté
            for (int i = 0; i < client_count; i++) {
                if (client_sockets[i] == client_socket) {
                    client_sockets[i] = client_sockets[client_count - 1];
                    break;
                }
            }
            client_count--;
            break;
        }

        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] != client_socket) {
                send(client_sockets[i], buffer, bytes_received, 0);
            }
        }
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;

    // Création du socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Erreur lors de la création du socket");
        exit(1);
    }

    // Configuration de l'adresse du serveur
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(12345); // Utilisez un numéro de port de votre choix

    // Attacher le socket à l'adresse du serveur
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Erreur lors de l'attachement du socket à l'adresse");
        exit(1);
    }

    // Écoute des connexions entrantes
    if (listen(server_socket, 5) < 0) {
        perror("Erreur lors de l'écoute des connexions entrantes");
        exit(1);
    }

    printf("Serveur en attente de connexions...\n");

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);

        // Accepter une nouvelle connexion client
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket < 0) {
            perror("Erreur lors de l'acceptation de la connexion client");
            exit(1);
        }

        if (client_count < MAX_CLIENTS) {
            // Ajouter le socket du client à la liste
            client_sockets[client_count++] = client_socket;

            // Créer un thread pour gérer la communication avec le client
            pthread_create(&client_threads[client_count - 1], NULL, handle_client, &client_sockets[client_count - 1]);

            printf("Nouvelle connexion acceptée. Client IP: %s\n", inet_ntoa(client_address.sin_addr));
        } else {
            // Trop de clients connectés, fermer la connexion
            printf("Trop de clients connectés. Connexion refusée. Client IP: %s\n", inet_ntoa(client_address.sin_addr));
            close(client_socket);
        }
    }

    // Fermer tous les sockets des clients
    for (int i = 0; i < client_count; i++) {
        pthread_join(client_threads[i], NULL);
        close(client_sockets[i]);
    }

    // Fermer le socket du serveur
    close(server_socket);

    return 0;
}