#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
    NOTYPE = 256, EQ, NUM, LEFT, RIGHT, DEREF, HEX
};

static struct rule {
    char *regex;
    int token_type;
} rules[] = {
    {"-[0-9]+", NUM},         /* 负十进制数字 */
    {"-0x[0-9a-fA-F]+", HEX}, /* 负十六进制数字 */
    {"[0-9]+", NUM},          /* 正十进制数字 */
    {"0x[0-9a-fA-F]+", HEX},  /* 正十六进制数字 */
    {" +",    NOTYPE},        /* spaces */
    {"\\+",   '+'},           /* plus */
    {"\\*",   '*'},           /* star (可能是乘法或解引用) */
    {"-",     '-'},           /* minus (只能是二元减法) */
    {"/",     '/'},           /* divide */
    {"\\(",   LEFT},          /* left paren */
    {"\\)",   RIGHT},         /* right paren */
    {"==",    EQ}             /* equal */
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

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
                int substr_len = (int)(pmatch.rm_eo - pmatch.rm_so);

                Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", 
                    i, rules[i].regex, position, substr_len, substr_len, substr_start);
                position += substr_len;

                /* 特殊处理星号：判断是乘法还是解引用 */
                if (rules[i].token_type == '*') {
                    if (nr_token == 0 || 
                        tokens[nr_token-1].type == '+' ||
                        tokens[nr_token-1].type == '-' ||
                        tokens[nr_token-1].type == '*' ||
                        tokens[nr_token-1].type == '/' ||
                        tokens[nr_token-1].type == EQ ||
                        tokens[nr_token-1].type == LEFT) {
                        tokens[nr_token].type = DEREF;
                    } else {
                        tokens[nr_token].type = '*';
                    }
                    tokens[nr_token].str[0] = '\0';
                    nr_token ++;
                }
                else {
                    switch(rules[i].token_type) {
                        case NUM:
                        case HEX: {
                            tokens[nr_token].type = rules[i].token_type;
                            if (substr_len >= (int)sizeof(tokens[nr_token].str))
                                substr_len = (int)sizeof(tokens[nr_token].str) - 1;
                            strncpy(tokens[nr_token].str, substr_start, substr_len);
                            tokens[nr_token].str[substr_len] = '\0';
                            nr_token ++;
                            break;
                        }
                        case NOTYPE:
                            /* skip spaces */
                            break;
                        default: {
                            tokens[nr_token].type = rules[i].token_type; 
                            tokens[nr_token].str[0] = '\0';
                            nr_token ++;
                            break;
                        }
                    }
                }
                break;
            }
        }

        if(i == NR_REGEX) {
            printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
            return false;
        }
    }

    return true; 
}

uint32_t vaddr_read(uint32_t addr, int len);

static bool check_parentheses(int p, int q) {
    if (p > q) return false;
    if (tokens[p].type != LEFT || tokens[q].type != RIGHT) return false;

    int count = 0;
    int i;
    for (i = p; i <= q; i++) {
        if (tokens[i].type == LEFT) count++;
        else if (tokens[i].type == RIGHT) count--;
        if (count == 0 && i < q) return false;
        if (count < 0) return false;
    }
    return count == 0;
}

static int find_main_operator(int p, int q) {
    int i;
    int level = 0;
    int main_op_pos = -1;
    int min_priority = 999;

    for (i = p; i <= q; i++) {
        if (tokens[i].type == LEFT) { level++; continue; }
        if (tokens[i].type == RIGHT) { level--; continue; }
        if (level != 0) continue;

        if (tokens[i].type == EQ) {
            if (0 <= min_priority) { min_priority = 0; main_op_pos = i; }
        } else if (tokens[i].type == '+' || tokens[i].type == '-') {
            if (1 <= min_priority) { min_priority = 1; main_op_pos = i; }
        } else if (tokens[i].type == '*' || tokens[i].type == '/') {
            if (2 <= min_priority) { min_priority = 2; main_op_pos = i; }
        } else {
            continue;
        }
    }
    return main_op_pos;
}

static uint32_t eval(int p, int q, bool *success) {
    if (p > q) {
        *success = false;
        return 0;
    }

    /* 单个 token */
    if (p == q) {
        if (tokens[p].type == NUM) {
            int val = atoi(tokens[p].str);
            *success = true;
            return (uint32_t)val;
        } else if (tokens[p].type == HEX) {
            char *str = tokens[p].str;
            int is_negative = 0;
            
            if (str[0] == '-') {
                is_negative = 1;
                str++;
            }
            
            uint32_t val = (uint32_t)strtoul(str, NULL, 16);
            *success = true;
            return is_negative ? -val : val;
        } else {
            *success = false;
            return 0;
        }
    }

    /* 处理解引用 */
    if (tokens[p].type == DEREF) {
        uint32_t addr = eval(p + 1, q, success);
        if (!*success) return 0;
        return swaddr_read(addr, 4);
    }

    /* 被括号包围 */
    if (check_parentheses(p, q)) {
        uint32_t v = eval(p + 1, q - 1, success);
        return v;
    }

    /* 查找主运算符 */
    int op_pos = find_main_operator(p, q);
    if (op_pos == -1) {
        *success = false;
        return 0;
    }

    /* 递归计算左右操作数 */
    bool s1 = false, s2 = false;
    uint32_t left = eval(p, op_pos - 1, &s1);
    if (!s1) { *success = false; return 0; }
    uint32_t right = eval(op_pos + 1, q, &s2);
    if (!s2) { *success = false; return 0; }

    switch (tokens[op_pos].type) {
        case '+': *success = true; return left + right;
        case '-': *success = true; return left - right;
        case '*': *success = true; return left * right;
        case '/':
            if (right == 0) { *success = false; return 0; }
            *success = true; return left / right;
        case EQ: *success = true; return (uint32_t)(left == right);
        default: *success = false; return 0;
    }
}

uint32_t expr(char *e, bool *success) {
    if (!make_token(e)) {
        *success = false;
        return 0;
    }
    if (nr_token == 0) {
        *success = true;
        return 0;
    }
    return eval(0, nr_token - 1, success);
}