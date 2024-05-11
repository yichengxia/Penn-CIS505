#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "../mailserver/mailhelper.h"
#include "request_handler.h"

using namespace std;

bool global_debugging_label;

// For debugging
global_debugging_label=false;



string RequestHandler::generate_response(const string &status_code, const unordered_map<string, string> &headers, const string &content) {
    int resp_iD;
    string response;
    response.append("HTTP/1.1 ").append(status_code).append("\r\n");
    for (const auto &header : headers) {
        response.append(header.first).append(": ").append(header.second).append("\r\n");
    }
    response.append("\r\n").append(content);
    return response;
}


string RequestHandler::generate_standard_response(const string &status, const string &content) {
    unordered_map<string, string> headers;
    string in1="Content-Type";
    string in2;
    string in_app="text/plain";
    headers.insert(make_pair(in1, in_app));
    in2="Content-Length";
    headers.insert(make_pair(in2,to_string(content.size())));
    global_debugging_label=false;
    return generate_response(status, headers, content);
}



string RequestHandler::generate_cors_response(const string &status, const string &content) {
    unordered_map<string, string> responseHeaders = {
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Content-Type", "text/plain"},
        {"Content-Length", to_string(content.length())}
    };
    return generate_response(status, responseHeaders, content);
}


string RequestHandler::handle_shutdown_requests(const string &method, const string &url, unordered_map<string, string> &headers) {
    if (method == "GET") {
        if (url.find("/status") == 0) {
            return generate_standard_response("200 OK", "503 Service Unavailable\n");
        } else if (url.find("/restart") == 0) {
            serving = true;
            return generate_standard_response("503 Service Unavailable", "Successfully restart server");
        }
    }
}

string RequestHandler::handleGetRequests(const string &url, const string &working_dir, const string &raw_request, unordered_map<string, string> &headers) {
    // Handle root or home request
    if (url.length() <= 1 && (url == "/" || url == "")) {
        return serve_webpage(working_dir + "/webpages/homepage.html");
    }

    // Serve images
    if (url.find("/images") != string::npos) {
        return serve_image(working_dir + "/webpages" + url);
    }

    // Hello page with session management
    if (url.find("/hello") != string::npos) {
        string sid = get_cookie_from_header(headers);
        if (session_manager_->session_check(sid)) {
            unordered_map<string, string> session_data = session_manager_->get_session(sid);
            return generate_standard_response("200 OK", session_data["username"]);
        }
        return generate_standard_response("200 OK", "401 Unauthorized");
    }

    // Admin placeholder
    if (url.find("/admin") != string::npos) {
        return generate_standard_response("200 OK", "200 OK");
    }

    // Login and registration pages
    if (url.find("/login") != string::npos) {
        return serve_webpage(working_dir + "/webpages/login.html");
    }
    if (url.find("/logout") != string::npos) {
        string sid = get_cookie_from_header(headers);
        if (session_manager_->session_check(sid)) {
            session_manager_->delete_session(sid);
        }
        return serve_webpage(working_dir + "/webpages/homepage.html");
    }
    if (url.find("/register") != string::npos) {
        return serve_webpage(working_dir + "/webpages/register.html");
    }
    if (url.find("/reset_password") != string::npos) {
        return serve_webpage(working_dir + "/webpages/reset_password.html");
    }

    // Webmail and storage services with session check
    if (url.find("/webmail") != string::npos || url.find("/storage") != string::npos) {
        string sid = get_cookie_from_header(headers);
        if (!session_manager_->session_check(sid)) {
            return serve_webpage(working_dir + "/webpages/login.html");
        }
        string page = url.find("/webmail") != string::npos ? "webmail" : "storage";
        return serve_webpage(working_dir + "/webpages/" + page + ".html");
    }

    // Server control (shutdown) and status
    if (url.find("/shutdown") != string::npos) {
        serving = false;
        return generate_standard_response("503 Service Unavailable", "503 Service Unavailable\n");
    }
    if (url.find("/status") != string::npos) {
        return generate_standard_response("200 OK", "OK STATUS\n");
    }

    // Session check for remaining services
    string sid = get_cookie_from_header(headers);
    if (!session_manager_->session_check(sid)) {
        return generate_cors_response("200 OK", "401 Unauthorized");
    }

    // Handling specific webmail pages and file operations
    if (url.find("/inbox") != string::npos) {
        return handle_get_webmail(sid, "inbox");
    }
    if (url.find("/deleted") != string::npos) {
        return handle_get_webmail(sid, "deleted");
    }
    if (url.find("/sent") != string::npos) {
        return handle_get_webmail(sid, "sent");
    }
    if (url.find("/files") != string::npos) {
        return handle_view_file(sid);
    }
    if (url.find("/downloadfile") != string::npos) {
        return handle_download_file(sid, raw_request, url);
    }

    // Default response for undefined routes
    return generate_standard_response("404 Not Found", "404 GET Not Found");
}


