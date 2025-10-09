#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

// Worker function: evaluate a simple operation
int evaluate_op(int a, char op, int b) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return (b != 0) ? a / b : 0;
        default: return 0;
    }
}

// Thread function: worker task
void worker_task(int a, char op, int b, int &result) {
    result = evaluate_op(a, op, b);
}

// Evaluate a fully parenthesized expression recursively
int evaluate_expression(string expr) {
    // Remove spaces
    expr.erase(remove(expr.begin(), expr.end(), ' '), expr.end());

    // If it's just a number
    if (expr.find('(') == string::npos) {
        return stoi(expr);
    }

    // Find innermost parenthesis
    int open = -1;
    for (int i = 0; i < expr.size(); i++) {
        if (expr[i] == '(') open = i;
        if (expr[i] == ')' && open != -1) {
            string sub = expr.substr(open + 1, i - open - 1);
            // sub is like "5+3"
            int a, b;
            char op;
            sscanf(sub.c_str(), "%d%c%d", &a, &op, &b);

            int result;
            thread t(worker_task, a, op, b, ref(result));
            t.join();

            // Replace subexpression with result
            string newExpr = expr.substr(0, open) + to_string(result) + expr.substr(i + 1);
            return evaluate_expression(newExpr);
        }
    }
    return 0;
}

int main() {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port " << PORT << endl;

    while (true) {
        client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_sock < 0) {
            perror("Accept");
            continue;
        }
        memset(buffer, 0, BUFFER_SIZE);
        read(client_sock, buffer, BUFFER_SIZE);
        string expr(buffer);

        cout << "Received expression: " << expr << endl;
        int result = evaluate_expression(expr);

        string res = "Result: " + to_string(result) + "\n";
        send(client_sock, res.c_str(), res.size(), 0);
        close(client_sock);
    }
    return 0;
}
