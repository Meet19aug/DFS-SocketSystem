#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 10300
#define BUFFER_SIZE 1024
#define BASE_PATH "~/smain/"

int validate_command(char *command);
void send_command(int socket);
void handle_ufile(int socket, char *filename, char *destination_path);
void handle_dfile(int socket, char *filename);
void handle_rmfile(int socket, char *filename);
void handle_dtar(int socket, char *filetype);
void handle_display(int socket, char *pathname);
char* extract_filename(const char* path);
char *generate_tar_filename(const char *filetype);
void process_file_list(char *file_list) ;


int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.2", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    while (1) {
        send_command(client_socket);
    }

    close(client_socket);
    return 0;
}

void send_command(int socket) {
    char buffer[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];
    char arg1[BUFFER_SIZE];
    char arg2[BUFFER_SIZE];

    printf("Enter command: ");
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

    int arg_count = sscanf(buffer, "%s %s %s", cmd, arg1, arg2);

    if (validate_command(buffer)) {
        if (strcmp(cmd, "ufile") == 0) {
            handle_ufile(socket, arg1, arg2);
        } else if (strcmp(cmd, "dfile") == 0) {
            handle_dfile(socket, arg1);
        } else if (strcmp(cmd, "rmfile") == 0) {
            handle_rmfile(socket, arg1);
        } else if (strcmp(cmd, "dtar") == 0) {
            handle_dtar(socket, arg1);
        } else if (strcmp(cmd, "display") == 0) {
            handle_display(socket, arg1);
        }
    } else {
        printf("Invalid command format or parameters. Please try again.\n");
    }
}

int validate_command(char *command) {
    char cmd[BUFFER_SIZE];
    char arg1[BUFFER_SIZE];
    char arg2[BUFFER_SIZE];
    int arg_count;

    // Split command into its parts
    arg_count = sscanf(command, "%s %s %s", cmd, arg1, arg2);

    if (arg_count < 2) {
        // At least one argument should be present
        return 0;
    }

    // 1. Validate "ufile filename destination_path"
    if (strcmp(cmd, "ufile") == 0) {
        if (arg_count != 3) {
            printf("Error: ufile requires a filename and a destination path.\n");
            return 0;
        }
        char *extension = strrchr(arg1, '.');
        if (extension && (strcmp(extension, ".c") == 0 || strcmp(extension, ".pdf") == 0 || strcmp(extension, ".txt") == 0)) {
            struct stat file_stat;
            if (stat(arg1, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                return 1; // Valid command
            } else {
                printf("Error: File does not exist in the current directory.\n");
                return 0;
            }
        } else {
            printf("Error: Filename must have a .c, .pdf, or .txt extension.\n");
            return 0;
        }
    }

    // 2. Validate "dfile filename"
    else if (strcmp(cmd, "dfile") == 0) {
        if (arg_count != 2) {
            printf("Error: dfile requires a filename.\n");
            return 0;
        }
        char *extension = strrchr(arg1, '.');
        if (extension && (strcmp(extension, ".c") == 0 || strcmp(extension, ".pdf") == 0 || strcmp(extension, ".txt") == 0)) {
            return 1; // Valid command
        } else {
            printf("Error: Filename must have a .c, .pdf, or .txt extension.\n");
            return 0;
        }
    }

    // 3. Validate "rmfile filename"
    else if (strcmp(cmd, "rmfile") == 0) {
        if (arg_count != 2) {
            printf("Error: rmfile requires a filename.\n");
            return 0;
        }
        char *extension = strrchr(arg1, '.');
        if (extension && (strcmp(extension, ".c") == 0 || strcmp(extension, ".pdf") == 0 || strcmp(extension, ".txt") == 0)) {
            return 1; // Valid command
        } else {
            printf("Error: Filename must have a .c, .pdf, or .txt extension.\n");
            return 0;
        }
    }

    // 4. Validate "dtar filetype"
    else if (strcmp(cmd, "dtar") == 0) {
        if (arg_count != 2) {
            printf("Error: dtar requires a filetype.\n");
            return 0;
        }
        if (strcmp(arg1, ".c") == 0 || strcmp(arg1, ".pdf") == 0 || strcmp(arg1, ".txt") == 0) {
            return 1; // Valid command
        } else {
            printf("Error: Filetype must be .c, .pdf, or .txt.\n");
            return 0;
        }
    }

    // 5. Validate "display pathname"
    else if (strcmp(cmd, "display") == 0) {
        if (arg_count != 2) {
            printf("Error: display requires a pathname.\n");
            return 0;
        }
        return 1; // Valid command
    }

    // If the command does not match any of the above
    printf("Error: Unknown command.\n");
    return 0;
}

void handle_ufile(int socket, char *filename, char *destination_path) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Unable to open file %s\n", filename);
        return;
    }

    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "ufile %s %s", filename, destination_path);
    write(socket, command, strlen(command));

    // Send file content in binary form
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(socket, buffer, bytes_read);
    }

    fclose(file);
    printf("File closed!!!!\n");

    // Send end-of-file marker
    char eof_marker[] = "EOF";
    write(socket, eof_marker, strlen(eof_marker));

    // Wait for server acknowledgment
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t recv_bytes = read(socket, buffer, BUFFER_SIZE);
    if (recv_bytes > 0) {
        printf("Server response: %s", buffer);
    } else {
        printf("No response received from server.\n");
    }

    printf("File %s sent to server.\n", filename);
}

