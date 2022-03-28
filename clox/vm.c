#include "vm.h"
#include <stdio.h>
#include "common.h"
#include "debug.h"

VM vm;

void initVM() {}

void freeVM() {}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)  // 宏定义：读取一个指令，ip++
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // READ_BYTE()获取常量在数组中的索引，再从常量池中读取常量

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        // debug模式每次输出要执行的指令。 ip存的是指针地址，vm.ip - vm.chunk->code 代表下一个指令在code数组中的offset
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));  
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                printValue(constant);
                printf("\n");
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
}

/**
 * @brief 解释执行代码
 * 
 * @param chunk 
 * @return InterpretResult 
 */
InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code; // 下个待执行的指令指针，是code的位置
    return run();
}