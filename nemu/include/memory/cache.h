#ifndef __MEMORY_CACHE_H__
#define __MEMORY_CACHE_H__

#include "common.h"

#define CACHE_b 6  //b
#define CACHE_L1_e 3  //一级高速缓存的e
#define CACHE_L1_s 7  //一级高速缓存的s
#define CACHE_L2_e 4  //二级高速缓存的e
#define CACHE_L2_s 12 //二级高速缓存的s
#define CACHE_L1_CAP (64 * 1024)  //一级高速缓存的C
#define CACHE_L2_CAP (4 * 1024 * 1024) //二级高速缓存的C
//通过对1进行右移小写字母位的操作对应的就是转化为大写字母的过程
#define CACHE_B (1 << CACHE_b)
#define CACHE_L1_E (1 << CACHE_L1_e)
#define CACHE_L1_S (1 << CACHE_L1_s)
#define CACHE_L2_E (1 << CACHE_L2_e)
#define CACHE_L2_S (1 << CACHE_L2_s)

//一级高速缓存L1的定义
typedef struct{
    uint8_t data[CACHE_B]; //字节数组存储块内数据
    uint32_t tag;  //标签位
    bool validVal;  //有效位
} L1;
//缓存块数组表示整个一级高速缓存
extern L1 cache_L1[CACHE_L1_S * CACHE_L1_E];

//二级高速缓存L2的定义，与一级高速缓存相比多了一个脏标签
typedef struct{
    uint8_t data[CACHE_B];
    uint32_t tag;
    bool validVal;
    bool dirtyVal;
} L2;

extern L2 cache_L2[CACHE_L2_S * CACHE_L2_E];

// 函数声明
void init_cache(void);
bool cache_read(hwaddr_t addr, size_t len, uint32_t *data);
void cache_write(hwaddr_t addr, size_t len, uint32_t data);

#endif