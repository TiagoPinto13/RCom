// alarm.h
#ifndef ALARM_H
#define ALARM_H

#include <signal.h>

#define FALSE 0
#define TRUE 1
extern int alarmEnabled;
extern int alarmCount;

void alarmHandler(int signal);
int alarmTriggers(int seconds);

#endif // ALARM_H