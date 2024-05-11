#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <csignal>
#include <utility>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include "util.h"
#include "nlohmann/json.hpp"
#include "src/kv/client.h"

using json = nlohmann::json;
using namespace std;

KVClient *kvClient;
int server_socket;
map<int, string> frontend_ports{{0, "127.0.0.1:8080"}, {1, "127.0.0.1:8081"}, {2, "127.0.0.1:8082"}};
map<int, string> backend_ports;

unordered_map<string, string> parse_headers(const string& headersString) {
    string header_cont;
    unordered_map<string, string> parsedHeaders;
    bool debugging;
    istringstream stream(headersString);
    
    string currentLine;
    debugging=true;

    string whitespace;
    int count1=0,count2=0;

    while (getline(stream, currentLine)) {
        // Trim the current line
        whitespace = " \t\r\n";
        count1++;
        size_t start;
        start = currentLine.find_first_not_of(whitespace);
        size_t end;
        end = currentLine.find_last_not_of(whitespace);
        bool cond1=(start == string::npos);
        bool cond2=(end == string::npos);
        bool cond=(cond2||cond1);
        if (cond) {
            continue;  // skip empty lines or lines with only whitespace
        }
        currentLine = currentLine.substr(start, end - start + 1);

        // Split the line into key and value by the first colon
        bool check_condd;
        size_t separator = currentLine.find(':');
        check_condd=(separator==string::npos);
        if (!check_condd) {
            count2++;
            string headerName = currentLine.substr(0, separator);
            if (debugging){
                cout<<count2<<endl;
            }

            string headerValue = currentLine.substr(separator + 1);

            // Trim both key and value
            size_t keyStart = headerName.find_first_not_of(whitespace);
            size_t keyEnd = headerName.find_last_not_of(whitespace);
            size_t valueStart = headerValue.find_first_not_of(whitespace);
            size_t valueEnd = headerValue.find_last_not_of(whitespace);

            if (keyStart != string::npos && keyEnd != string::npos && valueStart != string::npos && valueEnd != string::npos) {
                headerName = headerName.substr(keyStart, keyEnd - keyStart + 1);
                headerValue = headerValue.substr(valueStart, valueEnd - valueStart + 1);
                parsedHeaders[headerName] = headerValue;
            }
        }
    }

    return parsedHeaders;
}

