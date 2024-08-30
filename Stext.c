#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define SERVER_PORT 10450
#define BASE_DIR "stext"  // Change this to your actual path

void handle_transfer(int client_socket);
void handle_download(int client_socket,const char *buffer);
int mkdir_recursive(const char *path, mode_t mode);
void handle_remove(int client_socket,const char *buffer);
void handle_execute(int client_socket,const char *buffer);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        exit(1);
    }
    // Allow address reuse
    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(1);
    }

    // Listen for incoming connections
    listen(server_socket, 3);

    printf("Stxt server listening on port %d...\n", SERVER_PORT);

    while (1) {
        // Accept incoming connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        handle_transfer(client_socket);

        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void handle_transfer(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char filename[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];
    char full_path[BUFFER_SIZE];
    char temp_full_path[BUFFER_SIZE];
    int file_fd;
    struct stat st = {0};

    // Read the transfer command from smain
    memset(buffer,0,BUFFER_SIZE);
    memset(filename,0,BUFFER_SIZE);
    memset(dest_path,0,BUFFER_SIZE);
    memset(full_path,0,BUFFER_SIZE);
    memset(temp_full_path,0,BUFFER_SIZE);

    bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        perror("Failed to receive command");
        return;
    }

    if(strstr(buffer,"download")){
        printf("handeling downlaod....\n");
        handle_download(client_socket,buffer);
        return ;
    }else if(strstr(buffer,"remove")){
         printf("handeling remove....\n");
        handle_remove(client_socket,buffer);
        return ;
    }else if(strstr(buffer,"execute")){
        printf("handeling execute....\n");
        handle_execute(client_socket,buffer);
        return ;
    }

    // Parse the command to extract filename and destination path
    sscanf(buffer, "transfer %s %s", filename, dest_path);
    printf("buffer is : %s.\n",buffer);
    printf("Filename is : %s.\n",filename);
    printf("dest_path is : %s.\n",dest_path);


    // Create the full destination path
    snprintf(full_path, BUFFER_SIZE, "%s%s", BASE_DIR, dest_path);
    snprintf(temp_full_path, BUFFER_SIZE, "%s%s", BASE_DIR, dest_path);
    printf("Full Path is  : %s.\n",full_path);
    printf("temp Full Path is  : %s.\n",full_path);


    mkdir_recursive(temp_full_path,0700);
    printf("Full Path is  : %s.\n",full_path);
    printf("temp Full Path is  : %s.\n",full_path);

    // Append the filename to the full path
    snprintf(full_path, BUFFER_SIZE, "%s/%s", temp_full_path, filename);
    printf("Full path with filename is : %s.\n",full_path);

    // Open the file to write to
    file_fd = open(full_path,  O_CREAT | O_WRONLY |O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("File open error");
        write(client_socket, "NACK: Unable to create file on server\n", 38);
        return;
    }

    // Send acknowledgment to smain that we're ready to receive the file
    write(client_socket, "ACK", 3);

    // Receive file content and write to the file
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        // Check for EOF marker
        if (bytes_read >= 3 && memcmp(buffer + bytes_read - 3, "EOF", 3) == 0) {
            bytes_read -= 3;
            if (write(file_fd, buffer, bytes_read) < 0) {
                perror("File write error");
                write(client_socket, "NACK: File write failed\n", 25);
                close(file_fd);
                return;
            }
            break;
        }

        if (write(file_fd, buffer, bytes_read) < 0) {
            perror("File write error");
            write(client_socket, "NACK: File write failed\n", 25);
            close(file_fd);
            return;
        }
    }

    close(file_fd);

    // Send final acknowledgment to smain
    write(client_socket, "ACK: Transfer successful\n", 26);
}

