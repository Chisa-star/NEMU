#include "common.h"
#include "memory/cache.h"  // 添加cache头文件

uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);

/* Memory accessing interfaces */

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
#ifdef CACHE_ENABLED
    // 先尝试从cache读取
    uint32_t data;
    if (cache_read(addr, len, &data)) {
        return data & (~0u >> ((4 - len) << 3));
    } else {
        // cache缺失，从DRAM读取并填充cache
        data = dram_read(addr, len);
        cache_write(addr, len, data);  // 填充cache
        return data & (~0u >> ((4 - len) << 3));
    }
#else
    // 如果没有启用cache，直接读取DRAM
    return dram_read(addr, len) & (~0u >> ((4 - len) << 3));
#endif
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
#ifdef CACHE_ENABLED
    // 先写cache（写分配或写回策略取决于cache实现）
    cache_write(addr, len, data);
    // 根据写策略决定是否立即写回DRAM
    // 如果是写直达(write-through)，需要同时写DRAM
    // 如果是写回(write-back)，可以延迟写回
    dram_write(addr, len, data);  // 这里假设写直达策略
#else
    // 如果没有启用cache，直接写入DRAM
    dram_write(addr, len, data);
#endif
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
    return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
    hwaddr_write(addr, len, data);
}

uint32_t swaddr_read(swaddr_t addr, size_t len) {
#ifdef DEBUG
    assert(len == 1 || len == 2 || len == 4);
#endif
    return lnaddr_read(addr, len);
}

void swaddr_write(swaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
    assert(len == 1 || len == 2 || len == 4);
#endif
    lnaddr_write(addr, len, data);
}