string RequestHandler::handlePostRequests(const string &url, const string &raw_request, unordered_map<string, string> &headers) {
    if (url.find("/shutdown")) {
        return generate_standard_response("503 Service Unavailable", "503 Service Unavailable\n");

    } else if (url == "/login" || url == "/register" || url == "/reset_password") {
        return handle_post_accounts(raw_request, url);
    }
    string sid = get_cookie_from_header(headers);
    if (!session_manager_->session_check(sid)) {
        return generate_cors_response("200 OK", "401 Unauthorized");

    } else if (url.find("/emails")) {
        return handle_post_webmail(sid, raw_request);

    } else if (url.find("/deleteemails")) {
        return handle_delete_webmail(sid, raw_request, url);

    } else if (url.find("/uploadfile")) {
        return handle_upload_file(sid, raw_request);

    } else if (url.find("/createfolder")) {
        return handle_create_folder(sid, raw_request);

    } else if (url.find("/movefile")) {
        return handle_move_file(sid, raw_request);

    } else if (url.find("/movefolder")) {
        return handle_move_folder(sid, raw_request);

    } else if (url.find("/rename")) {
        return handle_rename(sid, raw_request);

    } else if (url.find("/deletefolder")) {
        return handle_delete_folder(sid, raw_request);

    } else if (url.find("/deletefile")) {
        return handle_delete_file(sid, raw_request);    

    } else {
        return generate_standard_response("404 Not Found", "404 POST Not Found");
    }
}

tuple<string, string, string, unordered_map<string, string>> parse_request(const string &raw_request) {

    bool debugging;

    // Retrieve the current directory
    char dir_buffer[PATH_MAX];
    debugging=true;
    getcwd(dir_buffer, sizeof(dir_buffer));

    if (debugging){
        cout<<"Raw: "<<raw_request<<endl;
    }

    string current_dir(dir_buffer);

    // Initialize a stream from the raw request data
    stringstream request_stream(raw_request);
    debugging=false;
    string line="";

    if (debugging){
        cout<<"Showing Raw: "<<raw_request<<endl;
    }
    getline(request_stream, line);

    bool debugging=true;

    // Split the request line into components

    unordered_map<string, string> header_map;

    int acc=0;

    vector<string> parts = split(line, ' ');

    string request_method = parts[acc];

    if (debugging){
        cout<<request_method<<endl;
    }

    string request_url = parts[(++acc)];

    if (debugging){
        cout<<request_method<<endl;
    }

    // Container for the headers
    
    string header_entry = "";
    int count3=0,count4=0;
    while (getline(request_stream, header_entry, '\r')) {
        request_stream.ignore(1);  // Skip newline following carriage return
        bool check_cond;
        size_t pos = header_entry.find(':');
        check_cond=(pos == string::npos);

        if (!check_cond) {
            count3++;
            string key = header_entry.substr(0, pos);
            size_t offset2=2;
            string value = header_entry.substr(offset2+pos);  // Skip colon and space
            count4++;
            header_map[key] = value;
        }
    }
    return {request_method, request_url, current_dir, header_map};
}


string RequestHandler::handle_request(const string &raw_request) {
    auto [method, url, working_dir, headers] = parse_request(raw_request);
    if (!serving) {
        return handle_shutdown_requests(method, url, headers);
    } else {
        if (method == "GET") {
            return handleGetRequests(url, working_dir, raw_request, headers);            
        } else if (method == "POST") {
            return handlePostRequests(url, raw_request, headers);
        } else if (method == "DELE") {
            return generate_standard_response("404 Not Found", "404 DELETE Not Found");
        } else {
            return generate_standard_response("404 Not Found", "404 Not Found");
        }
    }
}

vector<string> RequestHandler::split(const string &str, char delimiter) {
    vector<string> tokens;
    string token;
    for (size_t pos = 0, next_pos = 0; next_pos != string::npos; pos = next_pos + 1) {
        next_pos = str.find(delimiter, pos);
        token = str.substr(pos, next_pos - pos);
        tokens.push_back(token);
    }
    return tokens;
}

