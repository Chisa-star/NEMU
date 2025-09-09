#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si", "Excute N instructions and then halt", cmd_si },
	{ "info", "Display the registers status", cmd_info},
	{ "x", "Find the value of the expression ExpR and use the result as the starting memory address to output n conecutive four bytes in hexadecimal format", cmd_x},
	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

static int cmd_si(char *args){
	
	uint64_t N = 0;
	
	if (args == NULL)
		N = 1;
	else 
	{
		int f = sscanf(args, "%d", &N);
		if (f <= 0)
		{ 
			printf("Args error in cmd_si\n");
			return 0;
		}
	}

	cpu_exec(N);
	
	return 0;
}

static int cmd_info(char *args)
{

	if (args[0] == 'r')
	{
		int i;
		for (i = R_EAX; i < R_EDI; i ++)
			printf("$%s\t0x%08x\t%u\n", regsl[i], reg_l(i), reg_l(i));
	
		printf("$eip\tex%08x\t%u\n", cpu.eip, cpu.eip);
	}
	return 0;

}

static int cmd_x(char *args)
{
	char *narg= strtok(args,"");
	if (narg == NULL){
		printf("Input error");
		return 0;
	}

	char *expr = narg + strlen(narg) + 1;
	int n = 0;
	swaddr_t init_address=0;

	sscanf(narg,"%d", &n);
	sscanf(expr,"%x", &init_address);

	int i;
	for(i = 0; i < n; i ++){
	if(i % 4 == 0)
		printf("0x%88x:0x%08x",init_address,swaddr_read(init_address,4));else if((i+1)%4==0)printf("0x%08x\n",swaddr_read(init_address,4));
	else
		printf("0x%08x",swaddr_read(init_address,4));init_address += 4;
	}
	printf("\n");
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
