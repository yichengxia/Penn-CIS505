#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <climits>
#include <map>
#include <sstream>
#include <thread>
#include <chrono>

using namespace std;

class LoadBalancer {
    uint16_t port0=8080;
    uint16_t port1=8081;
    uint16_t port2=8082;
    int nilt=0;
    uint16_t port_lb=8090;
public:
    
    LoadBalancer() : frontend_ports{{port0,nilt}, {port1,nilt}, {port2,nilt}} {
        for (const auto &entry : frontend_ports) {
            frontend_status[entry.first] = true;
        }
    }

    void run() {
        start_update_thread();
        start_server(port_lb);
    }

private:
    map<uint16_t, int> frontend_ports;
    map<uint16_t, bool> frontend_status;
    mutex mtx;

    vector<string> split(const string &str, char delimiter) {
    vector<string> tokens;
    string token;

    for (char ch : str) {
        if (ch == delimiter) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
            token.clear();
        } else {
            token += ch;
        }
    }

    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens;
}


    uint16_t get_next_frontend_port() {
    lock_guard<mutex> lock(mtx);
    uint16_t selected_port = 0;
    int min_count = INT_MAX;

    for (const auto &entry : frontend_ports) {
        const uint16_t &port = entry.first;
        const int &count = entry.second;

        if (frontend_status[port] && count < min_count) {
            selected_port = port;
            min_count = count;
        }
    }

    if (selected_port) {
      // Selected port is alive and has the least number of clients
        frontend_ports[selected_port]++;
    } else {
        selected_port = frontend_ports.begin()->first;
        frontend_ports[selected_port]++;
    }

    return selected_port;
}


    bool is_frontend_alive(uint16_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return false;
        }

        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;

        bool out_bool;

        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        out_bool=false;
        int output;

        output = connect(sock, (sockaddr*)&addr, sizeof(addr));
        close(sock);
        
        out_bool=(output==0);
        return out_bool;
    }

    void update_frontend_servers() {
        while (true) {
            lock_guard<mutex> lock(mtx);
            for (const auto &entry : frontend_ports) {
                uint16_t port = entry.first;
                frontend_status[port] = is_frontend_alive(port);
            }
            this_thread::sleep_for(chrono::seconds(5));
        }
    }

    void handle_client(int client_socket) {
    try {
        uint16_t frontend_port = get_next_frontend_port();

        // Receive the client request
        constexpr size_t buffer_size = 1024;
        char buffer[buffer_size] = {0};
        ssize_t bytes_read = recv(client_socket, buffer, buffer_size - 1, 0);
        if (bytes_read <= 0) {
            cerr << "recv failed or connection closed" << endl;
            close(client_socket);
            return;
        }

        // Parse URL
        string raw_request(buffer, bytes_read);
        auto request_lines = split(raw_request, '\n');
        string request_line = !request_lines.empty() ? request_lines[0] : "";
        auto request_parts = split(request_line, ' ');
        string url = (request_parts.size() > 1) ? request_parts[1] : "";

        size_t pos = url.find("/", url.find("/", url.find("/") + 1) + 1);
        string path = (pos != string::npos) ? url.substr(pos) : "/";

        //Create redirect response
        string redirect_response = "HTTP/1.1 302 Found\r\n";
        redirect_response += "Location: http://127.0.0.1:" + to_string(frontend_port) + path + "\r\n";
        redirect_response += "Connection: close\r\n\r\n";

        // Step 5: Send the redirect response
        ssize_t bytes_sent = send(client_socket, redirect_response.c_str(), redirect_response.size(), 0);
        if (bytes_sent < 0) {
            cerr << "Failed to send redirect response" << endl;
        }

        close(client_socket);
    } catch (const exception &e) {
        cerr << "Exception in handle_client: " << e.what() << endl;
    }
}


    void start_update_thread() {
        thread update_thread(&LoadBalancer::update_frontend_servers, this);
        update_thread.detach();
    }

    void start_server(uint16_t port) {
        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            cerr << "Failed to create server socket" << endl;
            return;
        }

        sockaddr_in server_address;
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_port = htons(port);
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);

        if (::bind(server_socket, (sockaddr *)&server_address, sizeof(server_address)) < 0) {
            cerr << "Failed to bind server socket" << endl;
            close(server_socket);
            return;
        }

        if (listen(server_socket, 10) < 0) {
            cerr << "Failed to listen on server socket" << endl;
            close(server_socket);
            return;
        }

        while (true) {
            int client_socket = accept(server_socket, nullptr, nullptr);
            if (client_socket < 0) {
                cerr << "Failed to accept client connection" << endl;
            } else {
                thread client_thread(&LoadBalancer::handle_client, this, client_socket);
                client_thread.detach();
            }
        }

        close(server_socket);
    }
};

int main() {
    LoadBalancer lb;
    lb.run();
    return 0;
}