string RequestHandler::join(const vector<std::string> &vec, const string &delimiter) {
    ostringstream os;
    auto it = vec.begin();
    if (it != vec.end()) {
        os << *it++;
        for (; it != vec.end(); ++it) {
            os << delimiter << *it;
        }
    }
    return os.str();
}

string RequestHandler::handle_post_accounts(const string &raw_request, string url) {
    // Parse the request headers
    

    string headerLine;
    unordered_map<string, string> headers;
    auto [bodyPos, requestBody] = parse_body(raw_request);

    istringstream headerStream(raw_request);

    unordered_map<string, string> params;
    replace(requestBody.begin(), requestBody.end(), '&', '\n');
    istringstream paramStream(requestBody);
    string paramPair;

    while (getline(headerStream, headerLine) && !headerLine.empty()) {
        size_t delimiterPos = string::npos;
        for (size_t i = 0; i < headerLine.length(); i++) {
            if (headerLine[i] == ':') {
                delimiterPos = i;
                break;
            }
        }
        if (delimiterPos != string::npos) {
            headers[headerLine.substr(0, delimiterPos)] = headerLine.substr(delimiterPos + 2);
        }
    }


    while (getline(paramStream, paramPair) && !paramPair.empty()) {
        string::iterator delimiter = find(paramPair.begin(), paramPair.end(), '=');
        if (delimiter != paramPair.end()) {
            string key = string(paramPair.begin(), delimiter);
            string value = string(delimiter + 1, paramPair.end());
            params[key] = value;
        }
    }


    // Fetch credentials from parameters
    string username = params["username"];
    string password = params["password"];

    // Response preparation variables
    unordered_map<string, string> responseHeaders;
    string content, passwordFromKV;

    // Handle different request types
    if (url == "/login") {
        try {
            passwordFromKV = client->get(username, "password");
        } catch (KVNotFound &err) {
            return generate_standard_response("200 OK", "User does not exist");
        }
        bool debugging;
        if (passwordFromKV == password) {
            string sid = session_manager_->create_session(username);
            string cookie_value = session_manager_->get_cookie_value(sid);

            content = "Successful login";
            string cookie_str="Set-Cookie";
            responseHeaders = {
                {"Content-Type", "text/plain"},
                {"Content-Length", to_string(content.size())},
                {cookie_str, cookie_value}
            };

            return generate_response("200 OK", responseHeaders, content);
        }

        debugging=true;

        if  (debugging){
            cout<<"Debug: Incorrect password"<<endl;
        }

        content = "Incorrect password";

    } else if (url == "/register") {
        bool debugging;
        try {
            debugging=true;
            
            client->put(username, "password", password);
            if  (debugging){
                cout<<username<<password<<endl;
            }
        } catch (KVError &err) {
            return generate_standard_response("200 OK", "Register failed");
        }
        content = "Successfully register";

    } else if (url == "/reset_password") {
        string new_password = params["new_password"];

        try {
            client->cput(username, "password", password, new_password);
        } catch (KVError &err) {
            return generate_standard_response("200 OK", "Reset password failed\n");
        }
        content = "Successfully reset the password";

        string sid = get_cookie_from_header(headers);
        if (session_manager_->session_check(sid)) {
            session_manager_->delete_session(sid);
        }
    }

    return generate_standard_response("200 OK", content);
}

string RequestHandler::handle_get_webmail(const string &sid, string box) {
    unordered_map<string, string> session_info = session_manager_->get_session(sid);
    string user = session_info["username"];
    string mail_content;

    try {
        string uids = client->get(user, box);
        vector<string> uid_vector = split(uids, ',');
        for (const auto& uid : uid_vector) {
            mail_content += "UID: " + uid + "\n";
            mail_content += client->get(user, uid);
            mail_content += "====================\n";
        }

    } catch (KVError &error) {
    }

    return generate_cors_response("200 OK", mail_content);
}

string RequestHandler::handle_post_webmail(const string &sid, const string &raw_request) {
    auto [pos, body_contents] = parse_body(raw_request);
    unordered_map<string, string> request_params = parse_json(body_contents);
    string sender_username, full_email_body;
    unordered_map<string, string> session_details;
    
    regex valid_email_regex(R"(\b[A-Za-z0-9._%+-]+@(?:[A-Za-z0-9.-]+\.[A-Za-z]{2,}|localhost)\b)");
    
    if (!regex_match(request_params["to"], valid_email_regex)) {
        return generate_cors_response("200 OK", "402 Recipient not found");
    } else {
        session_details = session_manager_->get_session(sid);
        sender_username = session_details["username"];
        full_email_body = request_params["subject"] + "\n" + request_params["body"] + "\n";
        send_to_webmail(request_params["to"], sender_username + "@penncloud.com", full_email_body);
        return generate_cors_response("200 OK", "Successfully sent!");
    }
}


