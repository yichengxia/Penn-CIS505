#include <cstddef>
#include <cstring>
#include <ctime>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <csignal>
#include <regex>
#include <fcntl.h>
#include <chrono>
#include <filesystem>
#include <sys/file.h>


#define READ_BUFFER 1024
#define MAX_LEN 8192
#define DEFAULT_PORT 2500
#define MAX_CONNECTIONS 1500
#define RSP_LEN 128

bool verbose = false;
int listen_fd;
std::string directory = std::string();

pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;
volatile int active_connections[MAX_CONNECTIONS]; // Store fd's of all active connections
int conn_cnt = 0;

/* User mail box locks */
// Each user (with a mbox file in `directory`) has a mutex to lock the file
std::vector<pthread_mutex_t> user_locks;
std::vector<std::string> localhost_users; // Store all users in localhost (`directory`). No changes are made to users after the server starts. So the idx of a user in localhost_users is the same as the idx of the user's mutex in user_locks. 

volatile int shutting_down = 0;



void send_to_client(int comm_fd, const char *response, bool verbose) {
  send(comm_fd, response, strlen(response), 0);
  if (verbose) {
    fprintf(stderr, "[%i] S: %s", comm_fd, response);
  }
}


void *worker(void *arg) {
  int comm_fd = *(int*)arg;
  free(arg);

  std::string client_domain = std::string(); // Domain of the client. It will be set after HELO command.
  bool initial_state = true; // The server is in initial_state iff it is not in middle of a transaction. 
  std::string mail_from_user = std::string(); // The sender: user.
  std::string mail_from_domain = std::string(); // The sender: domain.
  std::vector<std::array<std::string, 2>> rcpt_to; // The recipient(s) of the email. Each element is a pair of user and domain.
  std::string data = std::string(); // The email data.
  bool data_mode = false; // Whether the server is currently accepting email data.

  // Store fd into active_connections
  pthread_mutex_lock(&conn_lock);
  if (conn_cnt >= MAX_CONNECTIONS) { // Disconnect it reached max connection limit
    pthread_mutex_unlock(&conn_lock);
    if (verbose) {
      fprintf(stderr, "[%i] S: Max connection reached. Connection closed\n", comm_fd);
    }
    char max_conn_reached[] = "421 Maximum connections of 100 reached. You will be disconnected.\r\n";
    send(comm_fd, max_conn_reached, strlen(max_conn_reached), 0);
    close(comm_fd);
    return(0);
  }

  // Find an available slot to store my fd
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (active_connections[i] == -1) {
      active_connections[i] = comm_fd;
      break;
    }
  }
  conn_cnt++;
  pthread_mutex_unlock(&conn_lock);

  // 220 service ready message
  char greeting[] = "220 localhost SMTP service ready\r\n";
  send(comm_fd, greeting, strlen(greeting), 0);

  if (verbose) {
    fprintf(stderr, "[%i] S: %s", comm_fd, greeting);
  }

  // Read to msg_buffer, then copy to msg
  char msg[MAX_LEN + 1];
  int msg_len = 0;
  ssize_t read_size;
  char msg_buffer[READ_BUFFER+1];
  while (true) {
    read_size = recv(comm_fd, msg_buffer, READ_BUFFER, 0);
    if (read_size > 0) {
      if (msg_len + read_size > MAX_LEN) {
        char length_exceeded[] = "Command exceeded length limit! Close connection.\r\n";
        send(comm_fd, length_exceeded, strlen(length_exceeded), 0);

        // Remove myself from active connections array
        pthread_mutex_lock(&conn_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
          if (active_connections[i] == comm_fd) {
            active_connections[i] = -1;
            break;
          }
        }
        conn_cnt--;
        pthread_mutex_unlock(&conn_lock);

        close(comm_fd);
        return(0);
      }

      if (verbose) {
        std::cerr << "[" << comm_fd << "] C: " << std::string(msg_buffer, msg_buffer+read_size);
      }

      // Copy message in buffer to msg
      msg_buffer[read_size] = '\0'; // Null-terminate the message in buffer
      memcpy(msg + msg_len, msg_buffer, read_size+1);
      msg_len += read_size;

      // Locate the end of command
      char* cmd_end = strstr(msg, "\r\n");
      while (cmd_end != NULL) {
        *cmd_end = '\0'; // it was pointing to \r, now change it to null terminator

        char response[RSP_LEN];

        /* Data mode */
        if (data_mode) {
          if (strncmp(msg, ".", 1) == 0) {
            
            data_mode = false;
            initial_state = true;

            /* Write data to file */
            for (int i = 0; i < rcpt_to.size(); i++) {
              std::string user = rcpt_to[i][0];
              std::string domain = rcpt_to[i][1];

              // locate user idx
              int user_idx = -1;
              auto it = find(localhost_users.begin(), localhost_users.end(), user);
              if (it != localhost_users.end()) {
                user_idx = it - localhost_users.begin();
              } else {
                snprintf(response, sizeof(response), "550: user mailbox not found (was probably deleted after server starts)!\r\n");
                send_to_client(comm_fd, response, verbose);
                continue;
              }

              if (strcmp(domain.c_str(), "localhost") == 0){
                std::string file_path = directory + "/" + user + ".mbox";
                FILE *file = fopen(file_path.c_str(), "a+b");
                if (file == NULL) {
                  snprintf(response, sizeof(response), "451 Requested action aborted: error when writing to local file.\r\n");
                  send(comm_fd, response, strlen(response), 0);
                  if (verbose) {
                    fprintf(stderr, "[%i] S: %s", comm_fd, response);
                  }
                  continue;
                }
                int fd = fileno(file);
                flock(fd, LOCK_EX);
                pthread_mutex_lock(&user_locks[user_idx]);
                fprintf(file, "%s", data.c_str());
                fclose(file);
                pthread_mutex_unlock(&user_locks[user_idx]);
                flock(fd, LOCK_UN);
              }
            }
            snprintf(response, sizeof(response), "250 OK\r\n");
            send_to_client(comm_fd, response, verbose);
          } else {
            data += std::string(msg) + "\r\n";
          }
        }

        /* Command mode */
        else {
          if (strncmp(msg, "HELO ", 5) == 0 or strncmp(msg, "helo ", 5) == 0) {
            // Respond 503 if server not in initial state
            if (!initial_state) {
              snprintf(response, sizeof(response), "503 Bad sequence of commands\r\n");
            } else {
              // If in initial state: Set client_domain to the domain after "HELO "
              client_domain = std::string(msg + 5);
              if (client_domain.empty()) { // Check argument is not empty
                snprintf(response, sizeof(response), "501 Syntax error: argument of HELO cannot be empty!\r\n");
              } else {
                snprintf(response, sizeof(response), "250 localhost\r\n");
              }
            }
            send_to_client(comm_fd, response, verbose);

          } else if (strncmp(msg, "MAIL FROM:", 10) == 0 or strncmp(msg, "mail from:", 10) == 0){
            // Respond 503 if server has not received HELO command (which means client_domain is not set)
            if (client_domain.empty()) {
              snprintf(response, sizeof(response), "503 Bad sequence of commands: send HELO first.\r\n");
            } else {
              if (rcpt_to.empty()) {
                std::string mail_from = std::string(msg + 10);
                // Use regex to check if the mail_from is valid, which is the form "<user@domain>"", and assign values to user and domain.
                std::regex email_pattern("^<([a-zA-Z0-9.]+)@([a-zA-Z0-9.]+)>$");
                std::smatch matches;
                if (std::regex_match(mail_from, matches, email_pattern)) {
                  mail_from_user = matches[1];
                  mail_from_domain = matches[2];
                  snprintf(response, sizeof(response), "250 OK\r\n");
                  initial_state = false;
                } else {
                  snprintf(response, sizeof(response), "501 Syntax error: invalid email address, must be <user@domain>\r\n");
                }
              } else {
                snprintf(response, sizeof(response), "503 Bad sequence of commands: You have already issued RCPT TO so you cannot change MAIL FROM (only RCPT TO or DATA allowed)\r\n");
              }
            }
            send_to_client(comm_fd, response, verbose);

          } else if (strncmp(msg, "RCPT TO:", 8) == 0 or strncmp(msg, "rcpt to:", 8) == 0){
            // Respond 503 if server has not received MAIL FROM command
            if (mail_from_user.empty() and mail_from_domain.empty()) {
              snprintf(response, sizeof(response), "503 Bad sequence of commands: send MAIL FROM first.\r\n");
            } else {
              std::string rcpt_text = std::string(msg + 8);
              // Use regex to check if the rcpt_to is valid, which is the form "<user@domain>"", and assign values to user and domain.
              std::regex email_pattern("^<([a-zA-Z0-9.]+)@([a-zA-Z0-9.]+)>$");
              std::smatch matches;
              if (std::regex_match(rcpt_text, matches, email_pattern)) {
                std::string user = matches[1];
                std::string domain = matches[2];

                // If domain is local host, check if user exists
                if (strcmp(domain.c_str(), "localhost") == 0) {
                  if (std::find(localhost_users.begin(), localhost_users.end(), user) == localhost_users.end()) {
                    snprintf(response, sizeof(response), "550 User not found in localhost\r\n");
                  } else {
                    rcpt_to.push_back({user, domain});
                    snprintf(response, sizeof(response), "250 OK\r\n");
                  }
                } else {
                  rcpt_to.push_back({user, domain});
                  snprintf(response, sizeof(response), "250 OK\r\n");
                }
              } else {
                snprintf(response, sizeof(response), "501 Syntax error: invalid email address, must be <user@domain>\r\n");
              }
            }
            send_to_client(comm_fd, response, verbose);

          } else if (strncmp(msg, "DATA", 4) == 0 or strncmp(msg, "data", 4) == 0){
            // Respond 503 if server has not received RCPT TO command
            if (rcpt_to.empty()) {
              snprintf(response, sizeof(response), "503 Bad sequence of commands: send RCPT TO first.\r\n");
            } else {
              snprintf(response, sizeof(response), "354 Accepting data.\r\n");
              data_mode = true;

              // Initialize data: insert "From <email> <date>\r\n"
              std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
              data = std::string();
              data += "From <" + mail_from_user + "@" + mail_from_domain + "> " + std::ctime(&now_time);
            }
            send_to_client(comm_fd, response, verbose);

          } else if (strncmp(msg, "QUIT", 4) == 0 or strncmp(msg, "quit", 4) == 0){
            // Allow user to quit no matter it has send HELO or not
            snprintf(response, sizeof(response), "221 quiting\r\n");
            send(comm_fd, response, strlen(response), 0);
            close(comm_fd);
            pthread_mutex_lock(&conn_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
              if (active_connections[i] == comm_fd) {
                active_connections[i] = -1;
                break;
              }
            }
            conn_cnt--;
            pthread_mutex_unlock(&conn_lock);

            if (verbose) {
              fprintf(stderr, "[%i] S: %s", comm_fd, response);
              fprintf(stderr, "[%i] Connection closed\n", comm_fd);
            }

            return(0);
          } else if (strncmp(msg, "RSET", 4) == 0 or strncmp(msg, "rset", 4) == 0){
            if (client_domain.empty()) {
              snprintf(response, sizeof(response), "503 Bad sequence of commands: send HELO first.\r\n");
            } else {
              // Reset the server to initial state
              client_domain = std::string();
              initial_state = true;
              mail_from_user = std::string();
              mail_from_domain = std::string();
              rcpt_to.clear();
              data = std::string();
              data_mode = false;
              snprintf(response, sizeof(response), "250 OK\r\n");
            }
            send_to_client(comm_fd, response, verbose);
          } else if (strncmp(msg, "NOOP", 4) == 0 or strncmp(msg, "noop", 4) == 0){
            if (client_domain.empty()) {
              snprintf(response, sizeof(response), "503 Bad sequence of commands: send HELO first.\r\n");
            } else {
              snprintf(response, sizeof(response), "250 OK\r\n");
            }
            send_to_client(comm_fd, response, verbose);
          }
          
          else {
            snprintf(response, sizeof(response), "500 Syntax error: command unrecognized\r\n");
            send_to_client(comm_fd, response, verbose);
          }
        }

        msg_len -= (cmd_end - msg) + 2; // (cmd_end - msg) is the length of the command we just processed, without the \r\n
        memmove(msg, cmd_end+2, msg_len+1); // Now move unprocessed message (after cmd_end) to msg

        cmd_end = strstr(msg, "\r\n");
      }
    }
  }

}

