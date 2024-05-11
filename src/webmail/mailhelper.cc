#include <algorithm>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include "mailhelper.h"

/**
 * @brief Send mail message to the local mail server
 * 
 * @param recipient_address 
 * @param sender_address 
 * @param message 
 */
void send_to_webmail(const std::string &recipient_address, const std::string &sender_address, std::string message)
{
  // Extract the domain from the recipient address
  std::string host = recipient_address.substr(recipient_address.find("@") + 1, recipient_address.find(">") - recipient_address.find("@") - 1);
  std::string src_cmd = "MAIL FROM: <" + sender_address + ">\r\n";
  std::string des_cmd = "RCPT TO: <" + recipient_address + ">\r\n";
  message = "From: <" + sender_address + ">\r\n" + "To: <" + recipient_address + ">\r\n" + message;

  std::cout << "source : " << sender_address << std::endl;
  std::cout << "destination : " << recipient_address << std::endl;
  std::cout << "Domain : " << host << std::endl;

  // Create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Error creating socket." << std::endl;
    return;
  }

  // Connect to the local mail server
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(2500);
  inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    std::cerr << "Error connecting to the server." << std::endl;
    close(sockfd);
    return;
  }

  // Send HELO command
  const char *command = "HELO Tester webmail\r\n";
  if (send(sockfd, command, std::strlen(command), 0) < 0)
  {
    std::cerr << "Error sending command to the server." << std::endl;
    close(sockfd);
    return;
  }

  // Receive response for HELO command
  char buffer[1024];
  if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
  {
    std::cerr << "Error receiving response for: " << command << std::endl;
    close(sockfd);
    return;
  }

  // Send source command
  if (send(sockfd, src_cmd.c_str(), src_cmd.length(), 0) < 0)
  {
    std::cerr << "Error sending source command." << std::endl;
    close(sockfd);
    return;
  }

  // Receive response for source command
  std::memset(buffer, 0, sizeof(buffer));
  if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
  {
    std::cerr << "Error receiving response for source command." << std::endl;
    close(sockfd);
    return;
  }

  // Send destination command
  if (send(sockfd, des_cmd.c_str(), des_cmd.length(), 0) < 0)
  {
    std::cerr << "Error sending destination command." << std::endl;
    close(sockfd);
    return;
  }

  // Receive response for destination command
  std::memset(buffer, 0, sizeof(buffer));
  if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
  {
    std::cerr << "Error receiving response for destination command." << std::endl;
    close(sockfd);
    return;
  }

  // Send DATA command
  command = "DATA\r\n";
  if (send(sockfd, command, std::strlen(command), 0) < 0)
  {
    std::cerr << "Error sending DATA command." << std::endl;
    close(sockfd);
    return;
  }

  // Receive response for DATA command
  std::memset(buffer, 0, sizeof(buffer));
  if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
  {
    std::cerr << "Error receiving response for DATA command." << std::endl;
    close(sockfd);
    return;
  }

  // Send message
  size_t pos = 0;
  while ((pos = message.find("\\n", pos)) != std::string::npos)
  {
    message.replace(pos, 2, "\r\n");
    pos += 2;
  }
  message += "\r\n";
  if (send(sockfd, message.c_str(), message.length(), 0) < 0)
  {
    std::cerr << "Error sending message." << std::endl;
    close(sockfd);
    return;
  }

  std::memset(buffer, 0, sizeof(buffer));
  command = ".\r\n";
  if (send(sockfd, command, std::strlen(command), 0) < 0)
  {
    std::cerr << "Error sending final command." << std::endl;
    close(sockfd);
    return;
  }

  // Receive response for final command
  if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
  {
    std::cerr << "Error receiving response for final command." << std::endl;
  }

  close(sockfd);
}

/**
 * @brief Send mail message to the remote mail server
 * 
 * @param recipient_address 
 * @param sender_address 
 * @param message 
 */