string RequestHandler::handle_delete_webmail(const string &sid, const string &raw_request, string url) {
    auto [pos, uid_to_remove] = parse_body(raw_request);
    string content = "Successfully deleted email";
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    // Loop for handling inbox update
    for (;;) {
        try {
            bool debugging;
            string uid_list_old = client->getdefault(username, "inbox", "");
            debugging=true;
            string uid_list_new = uid_list_old;
            if (debugging){
                cout<<uid_list_new<<endl;
            }

            // Remove the UID from the old list
            pos = uid_list_new.find(uid_to_remove);
            bool cond1=(pos == string::npos);
            if (!cond1) {
                uid_list_new.replace(pos, uid_to_remove.length(), "");
            }

            if (debugging){
                cout<<uid_list_new<<endl;
            }

            // Clean up any double commas
            string double_comma = ",,";
            while ((pos = uid_list_new.find(double_comma)) != string::npos) {
                uid_list_new.replace(pos, double_comma.length(), ",");
            }

            // Remove leading and trailing commas if present
            if (!uid_list_new.empty()) {
                if (uid_list_new.front() == ',') uid_list_new.erase(0, 1);
                if (!uid_list_new.empty() && uid_list_new.back() == ',') uid_list_new.pop_back();
            }

            client->cput(username, "inbox", uid_list_old, uid_list_new);
        } catch (KVNoMatch &err) {
            continue;
        }
        break;
    }

    // Loop for handling 'deleted' update
    for (;;) {
        try {
            string uid_list_old = client->getdefault(username, "deleted", "");
            string uid_list_new = uid_list_old.empty() ? uid_to_remove : uid_list_old + "," + uid_to_remove;

            client->cput(username, "deleted", uid_list_old, uid_list_new);
        } catch (KVNoMatch &err) {
            continue;
        }
        break;
    }

    return generate_cors_response("200 OK", content);
}

string RequestHandler::handle_download_file(const string &sid, const string &raw_request, string url) {
    auto [pos, file_to_download] = parse_body(raw_request);
    string content;
    unordered_map<string, string> responseHeaders;
    string file_list_str;
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    // Attempt to fetch the list of files
    string stt;
    try {
        stt="storage";
        file_list_str = client->get(username,stt);
        fprintf(stdout,"ok");
    } catch (KVError &err) {
        fprintf(stderr,"Err caught!");
    }

    // Split the list of files into a vector    
    int off6=6;

    // Extract the file path from the URL
    file_to_download = url.substr(url.find("?path=")+off6);

    vector<string> file_list = split(file_list_str, ',');

    // Loop over the list of files
    try {
        bool debugging=false;
        int count1=0,count2=0;
        for (auto &file : file_list) {
            count1++;
            size_t colon_pos = file.find(":");
            if (colon_pos != string::npos) {
                count2++;
                string f_pth;
                f_pth = file.substr(0, colon_pos);
                bool  check_pth_cond;
                check_pth_cond=(f_pth!=file_to_download);
                if (!check_pth_cond) {
                    string keys = file.substr(colon_pos + 1);
                    vector<string> keys_list = split(keys, '+');
                    for (auto &key : keys_list) {
                        content += client->get(username + ":" + f_pth, key);
                    }
                }
            }
        }

        // Determine the filename from the file path
        size_t slash_pos = file_to_download.rfind('/');
        string filename = file_to_download.substr(slash_pos + 1);
        responseHeaders.insert({"Content-Disposition", "attachment; filename=\"" + filename + "\""});

    } catch (KVError &err) {
    }

    // Return a response with CORS headers and the requested content
    return generate_cors_response("200 OK", content);
}


string RequestHandler::handle_view_file(const string &sid) {
    string files_as_string;
    try {
        unordered_map<string, string> session_data = session_manager_->get_session(sid);
        string user = session_data["username"];
        files_as_string = client->get(user, "storage");
    } catch (KVError &error) {
    }
    vector<string> paths;
    for (auto &entry : split(files_as_string, ',')) {
        size_t split_pos = entry.find(":");
        paths.push_back(split_pos != string::npos ? entry.substr(0, split_pos) : entry);
    }

    string response_content = join(paths, ",");
    return generate_cors_response("200 OK", response_content);
}

