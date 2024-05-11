#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iostream>
#include <utility>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <grpcpp/grpcpp.h>
#include <fstream>
#include <random>

using grpc::ClientContext;
using grpc::Status;
using grpc::Channel;

#include "src/kv/master_client.h"
#include "src/protos/kv.grpc.pb.h"

class AdminClient : public KVMasterClient {
public:
    explicit AdminClient(vector<MasterPartitionView>& partitions)
        : KVMasterClient(partitions) {}
};

class SessionManager {
public:
    std::string create_session(const std::string& username) {
        std::string session_id = generate_session_id();
        std::unordered_map<std::string, std::string> session_data{{"username", username}};
        sessions_[session_id] = std::move(session_data);
        return session_id;
    }

    std::unordered_map<std::string, std::string> get_session(const std::string& session_id) const {
        auto it = sessions_.find(session_id);
        return it != sessions_.end() ? it->second : std::unordered_map<std::string, std::string>{};
    }

    void update_session(const std::string& session_id, const std::unordered_map<std::string, std::string>& data) {
        sessions_[session_id] = data;
    }

    void delete_session(const std::string& session_id) {
        sessions_.erase(session_id);
    }

    std::string get_cookie_value(const std::string& session_id) const {
        return "session_id=" + session_id + "; HttpOnly; Secure";
    }

    bool session_check(const std::string& session_id) const {
        return sessions_.find(session_id) != sessions_.end();
    }

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sessions_;
    random_device rd;
    mt19937 gen{rd()};
    std::uniform_int_distribution<int> dis{0, 15};

   std::string generate_session_id() {
        std::string session_id(32, ' ');
        generate(session_id.begin(), session_id.end(), [this] { return hex_digit(); });
        return session_id;
    }

    char hex_digit() {
        static const char* digits = "0123456789abcdef";
        return digits[dis(gen)];
    }
};

std::string generate_response(const std::string &status_code, const std::unordered_map<std::string, std::string> &headers, const std::string &content) {
    std::stringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    for (const auto& [key, value] : headers) {
        response << key << ": " << value << "\r\n";
    }
    response << "\r\n" << content;
    return response.str();
}

std::string trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    return start == std::string::npos ? "" : str.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> get_frontend_nodes() {
    // Dummy implementation
    return {"127.0.0.1:8080", "127.0.0.1:8081", "127.0.0.1:8082"};
}

std::vector<bool> check_frontend_status() {
    // Dummy implementation
    return {true, false, true};
}

bool disable_frontend(const std::string& node) {
    // Dummy implementation
    std::cout << "Disabling frontend: " << node << std::endl;
    return true;
}

bool restart_frontend(const std::string& node) {
    // Dummy implementation
    std::cout << "Restarting frontend: " << node << std::endl;
    return true;
}
