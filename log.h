#ifndef LOG_H
#define LOG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define BILLION 1000000000L

void LOGOPEN(char *path);
void LOGCLOSE();
void LOGIN(int, int);

#endif

