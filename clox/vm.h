#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

// 虚拟机对象
typedef struct {
    Chunk* chunk;  // 包含的chunk
    uint8_t* ip;   // 下一个要执行的指令指针
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);

#endif