void send_to_remote(const std::string &recipient_address, const std::string &sender_address, std::string message)
{
  // Extract the domain from the recipient address
  std::string host = recipient_address.substr(recipient_address.find("@") + 1, recipient_address.find(">") - recipient_address.find("@") - 1);
  std::string src_cmd = "MAIL FROM: <" + sender_address + ">\r\n";
  std::string des_cmd = "RCPT TO: <" + recipient_address + ">\r\n";
  message = "From: <" + sender_address + ">\r\n" + "To: <" + recipient_address + ">\r\n" + message;

  std::cout << "source : " << sender_address << std::endl;
  std::cout << "destination : " << recipient_address << std::endl;
  std::cout << "Domain : " << host << std::endl;

  // Get the MX record for the domain
  u_char nsbuf[4096];
  ns_msg msg;
  ns_rr rr;
  int l = res_query(host.c_str(), ns_c_in, ns_t_mx, nsbuf, sizeof(nsbuf));
  if (l < 0)
  {
    std::cerr << "res_query fails." << std::endl;
    return;
  }

  // Parse the DNS response
  ns_initparse(nsbuf, l, &msg);
  ns_parserr(&msg, ns_s_an, 0, &rr);
  ns_sprintrr(&msg, &rr, nullptr, nullptr, reinterpret_cast<char *>(nsbuf), sizeof(nsbuf));
  std::string dns = std::strtok(reinterpret_cast<char *>(nsbuf), " ");
  for (int i = 1; i < 4; ++i)
  {
    dns = std::strtok(nullptr, " ");
  }

  // Get the IP address for the domain
  struct addrinfo hints
  {
  }, *result;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  int error = getaddrinfo(dns.c_str(), nullptr, &hints, &result);
  if (error)
  {
    std::cerr << "Error getting IP address for " << host << ": " << gai_strerror(error) << std::endl;
    return;
  }

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in *>(result->ai_addr)->sin_addr, ip, sizeof(ip));
  std::cout << host << ": " << ip << std::endl;

  // Send mail to the remote mail server
  struct connection conn1;
  initializeBuffers(&conn1, 5000);

  connectToPort(&conn1, 25, ip);
  expectToRead(&conn1, "220 *");

  writeString(&conn1, "HELO tester remote\r\n");
  expectToRead(&conn1, "250 *");

  writeString(&conn1, src_cmd.c_str());
  expectToRead(&conn1, "250 OK");

  writeString(&conn1, des_cmd.c_str());
  expectToRead(&conn1, "250 OK");

  writeString(&conn1, "DATA\r\n");
  expectToRead(&conn1, "354 *");

  std::string line;
  size_t pos = 0;
  while ((pos = message.find('\n')) != std::string::npos)
  {
    line = message.substr(0, pos);
    writeString(&conn1, (line + "\r\n").c_str());
    message.erase(0, pos + 1);
  }
  writeString(&conn1, ".\r\n");
  expectToRead(&conn1, "250 OK");
}

/**
 * @brief Log the data; helper function from HW2:common.cc
 * 
 * @param prefix 
 * @param data 
 * @param len 
 * @param suffix 
 */
void log(const char *prefix, const char *data, int len, const char *suffix)
{
  std::printf("%s", prefix);
  for (int i = 0; i < len; ++i)
  {
    if (data[i] == '\n')
      std::printf("<LF>");
    else if (data[i] == '\r')
      std::printf("<CR>");
    else if (isprint(data[i]))
      std::printf("%c", data[i]);
    else
      std::printf("<0x%02X>", (unsigned int)(unsigned char)data[i]);
  }
  std::printf("%s", suffix);
}

/**
 * @brief Write string to the connection; helper function from HW2:common.cc
 * 
 * @param conn 
 * @param data 
 */
void writeString(struct connection *conn, const char *data)
{
  int len = std::strlen(data);
  log("C: ", data, len, "\n");
  int wptr = 0;
  while (wptr < len)
  {
    int w = write(conn->fd, &data[wptr], len - wptr);
    if (w < 0)
    {
      panic("Cannot write to connection (%s)", std::strerror(errno));
    }
    if (w == 0)
    {
      panic("Connection closed unexpectedly");
    }
    wptr += w;
  }
}

/**
 * @brief Expect no more data from the connection; helper function from HW2:common.cc
 * 
 * @param conn 
 */
void expectNoMoreData(struct connection *conn)
{
  int flags = fcntl(conn->fd, F_GETFL, 0);
  fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK);
  int r = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
  if ((r < 0) && (errno != EAGAIN))
  {
    panic("Read from connection failed (%s)", std::strerror(errno));
  }
  if (r > 0)
  {
    log("S: ", conn->buf, r + conn->bytesInBuffer, " [unexpected; server should not have sent anything!]\n");
    conn->bytesInBuffer = 0;
  }
  fcntl(conn->fd, F_SETFL, flags);
}

