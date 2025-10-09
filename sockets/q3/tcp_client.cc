#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

void receive_messages(int sock) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = recv(sock, buffer, BUFFER_SIZE, 0);
        if (valread <= 0) {
            cout << "Disconnected from server." << endl;
            close(sock);
            exit(0);
        }
        cout << buffer;
        fflush(stdout);
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Connect to localhost (127.0.0.1)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    cout << "Connected to chat server. Use /join <username> to set your name." << endl;

    // Start thread to receive messages
    thread recvThread(receive_messages, sock);
    recvThread.detach();

    // Main thread handles user input
    string input;
    while (true) {
        getline(cin, input);
        if (input == "/quit") {
            send(sock, input.c_str(), input.size(), 0);
            close(sock);
            cout << "You left the chat." << endl;
            break;
        }
        send(sock, input.c_str(), input.size(), 0);
    }

    return 0;
}
