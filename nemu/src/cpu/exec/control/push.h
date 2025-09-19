#ifndef __PUSH_H__
#define __PUSH_H__

#include "cpu/exec/helper.h"

// 声明 push 指令相关的 helper
make_helper(push_i_v);       // push 立即数
make_helper(push_r_v);       // push 寄存器
make_helper(push_rm_v);      // push 内存操作数

#endif
