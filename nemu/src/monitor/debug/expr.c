#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
    NOTYPE = 256, EQ, NUM, LEFT, RIGHT, DEREF, HEX,
    NEQ, AND, OR, NOT, REG
};

static struct rule {
    char *regex;
    int token_type;
} rules[] = {
    {"-0x[0-9a-fA-F]+", HEX},     /* 负十六进制数字 */
    {"0x[0-9a-fA-F]+", HEX},      /* 正十六进制数字 */
    {"-[0-9]+", NUM},             /* 负十进制数字 */
    {"[0-9]+", NUM},              /* 正十进制数字 */
    {" +",    NOTYPE},            /* spaces */
    {"==",    EQ},                /* equal */
    {"!=",    NEQ},               /* not equal */
    {"&&",    AND},               /* logical and */
    {"\\|\\|", OR},               /* logical or */
    {"\\+",   '+'},               /* plus */
    {"\\*",   '*'},               /* star (可能是乘法或解引用) */
    {"-",     '-'},               /* minus */
    {"/",     '/'},               /* divide */
    {"!",     NOT},               /* logical not */
    {"\\(",   LEFT},              /* left paren */
    {"\\)",   RIGHT},             /* right paren */
    {"\\$[a-zA-Z]+[0-9]*", REG}   /* register: $eax, $eip ... */
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

                if (rules[i].token_type == '*') {
                    if (nr_token == 0 || 
                        tokens[nr_token-1].type == '+' ||
                        tokens[nr_token-1].type == '-' ||
                        tokens[nr_token-1].type == '*' ||
                        tokens[nr_token-1].type == '/' ||
                        tokens[nr_token-1].type == EQ ||
                        tokens[nr_token-1].type == NEQ ||
                        tokens[nr_token-1].type == AND ||
                        tokens[nr_token-1].type == OR ||
                        tokens[nr_token-1].type == LEFT) {
                        tokens[nr_token].type = DEREF;
                    } else {
                        tokens[nr_token].type = '*';
                    }
                    tokens[nr_token].str[0] = '\0';
                    nr_token ++;
                }
                else if (rules[i].token_type == NOT) { // 一元运算符 !
                    tokens[nr_token].type = NOT;
                    tokens[nr_token].str[0] = '\0';
                    nr_token ++;
                }
                else {
                    switch(rules[i].token_type) {
                        case NUM:
                        case HEX:
                        case REG: {
                            tokens[nr_token].type = rules[i].token_type;
                            if (substr_len >= (int)sizeof(tokens[nr_token].str))
                                substr_len = (int)sizeof(tokens[nr_token].str) - 1;
                            strncpy(tokens[nr_token].str, substr_start, substr_len);
                            tokens[nr_token].str[substr_len] = '\0';
                            nr_token ++;
                            break;
                        }
                        case NOTYPE:
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

uint32_t swaddr_read(uint32_t addr, int len);

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

        int pri = 999;
        switch (tokens[i].type) {
            case OR:  pri = -2; break;
            case AND: pri = -1; break;
            case EQ: case NEQ: pri = 0; break;
            case '+': case '-': pri = 1; break;
            case '*': case '/': pri = 2; break;
            default: continue;
        }

        if (pri <= min_priority) {
            min_priority = pri;
            main_op_pos = i;
        }
    }
    return main_op_pos;
}

uint32_t isa_reg_str2val(const char *s, bool *success) {
    int i;
    // 32位寄存器
    for (i = 0; i < 8; i++) {
        if (strcmp(s, regsl[i]) == 0) { *success = true; return reg_l(i); }
        if (strcmp(s, regsw[i]) == 0) { *success = true; return reg_w(i); }
        if (strcmp(s, regsb[i]) == 0) { *success = true; return reg_b(i); }
    }
    // 特殊寄存器
    if (strcmp(s, "eip") == 0) { *success = true; return cpu.eip; }

    *success = false;
    return 0;
}

static uint32_t eval(int p, int q, bool *success) {
    if (p > q) {
        *success = false;
        return 0;
    }

    if (p == q) {
        if (tokens[p].type == NUM) {
            int val = atoi(tokens[p].str);
            *success = true;
            return (uint32_t)val;
        } else if (tokens[p].type == HEX) {
            char *str = tokens[p].str;
            int is_negative = 0;
            if (str[0] == '-') { is_negative = 1; str ++; }
            uint32_t val = (uint32_t)strtoul(str, NULL, 16);                
            *success = true;
            return is_negative ? -val : val;
        } else if (tokens[p].type == REG) {
            uint32_t val = isa_reg_str2val(tokens[p].str + 1, success);
            return val;
        } else {
            *success = false;
            return 0;
        }
    }

    if (tokens[p].type == DEREF) {
        uint32_t addr = eval(p + 1, q, success);
        if (!*success) return 0;
        return swaddr_read(addr, 4);
    }
    if (tokens[p].type == NOT) {
        uint32_t val = eval(p + 1, q, success);
        if (!*success) return 0;
        return !val;
    }

    if (check_parentheses(p, q)) {
        return eval(p + 1, q - 1, success);
    }

    int op_pos = find_main_operator(p, q);
    if (op_pos == -1) {
        *success = false;
        return 0;
    }

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
        case EQ:  *success = true; return (uint32_t)(left == right);
        case NEQ: *success = true; return (uint32_t)(left != right);
        case AND: *success = true; return (uint32_t)(left && right);
        case OR:  *success = true; return (uint32_t)(left || right);
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
