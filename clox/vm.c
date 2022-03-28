#include "vm.h"
#include <stdio.h>
#include "common.h"
#include "debug.h"

// 虚拟机对象
VM vm;

/**
 * @brief 重置栈。把栈顶移向数组头部，相当于清空栈
 * 
 */
static void resetStack() {
    vm.stackTop = vm.stack;
}

void initVM() {
    resetStack();
}

void freeVM() {}

void push(Value value) {
    *vm.stackTop = value; // 栈顶指针位置的值置为value
    vm.stackTop++; // 栈顶指针向后移动一位
}

Value pop() {
    vm.stackTop--; // 指针回一位
    return *vm.stackTop; // 获取值
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)  // 宏定义：读取一个指令，ip++
#define READ_CONSTANT()   (vm.chunk->constants.values[READ_BYTE()])  // READ_BYTE()获取常量在数组中的索引，再从常量池中读取常量

// 二元操作的宏。先出栈两个元素，再执行op。
// 定义为do while 是为了方便后面加分号
#define BINARY_OP(op)     \
    do {                  \
        double b = pop(); \
        double a = pop(); \
        push(a op b);     \
    } while (false)

    // 依次执行chunk中的指令
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION

        // 执行前打印出执行栈中的所有元素，方便调试
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");

        // debug模式每次输出要执行的指令。 ip存的是指针地址，vm.ip -
        // vm.chunk->code 代表下一个指令在code数组中的offset
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                printValue(constant);
                printf("\n");
                push(constant);
                break;
            }
            case OP_ADD:    // 利用宏展开来执行二元操作
                BINARY_OP(+);
                break;
            case OP_SUBTRACT:
                BINARY_OP(-);
                break;
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
            case OP_DIVIDE:
                BINARY_OP(/);
                break;
            case OP_NEGATE:
                push(-pop());
                break;
            case OP_RETURN: {
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

/**
 * @brief 解释执行代码
 *
 * @param chunk
 * @return InterpretResult
 */
InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;  // 下个待执行的指令指针，是code的位置
    return run();
}