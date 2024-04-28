#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8889
#define SERVER_IP "127.0.0.1" // server's IP address
#define MAX_COMMAND_LENGTH 1024
#define MAX_RESPONSE_LENGTH 4096

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char command[MAX_COMMAND_LENGTH];
    char response[MAX_RESPONSE_LENGTH];

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    while (1) {
        printf("Enter command (or 'quitc' to quit): ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;  // Remove newline character

        // Send command to server
        if (send(client_socket, command, strlen(command), 0) == -1) {
            perror("Error sending command to server");
            continue;
        }

        // Receive response from server
        if (recv(client_socket, response, sizeof(response), 0) == -1) {
            perror("Error receiving response from server");
            continue;
        }

        // Print response
        printf("Response from server:\n%s\n", response);

        // Check for quit command
        if (strcmp(command, "quitc") == 0) {
            break;
        }
    }

    // Close socket
    close(client_socket);

    return 0;
}
