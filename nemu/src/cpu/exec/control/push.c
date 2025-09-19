#include "cpu/exec/helper.h"

// ----------------------------
// 1. 包含模板，生成 8 位 push
#define DATA_BYTE 1
#include "push-template.h"
#undef DATA_BYTE

// ----------------------------
// 2. 包含模板，生成 16 位 push
#define DATA_BYTE 2
#include "push-template.h"
#undef DATA_BYTE

// ----------------------------
// 3. 包含模板，生成 32 位 push
#define DATA_BYTE 4
#include "push-template.h"
#undef DATA_BYTE

// ----------------------------
// 4. 生成统一接口，根据操作数大小自动选择 push helper
make_helper_v(push_i)   // push 立即数
make_helper_v(push_r)   // push 寄存器
make_helper_v(push_rm)  // push 内存操作数
