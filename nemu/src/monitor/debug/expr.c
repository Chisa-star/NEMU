#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 使用POSIX正则表达式函数处理正则表达式 */
enum {
    NOTYPE = 256,  // 空格（无类型）
    EQ,            // 相等运算符 "=="
    NUM,           // 十进制数字
    HEX,           // 十六进制数字
    REGISTER,      // 寄存器标识符
    LEFT,          // 左括号 "("
    RIGHT,         // 右括号 ")"
    NEG,           // 一元负号
    DEREF          // 解引用运算符 *
};

/* 正则表达式规则定义 */
static struct rule {
    char *regex;      // 正则表达式模式
    int token_type;   // 对应的标记类型
} rules[] = {
    {"[0-9]+", NUM},                 // 匹配十进制数字
    {"0x[0-9a-fA-F]+", HEX},         // 匹配十六进制数字（0x开头）
    {"\\$[a-z]+", REGISTER},         // 匹配寄存器标识符（$开头）
    {" +", NOTYPE},                  // 匹配空格（忽略）
    {"\\+", '+'},                    // 匹配加号
    {"\\*", '*'},                    // 匹配乘号（可能是乘法或解引用）
    {"-", '-'},                      // 匹配减号（可能是二元减号或一元负号）
    {"/", '/'},                      // 匹配除号
    {"\\(", LEFT},                   // 匹配左括号
    {"\\)", RIGHT},                  // 匹配右括号
    {"==", EQ}                       // 匹配相等运算符
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))  // 规则数量

static regex_t re[NR_REGEX];  // 编译后的正则表达式

/* 初始化函数：编译所有正则表达式规则 */
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

/* 标记结构体 */
typedef struct token {
    int type;        // 标记类型
    char str[32];    // 标记字符串内容（用于数字和标识符）
} Token;

Token tokens[32];    // 标记数组
int nr_token;        // 标记数量

/* 内存读取函数 - 使用NEMU的vaddr_read函数 */
uint32_t vaddr_read(uint32_t addr, int len) {
    // 调用NEMU提供的内存读取函数
    // 这个函数应该已经在nemu.h中声明了
    return vaddr_read(addr, len);
}

/* 词法分析函数：将输入字符串转换为标记序列 */
static bool make_token(char *e) {
    int position = 0;
    int i;
    regmatch_t pmatch;
    
    nr_token = 0;

    while(e[position] != '\0') {
        /* 依次尝试所有规则 */
        for(i = 0; i < NR_REGEX; i ++) {
            if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
                char *substr_start = e + position;
                int substr_len = pmatch.rm_eo;

                Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", 
                    i, rules[i].regex, position, substr_len, substr_len, substr_start);
                
                position += substr_len;

                /* 特殊处理星号：判断是乘法还是解引用 */
                if (rules[i].token_type == '*') {
                    // 如果星号在开头，或者前面是运算符、左括号、逗号，则是解引用
                    if (nr_token == 0 || 
                        tokens[nr_token-1].type == '+' ||
                        tokens[nr_token-1].type == '-' ||
                        tokens[nr_token-1].type == '*' ||
                        tokens[nr_token-1].type == '/' ||
                        tokens[nr_token-1].type == EQ ||
                        tokens[nr_token-1].type == LEFT ||
                        tokens[nr_token-1].type == ',') {
                        tokens[nr_token].type = DEREF;  // 解引用运算符
                    } else {
                        tokens[nr_token].type = '*';    // 乘法运算符
                    }
                    tokens[nr_token].str[0] = '\0';
                    nr_token++;
                }
                /* 特殊处理负号：判断是一元负号还是二元减号 */
                else if (rules[i].token_type == '-') {
                    // 如果负号在开头，或者前面是运算符或左括号，则是一元负号
                    if (nr_token == 0 || 
                        tokens[nr_token-1].type == '+' ||
                        tokens[nr_token-1].type == '-' ||
                        tokens[nr_token-1].type == '*' ||
                        tokens[nr_token-1].type == '/' ||
                        tokens[nr_token-1].type == EQ ||
                        tokens[nr_token-1].type == LEFT ||
                        tokens[nr_token-1].type == ',') {
                        tokens[nr_token].type = NEG;  // 一元负号
                    } else {
                        tokens[nr_token].type = '-';  // 二元减号
                    }
                    tokens[nr_token].str[0] = '\0';
                    nr_token++;
                } 
                /* 处理其他标记 */
                else {
                    switch(rules[i].token_type) {
                        case NUM:
                        case HEX:
                        case REGISTER: {
                            // 数字和标识符：保存字符串内容
                            tokens[nr_token].type = rules[i].token_type;
                            strncpy(tokens[nr_token].str, substr_start, substr_len);
                            tokens[nr_token].str[substr_len] = '\0';
                            nr_token++;
                            break;
                        }
                        case NOTYPE: 
                            // 空格：忽略不处理
                            break;
                        default: {
                            // 运算符和其他标记：只记录类型
                            tokens[nr_token].type = rules[i].token_type; 
                            tokens[nr_token].str[0] = '\0';
                            nr_token++;
                            break;
                        }
                    }
                }
                break;
            }
        }

        /* 如果没有规则匹配，报错 */
        if(i == NR_REGEX) {
            printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
            return false;
        }
    }

    return true; 
}

