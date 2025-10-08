#include "cache.h"
#include "burst.h"
//初始化高速缓存
void init_cache() {
  //遍历所有高速缓存块，将有效位和脏标签清空即可
  int i;
  for (i = 0; i < CACHE_L1_S * CACHE_L1_E; i++) {
    cache_L1[i].validVal = false;
  }
  for (i = 0; i < CACHE_L2_S * CACHE_L2_E; i++) {
    cache_L2[i].dirtyVal = false;
    cache_L2[i].validVal = false;
  }
  return;
}

//声明需要调用的外部函数，实现对内存种数据的读取
void ddr3_read_me(hwaddr_t addr, void* data);
 
//查找地址对应的数据是否存在于一级高速缓存中，返回数据块位于缓存中的位置参数
int read_cache_L1(hwaddr_t addr) {
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L1_S - 1)); //获取组索引参数
  uint32_t tag = (addr >> (CACHE_b + CACHE_L1_s)); //获取标签参数
  // uint32_t block_start = ((addr >> CACHE_BLOCK_BIT) << CACHE_BLOCK_BIT);
  //遍历组内每一个缓存块进行匹配
  int block_i;
  int set_begin = set * CACHE_L1_E;
  int set_end = (set + 1) * CACHE_L1_E;
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (cache_L1[block_i].validVal && cache_L1[block_i].tag == tag) // Hit!
      return block_i;
  //如果一级高速缓存内未命中，则到二级高速缓存中查找
  srand(time(0));
  int block_i_L2 = read_cache_L2(addr);
  //二级缓存中要是也没有找到，就先看看一级高速缓存中是否存在空的缓存块
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (!cache_L1[block_i].validVal)
      break;
  //要是也没有空的缓存块，就采用随机替换策略，通过随机生成数在组内找一个缓存块进行替换
  if (block_i == set_end)
    block_i = set_begin + rand() % CACHE_L1_E;
  //写入数据到替换好的缓存块中，从二级高速缓存中读取即可，已经读入了二级高速缓存中
  memcpy(cache_L1[block_i].data, cache_L2[block_i_L2].data, CACHE_B);
 
  cache_L1[block_i].validVal = true;
  cache_L1[block_i].tag = tag;
  return block_i;
}


//声明对内存进行写操作的外部函数
void ddr3_write_me(hwaddr_t addr, void* data, uint8_t* mask);
 
// 查找二级高速缓存中与地址匹配的缓存块，返回缓存块的位置参数
int read_cache_L2(hwaddr_t addr) {
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L2_S - 1));
  uint32_t tag = (addr >> (CACHE_b + CACHE_L2_s));
  //涉及到块内写操作，要遵循空间局部性原理，因此要清空块内偏移量，找到块的起始位置
  uint32_t block_start = ((addr >> CACHE_b) << CACHE_b);
  int block_i;
  int set_begin = set * CACHE_L2_E;
  int set_end = (set + 1) * CACHE_L2_E;
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (cache_L2[block_i].validVal && cache_L2[block_i].tag == tag) 
      return block_i; // Hit!
  //二级缓存中要是没有找到，就先看看是否存在空的缓存块
  srand(time(0));
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (!cache_L2[block_i].validVal)
      break;
  //要是也没有空的缓存块，就采用随机替换策略，通过随机生成数在组内找一个缓存块进行替换
  if (block_i == set_end)
    block_i = set_begin + rand() % CACHE_L2_E;
  //随机替换找的缓存块要确定其是否脏标签被置位，如果是，需要先进行回写操作
  int i;
  if (cache_L2[block_i].validVal && cache_L2[block_i].dirtyVal) {
    //临时缓冲区结合突发读写技术读取需要写入的数据
    uint8_t tmp[BURST_LEN << 1];
    memset(tmp, 1, sizeof(tmp));
    uint32_t block_start_x = (cache_L2[block_i].tag << (CACHE_L2_s + CACHE_b)) | (set << CACHE_b);
    for (i = 0; i < CACHE_B / BURST_LEN; i++) {
      ddr3_write_me(block_start_x + BURST_LEN * i, cache_L2[block_i].data + BURST_LEN * i, tmp);
    }
  }
  //处理好回写后就可以替换块内数据，从内存中写入
  for (i = 0; i < CACHE_B / BURST_LEN; i++) {
    ddr3_read_me(block_start + BURST_LEN * i, cache_L2[block_i].data + BURST_LEN * i);
  }
  cache_L2[block_i].validVal = true;
  cache_L2[block_i].dirtyVal = false;
  cache_L2[block_i].tag = tag;
  return block_i;
}