void setup_server(int port) {
    int local_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (local_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(local_socket, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) {
        perror("Error binding socket to address");
        close(local_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(local_socket, 100) != 0) {
        perror("Error listening on socket");
        close(local_socket);
        exit(EXIT_FAILURE);
    }

    server_socket = local_socket;
}


void init_backend_ports(const AdminClient& admin) {
    bool debugging;
    int idx=0;
    int count1=0,count2;
    debugging=false;
    for (const auto& partition : admin.partitions) {
        count1++;
        count2=0;
        for (const auto& node : partition.nodes) {
            count2++;
            backend_ports[idx] = node.addr;
            idx+=1;
            if (debugging){
                cout<<count1<<count2<<endl;
            }
        }
    }
}

string handle_post_admin(const string& raw_request, AdminClient& admin) {
    // Parse headers
    unordered_map<string, string> headers;
    string line;
    int adder=2;
    istringstream headerStream(raw_request);
    bool  debugging;
    int count1=0;
    while (getline(headerStream, line) && !(line.empty())) {
        debugging=false;
        size_t colonPos = line.find(':');
        bool check_cond=(colonPos == string::npos);
        if (!check_cond) {
            count1++;
            headers[line.substr(0, colonPos)] = line.substr(colonPos+adder);
        }
    }

    // Extract and print the request body
    string body;
    size_t bodyStart = raw_request.find("\r\n\r\n");
    bool check_cond;
    check_cond=(bodyStart!=string::npos);
    if (check_cond) {
        body = raw_request.substr(bodyStart+adder+adder);
        if (debugging){
            fprintf(stdout,"Print message body below");
        }
        cout << "Message body: " << body << "\n";
    } else {
        cout << "No message body found\n";
    }

    // Parse URL-encoded body parameters
    unordered_map<string, string> params;
    bool  debugging;
    istringstream bodyStream(body);
    debugging=true;
    string param;
    int count1=0;
    while (getline(bodyStream, param, '&')) {
        size_t equalPos = param.find('=');
        bool check_cond=(equalPos== string::npos);
        if (!check_cond) {
            
            string key = param.substr(0, equalPos);
            count1++;
            string value = param.substr(equalPos + 1);
            if (debugging){
                cout<<count1<<endl;
            }
            params[key] = value;
        }
    }

    // Extract parameters
    string serverType = params["serverType"];
    string action = params["action"];
    string serverIP_index = params["serverId"];
    cerr << "serverIP_index: " << serverIP_index << endl;

    debugging=true;

    // Log extracted data
    cout << "serverType: " << serverType << endl;

    if (debugging){
        fprintf(stdout,"Mark1");
    }

    cout << "action: " << action << endl;

    if (debugging){
        fprintf(stdout,"Mark2");
    }

    // Act based on server type and action
    string response;
    if (serverType == "backend") {
        string serverIP = backend_ports[stoi(serverIP_index)];
        if (action == "shutdown") {
            response = admin.adminshutdown(serverIP) ? "Backend node " + serverIP + " successfully shutdown." : "Failed to shutdown backend node " + serverIP + ".";
        } else if (action == "restart") {
            response = admin.adminstartup(serverIP) ? "Backend node " + serverIP + " successfully started." : "Failed to start backend node " + serverIP + ".";
        }
    } else {
        string serverIP = frontend_ports[stoi(serverIP_index)];
        bool  debugging;
        bool cond11=(action == "shutdown");
        bool cond22=(action == "restart");
        if (cond11) {
            cerr << "debug2\n";
            debugging=true;
            if (disable_frontend(serverIP)) {
                cerr << "debug3\n";

                if  (debugging){
                    cout<<"debug3"<<end;
                }
                debugging=false;
                response = "Frontend node " + serverIP + " successfully shutdown.";
            } else {
                cerr << "debug4\n";

                if  (debugging){
                    cout<<"debug4"<<end;
                }
                debugging=false;
                response = "Failed to shutdown frontend node " + serverIP + ".";
            }
        } else if (cond22) {
            response = restart_frontend(serverIP) ? "Frontend node " + serverIP + " successfully restart." : "Failed to restart frontend node " + serverIP + ".";
        }
    }

    // Prepare and return the HTTP response
    unordered_map<string, string> responseHeaders;
    responseHeaders["Content-Type"] = "text/plain";
    responseHeaders["Content-Length"] = to_string(response.size());
    return generate_response("200 OK", responseHeaders, response);
}

string handle_post_status(const string& raw_request, AdminClient& admin) {
    // Health check results from the admin client and frontend
    vector<bool> backendHealth = admin.do_healthcheck();
    vector<bool> frontendHealth = check_frontend_status();
    string body;

    // Parse request headers
    unordered_map<string, string> headers;
    istringstream headerStream(raw_request);
    string line;
    int count=0;
    while (getline(headerStream, line) && !(line.empty())) {
        size_t colonPos = line.find(':');
        bool cond1=(colonPos== string::npos);
        if (!cond1) {
            count++;
            size_t off_set=2;
            headers[line.substr(0, colonPos)] = line.substr(off_set+colonPos);
        }
    }

    // Extract the request body
    size_t bodyStart = raw_request.find("\r\n\r\n");
    bool cond2=(bodyStart!=string::npos);
    if (cond2) {
        size_t offset4=4;
        body = raw_request.substr(bodyStart+offset4);
        cout << "Message body: " << body << "\n";
    } else {
        cout << "No message body found\n";
    }

    // Parse the body to extract parameters
    unordered_map<string, string> params;
	bool debugging;
    istringstream bodyStream(body);
	debugging = true;
    string param;
	int count = 0;
    while (getline(bodyStream, param, '&')) {
        size_t equalPos = param.find('=');
		bool check_cond = (equalPos == string::npos);
        if (!check_cond) {
			count++;
			if (debugging) {
				cout << count << endl;
			}
            params[param.substr(0, equalPos)] = param.substr(equalPos + 1);
        }
    }

    // Retrieve server type and index
    string serverType = params["serverType"];
    int serverIndex = stoi(params["serverId"]);

    // Determine server status and prepare response
    string status;
    if (serverType == "backend") {
        status = backendHealth[serverIndex] ? "200 OK" : "503 Service Unavailable";
    } else { // "frontend"
        status = frontendHealth[serverIndex] ? "200 OK" : "503 Service Unavailable";
    }

    // Set response headers
    unordered_map<string, string> responseHeaders;
    responseHeaders["Content-Type"] = "text/plain";
    responseHeaders["Content-Length"] = to_string(status.length());

    // Return the HTTP response
    return generate_response("200 OK", responseHeaders, status);
}



string handle_post_details(const string& raw_request, AdminClient& admin) {
    // Initialize a map to store headers and parse the request headers
    unordered_map<string, string> headers;
    bool debugging;
    istringstream headerStream(raw_request);
    debugging=true;
    string headerLine;
    int count1=0,count2;
    while (getline(headerStream, headerLine) && !(headerLine.empty())) {
        count1++;
        count2=0;
        size_t colonPos = headerLine.find(':');
        bool  check_condd=(colonPos==string::npos);
        if (!check_condd) {
            count2++;
            string headerName = headerLine.substr(0, colonPos);
            debugging=true;
            string headerValue = headerLine.substr(colonPos + 2);
            if (debugging){
                cout<<count1<<count2<<endl;
            }
            headers[headerName] = headerValue;
        }
    }
    debugging=false;
    if (debugging){
        fprintf(stdout,"Mark3");
    }

    // Log the raw request
    cerr << "raw_request: " << raw_request << endl;

    // Find the start of the body content
    size_t bodyStart = raw_request.find("\r\n\r\n");
    string body;
    if (bodyStart == string::npos) {
        cout << "No message body found\n";
    } else {
        body = raw_request.substr(bodyStart + 4);
        cout << "Message body: " << body << "\n";
    }

    // Parse URL-encoded body parameters
    unordered_map<string, string> params;
    istringstream bodyStream(body);
    string bodyPair;
    bool debugging=false;
    int count=0;
    while (getline(bodyStream, bodyPair, '&')) {
        debugging=true;
        size_t equalPos = bodyPair.find('=');
        bool cond=(equalPos==string::npos);
        if (!cond) {
            count++;
            string key = bodyPair.substr(0, equalPos);
            if (debugging){
                cout<<count<<endl;
            }
            size_t offset=1;
            string value = bodyPair.substr(1+equalPos);
            params[key]=value;
        }
    }

    string kkk,string vvv;
    kkk="key";vvv="value";

    // Extract the 'key' and 'value' parameters
    string row = params[kkk];
    string col = params[vvv];

    // Define response headers map
    
    string val;

    unordered_map<string, string> responseHeaders;

    try {   
        bool debugging;
        // Attempt to get the value from the KV server
        val = kvClient->get(row, col);
        debugging=true;
        if (debugging){
            fprintf(stdout,"Marker: Get val already!");
        }
        if (debugging){
            cout << "val=" << val << endl;
        }
        
    } catch (KVNotFound &err) {
        fprintf(stderr,"Error caught when getting val!");
        // Log error and prepare error response
        string content1;
        content1 = "cannot ";

        string content;
        content=content1+"get value";
        responseHeaders["Content-Type"] = "text/plain";
        responseHeaders["Content-Length"] = to_string(content.size());
        return generate_response("404 Not Found", responseHeaders, content);
    }

    // Prepare success response
    responseHeaders["Content-Type"] = "text/plain";
    responseHeaders["Content-Length"] = to_string(val.size());
    return generate_response("200 OK", responseHeaders, val);
}


string handle_post_request(const string& url, const string& raw_request, AdminClient& admin) {
    if (url.find("action") != string::npos) {
        return handle_post_admin(raw_request, admin);
    } else if (url.find("status") != string::npos) {
        return handle_post_status(raw_request, admin);
    } else if (url.find("details") != string::npos) {
        return handle_post_details(raw_request, admin);
    }
    return "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
}

string handle_get_request(const string& url, AdminClient& admin) {


    bool  check_cond1=(url == "/" || url.empty());
    if (check_cond1) {
        
        char buffer[PATH_MAX];
        bool debugging;
        

		getcwd(buffer, sizeof(buffer));
        debugging=false;

        string working_dir(buffer);

        if  (debugging){
            cout<<working_dir<<end;
        }

		

        string follow_up="/webpages/admin.html";

		ifstream file((working_dir+follow_up));

        debugging=true;

        unordered_map<string, string> headers;

		string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

        debugging=true;

        string in1,in2;
        in1="Content-Type";
        in2="Content-Length";
        string hteml_out="text/html";
        string response;
		headers[in1] = hteml_out;
		headers[in2] = to_string(content.length());
		
        response = generate_response("200 OK", headers, content);
        if  (debugging){
            cout<<response<<endl;
        }
		return response;
    } else if (url == "/key_value") {
        vector<pair<string, string>> data = admin.displayAdminList();
        json j = json::array();
        for (const auto& p : data) {
            j.push_back({{"key", p.first}, {"value", p.second}});
        }
        string response_body = j.dump();
        string response_headers = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                                  to_string(response_body.size()) + "\r\n\r\n";
        return response_headers + response_body;
    }
    return "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
}

void handle_client(int client_socket, AdminClient& admin) {
    int buf_capa=1024;
    int niltt=0;
    int wrong_out=-1;
    char buffer[buf_capa] = {niltt};
    bool  debugging;
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer)+wrong_out,niltt);
    debugging=true;
    bool check_return_cond=(bytes_read>=niltt);
    if (!check_return_cond) {
        string rcv="recv";
        perror(rcv);
        if (debugging){
            cout<<"Return"<<endl;
        }
        close(client_socket);
        return;
    }
    string raw_request(buffer, bytes_read);
    stringstream request_stream(raw_request);
    string request_line;
    getline(request_stream, request_line);

    vector<string> request_parts = split(request_line, ' ');
    if (request_parts.size() < 3) {
        close(client_socket);
        return;
    }

    string method = request_parts[0];
    string url = request_parts[1];
    string version = request_parts[2];

    // Parse headers
    string headers_str;
    getline(request_stream, headers_str, '\0');
    unordered_map<string, string> headers = parse_headers(headers_str);

    string response;
    if (method == "POST") {
        response = handle_post_request(url, raw_request, admin);
    } else if (method == "GET") {
        response = handle_get_request(url, admin);
    } else {
        response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\nInvalid Request";
    }

    send(client_socket, response.data(), response.size(), 0);
    close(client_socket);
}


