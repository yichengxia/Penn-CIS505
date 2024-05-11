#include "frontend_server.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unistd.h>
#include <vector>

using namespace std;

KVClient *kvClient;
SessionManager *session_manager;
bool serving = true;
string server_addr = "127.0.0.1:5000";
int server_socket;
int port;
mutex client_mutex;
#define BUFFER_SIZE 4096

feServer::feServer(int port){
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("socket bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, 10) < 0) {
        perror("socket listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    run_server();
}

void feServer::run_server() {
    kvClient = new KVClient(server_addr);
    session_manager = new SessionManager();

    while (serving) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1) {
            perror("Failed to accept client");
            continue;
        }
        thread client_thread(&feServer::handle_client, this, client_socket);
        client_thread.detach();
    }
}

void feServer::handle_client(int client_socket) {
    lock_guard<mutex> lock(client_mutex);
    unique_ptr<char[]> buffer(new char[BUFFER_SIZE]);
    memset(buffer.get(), 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_socket, buffer.get(), BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        perror("recv failed");
        close(client_socket);
        return;
    }

    string raw_request(buffer.get(), bytes_read);
    RequestHandler request_handler(session_manager, kvClient, serving);
    string response = request_handler.handle_request(raw_request);

    int nilt=0;
    size_t resp_size=response.size();
    send(client_socket, response.data(), resp_size,nilt);

    fprintf(stdout, "Closing socket!");

    close(client_socket);
}



int main(int argc, char *argv[]) {

    int req_arg_size=2;

    bool check_input=(argc==req_arg_size);

    if (!(check_input)) {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return EXIT_FAILURE;
    }
    int port = stoi(argv[1]);
    if (port <= 0) {
        cerr << "Invalid port number: " << port << endl;
        return EXIT_FAILURE;
    }
    feServer server(port);
    return EXIT_SUCCESS;
}