//声明向内存中写数据的外部函数
void dram_write(hwaddr_t addr, size_t len, uint32_t data);
 
void write_cache_L1(hwaddr_t addr, size_t len, uint32_t data) {
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L1_S - 1));
  uint32_t tag = (addr >> (CACHE_b + CACHE_L1_s));
  uint32_t block_bias = addr & (CACHE_B - 1);
  int block_i;
  int set_begin = set * CACHE_L1_E;
  int set_end = (set + 1) * CACHE_L1_E;
  for (block_i = set_begin; block_i < set_end; block_i++) {
    if (cache_L1[block_i].validVal && cache_L1[block_i].tag == tag) {
      // Hit!
      // 一级缓存命中后采取直写策略，内存和一级二级缓存块都要直接写入
      // 假如数据超出当前缓存块，需要分隔存储
      if (block_bias + len > CACHE_B) {
        dram_write(addr, CACHE_B - block_bias, data);
        memcpy(cache_L1[block_i].data + block_bias, &data, CACHE_B - block_bias);
        write_cache_L2(addr, CACHE_B - block_bias, data);
        write_cache_L1(addr + CACHE_B - block_bias, len - (CACHE_B - block_bias), data >> (CACHE_B - block_bias));
      } else {
        dram_write(addr, len, data);
        memcpy(cache_L1[block_i].data + block_bias, &data, len);
        write_cache_L2(addr, len, data);
      }
      return;
    }
  }
  // 假如未命中，则去二级缓存中查找，采取回写策略
  write_cache_L2(addr, len, data);
  return;
}
 
void write_cache_L2(hwaddr_t addr, size_t len, uint32_t data) {
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L2_S - 1));
  uint32_t tag = (addr >> (CACHE_b + CACHE_L2_s));
  uint32_t block_bias = addr & (CACHE_B - 1);
  int block_i;
  int set_begin = set * CACHE_L2_E;
  int set_end = (set + 1) * CACHE_L2_E;
  for (block_i = set_begin; block_i < set_end; block_i++) {
    if (cache_L2[block_i].validVal && cache_L2[block_i].tag == tag) {
      // Hit!
      //在二级缓存中命中后需要给脏标签置位，然后写入，先不需要写入内存
      cache_L2[block_i].dirtyVal = true;
      //也是需要考虑是否需要分隔写入两个缓存块
      if (block_bias + len > CACHE_B) {
        memcpy(cache_L2[block_i].data + block_bias, &data, CACHE_B - block_bias);
        write_cache_L2(addr + CACHE_B - block_bias, len - (CACHE_B - block_bias), data >> (CACHE_B - block_bias));
      } else {
        memcpy(cache_L2[block_i].data + block_bias, &data, len);
      }
      return;
    }
  }
  // 二级缓存也未命中的话，那就先替换出缓存块再进行写入
  block_i = read_cache_L2(addr);
  cache_L2[block_i].dirtyVal = true;
  memcpy(cache_L2[block_i].data + block_bias, &data, len);
  return;
}

//外部函数声明
uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);
 
/* Memory accessing interfaces */
 
uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
  int cache_L1_way_1_index = read_cache_L1(addr);
  uint32_t block_bias = addr & (CACHE_B - 1);
  uint8_t ret[BURST_LEN << 1];
  //printf("%d\n", block_bias);
  if (block_bias + len > CACHE_B) {
    int cache_L1_way_2_index = read_cache_L1(addr + CACHE_B - block_bias);
    memcpy(ret, cache_L1[cache_L1_way_1_index].data + block_bias, CACHE_B - block_bias);
    memcpy(ret  + CACHE_B - block_bias, cache_L1[cache_L1_way_2_index].data, len - (CACHE_B - block_bias));
  } else {
    memcpy(ret, cache_L1[cache_L1_way_1_index].data + block_bias, len);
  }
  int tmp = 0;
  uint32_t result = unalign_rw(ret + tmp, 4) & (~0u >> ((4 - len) << 3));
  return result;
}
 
void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
  write_cache_L1(addr, len, data);
}