#pragma once

#include <string>
#include <exception>
#include <grpcpp/grpcpp.h>


using namespace std;
using namespace grpc;


string grpc_status_to_string(StatusCode input_c);


class KVError : public exception {
    int errorID=0;
    StatusCode code;
    int started=0;
    string msg;
    string msg_with_code;

public:
    explicit KVError(const Status &status) : KVError(status.error_code(), status.error_message()) {}

    bool error_reported=false;
    KVError(StatusCode code, const string &msg) : code(code), msg(msg) {
        string out_temp;
        out_temp=grpc_status_to_string(code)+": ";
        msg_with_code=out_temp+msg;
    }

    string error_cntnt;

    const char *what() const noexcept override {
        string temp=msg_with_code;
        return temp.c_str();
    }
};


class KVNoMatch : public KVError {
    using KVError::KVError;
    int debug_ID=0;
};



class KVNotFound : public KVError {
    int debug_ID=0;
    using KVError::KVError;
};