/**
 * @brief Connect to the port; helper function from HW2:common.cc
 * 
 * @param conn 
 * @param portno 
 * @param ip 
 */
void connectToPort(struct connection *conn, int portno, const char *ip)
{
  conn->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (conn->fd < 0)
  {
    panic("Cannot open socket (%s)", std::strerror(errno));
  }
  struct sockaddr_in servaddr
  {
  };
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(portno);
  inet_pton(AF_INET, ip, &(servaddr.sin_addr));
  if (connect(conn->fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    panic("Cannot connect to IP: (%s)", std::strerror(errno));
  }
  conn->bytesInBuffer = 0;
}

/**
 * @brief Expect to read data from the connection; helper function from HW2:common.cc
 * 
 * @param conn 
 * @param data 
 */
void expectToRead(struct connection *conn, const char *data)
{
  int lfpos = -1;
  while (true)
  {
    for (int i = 0; i < conn->bytesInBuffer; ++i)
    {
      if (conn->buf[i] == '\n')
      {
        lfpos = i;
        break;
      }
    }
    if (lfpos >= 0)
    {
      break;
    }
    if (conn->bytesInBuffer >= conn->bufferSizeBytes)
    {
      panic("Read %d bytes, but no CRLF found", conn->bufferSizeBytes);
    }
    int bytesRead = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
    if (bytesRead < 0)
    {
      panic("Read failed (%s)", std::strerror(errno));
    }
    if (bytesRead == 0)
    {
      panic("Connection closed unexpectedly");
    }
    conn->bytesInBuffer += bytesRead;
  }

  log("S: ", conn->buf, lfpos + 1, "");

  bool crMissing = false;
  if ((lfpos == 0) || (conn->buf[lfpos - 1] != '\r'))
  {
    crMissing = true;
    conn->buf[lfpos] = 0;
  }
  else
  {
    conn->buf[lfpos - 1] = 0;
  }

  int argptr = 0, bufptr = 0;
  bool match = true;
  while (match && data[argptr])
  {
    if (data[argptr] == '*')
    {
      break;
    }
    if (data[argptr++] != conn->buf[bufptr++])
    {
      match = false;
    }
  }
  if (!data[argptr] && conn->buf[bufptr])
  {
    match = false;
  }

  if (match)
  {
    if (crMissing)
    {
      std::printf(" [Terminated by LF, not by CRLF]\n");
    }
    else
    {
      std::printf(" [OK]\n");
    }
  }
  else
  {
    log(" [Expected: '", data, std::strlen(data), "']\n");
  }

  for (int i = lfpos + 1; i < conn->bytesInBuffer; ++i)
  {
    conn->buf[i - (lfpos + 1)] = conn->buf[i];
  }
  conn->bytesInBuffer -= (lfpos + 1);
}

/**
 * @brief Expect remote close; helper function from HW2:common.cc
 * 
 * @param conn 
 */
void expectRemoteClose(struct connection *conn)
{
  int r = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
  if (r < 0)
  {
    panic("Read failed (%s)", std::strerror(errno));
  }
  if (r > 0)
  {
    log("S: ", conn->buf, r + conn->bytesInBuffer, " [unexpected; server should have closed the connection]\n");
    conn->bytesInBuffer = 0;
  }
}

/**
 * @brief Initialize buffers; helper function from HW2:common.cc
 * 
 * @param conn 
 * @param bufferSizeBytes 
 */
void initializeBuffers(struct connection *conn, int bufferSizeBytes)
{
  conn->fd = -1;
  conn->bufferSizeBytes = bufferSizeBytes;
  conn->bytesInBuffer = 0;
  conn->buf = static_cast<char *>(std::malloc(bufferSizeBytes));
  if (!conn->buf)
  {
    panic("Cannot allocate %d bytes for buffer", bufferSizeBytes);
  }
}

/**
 * @brief Close the connection; helper function from HW2:common.cc
 * 
 * @param conn 
 */
void closeConnection(struct connection *conn)
{
  close(conn->fd);
}

/**
 * @brief Free buffers; helper function from HW2:common.cc
 * 
 * @param conn 
 */
void freeBuffers(struct connection *conn)
{
  std::free(conn->buf);
  conn->buf = nullptr;
}

/**
 * @brief Panic function; helper function from HW2:common.cc
 * 
 * @param fmt 
 */
void panic(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
  exit(EXIT_FAILURE);
}
