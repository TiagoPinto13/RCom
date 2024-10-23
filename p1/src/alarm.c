// alarm.c
#include "alarm.h"
#include <unistd.h>
#include <stdio.h>

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal) {
    alarmEnabled = TRUE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}
