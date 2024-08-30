# Distributed File System with Socket Programming

## Overview

This project is an implementation of a distributed file system using socket programming, developed for the COMP-8567 course during Summer 2024. The system is designed to efficiently handle `.c`, `.pdf`, and `.txt` files by distributing them across three specialized servers (`Smain`, `Spdf`, `Stext`) while maintaining a unified interface for the client.

### Features
- **Multi-Server Architecture**: The system consists of three servers:
  - `Smain`: Handles `.c` files and coordinates with other servers.
  - `Spdf`: Dedicated to storing `.pdf` files.
  - `Stext`: Dedicated to storing `.txt` files.
- **Client Transparency**: Clients interact only with `Smain`, unaware of the underlying distribution of files across different servers.
- **Socket Programming**: Communication between servers and clients is achieved through socket programming, ensuring secure and efficient data transfer.
- **Custom Commands**: A set of custom commands allows clients to upload, download, remove, and manage files seamlessly.

### Project Structure
The project is organized into the following key files:
- **`Smain.c`**: The main server handling client requests and coordinating with `Spdf` and `Stext`.
- **`Spdf.c`**: The server responsible for storing `.pdf` files.
- **`Stext.c`**: The server responsible for storing `.txt` files.
- **`client24s.c`**: The client program allowing users to interact with the distributed file system using custom commands.

### Client Commands

1. **Upload a File (`ufile`)**: 
   - Upload a `.c`, `.pdf`, or `.txt` file to the distributed file system.
   - Example: `ufile sample.pdf ~smain/folder1/folder2`

2. **Download a File (`dfile`)**:
   - Download a file from the distributed file system to the client's current directory.
   - Example: `dfile ~smain/folder1/folder2/sample.pdf`

3. **Remove a File (`rmfile`)**:
   - Delete a file from the distributed file system.
   - Example: `rmfile ~smain/folder1/folder2/sample.pdf`

4. **Create a Tar Archive (`dtar`)**:
   - Create and download a tar archive of all files of a specific type.
   - Example: `dtar .pdf`

5. **Display Files (`display`)**:
   - Display a list of all files in a specified directory on the distributed file system.
   - Example: `display ~smain/folder1/folder2`

### Getting Started

#### Prerequisites
- GCC Compiler
- Linux Environment

#### Compilation
To compile the server and client programs:
```bash
gcc -o Smain Smain.c
gcc -o Spdf Spdf.c
gcc -o Stext Stext.c
gcc -o client24s client24s.c
```
#### Running the Servers
Each server (`Smain`, `Spdf`, `Stext`) must be run on different machines or terminals:
```bash
./Smain
./Spdf
./Stext
```

#### Running the Client
To interact with the distributed file system, run the client program:
```bash
./client24s
```

### Contribution Guidelines
- Ensure code is well-commented.
- Handle all error conditions gracefully.
- Adhere to the project guidelines and submission requirements.


### Acknowledgments
- Developed as part of COMP-8567 course, Summer 2024.
- Plagiarism Detection Tool: MOSS.

