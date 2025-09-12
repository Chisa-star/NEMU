#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	/* TODO: Add more members if necessary */
	char expr[128];
	uint32_t val;
} WP;

void setwp(char *, bool *);
void print_wp();
void do_int3();
bool check_watchpoints();
#endif
