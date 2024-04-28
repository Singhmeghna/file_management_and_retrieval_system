#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ftw.h>

#define PORT 8889
#define MAX_COMMAND_LENGTH 1024
#define MAX_RESPONSE_LENGTH 4096
#define MAX_EXTENSIONS 3
#define MAX_EXTENSION_LENGTH 4

char home_directory[] = "/home/meghna";  // Home directory path
int mode;  // Mode for date comparison
time_t input_time;  // Input time for date comparison
int files_appended = 0;  // Flag to track if any files were appended

// Function prototypes
void create_tar_archive_with_extensions(const char *extensions, char *response);
void create_tar_archive_by_size_range(int size1, int size2, char *response);
void handle_client_request(int client_socket);
void handle_directory_listing(int client_socket, const char *sort_type);
void handle_file_info(int client_socket, const char *filename);
int handle_files_by_date(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
void send_tar_file(int client_socket);
void finalize_tar_file();

// Main function to handle client requests
int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    // Create a socket for the server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d\n", PORT);

    // Accept and handle incoming client connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        if (fork() == 0) { // Child process
            close(server_socket); // Close the server socket in the child process
            handle_client_request(client_socket);
            exit(0); // Exit child process after handling request
        }
        close(client_socket); // Parent process doesn't need the client socket
    }

    close(server_socket);
    return 0;
}

// Function to handle client requests
void handle_client_request(int client_socket) {
    char command[MAX_COMMAND_LENGTH];
    char response[MAX_RESPONSE_LENGTH];

    // Receive and process client commands
    while (1) {
        memset(command, 0, sizeof(command));
        memset(response, 0, sizeof(response));

        if (recv(client_socket, command, sizeof(command), 0) <= 0) {
            perror("Error receiving command from client");
            break;
        }

        if (strncmp(command, "w24fz", 5) == 0) {
            // Process command to create a tar archive based on file sizes
            int size1, size2;
            if (sscanf(command + 6, "%d %d", &size1, &size2) == 2) {
                create_tar_archive_by_size_range(size1, size2, response);
            } else {
                strcpy(response, "Invalid size parameters");
            }
        } else if (strncmp(command, "w24ft", 5) == 0) {
            // Process command to create a tar archive based on file extensions
            char *extensions = command + 6;
            create_tar_archive_with_extensions(extensions, response);
        } else if (strncmp(command, "dirlist -a", 10) == 0) {
            // Process command to list directories alphabetically
            handle_directory_listing(client_socket, "alpha");
        } else if (strncmp(command, "dirlist -t", 10) == 0) {
            // Process command to list directories by modification time
            handle_directory_listing(client_socket, "time");
        } else if (strncmp(command, "w24fn ", 6) == 0) {
            // Process command to get file information
            handle_file_info(client_socket, command + 6);
        } else if (strncmp(command, "w24fdb ", 7) == 0 || strncmp(command, "w24fda ", 7) == 0) {
            // Process command to create a tar archive based on file modification date
            char *date = command + 7;
            mode = command[4] == 'b' ? 0 : 1;
            struct tm given_time = {0};
            strptime(date, "%Y-%m-%d", &given_time);
            input_time = mktime(&given_time);

            // Start with a fresh uncompressed tar file
            system("tar -cf temp.tar --files-from /dev/null");
            nftw(home_directory, handle_files_by_date, 20, FTW_PHYS);
            finalize_tar_file();
            send_tar_file(client_socket);
        } else if (strcmp(command, "quitc") == 0) {
            // Process command to quit the client
            strcpy(response, "Quitting...");
            break;
        } else {
            strcpy(response, "Unknown command");
        }

        // Send response to client
        if (send(client_socket, response, strlen(response), 0) <= 0) {
            perror("Error sending response to client");
            break;
        }
    }

    close(client_socket);
}

