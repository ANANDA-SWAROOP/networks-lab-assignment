#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
using namespace std;

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

unordered_map<int, string> sock_to_user;   // socket -> username
unordered_map<string, int> user_to_sock;   // username -> socket

void broadcast_message(int sender_sock, const string &msg) {
    for (auto &pair : sock_to_user) {
        int sock = pair.first;
        if (sock != sender_sock) {
            send(sock, msg.c_str(), msg.size(), 0);
        }
    }
}

void private_message(const string &to_user, const string &msg) {
    if (user_to_sock.find(to_user) != user_to_sock.end()) {
        int sock = user_to_sock[to_user];
        send(sock, msg.c_str(), msg.size(), 0);
    }
}

void remove_client(int client_sock) {
    if (sock_to_user.find(client_sock) != sock_to_user.end()) {
        string uname = sock_to_user[client_sock];
        user_to_sock.erase(uname);
        sock_to_user.erase(client_sock);
    }
    close(client_sock);
}

int main() {
    int server_fd, new_sock, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    fd_set master_set, read_fds;
    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    int fdmax = server_fd;

    cout << "Chat server started on port " << PORT << endl;

    while (true) {
        read_fds = master_set;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    // New connection
                    new_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    if (new_sock == -1) {
                        perror("accept");
                    } else {
                        FD_SET(new_sock, &master_set);
                        if (new_sock > fdmax) fdmax = new_sock;
                        sock_to_user[new_sock] = "guest" + to_string(new_sock);
                        user_to_sock[sock_to_user[new_sock]] = new_sock;
                        string welcome = "Welcome! Use /join <username> to set your name.\n";
                        send(new_sock, welcome.c_str(), welcome.size(), 0);
                    }
                } else {
                    // Handle data from client
                    memset(buffer, 0, BUFFER_SIZE);
                    valread = recv(i, buffer, BUFFER_SIZE, 0);
                    if (valread <= 0) {
                        // Connection closed
                        cout << "Client disconnected" << endl;
                        remove_client(i);
                        FD_CLR(i, &master_set);
                    } else {
                        string input(buffer);
                        input.erase(remove(input.begin(), input.end(), '\n'), input.end());
                        input.erase(remove(input.begin(), input.end(), '\r'), input.end());

                        if (input.rfind("/join", 0) == 0) {
                            string uname = input.substr(6);
                            if (!uname.empty()) {
                                string old = sock_to_user[i];
                                user_to_sock.erase(old);
                                sock_to_user[i] = uname;
                                user_to_sock[uname] = i;
                                string msg = "You are now known as " + uname + "\n";
                                send(i, msg.c_str(), msg.size(), 0);
                            }
                        } else if (input.rfind("/msg", 0) == 0) {
                            istringstream iss(input);
                            string cmd, uname, message;
                            iss >> cmd >> uname;
                            getline(iss, message);
                            if (!uname.empty()) {
                                private_message(uname, "[PM] " + sock_to_user[i] + ": " + message + "\n");
                            }
                        } else if (input == "/list") {
                            string userlist = "Connected users:\n";
                            for (auto &p : user_to_sock) {
                                userlist += p.first + "\n";
                            }
                            send(i, userlist.c_str(), userlist.size(), 0);
                        } else if (input == "/quit") {
                            string bye = "Goodbye!\n";
                            send(i, bye.c_str(), bye.size(), 0);
                            remove_client(i);
                            FD_CLR(i, &master_set);
                        } else {
                            string msg = sock_to_user[i] + ": " + input + "\n";
                            broadcast_message(i, msg);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
