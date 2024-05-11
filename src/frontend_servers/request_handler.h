#pragma once

#include "util.h"
#include "src/kv/client.h"
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class RequestHandler {
  public:

    int handler_ID;

    RequestHandler(SessionManager *session_manager, KVClient *kvClient, bool &serving_) : session_manager_(session_manager), client(kvClient), serving(serving_) {}

    bool is_handling;

    
    string generate_response(const string &status_code, const unordered_map<string, string> &headers, const string &content);

    string  handler_cont;

    string handle_request(const string &raw_request);

  private:

    KVClient *client;

    int client_ID;

    bool &serving;

    bool is_run_for_client;

    SessionManager *session_manager_; 

    string generate_cors_response(const string &status, const string &content);
    
    string handlePostRequests(const string &url, const string &raw_request, unordered_map<string, string> &headers);
    
    string serve_image(const string &location);

    string generate_standard_response(const string &status, const string &content);

    string handle_post_webmail(const string &sid,const string &raw_request);

    string handle_post_accounts(const string &raw_request, string url);

    unordered_map<string, string> parse_json(const string &body);

    string handle_create_folder(const string &sid, const string &raw_request);

    string handleGetRequests(const string &url, const string &working_dir, const string &raw_request, unordered_map<string, string> &headers);
    
    string handle_upload_file(const string &sid, const string &raw_request);

    string handle_get_webmail(const string &sid, string box);

    string handle_delete_file(const string &sid, const string &raw_request);

    string handle_shutdown_requests(const string &method, const string &url, unordered_map<string, string> &headers);

    string handle_delete_folder(const string &sid, const string &raw_request);

    string handle_rename(const string &sid, const string &raw_request);

    string handle_move_file(const string &sid, const string &raw_request);


    string get_cookie_from_header(const unordered_map<string, string> &headers);

    string join(const vector<std::string> &vec, const string &delimiter);

    string handle_delete_webmail(const string &sid, const string &raw_request, string url);

    string handle_get_storage(const string &raw_request, string url, string action, unordered_map<string, string> &headers);

    void process_path(string &path, const string &old_path, const string &target_path, const string &username);

    string handle_rename_folder(const string &username, const unordered_map<string, string> &params, const unordered_map<string, string> &session_data);

    unordered_map<string, string> parse_headers(const string &raw_headers);

    string handle_post_login(const string &raw_request);

    string handle_download_file(const string &sid, const string &raw_request, string url);

    string compute_new_path(const string &parent_path, const unordered_map<string, string> &params, const string &old_path);
    
    pair<size_t, string> parse_body(const string &raw_request);

    string handle_view_file(const string &sid);
    
    string handle_move_folder(const string &sid, const string &raw_request);
    
    vector<string> split(const string &str, char delimiter);
    
    void process_files(vector<string> &file_list, const string &old_path, const string &target_path, const string &username, const string &type);
    
    string handle_rename_file(const string &username, const unordered_map<string, string> &params, const unordered_map<string, string> &session_data);
    
    string serve_webpage(const string &location);

    string trim(const string &str);
    
};
