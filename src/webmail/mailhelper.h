#ifndef MAILHELPER_H
#define MAILHELPER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define panic(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); exit(1); } while (0) 

struct connection {
  int fd;
  char *buf;
  int bytesInBuffer;
  int bufferSizeBytes;
};

// Send mail message to the local mail server
void send_to_webmail(const std::string& recipient_address, const std::string& sender_address, std::string message);
// Send mail message to the remote mail server
void send_to_remote(const std::string& recipient, const std::string& sender, std::string message);

// Helper functions from HW2:test.h
void log(const char *prefix, const char *data, int len, const char *suffix);
void writeString(connection *conn, const char *data);
void expectNoMoreData(connection *conn);
void connectToPort(connection *conn, int portno, const char *ip);
void expectToRead(connection *conn, const char *data);
void expectRemoteClose(connection *conn);
void initializeBuffers(connection *conn, int bufferSizeBytes);
void closeConnection(connection *conn);
void freeBuffers(connection *conn);

#endif
