#include "packet.h"
#include "time.h"
#include "sys/time.h"

// funtion that stores log record in file
int record_log(char* node_name, char* event_type, char* timestamp, char* pkt_type, int pkt_no, int seq_no, char* source, char* dest) {
    
    FILE* fp_log = fopen("log.txt", "a");
    if(fp_log == NULL) {
        printf("Error! Cannot open log.txt for writing\n");
        return 0;
    }
    
    if(seq_no == -1 || pkt_no == -1)
        fprintf(fp_log, "%-9s  %-10s  %-15s  %-11s  Pkt. No  Seq. No  %-6s  %-6s\n", node_name, event_type, timestamp, pkt_type, source, dest);
    else
        fprintf(fp_log, "%-9s  %-10s  %-15s  %-11s  %-7d  %-7d  %-6s  %-6s\n", node_name, event_type, timestamp, pkt_type, pkt_no, seq_no, source, dest);
    
    fclose(fp_log);
    return 1;
}

// function to get current time
char* get_current_time() {
    char* str = (char*) malloc(sizeof(char)*20);
    int rc;
    time_t curr;
    struct tm* timeptr;
    struct timeval tv;

    curr = time(NULL);
    timeptr = localtime(&curr);
    // to get time in microseconds
    gettimeofday(&tv, NULL);
    // store time in format: %H:%M:%S
    rc = strftime(str, 20, "%H:%M:%S", timeptr);
    
    // append the microseconds part
    char ms[8];
    sprintf(ms, ".%06ld", tv.tv_usec);
    strcat(str, ms);
    return str;
}