/* 检查括号是否匹配 */
static bool check_parentheses(int p, int q) {
    int i;
    int count = 0;
    
    // 如果首尾不是匹配的括号，直接返回false
    if (tokens[p].type != LEFT || tokens[q].type != RIGHT) {
        return false;
    }
    
    count = 0;
    for (i = p; i <= q; i++) {
        if (tokens[i].type == LEFT) {
            count++;
        } else if (tokens[i].type == RIGHT) {
            count--;
            // 如果在到达末尾前括号就匹配完了，说明不是最外层括号
            if (count == 0 && i != q) {
                return false;
            }
        }
    }
    return count == 0;  // 最终括号数量应该平衡
}

/* 查找主运算符（根据运算符优先级） */
static int find_main_operator(int p, int q) {
    int i;
    int level = 0;          // 括号嵌套层级
    int main_op_pos = -1;   // 主运算符位置
    int min_priority = 999; // 最小优先级（数值越小优先级越高）
    
    level = 0;
    for (i = p; i <= q; i++) {
        // 跟踪括号层级
        if (tokens[i].type == LEFT) {
            level++;
        } else if (tokens[i].type == RIGHT) {
            level--;
        }
        
        // 只在最外层（括号层级为0）且不是一元操作符时考虑运算符
        if (level == 0 && tokens[i].type != NEG && tokens[i].type != DEREF) {
            int priority = 999;
            // 设置运算符优先级
            switch (tokens[i].type) {
                case '+':
                case '-':
                    priority = 1;  // 加减法优先级较低
                    break;
                case '*':
                case '/':
                    priority = 2;  // 乘除法优先级较高
                    break;
                case EQ:
                    priority = 0;  // 比较运算符优先级最低
                    break;
                default:
                    continue;      // 非运算符，跳过
            }
            
            // 找到优先级最低的运算符（即最后计算的运算符）
            if (priority <= min_priority) {
                min_priority = priority;
                main_op_pos = i;
            }
        }
    }
    
    return main_op_pos;
}

/* 递归求值函数 */
static uint32_t eval(int p, int q, bool *success) {
    if (p > q) {
        *success = false;
        return 0;
    }
    
    /* 基本情况：单个操作数 */
    if (p == q) {
        if (tokens[p].type == NUM) {
            // 十进制数字转换
            return atoi(tokens[p].str);
        } else if (tokens[p].type == HEX) {
            // 十六进制数字转换
            return strtoul(tokens[p].str, NULL, 16);
        } else if (tokens[p].type == REGISTER) {
            // 寄存器处理（这里需要根据具体实现来完善）
            *success = false;  // 暂不支持寄存器
            return 0;
        } else {
            *success = false;
            return 0;
        }
    }
    
    /* 处理一元操作符：解引用 */
    if (tokens[p].type == DEREF) {
        uint32_t addr = eval(p + 1, q, success);
        if (!*success) return 0;
        // 读取内存地址的值（32位）
        return vaddr_read(addr, 4);
    }
    
    /* 处理一元操作符：负号 */
    if (tokens[p].type == NEG) {
        uint32_t val = eval(p + 1, q, success);
        if (!*success) return 0;
        return -((int32_t)val);  // 取负值
    }
    
    /* 检查是否被括号包围 */
    if (check_parentheses(p, q)) {
        return eval(p + 1, q - 1, success);
    }
    
    /* 查找主运算符 */
    int op_pos = find_main_operator(p, q);
    if (op_pos == -1) {
        *success = false;
        return 0;
    }
    
    /* 递归计算左操作数 */
    uint32_t left_val = eval(p, op_pos - 1, success);
    if (!*success) return 0;
    
    /* 递归计算右操作数 */
    uint32_t right_val = eval(op_pos + 1, q, success);
    if (!*success) return 0;
    
    /* 执行运算 */
    switch (tokens[op_pos].type) {
        case '+': return left_val + right_val;
        case '-': return left_val - right_val;
        case '*': return left_val * right_val;  // 乘法
        case '/': 
            if (right_val == 0) {
                *success = false;  // 除零错误
                return 0;
            }
            return left_val / right_val;
        case EQ: return left_val == right_val;
        default:
            *success = false;
            return 0;
    }
}

/* 主表达式求值函数 */
uint32_t expr(char *e, bool *success) {
    // 首先进行词法分析
    if(!make_token(e)) {
        *success = false;
        return 0;
    }
    
    // 如果没有标记，返回0
    if (nr_token == 0) {
        *success = true;
        return 0;
    }
    
    // 进行递归求值
    uint32_t result = eval(0, nr_token - 1, success);
    
    return result;
}