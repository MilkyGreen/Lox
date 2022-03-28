#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#define STACK_MAX 256

// 虚拟机对象
typedef struct {
    Chunk* chunk;  // 包含的chunk
    uint8_t* ip;   // 下一个要执行的指令指针
    Value stack[STACK_MAX]; // 操作栈
    Value* stackTop; // 栈顶元素（下一个空位置）
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);

/**
 * @brief 栈中放入值
 * 
 * @param value 
 */
void push(Value value);
Value pop();

#endif