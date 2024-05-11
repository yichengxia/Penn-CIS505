#include "utils.h"


void logf(const char *fmt, ...) {
    bool checker;
    struct timespec t;
    int wrong_out=-1;
    int time_curr=clock_gettime(CLOCK_REALTIME,&t);
    checker=(time_curr!=wrong_out);
    va_list argptr;
    if (checker) {
        struct tm *timer=gmtime(&t.tv_sec);
        int militime_divider=1000;
        fprintf(stdout,"%02u:%02u.%06lu ",timer->tm_min, timer->tm_sec, (t.tv_nsec/militime_divider));
    } else {
        fprintf(stderr,"Time obtaining error!");
    }
    va_start(argptr,fmt);
    time_curr=clock_gettime(CLOCK_REALTIME,&t);
    vfprintf(stdout,fmt,argptr);
    bool checker2=(time_curr!=wrong_out);
    if (checker2){
        // Fine
    }
    va_end(argptr);
}
