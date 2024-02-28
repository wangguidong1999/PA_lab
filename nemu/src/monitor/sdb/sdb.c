/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/paddr.h>
static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
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
  nemu_state.state = NEMU_QUIT;
  return -1;
}


static int cmd_si(char *args) {
	if (args == NULL) {
	/* no argument given, default N = 1 */
		cpu_exec(1);
		return 0;
	}
	else {
		char *arg = strtok(NULL, " ");
		/* arg: char* -> uint64_t */
		if (arg != NULL) {
        int64_t N = strtoll(arg, NULL, 0);
				//printf("N(int64_t) = %ld\n", N);
				if (N <= 0) {
					printf("Please input a positive interger\n");
					return 0;
				}
				else {
					cpu_exec(N);
					return 0;
				}
    } else {
        // 处理 arg 为 NULL 的情况
        printf("Invalid argument.\n");
        return -1;
    }
	}
	return -1;
}

static int cmd_info(char *args) {
	if (args == NULL) {
	/* no argument given, input again */
		printf("Please input \"info [SUBCMD]\", SUBCMD can be \"r\" or \"w\"\n");
		return 0;
	}
	else {
		char *arg = strtok(NULL, " ");
		/* arg needs to be "r" or "w". */
		if (arg != NULL) {
			if (0 == strcmp(arg, "r")) {
				isa_reg_display();
				return 0;
			}
			else if (0 == strcmp(arg, "w")) {
			// TODO	
				return 0;
			}
			else {
				printf("SUBCMD not defined\n");
				return 0;
			}		
		}
		else {
			printf("Invalid argument.\n");
			return -1;
		}
	}
	return -1;
}

static int cmd_x(char *args) {

	if (args == NULL) {
	/* no argument given, input again */
		printf("Please input as like \"x N EXPR\",\n");
		return 0;
	}	
	char *N_char = strtok(NULL, " ");
	int32_t N = strtol(N_char, NULL, 0);
	if (N <= 0) {
		printf("Please input N as a positive interger\n");
		return 0;
	}
	char *EXPR_char = strtok(NULL, " ");//need to be positive
/*
	int32_t EXPR = strtol(EXPR_char, NULL, 0);
	if (EXPR <= 0) {
		printf("Please make EXPR a positive interger\n");
		return 0;
	}
	uint32_t addr = (uint32_t)EXPR;
*/
	uint32_t addr = strtol(EXPR_char, NULL, 0);	
	uint32_t mem_content = 0;
	//printf("N = %d\n", N);
	//printf("addr = %x\n", addr);
	//printf("mem_content = %x\n", paddr_read(addr, 4));
	
	for (int i = 0; i < N; i++) {
		mem_content = paddr_read(addr, 4);// 4 bytes : return *(uint32_t *)addr;
		addr = addr + 4;
		printf("%x\t%08x\n", addr, mem_content);
	}
	
	return 0;
}

static int cmd_p(char *args) {
	return 0;
}

static int cmd_w(char *args) {
	return 0;
}

static int cmd_d(char *args) {
	return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
	{ "si", "si [N] : Execute N instructions step by step, and pause. When N is not given, default=1", cmd_si},
	{ "info", "info r : Display state of registers.  info w : Display watchpoints information", cmd_info},
	{ "x", "x N EXPR : Calculate value of expression, Taking the value as beginning memory address, print sequential N 4-bytes with hex. ", cmd_x},
	{ "p", "p EXPR : Calculate value of expression", cmd_p},
	{ "w", "w EXPR : Set watchpoint, pause execution when value of EXPR changes.", cmd_w},
	{ "d", "d N: Delete watchpoint with number N", cmd_d},
	
	

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
