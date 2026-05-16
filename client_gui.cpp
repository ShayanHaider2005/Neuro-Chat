/* * NeuroChat GUI Client
 * Group: Badar (24k-3079), Shayan (24k-5613), Aarib (K24-3100)
 */

#include <gtk/gtk.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <ctime>

using namespace std;

int sock = 0;
GtkWidget *chat_buffer_view;
GtkTextBuffer *buffer;
GtkWidget *message_entry;
GtkWidget *thread_label;
GtkWidget *mutex_label;
GtkWidget *semaphore_label;
GtkWidget *activity_label;

void update_chat(const char* text) {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, text, -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);
    
    // Auto-scroll to the end
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(chat_buffer_view), &iter, 0.0, FALSE, 0.0, 1.0);
}

string extract_field(const string &payload, const string &field) {
    string key = field + "=";
    size_t start = payload.find(key);
    if (start == string::npos) {
        return "";
    }

    start += key.length();
    size_t end = payload.find('|', start);
    return payload.substr(start, end == string::npos ? string::npos : end - start);
}

gboolean append_chat_idle(gpointer data) {
    char *text = static_cast<char*>(data);
    
    // Add timestamp to message
    time_t now = time(nullptr);
    tm *local_time = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", local_time);
    
    string formatted = string("[") + timestamp + "] " + text;
    update_chat(formatted.c_str());
    g_free(text);
    return G_SOURCE_REMOVE;
}

gboolean update_monitor_idle(gpointer data) {
    char *payload_data = static_cast<char*>(data);
    string payload(payload_data);
    g_free(payload_data);

    string reason = extract_field(payload, "reason");
    string workers = extract_field(payload, "workers");
    string connections = extract_field(payload, "connections");
    string slots = extract_field(payload, "slots");
    string messages = extract_field(payload, "messages");
    string mutex_state = extract_field(payload, "mutex");

    if (!workers.empty()) {
        gtk_label_set_text(GTK_LABEL(thread_label), ("Thread Pool Workers: " + workers + " active").c_str());
    }
    if (!mutex_state.empty()) {
        gtk_label_set_text(GTK_LABEL(mutex_label), ("Mutex: [ " + mutex_state + " ]").c_str());
    }
    if (!slots.empty() && !connections.empty()) {
        gtk_label_set_text(GTK_LABEL(semaphore_label), ("Semaphore Slots: " + slots + " free / " + connections + " active").c_str());
    }
    if (!reason.empty()) {
        gtk_label_set_text(GTK_LABEL(activity_label), ("Last Event: " + reason + " | Messages: " + messages).c_str());
    }

    return G_SOURCE_REMOVE;
}

void* receiver(void* arg) {
    char server_reply[2048];
    while (true) {
        memset(server_reply, 0, 2048);
        int len = read(sock, server_reply, 2048);
        if (len <= 0) break;
        
        if (strncmp(server_reply, "__MONITOR__|", 12) == 0) {
            g_idle_add(update_monitor_idle, g_strdup(server_reply));
        } else {
            g_idle_add(append_chat_idle, g_strdup(server_reply));
        }
    }
    return NULL;
}

static void on_send_clicked(GtkWidget *widget, gpointer data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(message_entry));
    if (strlen(text) > 0) {
        send(sock, text, strlen(text), 0);
        gtk_entry_set_text(GTK_ENTRY(message_entry), "");
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9002);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) return -1;

    char buffer_in[1024];
    memset(buffer_in, 0, 1024);
    read(sock, buffer_in, 1024);
    cout << buffer_in;
    string mode;
    getline(cin, mode);
    send(sock, mode.c_str(), mode.length(), 0);

    for (int i = 0; i < 2; i++) {
        memset(buffer_in, 0, 1024);
        read(sock, buffer_in, 1024);
        cout << buffer_in;
        string input;
        getline(cin, input);
        send(sock, input.c_str(), input.length(), 0);
    }

    memset(buffer_in, 0, 1024);
    read(sock, buffer_in, 1024);
    cout << buffer_in << endl;

    if (string(buffer_in).find("Successful") == string::npos) {
        close(sock);
        return -1;
    }

    string monitor_enable = "/monitor on";
    send(sock, monitor_enable.c_str(), monitor_enable.length(), 0);

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "NeuroChat | Badar | Shayan | Aarib");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_hbox);

    // Left Side: Chat Area
    GtkWidget *chat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_hbox), chat_vbox, TRUE, TRUE, 5);

    chat_buffer_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_buffer_view), FALSE);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_buffer_view));
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), chat_buffer_view);
    gtk_box_pack_start(GTK_BOX(chat_vbox), scroll, TRUE, TRUE, 5);

    // Add welcome message
    update_chat("========================================");
    update_chat("Welcome to NeuroChat!");
    update_chat("Commands: /join room, /msg user text, /monitor on|off");
    update_chat("========================================");

    message_entry = gtk_entry_new();
    g_signal_connect(message_entry, "activate", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(chat_vbox), message_entry, FALSE, FALSE, 5);
    
    GtkWidget *send_btn = gtk_button_new_with_label("Send Message");
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(chat_vbox), send_btn, FALSE, FALSE, 5);

    // Right Side: OS Monitor Panel (Requirement #6)
    GtkWidget *monitor_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(main_hbox), monitor_vbox, FALSE, FALSE, 10);
    
    gtk_box_pack_start(GTK_BOX(monitor_vbox), gtk_label_new("--- OS MONITOR PANEL ---"), FALSE, FALSE, 5);
    thread_label = gtk_label_new("Thread Pool: [ ACTIVE ]");
    gtk_box_pack_start(GTK_BOX(monitor_vbox), thread_label, FALSE, FALSE, 5);
    mutex_label = gtk_label_new("Mutex: [ UNLOCKED ]");
    gtk_box_pack_start(GTK_BOX(monitor_vbox), mutex_label, FALSE, FALSE, 5);
    semaphore_label = gtk_label_new("Semaphore Slots: waiting for server status");
    gtk_box_pack_start(GTK_BOX(monitor_vbox), semaphore_label, FALSE, FALSE, 5);
    activity_label = gtk_label_new("Last Event: connected");
    gtk_box_pack_start(GTK_BOX(monitor_vbox), activity_label, FALSE, FALSE, 5);

    pthread_t tid;
    pthread_create(&tid, NULL, receiver, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
