#include "cpu/exec/helper.h"

// 16 位版本
#define DATA_BYTE 2
#include "push-template.h"
#undef DATA_BYTE

// 32 位版本
#define DATA_BYTE 4
#include "push-template.h"
#undef DATA_BYTE

// 生成统一接口
make_helper_v(push_i)   // push 立即数
make_helper_v(push_r)   // push 寄存器
make_helper_v(push_rm)  // push 内存