void handle_download(int client_socket,const char *buffer){
    ssize_t bytes_read;
    char filename[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    char full_path[BUFFER_SIZE];
    int file_fd;

    sscanf(buffer, "download %s", filename);

    printf("buffer is : %s.\n",buffer);
    printf("Filename is : %s.\n",filename);

    snprintf(full_path, BUFFER_SIZE, "%s/%s", BASE_DIR, filename);
    printf("full path : %s\n",full_path);
    if (access(full_path, F_OK) == -1)
    {
        write(client_socket, "NACK: File does not exist\n", 26);
        return;
    }

    // TODO: write a command to open the full path and read the content of the file in binary mode and send the content to client_socket.

    // Open the file in binary read mode
    file_fd = open(full_path, O_RDONLY);
    if (file_fd == -1) {
        perror("Error opening file");
        write(client_socket, "NACK: Unable to open file\n", 26);
        return;
    }else{
        write(client_socket, "ACK: Ready to send data.\n", 26);
    }

    // Read the content of the file and send it to the client
    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        if (write(client_socket, file_buffer, bytes_read) != bytes_read) {
            perror("Error sending file content");
            close(file_fd);
            return;
        }
    }

    // Check if there was an error during reading
    if (bytes_read == -1) {
        perror("Error reading file");
        write(client_socket, "NACK: Error reading file\n", 25);
        close(file_fd);
        return;
    }

    // Close the file
    close(file_fd);

    // Send EOF to client
    write(client_socket, "EOF", 3);

    printf("File sent successfully to client\n");
}

void handle_remove(int client_socket,const char *buffer){
    ssize_t bytes_read;
    char filename[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    char full_path[BUFFER_SIZE];
    int file_fd;

    sscanf(buffer, "remove %s", filename);

    printf("buffer is : %s.\n",buffer);
    printf("Filename is : %s.\n",filename);

    snprintf(full_path, BUFFER_SIZE, "%s/%s", BASE_DIR, filename);
    printf("full path : %s\n",full_path);
    if (access(full_path, F_OK) == -1)
    {
        write(client_socket, "NACK: File does not exist\n", 26);
        return;
    }

    if (remove(full_path) == 0)
    {
        write(client_socket, "ACK: File deleted successfully\n", 31);
        printf("File deleted successfully to client\n");
    }
    else
    {
        perror("File delete error");
        write(client_socket, "NACK: File deletion failed\n", 28);
    }

}

void handle_execute(int client_socket,const char *buffer){
    ssize_t bytes_read;
    char filename[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    char full_path[BUFFER_SIZE];
    char tar_file_path[BUFFER_SIZE];
    char tar_command[BUFFER_SIZE];

    int file_fd;
    char *filetype = ".txt";

    sscanf(buffer, "execute %s", filename);

    printf("buffer is : %s.\n",buffer);
    printf("Filename is : %s.\n",filename);

    snprintf(full_path, BUFFER_SIZE, "%s/%s", BASE_DIR, filename);
    printf("full path : %s\n",full_path);

    // Construct the full path to the tar file
    snprintf(tar_file_path, sizeof(tar_file_path), "%s/%s", BASE_DIR, filename);

    // Construct the tar command to find files and create the tar archive
    snprintf(tar_command, BUFFER_SIZE, "find %s -name \"*%s\" -type f | tar -cvf %s -T -", BASE_DIR, filetype, tar_file_path);
    printf("Tar Command: %s\n", tar_command);

    // Execute the tar command
    if (system(tar_command) != 0)
    {
        perror("Tar command error");
        write(client_socket, "NACK: Tar creation failed\n", 27);
        return;
    }
    printf("Tar file path is : %s.\n",tar_file_path);

    // Open the file in binary read mode
    file_fd = open(tar_file_path, O_RDONLY);
    if (file_fd == -1) {
        perror("Error opening file");
        write(client_socket, "NACK: Unable to open file\n", 26);
        return;
    }else{
        // Get file size
        struct stat st;
        if (stat(tar_file_path, &st) != 0)
        {
            perror("Failed to get file size");
            return;
        }
        long file_size = st.st_size;
        char file_size_str[50];   
        // Convert the long file size to a string
        sprintf(file_size_str, "%ld", file_size);
        // Send file size to client
        printf("File size is : %d\n",file_size);
        write(client_socket, &file_size_str, sizeof(file_size_str));
    }

    // Read the content of the file and send it to the client
    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        if (write(client_socket, file_buffer, bytes_read) != bytes_read) {
            perror("Error sending file content");
            close(file_fd);
            return;
        }
    }

    // Check if there was an error during reading
    if (bytes_read == -1) {
        perror("Error reading file");
        write(client_socket, "NACK: Error reading file\n", 25);
        close(file_fd);
        return;
    }

    // Close the file
    close(file_fd);

    // Send EOF to client
    //write(client_socket, "EOFx", 4);
    if (remove(tar_file_path) == 0)
    {
        printf("files are deleted suceessfully,\n");
    }
    close(client_socket);
    printf("File sent successfully to client\n");


}
// -------- helper function --------

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