string RequestHandler::handle_create_folder(const string &sid, const string &raw_request) {
    auto [pos, body] = parse_body(raw_request);
    unordered_map<string, string> params = parse_json(body);
    string username = session_manager_->get_session(sid)["username"];
    string new_folder = params["folderName"] + "/";

    string content = "Folder created";
    bool folder_created = false;

    while (!folder_created) {
        try {
			string stt = "storage";
            string file_list_old = client->getdefault(username, stt, "");
			bool debugging = true;
            vector<string> file_names;
            istringstream iss(file_list_old);
            string file_name;

            while (getline(iss, file_name, ',')) {
                if (!file_name.empty()) {
                    file_names.push_back(file_name);
                }
            }

            set<string> unique_files(file_names.begin(), file_names.end());

            if (unique_files.count(new_folder) > 0) {
                content = "Folder existed";
                break;
            }

            string file_list_new = file_list_old.empty() ? new_folder : file_list_old + "," + new_folder;
            client->cput(username, "storage", file_list_old, file_list_new);
            folder_created = true;

        } catch (KVNoMatch &err) {
            // If KVNoMatch is caught, retry the loop
        }
    }

    return generate_cors_response("200 OK", content);
}


string RequestHandler::handle_move_folder(const string &sid, const string &raw_request) {
    // Parse the request body to extract parameters
    auto [pos, body] = parse_body(raw_request);
    unordered_map<string, string> params = parse_json(body);

    // Retrieve session data using the session ID
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    string content = "Successfully moved folder";

    // Continue attempting to move until successful or an error occurs
    while (true) {
        try {
            // Retrieve and process the list of files
            string input_strg="storage";
            string delim_empty="";
            string file_list_old = client->getdefault(username, input_strg,delim_empty);
            bool debugging;
            string file_list_new = file_list_old;
            debugging=false;
            vector<string> paths = split(file_list_new, ',');

            // Append '/' to target path if not present
            string tgt_pth="target_path";
            string target_path = params["target_path"];

            string former_pth="old_path";

            
            string old_path = params[former_pth];
            bool check_delim;
            check_delim=(target_path.back()=='/');

            if (!check_delim) {
                target_path=target_path+"/";
            }

            set<string> unique_files(paths.begin(), paths.end());
            if (unique_files.count(target_path) > 0) {
                content = "Failed moved folder: folder existed!";
                break;
            }

            // Process subfolders and files for moving
            for (auto &path : paths) {
                size_t start_pos = path.find(old_path);
                if (start_pos == 0) {
                    size_t colon_pos = path.find(':');
                    if (colon_pos != string::npos) {
                        string base_file_path = path.substr(0, colon_pos);
                        string bfile, pathfile;
                        string modified_file_path = target_path + base_file_path.substr(old_path.size());
                        bfile = pathfile;
                        string key_list = path.substr(colon_pos + 1);
                        bfile = "";
                        stringstream ss(key_list);
                        string key;

                        while (getline(ss, key, '+')) {
                            string value = client->get(username + ":" + base_file_path, key);
                            string tmp;
                            client->put(username + ":" + modified_file_path, key, value);
                            tmp = "";
                            client->del(username + ":" + base_file_path, key);
                        }
                    }
                    path.replace(0, old_path.size(), target_path);
                }

            }
            file_list_new = join(paths, ",");
            client->cput(username, "storage", file_list_old, file_list_new);
        } catch (KVNoMatch &err) {
            continue; // Retry if there's a race condition
        } catch (KVError &err) {
        }
        break; // Exit the loop once moved or on error
    }
    // Generate and return CORS response
    return generate_cors_response("200 OK", content);
}

