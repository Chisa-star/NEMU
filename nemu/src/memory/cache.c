#include "memory/cache.h"
#include <string.h>

L1 cache_L1[CACHE_L1_S * CACHE_L1_E];
L2 cache_L2[CACHE_L2_S * CACHE_L2_E];

void init_cache(void) {
    // 初始化L1缓存，将所有valid bit置为无效
    int i;
    for (i = 0; i < CACHE_L1_S * CACHE_L1_E; i++) {
        cache_L1[i].validVal = false;
        cache_L1[i].tag = 0;
        memset(cache_L1[i].data, 0, CACHE_B);
    }
    
    // 初始化L2缓存，将所有valid bit置为无效
    for (i = 0; i < CACHE_L2_S * CACHE_L2_E; i++) {
        cache_L2[i].validVal = false;
        cache_L2[i].dirtyVal = false;
        cache_L2[i].tag = 0;
        memset(cache_L2[i].data, 0, CACHE_B);
    }
}

bool cache_read(hwaddr_t addr, size_t len, uint32_t *data) {
    // 这里实现cache读取逻辑
    // 返回true表示命中，false表示缺失
    // 需要实现L1和L2的查找逻辑
    return false; // 暂时返回false
}

void cache_write(hwaddr_t addr, size_t len, uint32_t data) {
    // 这里实现cache写入逻辑
    // 需要处理写分配或写回策略
}