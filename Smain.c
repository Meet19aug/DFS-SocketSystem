#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 10300
#define BUFFER_SIZE 1024
#define BASE_PATH "~/smain/"
#define BASE_DIR "smain"

#define STXT_SERVER_PORT 10450
#define STXT_SERVER_IP "127.0.0.1" // Change this to the actual IP if necessary

#define SPDF_SERVER_PORT 10453
#define SPDF_SERVER_IP "127.0.0.1" // Change this to the actual IP if necessary

void send_to_stxt(char *filename, char *content);
void prcclient(int client_socket);
int validate_command(char *command);
void handle_ufile(int client_socket, char *filename, char *dest_path);
void handle_dfile(int client_socket, char *filename);
void handle_rmfile(int client_socket, char *filename);
void handle_dtar(int client_socket, char *filetype);
void handle_display(int client_socket, char *pathname);
void remove_smain_prefix(char *input);
char *trim_whitespace(char *str);
int mkdir_recursive(const char *path, mode_t mode);
char *generate_tar_filename(const char *filetype);
void list_files_recursive(const char *path, char *file_list, size_t offset);
int transfer_to_stxt(int client_socket, const char *full_path, const char *filename, const char *dest_path);
int transfer_to_spdf(int client_socket, const char *full_path, const char *filename, const char *dest_path);
int connect_to_server(const char *server_ip, int server_port);
int delete_empty_dirs(const char *path);
int is_directory_empty(const char *path);
void handle_dtar_special(int client_socket,char * filetype,char *tar_file_name, int istext);

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    pid_t child_pid;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1)
    {
        addr_len = sizeof(client_addr);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("Accepted new client connection\n");

        // Fork a child process to handle the client
        if ((child_pid = fork()) == 0)
        {
            close(server_socket);
            prcclient(client_socket);
            close(client_socket);
            exit(0);
        }
        else if (child_pid > 0)
        {
            close(client_socket);
            waitpid(-1, NULL, WNOHANG); // Clean up zombie processes
        }
        else
        {
            perror("Fork failed");
        }
    }

    close(server_socket);
    return 0;
}

void prcclient(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int command_valid;
    char cmd[BUFFER_SIZE], arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        if (read(client_socket, buffer, BUFFER_SIZE) <= 0)
        {
            perror("Read failed");
            break;
        }

        printf("Received command: %s\n", buffer);
        command_valid = sscanf(buffer, "%s %s %s", cmd, arg1, arg2);

        if (command_valid < 2)
        {
            write(client_socket, "NACK: Invalid Command\n", 22);
            continue;
        }
        if (strcmp(cmd, "ufile") == 0)
        {
            remove_smain_prefix(arg2);

            handle_ufile(client_socket, arg1, arg2);
        }
        else if (strcmp(cmd, "dfile") == 0)
        {
            remove_smain_prefix(arg1);
            handle_dfile(client_socket, arg1);  
            
        }
        else if (strcmp(cmd, "rmfile") == 0)
        {
            remove_smain_prefix(arg1);
            handle_rmfile(client_socket, arg1);
        }
        else if (strcmp(cmd, "dtar") == 0)
        {
            handle_dtar(client_socket, arg1);
        }
        else if (strcmp(cmd, "display") == 0)
        {
            remove_smain_prefix(arg1);
            printf("Arg is : %s.\n", arg1);
            handle_display(client_socket, arg1);
        }
        else
        {
            write(client_socket, "NACK: Invalid Command\n", 22);
        }
    }
}

