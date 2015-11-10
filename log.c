#include "log.h"

static FILE *log_fd;
static struct timespec start_time;

void LOGOPEN(char *path){
    if(path == NULL)
        log_fd = fopen("problem2-peer.txt", "a+");
    else
        log_fd = fopen(path, "a+");

    clock_gettime(CLOCK_MONOTONIC, &start_time); /* mark start time */

}

void LOGCLOSE(){
    fclose(log_fd);
}

void LOGIN(int id, int window_size){
    uint64_t diff;
    struct timespec current_time;

    if(log_fd == NULL)
        return;
    clock_gettime(CLOCK_MONOTONIC, &current_time); /* mark current time */
    // Transfer to nano second
    diff = BILLION * (current_time.tv_sec - start_time.tv_sec) + current_time.tv_nsec - start_time.tv_nsec;
    // Retransfer to millisec
    diff /= 1000000;
    
    fprintf(log_fd, "f%d\t%llu\t%d\n", id, (long long unsigned int)diff, window_size);
    fflush(log_fd);
}
