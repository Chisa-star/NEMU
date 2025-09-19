#include "cpu/exec/template-start.h"

#define instr push

// 执行核心模板
static void do_execute() {
    // op_dest 保存要压入栈的值
    reg_l(R_ESP) -= DATA_BYTE;
    swaddr_write(reg_l(R_ESP), DATA_BYTE, op_src->val);
    print_asm_template1();  // 打印反汇编
}

// 生成不同寻址模式的 helper
make_instr_helper(i)      // 立即数 -> stack
make_instr_helper(r)      // 寄存器 -> stack
make_instr_helper(rm)     // 内存 -> stack

#include "cpu/exec/template-end.h"
