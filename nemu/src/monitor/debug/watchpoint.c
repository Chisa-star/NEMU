#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <stdlib.h>
#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

WP *new_wp();
void free_wp(WP *wp);

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP *new_wp()
{
	if (free_ == NULL) assert(0);
	WP *p = head;
	head = free_;
	free_ = free_ -> next;
	head -> next = p;
	return head;
}

void free_wp(WP *wp)
{
	WP *i;
	if (head == wp) head = head -> next;
	else
	{
		for (i = head; i != NULL; i = i -> next)
			if (i -> next == wp)
			{
				i -> next = wp -> next;
				break;
			}
	}
	wp -> next = free_;
	free_ = wp;
	return;
}

void setwp(char *s, bool *suc)
{
	WP *wp = new_wp();
	strcpy(wp -> expr, s);
	wp->val = expr(s, suc);
	printf("Watchpoint %d set on %s, initial value = %u\n", wp->NO, s, wp -> val);
}

void print_wp() {
    printf("%-6s%-15s%-6s\n", "Num", "Expr", "Value");
	WP *p = head;
    for (; p != NULL; p = p->next) {
        printf("%-6d%-15s%-6u\n", p->NO, p->expr, p-> val);
    }
}

bool check_watchpoints() {
    bool stop = false;
	WP *p = head;
    for (; p != NULL; p = p->next) {
        bool success = true;
        uint32_t new_val = expr(p->expr, &success);
        if (!success) continue;

        if (new_val != p->val) {
            printf("\nHit watchpoint %d: %s\n", p->NO, p->expr);
            printf("Old value = %u (0x%x)\n", p->val, p->val);
            printf("New value = %u (0x%x)\n", new_val, new_val);

            p-> val = new_val;  // 更新监视点记录的值
            do_int3();
            stop = true;
        }
    }

    return stop;
}