// Modify the existing handle_ufile function
void handle_ufile(int client_socket, char *filename, char *dest_path) {
    char full_path[BUFFER_SIZE];
    int file_fd;
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];
    struct stat st = {0};

    snprintf(full_path, BUFFER_SIZE, "%s/%s", BASE_DIR, dest_path);

    if (stat(full_path, &st) == -1) {
        if (mkdir_recursive(full_path, 0700) != 0) {
            perror("Failed to create directory");
            write(client_socket, "NACK: Unable to create directory on server\n", 44);
            return;
        }
    }

    snprintf(full_path, BUFFER_SIZE, "%s%s/%s", BASE_DIR, dest_path, filename);
    file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("File open error");
        write(client_socket, "NACK: Unable to create file on server\n", 38);
        return;
    }

    while (1) {
        bytes_read = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            break;
        }

        if (bytes_read >= 3 && memcmp(buffer + bytes_read - 3, "EOF", 3) == 0) {
            bytes_read -= 3;
            write(file_fd, buffer, bytes_read);
            break;
        }
        write(file_fd, buffer, bytes_read);
    }

    close(file_fd);
    printf("filename is :%s.\n",filename);
    printf("full_path is : %s.\n",full_path );
    printf("dest_path is : %s.\n",dest_path );

    // Only send to Stxt if it's a .txt file
    if (strstr(filename, ".txt") != NULL) {
        if (transfer_to_stxt(client_socket, full_path, filename, dest_path) != 0) {
            // If transfer_to_stxt fails, it will have already sent an NACK to the client
            return;
        }
    }else if(strstr(filename, ".pdf") != NULL){
        if (transfer_to_spdf(client_socket, full_path, filename, dest_path) != 0) {
            // If transfer_to_stxt fails, it will have already sent an NACK to the client
            return;
        }
    }else{
        write(client_socket, "ACK: Upload successful\n", 24);
    }
    
}
void handle_dfile(int client_socket, char *filename)
{
    printf("handle Dfile....\n");
    char full_path[BUFFER_SIZE];
    int file_fd;
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];

    // Construct the full path
    snprintf(full_path, BUFFER_SIZE, "%s%s", BASE_DIR, filename);
    printf("Full Path is : %s\n", full_path);

    // Check if the file exists
    printf("Check if the file exists File name is : %s\n",filename);
    if (access(full_path, F_OK) == -1)
    {
        printf(".access(full_path, F_OK)\n");
        if(strstr(filename,".txt")){
            printf(".txt contians...\n");
            int server_socket_2 = -1;
            const char *server_ip_2;
            int server_port_2;
            server_ip_2 = STXT_SERVER_IP;
            server_port_2 = STXT_SERVER_PORT;
            // Connect to the appropriate server
            server_socket_2 = connect_to_server(server_ip_2, server_port_2);
            if (server_socket_2 < 0) {
                write(client_socket, "NACK: Unable to connect to file server\n", 39);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            // Send download request to the server
            snprintf(buffer, BUFFER_SIZE, "download %s", filename);
            printf("Send command is : %s.\n",buffer);
            if (send(server_socket_2, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                close(server_socket_2);
                write(client_socket, "NACK: Failed to send request to file server\n", 44);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            printf("Server connected successfully with stext and send command sucessful..\n");
            if (read(server_socket_2, buffer, BUFFER_SIZE) <= 0 || strncmp(buffer, "NACK", 4) == 0 ) {
                printf("inside if condition...%s\n",buffer);
                write(client_socket, buffer, strlen(buffer));
                close(server_socket_2);
                return;
            }
            printf("Smain got ACK : %s.\n",buffer);
            memset(buffer, 0, BUFFER_SIZE);
            // Receive file content from server and forward to client
            while ((bytes_read = recv(server_socket_2, buffer, BUFFER_SIZE, 0)) > 0) {
                if (strncmp(buffer + bytes_read - 3, "EOF", 3) == 0) {
                    bytes_read -= 3;
                    write(client_socket, buffer, bytes_read);
                    break;
                }
                write(client_socket, buffer, bytes_read);
            }
                    // Send EOF to client
            write(client_socket, "EOF", 3);

            // Send acknowledgement to server
            send(server_socket_2, "ACK", 3, 0);

            close(server_socket_2);
            printf("ACK: File transfer complete\n");

        }else if(strstr(filename,".pdf")){
            printf(".pdf contians...\n");
            int server_socket_2 = -1;
            const char *server_ip_2;
            int server_port_2;
            server_ip_2 = SPDF_SERVER_IP;
            server_port_2 = SPDF_SERVER_PORT;
            // Connect to the appropriate server
            server_socket_2 = connect_to_server(server_ip_2, server_port_2);
            if (server_socket_2 < 0) {
                write(client_socket, "NACK: Unable to connect to file server\n", 39);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            // Send download request to the server
            snprintf(buffer, BUFFER_SIZE, "download %s", filename);
            printf("Send command is : %s.\n",buffer);
            if (send(server_socket_2, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                close(server_socket_2);
                write(client_socket, "NACK: Failed to send request to file server\n", 44);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            printf("Server connected successfully with stext and send command sucessful..\n");
            if (read(server_socket_2, buffer, BUFFER_SIZE) <= 0 || strncmp(buffer, "NACK", 4) == 0 ) {
                printf("inside if condition...%s\n",buffer);
                write(client_socket, buffer, strlen(buffer));
                close(server_socket_2);
                return;
            }
            printf("Smain got ACK : %s.\n",buffer);
            memset(buffer, 0, BUFFER_SIZE);
            // Receive file content from server and forward to client
            while ((bytes_read = recv(server_socket_2, buffer, BUFFER_SIZE, 0)) > 0) {
                if (strncmp(buffer + bytes_read - 3, "EOF", 3) == 0) {
                    bytes_read -= 3;
                    write(client_socket, buffer, bytes_read);
                    break;
                }
                write(client_socket, buffer, bytes_read);
            }
                    // Send EOF to client
            write(client_socket, "EOF", 3);

            // Send acknowledgement to server
            send(server_socket_2, "ACK", 3, 0);

            close(server_socket_2);
            printf("ACK: File transfer complete\n");

        }else{
            write(client_socket, "NACK: File does not exist\n", 26);    
        }
        return;
    }

    // Open the file for reading
    file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0)
    {
        perror("File open error");
        write(client_socket, "NACK: Unable to open file on server\n", 36);
        return;
    }

    // Send the file content to the client
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0)
    {
        if (write(client_socket, buffer, bytes_read) < 0)
        {
            perror("Socket write error");
            close(file_fd);
            return;
        }
    }

    // Send EOF marker
    write(client_socket, "EOF", 3);

    close(file_fd);
    printf("ACK: File transfer complete\n");
}

void handle_rmfile(int client_socket, char *filename)
{
    char full_path[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    // Construct the full path
    snprintf(full_path, BUFFER_SIZE, "%s%s", BASE_DIR, filename);
    printf("Full Path is : %s.\n", full_path);

    // Check if the file exists
    if (access(full_path, F_OK) == -1)
    {
        if(strstr(full_path,".txt")){
            printf(".txt contians...\n");
            int server_socket_2 = -1;
            const char *server_ip_2;
            int server_port_2;
            server_ip_2 = STXT_SERVER_IP;
            server_port_2 = STXT_SERVER_PORT;
            // Connect to the appropriate server
            server_socket_2 = connect_to_server(server_ip_2, server_port_2);
            if (server_socket_2 < 0) {
                write(client_socket, "NACK: Unable to connect to file server\n", 39);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            // Send download request to the server
            snprintf(buffer, BUFFER_SIZE, "remove %s", filename);
            printf("Send command is : %s.\n",buffer);
            if (send(server_socket_2, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                close(server_socket_2);
                write(client_socket, "NACK: Failed to send request to file server\n", 44);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            printf("Server connected successfully with stext and send command sucessful..\n");
            if (read(server_socket_2, buffer, BUFFER_SIZE) <= 0 ) {
                printf("inside if condition...%s\n",buffer);
                write(client_socket, buffer, strlen(buffer));
                close(server_socket_2);
                return;
            }
            if(strncmp(buffer, "ACK", 3) == 0 ||  strncmp(buffer, "NACK", 4) == 0){
                write(client_socket, buffer, strlen(buffer));
                close(server_socket_2);
                return;
            }
        }else if(strstr(full_path,".pdf")){
            printf(".pdf contians...\n");
            int server_socket_2 = -1;
            const char *server_ip_2;
            int server_port_2;
            server_ip_2 = SPDF_SERVER_IP;
            server_port_2 = SPDF_SERVER_PORT;
            // Connect to the appropriate server
            server_socket_2 = connect_to_server(server_ip_2, server_port_2);
            if (server_socket_2 < 0) {
                write(client_socket, "NACK: Unable to connect to file server\n", 39);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            // Send download request to the server
            snprintf(buffer, BUFFER_SIZE, "remove %s", filename);
            printf("Send command is : %s.\n",buffer);
            if (send(server_socket_2, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                close(server_socket_2);
                write(client_socket, "NACK: Failed to send request to file server\n", 44);
                return;
            }
            memset(buffer, 0, BUFFER_SIZE);
            printf("Server connected successfully with stext and send command sucessful..\n");
            if (read(server_socket_2, buffer, BUFFER_SIZE) <= 0 ) {
                printf("inside if condition...%s\n",buffer);
                write(client_socket, buffer, strlen(buffer));
                close(server_socket_2);
                return;
            }
            if(strncmp(buffer, "ACK", 3) == 0 ||  strncmp(buffer, "NACK", 4) == 0){
                write(client_socket, buffer, strlen(buffer));
                close(server_socket_2);
                return;
            }
        }
        return;
    }

    printf("Before delete: full path is %s\n", full_path);
    // Delete the file
    if (remove(full_path) == 0)
    {
        write(client_socket, "ACK: File deleted successfully\n", 31);
        delete_empty_dirs(BASE_DIR);
    }
    else
    {
        perror("File delete error");
        write(client_socket, "NACK: File deletion failed\n", 28);
    }
    delete_empty_dirs(BASE_DIR);
}

void handle_dtar(int client_socket, char *filetype)
{

    char tar_command[BUFFER_SIZE];
    char *tar_file = generate_tar_filename(filetype);
    char tar_file_path[BUFFER_SIZE];

    if(strstr(filetype,".txt")){
        handle_dtar_special(client_socket,filetype,tar_file,1);
        return;
    }else if(strstr(filetype,".pdf")) {
        handle_dtar_special(client_socket,filetype,tar_file,0);
        return;
    }
    // Construct the full path to the tar file
    snprintf(tar_file_path, sizeof(tar_file_path), "%s/%s", BASE_DIR, tar_file);

    // Construct the tar command to find files and create the tar archive
    snprintf(tar_command, BUFFER_SIZE, "find %s -name \"*%s\" -type f | tar -cvf %s -T -", BASE_DIR, filetype, tar_file_path);
    printf("Tar Command: %s\n", tar_command);

    // Execute the tar command
    if (system(tar_command) != 0)
    {
        perror("Tar command error");
        write(client_socket, "NACK: Tar creation failed\n", 27);
        free(tar_file);
        return;
    }

    // Send ACK with tar filename
    char ack_msg[BUFFER_SIZE];
    snprintf(ack_msg, BUFFER_SIZE, "ACK %s", tar_file);
    write(client_socket, ack_msg, strlen(ack_msg));

    // Wait for client ready signal
    char client_ready[BUFFER_SIZE];
    if (read(client_socket, client_ready, BUFFER_SIZE) <= 0 || strcmp(client_ready, "READY") != 0)
    {
        printf("Client not ready or disconnected\n");
        free(tar_file);
        return;
    }

    // Get file size
    struct stat st;
    if (stat(tar_file_path, &st) != 0)
    {
        perror("Failed to get file size");
        free(tar_file);
        return;
    }
    long file_size = st.st_size;

    // Send file size to client
    write(client_socket, &file_size, sizeof(long));

    // Open the tar file for reading
    int tar_fd = open(tar_file_path, O_RDONLY);
    if (tar_fd < 0)
    {
        perror("Tar file open error");
        write(client_socket, "NACK: Unable to open tar file\n", 30);
        free(tar_file);
        return;
    }

    // Send the tar file content to the client
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    long total_sent = 0;
    while ((bytes_read = read(tar_fd, buffer, BUFFER_SIZE)) > 0)
    {
        if (write(client_socket, buffer, bytes_read) < 0)
        {
            perror("Socket write error");
            break;
        }
        total_sent += bytes_read;
    }
    close(tar_fd);

    printf("Sent %ld of %ld bytes\n", total_sent, file_size);
    if (remove(tar_file_path) == 0)
    {
        delete_empty_dirs(BASE_DIR);
    }
    // Clean up
    free(tar_file);
}

void handle_dtar_special(int client_socket,char * filetype,char *tar_file_name, int istext){

    char tar_command[BUFFER_SIZE];
    char *tar_file = generate_tar_filename(filetype);
    char tar_file_path[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    long file_size;

    printf(".txt contians...\n");
    int server_socket_2 = -1;
    int bytes_read;
    const char *server_ip_2;
    int server_port_2;
    if(istext){
        server_ip_2 = STXT_SERVER_IP;
        server_port_2 = STXT_SERVER_PORT;
    }else{
        server_ip_2 = SPDF_SERVER_IP;
        server_port_2 = SPDF_SERVER_PORT;
    }
    
    // Connect to the appropriate server
    server_socket_2 = connect_to_server(server_ip_2, server_port_2);
    if (server_socket_2 < 0) {
        write(client_socket, "NACK: Unable to connect to file server\n", 39);
        return;
    }
    memset(buffer, 0, BUFFER_SIZE);
    // Send execute request to the server
    snprintf(buffer, BUFFER_SIZE, "execute %s", tar_file);
    printf("Send command is : %s.\n",buffer);
    if (send(server_socket_2, buffer, strlen(buffer), 0) < 0) {
        perror("Send failed");
        close(server_socket_2);
        write(client_socket, "NACK: Failed to send request to file server\n", 44);
        return;
    }
    memset(buffer, 0, BUFFER_SIZE);
    printf("Server connected successfully with stext and send command sucessful..\n");
    
    if (read(server_socket_2, buffer, BUFFER_SIZE) <= 0 || strncmp(buffer, "NACK", 4) == 0 ) {
        printf("inside if condition...%s\n",buffer);
        write(client_socket, buffer, strlen(buffer));
        close(server_socket_2);
        return;
    }
    printf("Smain got ACK : %s.\n",buffer);
    // Convert the string in buffer to a long int
    file_size = strtol(buffer, NULL, 10);
    memset(buffer, 0, BUFFER_SIZE);
    printf("size if - to ready to send [1024] : %ld.\n",file_size);
    
    // Send ACK with tar filename
    char ack_msg[BUFFER_SIZE];
    snprintf(ack_msg, BUFFER_SIZE, "ACK %s", tar_file);
    write(client_socket, ack_msg, strlen(ack_msg));
    printf("cleint ack msg is : %s.\n",ack_msg);

    // Wait for client ready signal
    char client_ready[BUFFER_SIZE];
    if (read(client_socket, client_ready, BUFFER_SIZE) <= 0 || strncmp(client_ready, "READY",5) != 0)
    {
        printf("Client ready buffer is : -- %s.\n",client_ready);
        printf("condition is %d.\n", strcmp(client_ready, "READY"));
        printf("Client not ready or disconnected\n");
        free(tar_file);
        close(server_socket_2);
        return;
    }

    // Send file size to client
    printf("sending file size : %ld.\n",file_size);

    write(client_socket, &file_size, sizeof(long));
    file_size=file_size+3;

    memset(buffer, 0, BUFFER_SIZE);
    // Receive file content from server and forward to client
    while ((bytes_read = recv(server_socket_2, buffer, BUFFER_SIZE, 0)) > 0) {
        if (strncmp(buffer + bytes_read - 3, "EOF", 3) == 0) {
            printf("EOF found.\n");
            bytes_read -= 3;
            write(client_socket, buffer, bytes_read);
            break;
        }
        write(client_socket, buffer, bytes_read);
    }

    // Send EOF to client
    //write(client_socket, "EOFsmain", 8);

    close(server_socket_2);
    printf("END: ACK: File transfer complete\n");
    
}

void handle_display(int client_socket, char *pathname)
{
    char full_path[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    FILE *fp;
    size_t bytes_read;

    // Construct the full path
    snprintf(full_path, BUFFER_SIZE, "%s/%s", BASE_DIR, pathname);
    printf("Full Path is: %s\n", full_path);

    // Check if the path is valid
    if (access(full_path, F_OK) == -1)
    {
        write(client_socket, "NACK: Invalid pathname\n", 23);
        return;
    }

    // Construct the find command
    snprintf(command, BUFFER_SIZE,
             "find %s -name \"*.c\" -o -name \"*.pdf\" -o -name \"*.txt\" -type f",
             full_path);

    // Execute the command and capture its output
    fp = popen(command, "r");
    if (fp == NULL)
    {
        write(client_socket, "NACK: Error executing command\n", 30);
        return;
    }

    // Read and send the command output to the client
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE - 1, fp)) > 0)
    {
        buffer[bytes_read] = '\0';
        write(client_socket, buffer, bytes_read);
    }

    pclose(fp);

    // Send completion message
    write(client_socket, "ACK: Display complete\n", 22);
}

// ---------- Helper functions : ------------------ //
// Helper function to trim leading and trailing whitespace
char *trim_whitespace(char *str)
{
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

// Function to remove "~smain/" or "~/smain/" from the start of the string
void remove_smain_prefix(char *input)
{
    char *trimmed = trim_whitespace(input); // Trim leading and trailing whitespace
    char *smain_pattern = "~smain";
    size_t smain_len = strlen(smain_pattern);

    // Check if the string starts with "~smain/" or "~/smain/"
    if (strncmp(trimmed, smain_pattern, smain_len) == 0)
    {
        // Remove the "~smain/" prefix by shifting the string left
        memmove(trimmed, trimmed + smain_len, strlen(trimmed + smain_len) + 1);
    }

    // Print the resulting string for testing
    printf("Modified string: '%s'\n", trimmed);
}

int mkdir_recursive(const char *path, mode_t mode)
{
    if (mkdir(path, mode) == 0 || errno == EEXIST)
    {
        return 0; // Directory created successfully or already exists
    }

    if (errno == ENOENT)
    {
        // Parent directory doesn't exist, so create it recursively
        char parent[256];
        strncpy(parent, path, sizeof(parent));
        parent[sizeof(parent) - 1] = '\0';

        char *slash = strrchr(parent, '/');
        if (slash != NULL)
        {
            *slash = '\0'; // Remove the last component to get the parent directory
            if (mkdir_recursive(parent, mode) != 0)
            {
                return -1; // Recursively create the parent directory
            }
        }

        // Try to create the directory again after creating the parent
        return mkdir(path, mode);
    }

    return -1; // Some other error occurred
}

// Function to generate a unique tar file name
char *generate_tar_filename(const char *filetype)
{
    // Allocate memory for the output filename
    char *output_filename = malloc(100);
    if (!output_filename)
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Get the current time
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[20];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Format the timestamp as dd-mm-yyyy-hh-mm-ss
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y-%H-%M-%S", timeinfo);

    // Generate the output filename
    snprintf(output_filename, 100, "tar-%s-%s.tar", filetype + 1, timestamp);
    return output_filename;
}

void list_files_recursive(const char *path, char *file_list, size_t offset)
{
    DIR *dir;
    struct dirent *entry;
    char full_path[BUFFER_SIZE];

    if ((dir = opendir(path)) == NULL)
    {
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            snprintf(full_path, BUFFER_SIZE, "%s/%s", path, entry->d_name);
            list_files_recursive(full_path, file_list, strlen(file_list));
        }
        else
        {
            char *extension = strrchr(entry->d_name, '.');
            if (extension && (strcmp(extension, ".c") == 0 || strcmp(extension, ".pdf") == 0 || strcmp(extension, ".txt") == 0))
            {
                offset += snprintf(file_list + offset, BUFFER_SIZE * 10 - offset, "%s/%s\n", path, entry->d_name);
            }
        }
    }

    closedir(dir);
}

int transfer_to_stxt(int client_socket, const char *full_path, const char *filename, const char *dest_path) {
    int sock = 0, file_fd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read;

    // Open the file
    file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        perror("File open error");
        write(client_socket, "NACK: Unable to read file for transfer\n", 39);
        return 1;
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        close(file_fd);
        write(client_socket, "NACK: Unable to connect to Stxt server\n", 39);
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(STXT_SERVER_PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, STXT_SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(file_fd);
        close(sock);
        write(client_socket, "NACK: Unable to connect to Stxt server\n", 39);
        return 1;
    }

    // Connect to Stxt server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(file_fd);
        close(sock);
        write(client_socket, "NACK: Unable to connect to Stxt server\n", 39);
        return 1;
    }

    printf("Filename is :%s.\n",filename);
    printf("Detination path is :%s.\n",dest_path);

    // Send transfer command to Stxt server
    snprintf(buffer, BUFFER_SIZE, "transfer %s %s", filename, dest_path);
    send(sock, buffer, strlen(buffer), 0);

    // Wait for acknowledgment from Stxt server
    memset(buffer, 0, BUFFER_SIZE);
    if (recv(sock, buffer, BUFFER_SIZE, 0) <= 0 || strncmp(buffer, "ACK", 3) != 0) {
        close(file_fd);
        close(sock);
        write(client_socket, "NACK: Stxt server rejected transfer\n", 36);
        return 1;
    }

    // Send file content to Stxt server
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (send(sock, buffer, bytes_read, 0) != bytes_read) {
            perror("Send failed");
            close(file_fd);
            close(sock);
            write(client_socket, "NACK: Failed to transfer file to Stxt server\n", 45);
            return 1;
        }
    }

    // Send EOF marker
    send(sock, "EOF", 3, 0);

    close(file_fd);

    // Wait for final acknowledgment from Stxt server
    memset(buffer, 0, BUFFER_SIZE);
    if (recv(sock, buffer, BUFFER_SIZE, 0) <= 0 || strncmp(buffer, "ACK", 3) != 0) {
        close(sock);
        write(client_socket, "NACK: File transfer to Stxt server failed\n", 41);
        return 1;
    }

    close(sock);

    // If we've made it this far, the transfer was successful
    write(client_socket, "ACK: Upload successful\n", 24);

    // Remove the file after successful transfer
    if (remove(full_path) == 0) {
        printf("File deleted successfully: %s\n", full_path);
    } else {
        perror("Error deleting file");
    }
    // Check if the directory is empty and remove it
    printf("Dir path is : %s.\n",dest_path);
    sleep(1000);
    delete_empty_dirs(dest_path);
    return 0;
}


int transfer_to_spdf(int client_socket, const char *full_path, const char *filename, const char *dest_path) {
    int sock = 0, file_fd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read;

    // Open the file
    file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        perror("File open error");
        write(client_socket, "NACK: Unable to read file for transfer\n", 39);
        return 1;
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        close(file_fd);
        write(client_socket, "NACK: Unable to connect to Stxt server\n", 39);
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SPDF_SERVER_PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, SPDF_SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(file_fd);
        close(sock);
        write(client_socket, "NACK: Unable to connect to Stxt server\n", 39);
        return 1;
    }

    // Connect to Stxt server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(file_fd);
        close(sock);
        write(client_socket, "NACK: Unable to connect to Stxt server\n", 39);
        return 1;
    }

    printf("Filename is :%s.\n",filename);
    printf("Detination path is :%s.\n",dest_path);

    // Send transfer command to Stxt server
    snprintf(buffer, BUFFER_SIZE, "transfer %s %s", filename, dest_path);
    send(sock, buffer, strlen(buffer), 0);

    // Wait for acknowledgment from Stxt server
    memset(buffer, 0, BUFFER_SIZE);
    if (recv(sock, buffer, BUFFER_SIZE, 0) <= 0 || strncmp(buffer, "ACK", 3) != 0) {
        close(file_fd);
        close(sock);
        write(client_socket, "NACK: Stxt server rejected transfer\n", 36);
        return 1;
    }

    // Send file content to Stxt server
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (send(sock, buffer, bytes_read, 0) != bytes_read) {
            perror("Send failed");
            close(file_fd);
            close(sock);
            write(client_socket, "NACK: Failed to transfer file to Stxt server\n", 45);
            return 1;
        }
    }

    // Send EOF marker
    send(sock, "EOF", 3, 0);

    close(file_fd);

    // Wait for final acknowledgment from Stxt server
    memset(buffer, 0, BUFFER_SIZE);
    if (recv(sock, buffer, BUFFER_SIZE, 0) <= 0 || strncmp(buffer, "ACK", 3) != 0) {
        close(sock);
        write(client_socket, "NACK: File transfer to Stxt server failed\n", 41);
        return 1;
    }

    close(sock);

    // If we've made it this far, the transfer was successful
    write(client_socket, "ACK: Upload successful\n", 24);

    // Remove the file after successful transfer
    if (remove(full_path) == 0) {
        printf("File deleted successfully: %s\n", full_path);
    } else {
        perror("Error deleting file");
    }
    // Check if the directory is empty and remove it
    printf("Dir path is : %s.\n",dest_path);
    sleep(1000);
    delete_empty_dirs(dest_path);
    return 0;
}

// Function to check if a directory is empty

int is_directory_empty(const char *path) {
    int n = 0;
    struct dirent *d;
    DIR *dir = opendir(path);
    
    if (dir == NULL) {
        return 0;  // Not empty (or error)
    }
    
    while ((d = readdir(dir)) != NULL) {
        if(++n > 2) {
            break;  // Directory contains more than "." and ".."
        }
    }
    
    closedir(dir);
    return (n <= 2);  // Empty if only "." and ".." are present
}

// Recursive function to delete empty directories
int delete_empty_dirs(const char *path) {
    DIR *dir;
    struct dirent *entry;
    char full_path[1024];
    int is_empty = 1;

    // Open the directory
    dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        return 0;
    }

    // Iterate through directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full path
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Check if it's a directory
        struct stat statbuf;
        if (stat(full_path, &statbuf) != 0) {
            perror("Error getting file status");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Recursively process subdirectory
            if (!delete_empty_dirs(full_path)) {
                is_empty = 0;  // Subdirectory is not empty or couldn't be deleted
            }
        } else {
            is_empty = 0;  // Found a file, so this directory is not empty
        }
    }

    closedir(dir);

    // If directory is empty, try to delete it
    if (is_empty) {
        if (rmdir(path) == 0) {
            printf("Deleted empty directory: %s\n", path);
            return 1;  // Successfully deleted
        } else {
            perror("Error deleting directory");
            return 0;  // Failed to delete
        }
    }

    return 0;  // Directory is not empty
}

int connect_to_server(const char *server_ip, int server_port) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return -1;
    }

    return sock;
}



