#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3

int server_socket;
int client_sockets[MAX_CLIENTS];

FILE *log_file;

void log_message(const char *level, const char *message) {
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    char time_string[20];
    strftime(time_string, sizeof(time_string), "%b %d %H:%M:%S", local_time);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    fprintf(log_file, "%s %s dhcp service[%s] %s\n", time_string, hostname, level, message);
    fflush(log_file);  // Pour s'assurer que le message est écrit immédiatement dans le fichier
}

/// @brief Envoie des messages aux clients
/// @param message 
/// @param sender_socket 
void send_message_to_all_clients(const char *message, int sender_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = client_sockets[i];

        if (client_socket != -1 && client_socket != sender_socket) {
            if (send(client_socket, message, strlen(message), 0) == -1) {
                perror("Erreur lors de l'envoi du message");
                exit(EXIT_FAILURE);
            }
        }
    }
}

/// @brief Gère les clients
/// @param arg 
/// @return 
void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char username[BUFFER_SIZE];

    // Entrer votre pseudo :
    char prompt_message[] = "Entrer votre pseudo : ";
    if (send(client_socket, prompt_message, strlen(prompt_message), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Réception du pseudo du client
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

/// @brief Gère la fermeture et le redémarrage du serveur
/// @param reboot 
/// @param reboot_daemon 
void Gestion_server(bool reboot, bool reboot_daemon) {
    // Fermeture de la socket du server
    if (close(server_socket) == -1) {
        perror("Erreur lors de la fermeture du serveur");
    }
    
    if(!reboot && !reboot_daemon) {
        printf("Server fermé.\n");
        log_message("info", "Server fermé");
    }
    
    // Fermeture de toutes les sockets des clients
    int client_close_success = 1;                       // Variable pour vérifier si la fermeture des clients a réussi
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = client_sockets[i];
        if (client_socket != -1) {
            if (close(client_socket) == -1) {
                perror("Erreur lors de la fermeture du socket d'un client");
                client_close_success = 0;               // La fermeture d'au moins un client a échoué
            }
        }
    }

    if (!client_close_success) {
        printf("La fermeture d'au moins un client socket a échoué.\n");
    }

    // Redémarrage du serveur
    if(reboot == true && reboot_daemon == false) {
        printf("Serveur redémarré\n");
        if (execl("./server", NULL) == -1) {
            perror("Erreur lors du redémarrage du serveur");
            exit(EXIT_FAILURE);
        }
    }

    // Redémarrage du serveur en mode daemon
    if(reboot_daemon == true && reboot == false) {
        printf("Serveur redémarré en mode daemon\n");
        if (execl("./server", "server", "-daemon") == -1) {
            perror("Erreur lors du redémarrage du serveur en mode daemon");
            exit(EXIT_FAILURE);
        }
    }

    fclose(log_file);  // Fermeture du fichier de logs
    exit(EXIT_SUCCESS);
}

/// @brief Gère les apples des signaux
/// @param signal 
void Gestion_signaux(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        Gestion_server(false, false);
    } else if (signal == SIGHUP) {
        Gestion_server(true, false);
    } else if (signal == SIGUSR1) {
        Gestion_server(false, true);
    }
}

int main(int argc, char *argv[]) {

    // Lancement du programme en arrière plan
    if (argc >= 2 && strcmp(argv[1], "-daemon") == 0) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Erreur lors de la création du processus fils");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            // Terminer le processus parent
            exit(EXIT_SUCCESS);
        }

        // Créer une nouvelle session et devenir le leader de la session
        if (setsid() < 0) {
            perror("Erreur lors de la création de la nouvelle session");
            exit(EXIT_FAILURE);
        }
    }
    
    // Ouvrerture du fichier des logs
    log_file = fopen("logs.txt", "a+");
    if (log_file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de journalisation");
        exit(EXIT_FAILURE);
    }

    // Signaux
    signal(SIGUSR1, Gestion_signaux);
    signal(SIGHUP, Gestion_signaux);
    signal(SIGTERM, Gestion_signaux);
    signal(SIGINT, Gestion_signaux);

    // Création de la soket serveur
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Erreur lors de la création de la socket du serveur");
        exit(EXIT_FAILURE);
    }
    else{
        log_message("info", "Server ouvert");
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
        perror("en écoute");
        exit(EXIT_FAILURE);
    }

    printf("Le serveur est prêt à recevoir des connexions...\n");

    // Initialisation des sockets des clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);

        // Acceptation de nouveaux client
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Ajout de la soket d'un nouveau client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == -1) {
                client_sockets[i] = client_socket;
                break;
            }
        }

        // Create a new thread to handle the client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, &client_socket) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    fclose(log_file);  // Fermeture du fichier de logs
    return 0;
}


