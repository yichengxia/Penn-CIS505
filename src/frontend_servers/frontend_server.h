#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include "req_handling.h"

class feServer {
public:
    feServer(int port);

private:
    void run_server();
    void handle_client(int client_socket);
};
