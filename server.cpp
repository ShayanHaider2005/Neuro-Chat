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
#include <cctype>

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
    bool monitor_enabled;
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
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
sem_t connection_semaphore;
const string USERS_FILE = "users.txt";
const string LOG_FILE = "server_log.txt";

int active_workers = 0;
int active_connections = 0;
long total_messages = 0;
bool clients_mutex_locked = false;

string trim_copy(string value) {
    value.erase(value.begin(), find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !isspace(ch);
    }));
    value.erase(find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !isspace(ch);
    }).base(), value.end());
    return value;
}

string current_timestamp() {
    time_t now = time(nullptr);
    tm *local_time = localtime(&now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
    return string(buffer);
}

void lock_clients_data() {
    pthread_mutex_lock(&clients_mutex);
    pthread_mutex_lock(&state_mutex);
    clients_mutex_locked = true;
    pthread_mutex_unlock(&state_mutex);
}

void unlock_clients_data() {
    pthread_mutex_lock(&state_mutex);
    clients_mutex_locked = false;
    pthread_mutex_unlock(&state_mutex);
    pthread_mutex_unlock(&clients_mutex);
}

void update_state(int worker_delta = 0, int connection_delta = 0, long message_delta = 0) {
    pthread_mutex_lock(&state_mutex);
    active_workers += worker_delta;
    active_connections += connection_delta;
    total_messages += message_delta;
    pthread_mutex_unlock(&state_mutex);
}

void log_activity(const string &record) {
    pthread_mutex_lock(&log_mutex);
    ofstream log_file(LOG_FILE, ios::app);
    if (log_file.is_open()) {
        log_file << "[" << current_timestamp() << "] " << record << endl;
        log_file.close();
    }
    pthread_mutex_unlock(&log_mutex);
}

bool user_exists(const string &username) {
    for (const auto &user : database) {
        if (user.username == username) {
            return true;
        }
    }
    return false;
}

void load_users_from_disk() {
    ifstream input(USERS_FILE);
    if (!input.is_open()) {
        return;
    }

    string line;
    while (getline(input, line)) {
        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }

        size_t separator = line.find(',');
        if (separator == string::npos) {
            continue;
        }

        string username = trim_copy(line.substr(0, separator));
        string password = trim_copy(line.substr(separator + 1));
        if (!username.empty() && !password.empty() && !user_exists(username)) {
            database.push_back({username, password});
        }
    }
}

bool save_user_to_disk(const string &username, const string &password) {
    ofstream output(USERS_FILE, ios::app);
    if (!output.is_open()) {
        return false;
    }

    output << username << "," << password << endl;
    output.close();
    return true;
}

bool authenticate(const string &u, const string &p) {
    for (const auto &user : database) {
        if (u == user.username && p == user.password) {
            return true;
        }
    }
    return false;
}

bool register_user(const string &username, const string &password) {
    if (username.empty() || password.empty() || user_exists(username)) {
        return false;
    }

    database.push_back({username, password});
    return save_user_to_disk(username, password);
}

struct MonitorSnapshot {
    int workers;
    int connections;
    int slots;
    long messages;
    bool mutex_locked;
};

MonitorSnapshot get_monitor_snapshot() {
    MonitorSnapshot snapshot{};
    pthread_mutex_lock(&state_mutex);
    snapshot.workers = active_workers;
    snapshot.connections = active_connections;
    snapshot.slots = MAX_CLIENTS - active_connections;
    snapshot.messages = total_messages;
    snapshot.mutex_locked = clients_mutex_locked;
    pthread_mutex_unlock(&state_mutex);
    return snapshot;
}

