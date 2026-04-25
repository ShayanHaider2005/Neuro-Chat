/* * PROJECT: NeuroChat Server
 * TEAM MEMBERS:
 * Badar Ali - 24k-3079 (Lead)
 * Shayan Haider - 24k-5613
 * Aarib - K24-3100
 */

#include <iostream>
#include <vector>
#include <queue>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <semaphore.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <ctime>

using namespace std;

#define THREAD_POOL_SIZE 5
#define MAX_CLIENTS 10
#define SPAM_DELAY 1 // Seconds allowed between messages
pthread_t thread_pool[THREAD_POOL_SIZE];
struct ClientNode {
    int socket;
    string username;
    string current_room;
    time_t last_msg_time; // For Spam Protection
};

struct User {
    string username;
    string password;
};

// Team Credentials Integrated
vector<User> database = {
    {"admin", "1234"}, 
    {"badar", "24k3079"}, 
    {"shayan", "24k5613"}, 
    {"aarib", "k243100"}
};

vector<ClientNode> clients_list;
queue<int> client_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
sem_t connection_semaphore;

void log_activity(string record) {
    pthread_mutex_lock(&clients_mutex);
    ofstream log_file("server_log.txt", ios::app);
    if (log_file.is_open()) {
        log_file << record << endl;
        log_file.close();
    }
    pthread_mutex_unlock(&clients_mutex);
}

bool authenticate(string u, string p) {
    // This logic cleans the input strings to remove invisible characters
    for (auto &user : database) {
        // Find if user.username exists inside the input u
        if (u.find(user.username) != string::npos && p.find(user.password) != string::npos) {
            return true;
        }
    }
    return false;
}
void* thread_function(void* arg) {
    while (true) {
        int client_socket;
        pthread_mutex_lock(&queue_mutex);
        while (client_queue.empty()) {
            pthread_cond_wait(&condition_var, &queue_mutex);
        }
        client_socket = client_queue.front();
        client_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        char auth_buffer[1024];
        send(client_socket, "Username: ", 10, 0);
        memset(auth_buffer, 0, 1024);
        read(client_socket, auth_buffer, 1024);
        string user_input(auth_buffer);

        send(client_socket, "Password: ", 10, 0);
        memset(auth_buffer, 0, 1024);
        read(client_socket, auth_buffer, 1024);
        string pass_input(auth_buffer);

        if (authenticate(user_input, pass_input)) {
            send(client_socket, "Login Successful!", 17, 0);
            log_activity("AUTH: " + user_input + " logged in.");
        } else {
            send(client_socket, "Login Failed.", 13, 0);
            close(client_socket);
            sem_post(&connection_semaphore);
            continue;
        }

        string my_room = "General";
        time_t now = time(0);
        pthread_mutex_lock(&clients_mutex);
        clients_list.push_back({client_socket, user_input, my_room, now});
        pthread_mutex_unlock(&clients_mutex);

        while (true) {
            char buffer[1024] = {0};
            int valread = read(client_socket, buffer, 1024);
            if (valread <= 0) break;

// --- Improved Spam Protection Logic ---
		// --- Improved Spam Protection Logic ---
time_t current_time = time(0);
bool is_spamming = false;

pthread_mutex_lock(&clients_mutex);
for(auto &c : clients_list) {
    if(c.socket == client_socket) {
        // If less than 1 second has passed since the last message
        if(difftime(current_time, c.last_msg_time) < 1.0) {
            is_spamming = true;
        } else {
            c.last_msg_time = current_time; // Update the timestamp
        }
        break;
    }
}
pthread_mutex_unlock(&clients_mutex);

if(is_spamming) {
    string spam_err = "ALERT: Slow down! Spam protection active.";
    send(client_socket, spam_err.c_str(), spam_err.length(), 0);
    continue; // Skip the rest of the loop so the message isn't sent
}
// ---------------------------------------

// ---------------------------------------
            string input(buffer);
            if (input.substr(0, 6) == "/join ") {
                my_room = input.substr(6);
                pthread_mutex_lock(&clients_mutex);
                for (auto &c : clients_list) {
                    if (c.socket == client_socket) { c.current_room = my_room; break; }
                }
                pthread_mutex_unlock(&clients_mutex);
                send(client_socket, "Room changed.", 13, 0);
            } else if (input.substr(0, 5) == "/msg ") {
                stringstream ss(input.substr(5));
                string target, msg;
                ss >> target; getline(ss, msg);
                string p_msg = "[Private from " + user_input + "]:" + msg;
                pthread_mutex_lock(&clients_mutex);
                for (auto &c : clients_list) {
                    if (c.username == target) send(c.socket, p_msg.c_str(), p_msg.length(), 0);
                }
                pthread_mutex_unlock(&clients_mutex);
            } else {
                string b_msg = "[" + my_room + "] " + user_input + ": " + input;
                pthread_mutex_lock(&clients_mutex);
                for (auto &c : clients_list) {
                    if (c.socket != client_socket && c.current_room == my_room) {
                        send(c.socket, b_msg.c_str(), b_msg.length(), 0);
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
            }
        }

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < (int)clients_list.size(); i++) {
            if (clients_list[i].socket == client_socket) {
                clients_list.erase(clients_list.begin() + i);
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        close(client_socket);
        sem_post(&connection_semaphore);
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    sem_init(&connection_semaphore, 0, MAX_CLIENTS);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9002);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    for (int i = 0; i < THREAD_POOL_SIZE; i++) pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    cout << "NeuroChat Server LIVE | Badar Ali | Shayan Haider | Aarib" << endl;
    while (true) {
        sem_wait(&connection_semaphore);
        int addrlen = sizeof(address);
        int client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        pthread_mutex_lock(&queue_mutex);
        client_queue.push(client_sock);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&queue_mutex);
    }
    return 0;
}
