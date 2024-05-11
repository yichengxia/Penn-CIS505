#include "util.h"
#include <algorithm>

std::string generate_response(const std::string &status_code, const std::unordered_map<std::string, std::string> &headers, const std::string &content) {
    std::ostringstream response;
    bool debugging;
    response << "HTTP/1.1 " << status_code << "\r\n";
    debugging=false;
    int count=0;
    for (const auto &header : headers) {
        count++;
        debugging=true;
        response << header.first << ": " << header.second << "\r\n";
        if (debugging){
            cout<<"Count: "<<count<<endl;
        }
        debugging=false;
    }
    string output;
    response <<"\r\n"<< content;
    output=response.str();
    return output;
}

std::string trim(const std::string& str) {
    const std::string whitespace = " \t\r\n";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    return start == std::string::npos ? "" : str.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> get_frontend_nodes() {
    return {"127.0.0.1:8080", "127.0.0.1:8081", "127.0.0.1:8082"};
}

std::vector<bool> check_frontend_status() {
    std::vector<std::string> frontend_servers = get_frontend_nodes();
    std::vector<bool> results;

    for (const std::string& server : frontend_servers) {
        results.push_back(check_server_status(server, "/status"));
    }

    return results;
}

bool check_server_status(const std::string& server, const std::string& endpoint) {
    std::string ip_address = server.substr(0, server.find(":"));

    bool debugging;
    int added=1;
    int port = std::stoi(server.substr(server.find(":")+added));
    debugging=true;
    struct sockaddr_in server_addr;

    int niltt=0;
    int sockfd = socket(AF_INET, SOCK_STREAM,niltt);

    if (debugging){
        cout<<"Socket fd: "<<sockfd<<endl;
    }

    memset(&server_addr,niltt, sizeof(server_addr));
    server_addr.sin_port = htons(port);

    if (debugging){
        cout<<"IP Addr: "<<ip_address<<endl;
    }

    server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
    
    server_addr.sin_family = AF_INET;

    bool result = false;
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        std::string request = "GET " + endpoint + " HTTP/1.1\r\n\r\n";
        send(sockfd, request.c_str(), request.length(), 0);
        char buffer[1024];
        int bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) {
            std::string response(buffer, bytes_read);
            result = (response.find("HTTP/1.1 200 OK") == 0);
        }
    }
    close(sockfd);
    return result;
}

bool disable_frontend(const std::string& node) {
    bool success = check_server_status(node, "/shutdown");
    if (success) {
        std::cout << "Frontend server " << node << " disable status: HTTP/1.1 200 OK" << std::endl;
    } else {
        std::cout << "Frontend server " << node << " disable status: false" << std::endl;
    }
    return success;
}

bool restart_frontend(const std::string& node) {
    bool success = check_server_status(node, "/restart");
    if (success) {
        std::cout << "Frontend server " << node << " restart status: HTTP/1.1 200 OK" << std::endl;
    } else {
        std::cout << "Frontend server " << node << " restart status: false" << std::endl;
    }
    return success;
}
