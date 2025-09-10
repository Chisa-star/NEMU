#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

enum {
  NOTYPE = 256, EQ, NUM,
  // 其他自定义 token_type 可以继续往下加
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {
  {" +",    NOTYPE},    // 空格
  {"\\+",   '+'},       // 加号
  {"-[0-9]+", NUM},     // 负数整体
  {"-",     '-'},       // 减号
  {"\\*",   '*'},       // 乘号
  {"/",     '/'},       // 除号
  {"[0-9]+", NUM},      // 正整数
  // 以后你要加括号也可以: {"\\(", '('}, {"\\)", ')'},
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

static regex_t re[NR_REGEX];

void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32];
static int nr_token;

static bool make_token(char *e) {
  int position = 0;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    int i;
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        position += substr_len;

        if (rules[i].token_type == NOTYPE) {
          break; // 空格直接跳过
        }

        tokens[nr_token].type = rules[i].token_type;
        if (rules[i].token_type == NUM) {
          if (substr_len >= sizeof(tokens[nr_token].str)) {
            panic("number too long");
          }
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
        } else {
          tokens[nr_token].str[0] = '\0';
        }
        nr_token ++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static int eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }
  else if (p == q) {
    if (tokens[p].type == NUM) {
      int val;
      sscanf(tokens[p].str, "%d", &val);
      return val;
    }
    else {
      *success = false;
      return 0;
    }
  }

  // 这里只实现最简单的左右递归，支持 + - * /
  int op = -1;
  int min_pri = 10;
  int i;
  for (i = p; i <= q; i++) {
    int pri = 10;
    if (tokens[i].type == '+' || tokens[i].type == '-') pri = 1;
    else if (tokens[i].type == '*' || tokens[i].type == '/') pri = 2;

    if (pri <= min_pri && pri != 10) {
      min_pri = pri;
      op = i;
    }
  }

  if (op == -1) {
    *success = false;
    return 0;
  }

  bool success1 = true, success2 = true;
  int val1 = eval(p, op - 1, &success1);
  int val2 = eval(op + 1, q, &success2);
  if (!success1 || !success2) {
    *success = false;
    return 0;
  }

  switch (tokens[op].type) {
    case '+': return val1 + val2;
    case '-': return val1 - val2;
    case '*': return val1 * val2;
    case '/': return val1 / val2;
    default: assert(0);
  }
  return 0;
}

int expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  return eval(0, nr_token - 1, success);
}
