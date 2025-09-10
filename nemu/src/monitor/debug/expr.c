#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>   // for atoi

enum {
  NOTYPE = 256, EQ, NUM, LEFT, RIGHT

  /* TODO: Add more token types */
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {
  {"[0-9]+", NUM},          // 数字
  {" +",  NOTYPE},          // 空格
  {"\\+", '+'},             // +
  {"\\*", '*'},             // *
  {"-", '-'},               // -
  {"/", '/'},               // /
  {"\\(", LEFT},            // (
  {"\\)", RIGHT},           // )
  {"==", EQ}                // ==
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for(i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if(ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while(e[position] != '\0') {
    for(i = 0; i < NR_REGEX; i ++) {
      if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
          i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        switch(rules[i].token_type) {
          case NUM: {
            tokens[nr_token].type = NUM;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token ++;
            break;
          }
          case NOTYPE: break;
          default: {
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].str[0] = '\0';
            nr_token ++;
            break;
          }
        }
        break;
      }
    }

    if(i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n",
        position, e, position, "");
      return false;
    }
  }

  return true;
}

/* 辅助函数：括号检查 */
static bool check_parentheses(int p, int q) {
  if (tokens[p].type != LEFT || tokens[q].type != RIGHT) return false;

  int balance = 0;
  int i;
  for (i = p; i <= q; i++) {
    if (tokens[i].type == LEFT) balance++;
    else if (tokens[i].type == RIGHT) balance--;

    if (balance == 0 && i < q) return false;
  }
  return balance == 0;
}

/* 查找主操作符 */
static int find_main_operator(int p, int q) {
  int i;
  int pos = -1;
  int min_pri = 100;
  int balance = 0;

  for (i = p; i <= q; i++) {
    if (tokens[i].type == LEFT) { balance++; continue; }
    if (tokens[i].type == RIGHT) { balance--; continue; }
    if (balance > 0) continue;

    int pri = 10;
    switch(tokens[i].type) {
      case EQ: pri = 1; break;
      case '+': case '-': pri = 2; break;
      case '*': case '/': pri = 3; break;
      default: pri = 100; break;
    }

    if (pri <= min_pri) {
      min_pri = pri;
      pos = i;
    }
  }
  return pos;
}

/* 表达式求值 */
static uint32_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }
  else if (p == q) {
    if (tokens[p].type != NUM) {
      *success = false;
      return 0;
    }
    return atoi(tokens[p].str);
  }
  else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }
  else {
    int op = find_main_operator(p, q);
    if (op == -1) {
      *success = false;
      return 0;
    }

    uint32_t val1 = eval(p, op - 1, success);
    uint32_t val2 = eval(op + 1, q, success);
    if (!*success) return 0;

    switch(tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': return val2 ? val1 / val2 : 0;
      case EQ:  return val1 == val2;
      default: *success = false; return 0;
    }
  }
}

uint32_t expr(char *e, bool *success) {
  if(!make_token(e)) {
    *success = false;
    return 0;
  }

  return eval(0, nr_token - 1, success);
}
