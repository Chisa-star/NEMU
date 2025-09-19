#ifndef __PUSH_H__
#define __PUSH_H__

#include "cpu/exec/helper.h"

make_helper(push_r_v); // push 寄存器（统一接口，根据 ops_decoded.is_operand_size_16 自动选择 16/32 位版本）

#endif
