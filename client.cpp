#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <pthread.h>

using namespace std;

void* receive_messages(void* arg) {
    int sock = *((int*)arg);
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int len = read(sock, buffer, 1024);
        if (len <= 0) {
            cout << "\nDisconnected from server." << endl;
            exit(0);
        }
        cout << "\nMessage: " << buffer << "\n> " << flush;
    }
    return NULL;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9002);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Connection Failed" << endl;
        return -1;
    }

    // Step 1: Handle Username and Password requests from server
    for(int i = 0; i < 2; i++) {
        memset(buffer, 0, 1024);
        read(sock, buffer, 1024);
        cout << buffer; 
        string input;
        getline(cin, input);
        send(sock, input.c_str(), input.length(), 0);
    }

    // Step 2: Check if login was successful
    memset(buffer, 0, 1024);
    read(sock, buffer, 1024);
    cout << buffer << endl;

    if (string(buffer).find("Successful") == string::npos) {
        close(sock);
        return 0;
    }

    // Step 3: Start the background thread to listen for incoming messages
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sock);

    cout << "Connected! Type your messages below:" << endl;
    while (true) {
        string message;
        cout << "> ";
        getline(cin, message);
        if (message == "exit") break;
        send(sock, message.c_str(), message.length(), 0);
    }

    close(sock);
    return 0;
}