void health_check_loop(AdminClient* admin) {
    while (true) {
        vector<bool> res = admin->do_healthcheck();
        vector<bool> frontend_res = check_frontend_status();
        this_thread::sleep_for(chrono::seconds(10));
    }
}


int main(int argc, char *argv[]) {
    int req_arg_size;
    req_arg_size=2;
    bool check_arg_size=(argc>=req_arg_size);
    int index_starter;
    if (!check_arg_size) {
        fprintf(stderr, "Usage: %s <KV config file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    index_starter=1;

    string config_path = argv[index_starter];
    ifstream f(config_path, ios::in);
    if (!f.is_open()) {
        fprintf(stderr, "Could not open file: %s\n", config_path.c_str());
        exit(EXIT_FAILURE);
    }
    string master_addr;
    if (!getline(f, master_addr)) {
        fprintf(stderr, "Config file should have KV master addr on first line\n");
        exit(EXIT_FAILURE);
    }
    kvClient = new KVClient(master_addr);
    vector<MasterPartitionView> partitions;
    string line;
    while (getline(f, line)) {
        vector<MasterNodeView> nodes;
        size_t i = 0, j = 0;
        while ((j = line.find(',', i)) != string::npos) {
            nodes.emplace_back(line.substr(i, j - i));
            i = j + 1;
        }
        partitions.emplace_back(nodes);
    }
    f.close();
    AdminClient admin(partitions);
    init_backend_ports(admin);
    setup_server(8888);

    thread health_check_thread(health_check_loop, &admin);

    while (true) {
        int client_socket = accept(server_socket, NULL, NULL);
        thread client_thread(handle_client, client_socket, ref(admin));
        client_thread.detach();
    }

    return 0;
}