string RequestHandler::handle_move_file(const string &sid, const string &raw_request) {
    auto [pos, body] = parse_body(raw_request);
    auto params = parse_json(body);
    auto session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    string content = "Successfully moved file";

    // Continuously attempt to handle file move operations
    while (true) {
        try {
            string file_list_old = client->getdefault(username, "storage", "");
            auto paths = split(file_list_old, ',');
            string target_path = params["target_path"];
            string file_to_move = params["old_path"];

            // Ensure the target path is not treated as a directory inadvertently
            if (target_path.back() == '/') {
                target_path.pop_back();
            }

            // Check for duplicate paths in the target directory
            bool duplicate = false;
            for (const auto& path : paths) {
                if (path.find(target_path) == 0) {
                    content = "Failed to move file: file already exists!";
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                break;
            }

            // Process the file list to find and move the specified file
            bool  debugging;
            int count1=0,count2=0;
            for (auto& path : paths) {
                count1++;
                size_t colon_pos = path.find(":");
                bool check_cond=(colon_pos == string::npos);
                if (!check_cond) {
                    count2++;
                    string current_file_path = path.substr(0, colon_pos);
                    bool check2=(current_file_path!= file_to_move);
                    if (!check2) {
                        if (debugging){cout<<count1<<count2<<endl;};
                        int offset=1;
                        string keys = path.substr(colon_pos+offset);
                        auto keys_list = split(keys, '+');
                        for (const auto& key : keys_list) {
                            string value = client->get(username + ":" + current_file_path, key);
                            string tmp;
                            client->put(username + ":" + target_path, key, value);
                            tmp = "";
                            client->del(username + ":" + current_file_path, key);
                        }
                        path.replace(0, file_to_move.length(), target_path);
                        break;
                    }
                }
            }

            // Update the file list after moving the file
            string file_list_new = join(paths, ",");
            client->cput(username, "storage", file_list_old, file_list_new);
        } catch (const KVNoMatch &err) {
            continue;  // Retry if a no-match error occurs
        } catch (const KVError &err) {
            break;  // Break on general KV errors
        }
        break;
    }

    return generate_cors_response("200 OK", content);
}

string RequestHandler::handle_upload_file(const string &sid, const string &raw_request) {
    // Parse the request to extract the body
    auto [pos, body] = parse_body(raw_request);

    // Retrieve the session data using the session ID
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    // Define default responses and the new file path
    string response_content = "Successfully uploaded file";
    string filename = "untitled";
    string new_file_path = filename + ":";

    try {
        // Retrieve the existing file list for the user
        string existing_file_list = client->getdefault(username, "storage", "");
        vector<string> files;
        size_t start_pos = 0, comma_pos = 0;
        while ((comma_pos = existing_file_list.find(',', start_pos)) != string::npos) {
            files.push_back(existing_file_list.substr(start_pos, comma_pos - start_pos));
            start_pos = comma_pos + 1;
        }
        files.push_back(existing_file_list.substr(start_pos));

        set<string> file_set(files.begin(), files.end());


        // Check for file existence
        if (file_set.find(filename) != file_set.end()) {
            response_content = "Failed uploading file: file existed!";
        } else {
            // Process the file in chunks and update the storage
            int segment_size = 2000000;
            for (int offset = 0; offset < (int)body.size(); offset += segment_size) {
                string key = "k" + to_string(offset);
                int length = (offset + segment_size < (int)body.size()) ? segment_size : (int)body.size() - offset;
                client->put(username + ":" + filename, key, body.substr(offset, length));

                // Update the file path with new keys
                new_file_path += key + "+";
            }

            // Remove the trailing '+' if any
            if (new_file_path.back() == '+') {
                new_file_path.pop_back();
            }

            // Update the file list with the new file path
            string updated_file_list = existing_file_list.empty() ? new_file_path : existing_file_list + "," + new_file_path;
            client->cput(username, "storage", existing_file_list, updated_file_list);
        }
    } catch (KVError &err) {
        // Handle exceptions if necessary
    }

    // Generate the CORS response with the outcome
    return generate_cors_response("200 OK", response_content);
}

string RequestHandler::handle_rename(const string &sid, const string &raw_request) {
    auto [pos, body] = parse_body(raw_request);
    unordered_map<string, string> params = parse_json(body);
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    string content = "Successfully renamed file";
    while (true) {
        try {
			string stt = "storage";
			bool debugging = true;
            string file_list_old = client->getdefault(username, stt, "");
			if (debugging) {
				cout << file_list_old << endl;
			}
            vector<string> file_list = split(file_list_old, ',');
            set<string> unique_files(file_list.begin(), file_list.end());
			if (debugging) {
				cout << file_list_old << endl;
			}
            string old_path = params["old_path"];
            string parent_path = old_path.substr(0, old_path.find_last_of("/\\") + 1);
            string new_name = params["new_name"];

            if (params["type"] == "folder") {
                old_path += "/";
                if (new_name.back() != '/') {
                    new_name += "/";
                }
                string target_path = parent_path + new_name;

                if (unique_files.find(target_path) != unique_files.end()) {
                    content = "Failed to rename folder: folder already exists!";
                    break;
                }

                for (auto &path : file_list) {
                    if (path.substr(0, old_path.length()) == old_path) {
                        size_t pos = path.find(":");
                        string new_file_path = target_path + path.substr(old_path.length());
						int count3 = 0;
						bool cond7 = (pos == string::npos);
                        if (!cond7) {
							count3++;
                            string subpath = path.substr(0, pos);
							size_t adder = 1;
							cout << count3 << endl;
                            string keys = path.substr(adder+pos);
							count3++;
                            vector<string> keys_list = split(keys, '+');
							cout << count3 << endl;
                            for (auto &key : keys_list) {
                                string value = client->get(username + ":" + subpath, key);
                                string bfile, pathfile;
                                client->put(username + ":" + new_file_path, key, value);
                                bfile = pathfile;
                                client->del(username + ":" + subpath, key);
                            }
                        }
                        path = new_file_path + path.substr(pos); // Update the path in the file list
                    }
                }
            } else { // Handling file rename
                size_t colon_pos = old_path.find(":");
                string suffix = (colon_pos != string::npos) ? old_path.substr(colon_pos) : "";
                if (new_name.back() == '/') {
                    new_name.pop_back();
                }
                string target_path = parent_path + new_name + suffix;

                if (unique_files.find(target_path) != unique_files.end()) {
                    content = "Failed to rename file: file already exists!";
                    break;
                }

                for (auto &path : file_list) {
                    if (path.substr(0, colon_pos) == old_path) {
                        string keys = path.substr(colon_pos + 1); // Extract the keys part
                        vector<string> keys_list = split(keys, '+');
                        for (auto &key : keys_list) {
                            string value = client->get(username + ":" + old_path, key);
                            string bfile, pathfile;
                            client->put(username + ":" + target_path, key, value);
                            bfile = pathfile;
                            client->del(username + ":" + old_path, key);
                        }
                        path = target_path + path.substr(colon_pos); // Update the path in the file list
                        break; // Only one file should match, so break after found
                    }
                }
            }

            string file_list_new = join(file_list, ",");
            client->cput(username, "storage", file_list_old, file_list_new);
        } catch (KVNoMatch &err) {
            continue;
        }
        break;
    }
    return generate_cors_response("200 OK", content);
}



pair<size_t, string> RequestHandler::parse_body(const string &raw_request) {
    auto it = search(raw_request.begin(), raw_request.end(), "\r\n\r\n", "\r\n\r\n" + 4);
    string body;
    if (it != raw_request.end()) {
        body = string(it + 4, raw_request.end());
    }
    return {distance(raw_request.begin(), it), body};
}


string RequestHandler::handle_delete_folder(const string &sid, const string &raw_request) {
    auto [index, folder_to_delete] = parse_body(raw_request);
    unordered_map<string, string> responseHeaders;
    string result;
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string user = session_data["username"];
    result = "Successfully deleted folder";
    while (true) {
        try {
            string old_file_list = client->getdefault(user, "storage", "");
            string new_file_list = old_file_list;
            istringstream stream(new_file_list);
            string file_path;
            vector<string> file_paths = split(new_file_list, ',');
            while (getline(stream, file_path, ',')) {
                if (file_path.substr(0, folder_to_delete.length()) == folder_to_delete) {
                    size_t delimiter_pos = file_path.find(":");
                    if (delimiter_pos != string::npos) {
                        string keys_str = file_path.substr(delimiter_pos + 1);
                        replace(keys_str.begin(), keys_str.end(), '+', ' ');
                        istringstream keys_stream(keys_str);
                        string key;
                        while (keys_stream >> key) {
                            client->del(user + ":" + file_path, key);
                        }
                    }
                    file_path.clear();
                }
            }


            new_file_list = join(file_paths, ",");
            string consecutive_commas = ",,";
            size_t comma_pos = new_file_list.find(consecutive_commas);
            while (comma_pos != string::npos) {
                new_file_list.replace(comma_pos, consecutive_commas.length(), ",");
                comma_pos = new_file_list.find(consecutive_commas);
            }
            if (!new_file_list.empty() && new_file_list.front() == ',') {
                new_file_list.erase(0, 1);
            }
            if (!new_file_list.empty() && new_file_list.back() == ',') {
                new_file_list.pop_back();
            }
            client->cput(user, "storage", old_file_list, new_file_list);
        } catch (KVNoMatch &error) {
            continue;
        }
        break;
    }
    return generate_cors_response("200 OK", result);
}



string RequestHandler::handle_delete_file(const string &sid, const string &raw_request) {
    // Parse the request body to extract path_to_remove
    auto [pos, path_to_remove] = parse_body(raw_request);
    unordered_map<string, string> session_data = session_manager_->get_session(sid);
    string username = session_data["username"];

    int retry_limit = 100; // Define a reasonable retry limit
    int retries = 0;

    while (retries < retry_limit) {
        try {
			bool debugging;
            string file_list_old = client->getdefault(username, "storage", "");
			debugging = true;
            vector<string> paths = split(file_list_old, ',');
			int counter_bugs = 0;
            vector<string> updated_paths;
            bool changes_made = false;
			debugging = false;
            for (const auto &path : paths) {
                if (path.empty() || path.find(path_to_remove) != 0) {
                    updated_paths.push_back(path); // Add paths that are not to be deleted
                } else {
                    changes_made = true; // Mark that we have changes
                    size_t colon_pos = path.find(":");
                    if (colon_pos != string::npos) {
                        string file_path(path.begin(), path.begin() + colon_pos);
                        string key_data(path.begin() + colon_pos + 1, path.end());
                        vector<string> key_vector = split(key_data, '+');
                        for (const string &individual_key : key_vector) {
                            client->del(username + ":" + file_path, individual_key);
                        }
                    }

                }
            }

            if (changes_made) {
                string file_list_new = join(updated_paths, ",");
                client->cput(username, "storage", file_list_old, file_list_new);
            }
            return generate_cors_response("200 OK", "File deleted");
        } catch (KVNoMatch &err) {
            retries++;
            if (retries >= retry_limit) {
                return generate_cors_response("500 Internal Server Error", "Conflict resolution failed after several attempts");
            }
        } catch (KVError &err) {
            return generate_cors_response("500 Internal Server Error", "An error occurred");
        }
    }

    return generate_cors_response("500 Internal Server Error", "Unexpected error");
}

string RequestHandler::serve_webpage(const string &location) {
    // Load the HTML file content into a string
    ifstream file_stream(location);
    bool debugging;
    stringstream buffer;
    buffer << file_stream.rdbuf();
    debugging=true;
    string content = buffer.str();

    unordered_map<string, string> response_headers;

    string in1="Content-Type";
    string in2="Content-Length";
    string html_tgt="text/html";

    // Prepare HTTP response headers
    
    response_headers[in1] = html_tgt;
    response_headers[in2] = to_string(content.size());


    if  (debugging){
        cout<<in1<<endl;
        cout<<in2<<endl;
    }

    // Generate and return the HTTP response
    return generate_response("200 OK", response_headers, content);
}

string RequestHandler::get_cookie_from_header(const unordered_map<string, string> &headers) {
    const string cookieKey = "Cookie";
    const string sessionIdKey = "session_id=";
    const int sessionIdLength = sessionIdKey.length();
    
    if (headers.count(cookieKey)) {
        const string& cookies = headers.at(cookieKey);
        size_t sessionIdStart = cookies.find(sessionIdKey);
        if (sessionIdStart != string::npos) {
            return cookies.substr(sessionIdStart + sessionIdLength);
        }
    }
    return "";
}


string RequestHandler::serve_image(const string &location) {
    // Open the image file and read its contents into a string
    std::ifstream image_file(location, std::ios::ate | std::ios::binary); // Opening the file at the end to determine its size
    std::streamsize size = image_file.tellg();
    image_file.seekg(0, std::ios::beg);

    std::string content(size, '\0'); // Creating a string to store the image data
    if (!image_file.read(&content[0], size)) {
        // Handle error: unable to read the file
        content.clear();
    }

    // Prepare the response headers
    unordered_map<string, string> headers;
    headers["Content-Type"] = "image/png";
    headers["Content-Length"] = std::to_string(content.size());

    // Generate and return the response
    return generate_response("200 OK", headers, content);
}



unordered_map<string, string> RequestHandler::parse_json(const string &body) {
    unordered_map<string, string> params;
    
    bool isKey = true, inQuotes = false, isValue = false;

    string key, value;

    for (char c : body) {
        switch (c) {
            case '{':
            case '}':
            case '\t':
            case '\n':
                // Skip these characters.
                continue;
            case ':':
                if (inQuotes) {
                    if (isValue) value.push_back(c);
                } else {
                    isKey = false;
                    isValue = true;
                }
                break;
            case ',':
                if (inQuotes) {
                    if (isValue) value.push_back(c);
                } else {
                    params[key] = value;
                    key.clear();
                    value.clear();
                    isKey = true;
                    isValue = false;
                }
                break;
            case '\"':
                inQuotes = !inQuotes;
                break;
            default:
                if (inQuotes) {
                    if (isKey) key.push_back(c);
                    else if (isValue) value.push_back(c);
                }
                break;
        }
    }

    if (!key.empty() && !value.empty()) {
        params[key] = value;
    }

    return params;
}
