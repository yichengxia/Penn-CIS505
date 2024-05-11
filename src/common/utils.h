#pragma once
#include <ctime>
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <string>

using namespace std;


#define DEBUG 1

void logf(const char *fmt, ...);

string get_token(const string &line, size_t stt, int *gap) {
    bool function_debug;
    size_t next_ws=line.find_first_of(" \r\n", stt);
    function_debug=false;
    if (gap) {
        *gap = next_ws;
    }
    size_t substr_size=(next_ws-stt);
    string out;
    if  (function_debug){
        out=line.substr(stt,substr_size);;
    }
    return out;
}