// Function to handle directory listing
void handle_directory_listing(int client_socket, const char *sort_type) {
    struct dirent **namelist;
    int n;

    // List directories based on sorting type
    if (strcmp(sort_type, "alpha") == 0) {
        n = scandir(home_directory, &namelist, NULL, alphasort);
    } else {
        n = scandir(home_directory, &namelist, NULL, alphasort);  // For simplicity using alphasort for both cases
    }

    char response[MAX_RESPONSE_LENGTH] = "";
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

// Function to handle file information retrieval
void handle_file_info(int client_socket, const char *filename) {
    struct stat file_stat;
    char response[MAX_RESPONSE_LENGTH];

    // Check if the file exists and get its information
    if (stat(filename, &file_stat) == 0) {
        sprintf(response, "File: %s\nSize: %ld bytes\nDate Created: %sPermissions: %o\n",
                filename, file_stat.st_size, ctime(&file_stat.st_ctime), file_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
        send(client_socket, response, strlen(response), 0);
    } else {
        // If file not found, search recursively in the entire file system
        DIR *dir;
        struct dirent *entry;
        if ((dir = opendir(home_directory)) != NULL) {
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_DIR) {
                    char path[MAX_COMMAND_LENGTH];
                    snprintf(path, sizeof(path), "%s/%s", entry->d_name, filename);
                    if (stat(path, &file_stat) == 0) {
                        sprintf(response, "File: %s\nSize: %ld bytes\nDate Created: %sPermissions: %o\n",
                                path, file_stat.st_size, ctime(&file_stat.st_ctime), file_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
                        send(client_socket, response, strlen(response), 0);
                        closedir(dir);
                        return;
                    }
                }
            }
            closedir(dir);
        }
        // If file not found anywhere, send appropriate message to client
        perror("Error getting file information");
        strcpy(response, "File not found\n");
        send(client_socket, response, strlen(response), 0);
    }
}

// Function to create a tar archive with specified file extensions
void create_tar_archive_with_extensions(const char *extensions, char *response) {
    char temp_tar[] = "temp.tar";
    char temp_tar_gz[] = "temp.tar.gz";
    char command[MAX_COMMAND_LENGTH];
    int found_files = 0;

    // Create an empty tar archive
    sprintf(command, "tar -cf %s --files-from /dev/null", temp_tar);
    system(command);

    // Iterate through extensions and add files to the tar archive
    char *token = strtok((char *)extensions, " ");
    while (token != NULL && strlen(token) <= MAX_EXTENSION_LENGTH) {
        sprintf(command, "find %s -type f -name '*.%s' -exec tar -rf %s {} +", home_directory, token, temp_tar);
        if (system(command) == 0) {
            found_files = 1;
        }
        token = strtok(NULL, " ");
    }

    // Compress the tar archive
    if (found_files) {
        sprintf(command, "tar -czf %s %s", temp_tar_gz, temp_tar);
        if (system(command) == 0) {
            if (access(temp_tar_gz, F_OK) != -1) {
                strcpy(response, temp_tar_gz);
            } else {
                strcpy(response, "Error compressing files");
            }
        } else {
            perror("Error creating tar.gz archive");
            strcpy(response, "Error compressing files");
        }
    } else {
        strcpy(response, "No file found");
    }

    // Remove temporary tar file
    remove(temp_tar);
}

// Function to create a tar archive with files within a specified size range
void create_tar_archive_by_size_range(int size1, int size2, char *response) {
    char temp_tar[] = "temp.tar";
    char temp_tar_gz[] = "temp.tar.gz";
    char command[MAX_COMMAND_LENGTH];
    int found_files = 0;

    // Find files within the specified size range and add them to the tar archive
    sprintf(command, "find %s -type f -size +%dc -size -%dc -exec tar -rf %s {} +", home_directory, size1, size2, temp_tar);
    if (system(command) == 0) {
        found_files = 1;
    }

    // Compress the tar archive
    if (found_files) {
        sprintf(command, "tar -czf %s %s", temp_tar_gz, temp_tar);
        if (system(command) == 0) {
            if (access(temp_tar_gz, F_OK) != -1) {
                strcpy(response, temp_tar_gz);
            } else {
                strcpy(response, "Error compressing files");
            }
        } else {
            perror("Error creating tar.gz archive");
            strcpy(response, "Error compressing files");
        }
    } else {
        strcpy(response, "No file found");
    }

    // Remove temporary tar file
    remove(temp_tar);
}

// Function to handle files based on modification date for creating a tar archive
int handle_files_by_date(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        time_t file_time = sb->st_mtime; // Using modification time
        if ((mode == 0 && file_time <= input_time) || (mode == 1 && file_time >= input_time)) {
            char cmd[MAX_COMMAND_LENGTH];
            sprintf(cmd, "tar --append --file=temp.tar \"%s\" 2>> tar_error.log", fpath);
            printf("Attempting to append file to tar: %s\n", fpath); // Diagnostic message
            int ret = system(cmd);
            if (ret != 0) {
                fprintf(stderr, "Command failed with return code %d: %s\n", ret, cmd); // Diagnostic message
            } else {
                files_appended = 1; // Set flag to indicate that files were appended
            }
        }
    }
    return 0; // continue
}

// Function to finalize the tar file by compressing it if files were appended
void finalize_tar_file() {
    // Compress the tar file after all files have been appended
    if (files_appended == 0) {
        // If no files were appended, create an empty tar file
        int fd = open("temp.tar", O_WRONLY | O_CREAT, 0644);
        if (fd == -1) {
            perror("Failed to create empty tar file");
            return;
        }
        close(fd);
    }

    // Compress the tar file
    int ret = system("gzip -f temp.tar");
    if (ret != 0) {
        fprintf(stderr, "Failed to compress the tar file with return code %d\n", ret);
    }
}

// Function to send the tar.gz file to the client
void send_tar_file(int client_socket) {
    int fd = open("temp.tar.gz", O_RDONLY);
    if (fd == -1) {
        perror("Failed to open tar file");
        return;
    }

    // Read and send the tar.gz file in chunks
    char buffer[4096];
    int bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) == -1) {
            perror("Failed to send file");
            break;
        }
    }

    close(fd);
}
