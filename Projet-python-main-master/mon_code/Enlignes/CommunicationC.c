#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 23456  // Port UDP pour les messages du jeu
#define IPC_PORT 12347  // Port TCP/IPC pour les commandes du client Python
#define BROADCAST_IP "192.168.1.255"  // Adresse IP de diffusion

typedef struct {
    SOCKET ipc_socket;  // Socket TCP pour IPC
    SOCKET udp_socket;  // Socket UDP pour les messages de jeu
    SOCKET python_client_socket;  // Socket TCP pour le client Python
} Sockets;

// Structure pour les messages de jeu
struct GameData {
    char message[256];
};

// Fonction pour envoyer un message au client Python
void send_to_python_client(Sockets *sockets, const char *message) {
    if (sockets->python_client_socket != INVALID_SOCKET) {
        send(sockets->python_client_socket, message, strlen(message), 0);
    }
}

DWORD WINAPI receive_messages(LPVOID lpParam) {
    Sockets *sockets = (Sockets *)lpParam;
    char buffer[1024];
    struct sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    printf("Écoute des messages UDP sur le port %d\n", PORT);

    while (1) {
        int recvBytes = recvfrom(sockets->udp_socket, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (recvBytes == SOCKET_ERROR) {
            printf("recvfrom échoué avec le code d'erreur: %d\n", WSAGetLastError());
            continue;
        }

        buffer[recvBytes] = '\0';  // Null-terminate the string
        printf("Message UDP reçu: %s\n", buffer);

        // Envoyer le message reçu au client Python
        send_to_python_client(sockets, buffer);
    }

    return 0;
}

// Thread pour écouter les commandes IPC et les rediriger via UDP
DWORD WINAPI listen_for_ipc_commands(LPVOID lpParam) {
    Sockets *sockets = (Sockets *)lpParam;
    SOCKET client_sock;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[1024];

    while (1) {
        client_sock = accept(sockets->ipc_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            printf("Acceptation échouée avec le code d'erreur: %d\n", WSAGetLastError());
            continue;
        }

        sockets->python_client_socket = client_sock;  // Enregistrer le client Python

        while (1) {
            int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read <= 0) {
                break;
            }

            buffer[bytes_read] = '\0';  // Null-terminate the string
            printf("Commande IPC reçue : %s\n", buffer);

            // Répondre avec le message reçu
            send(client_sock, buffer, bytes_read, 0);

            // Copier le message reçu dans la structure GameData
            struct GameData gameData;
            strcpy(gameData.message, buffer);  // Copier le message

            // Envoyer via le socket UDP
            struct sockaddr_in broadcastAddr;
            memset(&broadcastAddr, 0, sizeof(broadcastAddr));
            broadcastAddr.sin_family = AF_INET;
            broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
            broadcastAddr.sin_port = htons(PORT);

            if (sendto(sockets->udp_socket, (char *)&gameData, sizeof(gameData), 0, 
                       (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr)) == SOCKET_ERROR) {
                printf("Envoi UDP échoué avec le code d'erreur: %d\n", WSAGetLastError());
            }
        }

        closesocket(client_sock);
        sockets->python_client_socket = INVALID_SOCKET;  // Réinitialiser le client Python
    }

    return 0;
}

// Thread pour envoyer des messages via UDP en broadcast
DWORD WINAPI send_messages(LPVOID lpParam) {
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;
    char message[1024];
    int broadcast = 1; // Enable broadcast

    // Create a UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        printf("socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Set the socket to allow broadcasts
    if (setsockopt(clientSocket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, sizeof(broadcast)) == SOCKET_ERROR) {
        printf("setsockopt failed with error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    serverAddr.sin_port = htons(PORT);

    while (1) {
        printf("Enter a message to broadcast: ");
        fgets(message, sizeof(message), stdin);

        // Send message to broadcast IP
        int sentBytes = sendto(clientSocket, message, strlen(message), 0,
                               (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        if (sentBytes == SOCKET_ERROR) {
            printf("sendto failed with error: %d\n", WSAGetLastError());
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        printf("Message broadcast to %s:%d\n", BROADCAST_IP, PORT);
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup échouée avec le code d'erreur: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Créer le socket TCP pour IPC
    SOCKET ipc_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ipc_sockfd == INVALID_SOCKET) {
        printf("Création du socket IPC échouée avec le code d'erreur: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Configurer le socket IPC
    struct sockaddr_in ipc_addr;
    memset(&ipc_addr, 0, sizeof(ipc_addr));
    ipc_addr.sin_family = AF_INET;
    ipc_addr.sin_port = htons(IPC_PORT);
    ipc_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Lier et écouter
    if (bind(ipc_sockfd, (struct sockaddr *)&ipc_addr, sizeof(ipc_addr)) == SOCKET_ERROR) {
        printf("Liaison du socket IPC échouée avec le code d'erreur: %d\n", WSAGetLastError());
        closesocket(ipc_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (listen(ipc_sockfd, 1) == SOCKET_ERROR) {
        printf("Écoute échouée avec le code d'erreur: %d\n", WSAGetLastError());
        closesocket(ipc_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Créer le socket UDP
    SOCKET udp_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sockfd == INVALID_SOCKET) {
        printf("Création du socket UDP échouée avec le code d'erreur: %d\n", WSAGetLastError());
        closesocket(ipc_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(PORT);
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Lier le socket UDP
    if (bind(udp_sockfd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) == SOCKET_ERROR) {
        printf("Liaison du socket UDP échouée avec le code d'erreur: %d\n", WSAGetLastError());
        closesocket(ipc_sockfd);
        closesocket(udp_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Créer la structure Sockets
    Sockets sockets;
    sockets.ipc_socket = ipc_sockfd;
    sockets.udp_socket = udp_sockfd;
    sockets.python_client_socket = INVALID_SOCKET;

    // Démarrer le thread IPC
    HANDLE ipc_listen_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)listen_for_ipc_commands, &sockets, 0, NULL);
    if (ipc_listen_thread == NULL) {
        printf("Création du thread IPC échouée\n");
        closesocket(ipc_sockfd);
        closesocket(udp_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Démarrer le thread de réception des messages UDP
    HANDLE receive_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receive_messages, &sockets, 0, NULL);
    if (receive_thread == NULL) {
        printf("Erreur lors de la création du thread de réception\n");
        closesocket(ipc_sockfd);
        closesocket(udp_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Démarrer le thread d'envoi de messages
    HANDLE send_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)send_messages, NULL, 0, NULL);
    if (send_thread == NULL) {
        printf("Création du thread d'envoi échouée\n");
        closesocket(ipc_sockfd);
        closesocket(udp_sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Boucle principale pour maintenir le programme actif
    while (1) {
        Sleep(5000);  // Garder le programme actif
    }

    // Nettoyage (si la boucle principale se termine)
    CloseHandle(ipc_listen_thread);
    CloseHandle(send_thread);
    CloseHandle(receive_thread);
    closesocket(ipc_sockfd);
    closesocket(udp_sockfd);
    WSACleanup();

    return 0;
}
