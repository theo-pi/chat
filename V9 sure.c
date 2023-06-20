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

// Fonction pour envoyer un message à tous les clients, sauf l'expéditeur
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

// Fonction exécutée par chaque thread client
void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char username[BUFFER_SIZE];

    // Envoie le message "Entrer votre pseudo :" au client
    if (send(client_socket, "Entrer votre pseudo : ", strlen("Entrer votre pseudo : "), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Reçoit le nom d'utilisateur du client
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

            // Envoie le message à tous les clients
            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, BUFFER_SIZE, "\033[1;34m%s:\033[0m %s", username, message);
            send_message_to_all_clients(formatted_message, client_socket);
        } else if (bytes_received == 0) {
            // Le client s'est déconnecté
            printf("\033[1;34m%s a quitté(e) le chat\033[0m\n", username);

            char leave_message[BUFFER_SIZE];
            snprintf(leave_message, BUFFER_SIZE, "%s a quitté(e) le chat\n", username);
            send_message_to_all_clients(leave_message, client_socket);

            // Ferme le socket du client
            close(client_socket);

            // Supprime le socket du client du tableau
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

// Gère les signaux reçus par le serveur
void handle_signal(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        printf("Server fermé.\n");

        // Ferme le socket du serveur
        close(server_socket);

        // Ferme tous les sockets clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = client_sockets[i];
            if (client_socket != -1) {
                close(client_socket);
            }
        }

        exit(EXIT_SUCCESS);
    } else if (signal == SIGHUP) {
        printf("Redémarrage du serveur...\n");

        // Ferme le socket du serveur
        close(server_socket);

        // Ferme tous les sockets clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = client_sockets[i];
            if (client_socket != -1) {
                close(client_socket);
            }
        }

        execl("./server", NULL);
        //execl("./server", "server");

        perror("Erreur lors du redémarrage du serveur");
        exit(EXIT_FAILURE);
    } else if (signal == SIGUSR1) {
        printf("Redémarrage du serveur en mode daemon...\n");

        // Ferme le socket du serveur
        close(server_socket);

        // Ferme tous les sockets clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = client_sockets[i];
            if (client_socket != -1) {
                close(client_socket);
            }
        }

        execl("./server", "server", "-daemon");

        perror("Erreur lors du redémarrage du serveur en mode daemon");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {

    if (argc >= 2 && strcmp(argv[1], "-daemon") == 0) {
        daemon(0, 0); // Création du daemon
    }

    // Enregistre les gestionnaires de signaux
    signal(SIGUSR1, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Crée le socket du serveur
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configure l'adresse du serveur
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_address.sin_port = htons(SERVER_PORT);

    // Lie le socket du serveur à l'adresse spécifiée
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Écoute les connexions entrantes
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Le serveur est prêt à recevoir des connexions...\n");

    // Initialise le tableau de sockets clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);

        // Accepte une nouvelle connexion client
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Ajoute le socket client au tableau
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == -1) {
                client_sockets[i] = client_socket;
                break;
            }
        }

        // Crée un nouveau thread pour gérer le client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, &client_socket) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
