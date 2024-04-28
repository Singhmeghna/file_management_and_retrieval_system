#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8891
#define MAX_COMMAND_LENGTH 1024
#define MAX_RESPONSE_LENGTH 4096
#define MAX_FILENAME_LENGTH 256

// Function prototypes
void handle_directory_listing(int client_socket, const char* sort_type);
void handle_file_info(int client_socket, const char* filename);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Socket bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == -1) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Mirror2 started on port %d\n", PORT);

    // Accept incoming connections and handle requests
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size)) != -1) {
        char command[MAX_COMMAND_LENGTH];
        recv(client_socket, command, sizeof(command), 0);

        // Handle different types of commands
        if (strncmp(command, "dirlist -a", 10) == 0) {
            handle_directory_listing(client_socket, "alpha");
        } else if (strncmp(command, "dirlist -t", 10) == 0) {
            handle_directory_listing(client_socket, "time");
        } else if (strncmp(command, "w24fn ", 6) == 0) {
            char* filename = command + 6;
            handle_file_info(client_socket, filename);
        }

        close(client_socket);
    }

    close(server_socket);
    return 0;
}

// Function to handle directory listing
void handle_directory_listing(int client_socket, const char* sort_type) {
    struct dirent **namelist;
    int n;
    char response[MAX_RESPONSE_LENGTH] = "";

    // Scan directory and get list of entries
    if (strcmp(sort_type, "alpha") == 0) {
        n = scandir(".", &namelist, NULL, alphasort);
    } else {
        n = scandir(".", &namelist, NULL, NULL); // Implement your own sorting function for 'time'
    }

    // Process list of entries
    if (n < 0) {
        perror("scandir failed");
        strcpy(response, "Failed to list directories\n");
    } else {
        for (int i = 0; i < n; i++) {
            if (namelist[i]->d_type == DT_DIR) {
                strcat(response, namelist[i]->d_name);
                strcat(response, "\n");
            }
            free(namelist[i]);
        }
        free(namelist);
    }
    send(client_socket, response, strlen(response), 0);
}

// Function to handle file information request
void handle_file_info(int client_socket, const char* filename) {
    struct stat file_stat;
    char response[MAX_RESPONSE_LENGTH] = "";
    char file_path[MAX_FILENAME_LENGTH];

    // Construct file path
    snprintf(file_path, MAX_FILENAME_LENGTH, "./%s", filename);

    // Get file information
    if (stat(file_path, &file_stat) == 0) {
        // Format and send file information
        sprintf(response, "File: %s\nSize: %ld bytes\nDate Created: %sPermissions: %o\n",
                filename, file_stat.st_size, ctime(&file_stat.st_ctime), file_stat.st_mode);
    } else {
        strcpy(response, "File not found\n");
    }
    send(client_socket, response, strlen(response), 0);
}
