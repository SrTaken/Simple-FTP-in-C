🖧 FTP-like Client/Server Application

This project implements a client-server system in C (Linux/Unix) that simulates basic FTP functionalities.
It also includes a C++ version for Windows with equivalent logic.

The application allows clients to connect to the server via TCP sockets, authenticate, and execute commands such as listing directories, changing folders, downloading files, or downloading entire directories compressed as .tar.gz.

🚀 Features

🔐 User authentication (username/password).

📂 Supported commands:

dir → list current directory contents

cd → change directory

get → download a single file

rget → download an entire directory (compressed and transferred as .tar.gz)

exit → logout and disconnect

⚡ Multi-client support using threads (pthread).

🔄 Concurrency control with mutex locks.

🗂️ Secure path management with realpath to avoid directory traversal.

🖥️ Available versions:

C (Linux/Unix) → uses pthread, sys/socket.h, dirent.h, stat.h

C++ (Windows) → implemented with Winsock2 and equivalent libraries

📦 Requirements
Linux/Unix (C version)

gcc compiler

Standard libraries: pthread, socket, arpa/inet, dirent, sys/stat

Build:

gcc -o server server.c -lpthread
gcc -o client client.c -lpthread


Run:

# Start server
./server

# In another terminal, start client
./client

Windows (C++ version)

Compiler supporting Winsock2 (e.g., Visual Studio or MinGW)

Link against ws2_32.lib

Build (MinGW example):

g++ -o server.exe server.cpp -lws2_32
g++ -o client.exe client.cpp -lws2_32

🔧 Usage
Client

Client connects to the server on port 8778.

Enter username and password (default: Usuario/Usuario).

Available commands:

1) dir   → list files and folders
2) cd    → change directory
3) get   → download a file
4) rget  → download a directory (tar.gz)
5) exit  → quit

Example session
Enter your username
Usuario
Enter your password
Usuario
1) dir
2) cd
3) get
4) rget
5) exit