void send_monitor_update(const string &reason) {
    vector<int> monitor_clients;

    lock_clients_data();
    for (const auto &client : clients_list) {
        if (client.monitor_enabled) {
            monitor_clients.push_back(client.socket);
        }
    }
    unlock_clients_data();

    MonitorSnapshot snapshot = get_monitor_snapshot();
    string payload = "__MONITOR__|reason=" + reason +
                     "|workers=" + to_string(snapshot.workers) +
                     "|connections=" + to_string(snapshot.connections) +
                     "|slots=" + to_string(snapshot.slots) +
                     "|messages=" + to_string(snapshot.messages) +
                     "|mutex=" + string(snapshot.mutex_locked ? "locked" : "unlocked");

    for (int socket : monitor_clients) {
        send(socket, payload.c_str(), payload.length(), 0);
    }
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

        update_state(1, 0, 0);
        send_monitor_update("worker_assigned");

        char auth_buffer[1024];
        string mode_input;
        string user_input;
        string pass_input;

        send(client_socket, "Mode (login/register): ", 24, 0);
        memset(auth_buffer, 0, sizeof(auth_buffer));
        if (read(client_socket, auth_buffer, sizeof(auth_buffer)) <= 0) {
            close(client_socket);
            update_state(-1, -1, 0);
            sem_post(&connection_semaphore);
            send_monitor_update("client_disconnected");
            continue;
        }
        mode_input = trim_copy(string(auth_buffer));
        transform(mode_input.begin(), mode_input.end(), mode_input.begin(), [](unsigned char ch) {
            return static_cast<char>(tolower(ch));
        });

        bool register_mode = (mode_input == "register" || mode_input == "signup" || mode_input == "sign up");

        send(client_socket, "Username: ", 10, 0);
        memset(auth_buffer, 0, 1024);
        if (read(client_socket, auth_buffer, 1024) <= 0) {
            close(client_socket);
            update_state(-1, -1, 0);
            sem_post(&connection_semaphore);
            send_monitor_update("client_disconnected");
            continue;
        }
        user_input = trim_copy(string(auth_buffer));

        send(client_socket, "Password: ", 10, 0);
        memset(auth_buffer, 0, 1024);
        if (read(client_socket, auth_buffer, 1024) <= 0) {
            close(client_socket);
            update_state(-1, -1, 0);
            sem_post(&connection_semaphore);
            send_monitor_update("client_disconnected");
            continue;
        }
        pass_input = trim_copy(string(auth_buffer));

        if (register_mode) {
            if (register_user(user_input, pass_input)) {
                send(client_socket, "Registration Successful!", 24, 0);
                log_activity("AUTH: " + user_input + " registered.");
            } else {
                send(client_socket, "Registration Failed.", 20, 0);
                log_activity("AUTH: registration failed for " + user_input + ".");
                close(client_socket);
                update_state(-1, -1, 0);
                sem_post(&connection_semaphore);
                send_monitor_update("registration_failed");
                continue;
            }
        } else {
            if (authenticate(user_input, pass_input)) {
                send(client_socket, "Login Successful!", 17, 0);
                log_activity("AUTH: " + user_input + " logged in.");
            } else {
                send(client_socket, "Login Failed.", 13, 0);
                log_activity("AUTH: login failed for " + user_input + ".");
                close(client_socket);
                update_state(-1, -1, 0);
                sem_post(&connection_semaphore);
                send_monitor_update("login_failed");
                continue;
            }
        }

        string my_room = "General";
        time_t now = time(0);
        lock_clients_data();
        clients_list.push_back({client_socket, user_input, my_room, now, false});
        unlock_clients_data();
        log_activity("CONNECT: " + user_input + " joined the server.");
        send_monitor_update("client_connected");

        while (true) {
            char buffer[1024] = {0};
            int valread = read(client_socket, buffer, 1024);
            if (valread <= 0) break;

            string input = trim_copy(string(buffer));

            if (input == "/monitor on") {
                lock_clients_data();
                for (auto &client : clients_list) {
                    if (client.socket == client_socket) {
                        client.monitor_enabled = true;
                        break;
                    }
                }
                unlock_clients_data();
                send(client_socket, "Monitor mode enabled.", 21, 0);
                send_monitor_update("monitor_enabled");
                continue;
            }

            if (input == "/monitor off") {
                lock_clients_data();
                for (auto &client : clients_list) {
                    if (client.socket == client_socket) {
                        client.monitor_enabled = false;
                        break;
                    }
                }
                unlock_clients_data();
                send(client_socket, "Monitor mode disabled.", 22, 0);
                send_monitor_update("monitor_disabled");
                continue;
            }

            time_t current_time = time(0);
            bool is_spamming = false;

            lock_clients_data();
            for (auto &client : clients_list) {
                if (client.socket == client_socket) {
                    if (difftime(current_time, client.last_msg_time) < SPAM_DELAY) {
                        is_spamming = true;
                    } else {
                        client.last_msg_time = current_time;
                    }
                    break;
                }
            }
            unlock_clients_data();

            if (is_spamming) {
                string spam_err = "ALERT: Slow down! Spam protection active.";
                send(client_socket, spam_err.c_str(), spam_err.length(), 0);
                continue;
            }

            update_state(0, 0, 1);
            log_activity("MESSAGE: " + user_input + " -> " + input);

            if (input.substr(0, 6) == "/join ") {
                my_room = input.substr(6);
                lock_clients_data();
                for (auto &c : clients_list) {
                    if (c.socket == client_socket) { c.current_room = my_room; break; }
                }
                unlock_clients_data();
                send(client_socket, "Room changed.", 13, 0);
                log_activity("ROOM: " + user_input + " joined " + my_room + ".");
                send_monitor_update("room_changed");
            } else if (input.substr(0, 5) == "/msg ") {
                stringstream ss(input.substr(5));
                string target, msg;
                ss >> target; getline(ss, msg);
                string p_msg = "[Private from " + user_input + "]:" + msg;
                lock_clients_data();
                for (auto &c : clients_list) {
                    if (c.username == target) send(c.socket, p_msg.c_str(), p_msg.length(), 0);
                }
                unlock_clients_data();
                log_activity("PRIVATE: " + user_input + " -> " + target + ".");
                send_monitor_update("private_message");
            } else {
                string b_msg = "[" + my_room + "] " + user_input + ": " + input;
                lock_clients_data();
                for (auto &c : clients_list) {
                    if (c.socket != client_socket && c.current_room == my_room) {
                        send(c.socket, b_msg.c_str(), b_msg.length(), 0);
                    }
                }
                unlock_clients_data();
                send_monitor_update("broadcast_message");
            }
        }

        lock_clients_data();
        for (int i = 0; i < (int)clients_list.size(); i++) {
            if (clients_list[i].socket == client_socket) {
                clients_list.erase(clients_list.begin() + i);
                break;
            }
        }
        unlock_clients_data();
        close(client_socket);
        log_activity("DISCONNECT: " + user_input + " left the server.");
        update_state(-1, -1, 0);
        sem_post(&connection_semaphore);
        send_monitor_update("client_disconnected");
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    sem_init(&connection_semaphore, 0, MAX_CLIENTS);
    load_users_from_disk();
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
        log_activity("CONNECT: incoming connection accepted.");
        update_state(0, 1, 0);
        send_monitor_update("incoming_connection");
        pthread_mutex_lock(&queue_mutex);
        client_queue.push(client_sock);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&queue_mutex);
    }
    return 0;
}