void handle_dfile(int socket, char *filenamewithpath) {
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "dfile %s", filenamewithpath);
    write(socket, command, strlen(command));
    
    char *filename = extract_filename(filenamewithpath);
    //printf("Filename is : %s\n", filename);

    // Receive and save file
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Error: Unable to create file %s\n", filename);
        free(filename);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int eof_received = 0;
    // printf("waiting for reading the file content now...\n");
    while (!eof_received && (bytes_received = read(socket, buffer, BUFFER_SIZE)) > 0) {
        // Check for EOF marker
        //printf("Buffer is :%s -- %d\n",buffer,bytes_received);
        if (bytes_received >= 3 && memcmp(buffer + bytes_received - 3, "EOF", 3) == 0) {
            bytes_received -= 3;  // Adjust to not write EOF marker to file
            eof_received = 1;
        }
        else if (bytes_received >= 4 && strncmp(buffer, "NACK", 4) == 0 ) {
            free(filename);
            return;
        }else if (bytes_received > 0) {
            printf("filename Write buffer is : %s\n",buffer);
            fwrite(buffer, 1, bytes_received, file);
        }
    }
    // printf("file content read successfully.\n");

    fclose(file);
    
    if (eof_received) {
        printf("File %s received and saved successfully.\n", filename);
    } else {
        printf("Error: File transfer incomplete for %s\n", filename);
    }

    free(filename);
}

void handle_rmfile(int socket, char *filename) {
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "rmfile %s", filename);
    write(socket, command, strlen(command));

    // Wait for server response
    char response[BUFFER_SIZE];
    read(socket, response, BUFFER_SIZE);
    printf("Server response: %s\n", response);
}

void handle_dtar(int socket, char *filetype) {
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "dtar %s", filetype);
    write(socket, command, strlen(command));

    // Receive the server response (NACK or ACK with filename)
    char response[BUFFER_SIZE];
    ssize_t bytes_received = read(socket, response, sizeof(response) - 1);
    if (bytes_received < 0) {
        perror("Socket read error");
        return;
    }
    response[bytes_received] = '\0';  // Null-terminate the response

    // Check if the server returned an error
    if (strncmp(response, "NACK", 4) == 0) {
        printf("Server error: %s\n", response);
        return;
    }

    // Extract the tar filename from the ACK response
    char tar_filename[BUFFER_SIZE];
    if (sscanf(response, "ACK %s", tar_filename) != 1) {
        printf("Invalid server response is %s.\n",response);
        return;
    }

    // Send acknowledgment to start file transfer
    write(socket, "READY", 5);

    long file_size;
    bytes_received = read(socket, &file_size, sizeof(long));
    printf("byte recived filesize = %ld.\n",file_size);


    if (bytes_received != sizeof(long)) {
        printf("Error receiving file size\n");
        return;
    }
    // Convert the long file size to a string
    printf("file size is : %ld.\n",file_size);

    printf("Receiving tar file: %s (Size: %ld bytes)\n", tar_filename, file_size);

    // Open file for writing
    FILE *file = fopen(tar_filename, "wb");
    if (file == NULL) {
        printf("Error: Unable to create tar file %s\n", tar_filename);
        return;
    }

    // Receive and save tar file
    long total_received = 0;
    memset(response, 0, BUFFER_SIZE);
    while (total_received < (int)file_size) {
        bytes_received = read(socket, response, BUFFER_SIZE);
        if (bytes_received <= 0) {
            break;
        }
        fwrite(response, 1, bytes_received, file);
        total_received += bytes_received;
    }

    fclose(file);

    if (total_received == file_size) {
        printf("Tar file %s received and saved (%ld bytes).\n", tar_filename, total_received);
    } else {
        printf("Error: Incomplete file transfer. Received %ld of %ld bytes.\n", total_received, file_size);
    }
}

void handle_display(int socket, char *pathname) {
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "display %s", pathname);
    write(socket, command, strlen(command));

    // Receive and process server response
    char response[BUFFER_SIZE * 10];  // Increased buffer size
    ssize_t bytes_received = 0;
    size_t total_received = 0;

    while ((bytes_received = read(socket, response + total_received, BUFFER_SIZE - 1)) > 0) {
        total_received += bytes_received;
        if (total_received > BUFFER_SIZE * 10 - BUFFER_SIZE) {
            break;  // Avoid buffer overflow
        }
        if (strstr(response, "ACK: Display complete") || strstr(response, "NACK:")) {
            break;  // End of transmission
        }
    }

    response[total_received] = '\0';  // Null-terminate the response

    if (strncmp(response, "NACK:", 5) == 0) {
        printf("Server error: %s\n", response);
    } else {
        printf("Files found:\n");
        process_file_list(response);
    }
}

// ------------ Helper function ---------------- 

char* extract_filename(const char* path) {
    // Find the last occurrence of '/'
    const char* last_slash = strrchr(path, '/');
    
    if (last_slash != NULL) {
        // If a slash was found, return the part after it
        return strdup(last_slash + 1);
    } else {
        // If no slash was found, return a copy of the entire path
        return strdup(path);
    }
}

char *generate_tar_filename(const char *filetype) {
    char *output_filename = malloc(100);
    if (!output_filename) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[20];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y-%H-%M-%S", timeinfo);
    snprintf(output_filename, 100, "tar-%s-%s", filetype + 1, timestamp);

    return output_filename;
}

void process_file_list(char *file_list) {
    char *line = strtok(file_list, "\n");
    while (line != NULL) {
        if (strncmp(line, "ACK:", 4) != 0) {  // Ignore ACK message
            char *filename = strrchr(line, '/');
            if (filename) {
                printf("%s\n", filename + 1);  // Print only the filename
            } else {
                printf("%s\n", line);  // Print the whole line if no '/' found
            }
        }
        line = strtok(NULL, "\n");
    }
}
