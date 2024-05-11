#include "exceptions.h"



string resp_OK="OK";
string resp_CANCELLED="CANCELLED";

string resp_NOT_FOUND="NOT_FOUND";
string resp_ALREADY_EXISTS="ALREADY_EXISTS";
string resp_PERMISSION_DENIED="PERMISSION_DENIED";

string resp_UNKNOWN="UNKNOWN";
string resp_INVALID_ARGUMENT="INVALID_ARGUMENT";
string resp_DEADLINE_EXCEEDED="DEADLINE_EXCEEDED";

string resp_DEF="UNKNOWN_CODE";



string resp_UNAUTHENTICATED="UNAUTHENTICATED";
string resp_RESOURCE_EXHAUSTED="RESOURCE_EXHAUSTED";
string resp_FAILED_PRECONDITION="FAILED_PRECONDITION";
string resp_ABORTED="ABORTED";

string resp_UNAVAILABLE="UNAVAILABLE";
string resp_DATA_LOSS="DATA_LOSS";
string resp_DO_NOT_USE="DO_NOT_USE";
string resp_UNKNOWN_CODE="UNKNOWN_CODE";


string resp_OUT_OF_RANGE="OUT_OF_RANGE";
string resp_UNIMPLEMENTED="UNIMPLEMENTED";
string resp_INTERNAL="INTERNAL";

string grpc_status_to_string(StatusCode inputc) {
    switch (inputc) {
        case OK:
            return resp_OK;
        case CANCELLED:
            return resp_CANCELLED;
        case ALREADY_EXISTS:
            return resp_ALREADY_EXISTS;
        case PERMISSION_DENIED:
            return resp_PERMISSION_DENIED;
        case NOT_FOUND:
            return resp_NOT_FOUND;
        case DEADLINE_EXCEEDED:
            return resp_DEADLINE_EXCEEDED;
        case FAILED_PRECONDITION:
            return resp_FAILED_PRECONDITION;
        case ABORTED:
            return resp_ABORTED;
        case UNIMPLEMENTED:
            return resp_UNIMPLEMENTED;
        case INTERNAL:
            return resp_INTERNAL;
        case RESOURCE_EXHAUSTED:
            return resp_RESOURCE_EXHAUSTED;
        case UNKNOWN:
            return resp_UNKNOWN;
        case INVALID_ARGUMENT:
            return resp_INVALID_ARGUMENT;
        case UNAUTHENTICATED:
            return resp_UNAUTHENTICATED;
        case UNAVAILABLE:
            return resp_UNAVAILABLE;
        case DATA_LOSS:
            return resp_DATA_LOSS;
        case DO_NOT_USE:
            return resp_DO_NOT_USE;
        case OUT_OF_RANGE:
            return resp_OUT_OF_RANGE;
        default:
            return resp_DEF;
    }
}
