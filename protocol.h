#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <time.h>

typedef int bool;
#define true 1
#define false 0

#define JOIN_REQUEST -1
#define JOIN_CONFIRM -2
#define JOIN_ALLOW -3
#define PARK_ADD_CONFIRM -1
#define PARK_SUB_CONFIRM -2
#define PARK_ALLOW -3
#define PARK_DISALLOW -4
#define PARK_CONFIRM -5
#define ENTRANCE 0
#define EXIT 1
#define RING -6

void protocol_init();
void *start_request(void*);
void run();
void join_request();
void join_confirm();
void join_response();
void park_request();
void park_confirm(time_t);
void park_response();

#endif