void signal_handler(int sig) {
  shutting_down = 1;
  pthread_mutex_lock(&conn_lock); // Acquire the lock before shutting down so that no other connnection can be created
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    int conn = active_connections[i];
    if (conn != -1) {
      char closing_msg[] = "421 Server shutting down\r\n";
      send(conn, closing_msg, strlen(closing_msg), 0);
      close(conn);
      if (verbose) {
        fprintf(stderr, "[%i] S: %s", conn, closing_msg);
        fprintf(stderr, "[%i] Connection closed\n", conn);
      }
    }
  }
  close(listen_fd);
  exit(0);
}

int main(int argc, char *argv[]) {
  /* Your code here */

  signal(SIGINT, signal_handler);

  // Init connections array to -1 (which means an empty slot)
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    active_connections[i] = -1;
  }

  int port = DEFAULT_PORT;
  int opt;
  while ((opt = getopt(argc, argv, "p:av")) != -1) {
    switch (opt) {
      case 'p':
          port = atoi(optarg);
          break;
      case 'a':
          fprintf(stderr, "Author: Xuting Liu / xutingl\n");
          exit(0);
      case 'v':
          verbose = true;
          break;
      default:
          fprintf(stderr, "Unknown argument %s ! Only [-p port] [-a] [-v] are accepted.\n", argv[0]);
          exit(1);
    }
  }
  if (optind < argc) {
    directory = argv[optind];
  } else {
    fprintf(stderr, "Must provide a directory!\n");
    exit(1);
  }

  // Initialize usernames and user locks
  for (const auto & entry : std::filesystem::directory_iterator(directory)) {
    std::string file_name = entry.path().filename();
    std::string user = file_name.substr(0, file_name.find("."));
    localhost_users.push_back(user);
    pthread_mutex_t user_lock;
    pthread_mutex_init(&user_lock, NULL);
    user_locks.push_back(user_lock);
  }

  listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr)); // Initialize to 0 (same as bzero(&servaddr, sizeof(servaddr)))
  serv_addr.sin_family = AF_INET; // Should always be AF_INET for sockaddr_in (others like sockaddr_in6 have different values)
  serv_addr.sin_addr.s_addr = htons(INADDR_ANY); // Translate from host endian to network endian (Big Endian standard)
  serv_addr.sin_port = htons(port);

  if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
    fprintf(stderr, "Bind socket error!\n");
    exit(1);
  }

  listen(listen_fd, 10); // 10 is the backlog value (doesn't matter too much)

  while (true) {
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);

    int *fd = (int*)malloc(sizeof(int)); // This will be freed in the worker thread later after accepting a client
    *fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);

    if (verbose) {
      fprintf(stderr, "[%i] New connection\n", *fd);
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, fd) < 0) {
      fprintf(stderr, "Error when creating thread!");
      free(fd);
    }
    pthread_detach(thread);
  }

  return 0;
}
