#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// 函数调用帧，记录一个函数在执行中的状态
typedef struct {
    ObjFunction* function;  // 函数对象
    uint8_t*
        ip;  // 函数当前的指令位置，当前调用完一个函数之后，需要回到这里继续执行
    Value* slots;  // 该函数在VM的执行栈中第一个能用的值
} CallFrame;

// 虚拟机对象
typedef struct {
    CallFrame frames[FRAMES_MAX]; // 函数调用帧数组
    int frameCount; // 当前执行中的函数帧数量，即有几个函数在执行中
    Value stack[STACK_MAX];  // 操作栈
    Value* stackTop;         // 栈顶元素（下一个空位置）
    Table strings;  // 字符串缓存哈希表。相同的字符串会使用同一个对象
    Table globals;  // 全局变量
    Obj*
        objects;  // Obj链表，保存VM中所有的Obj对象引用，VM退出的时候释放掉这些内存
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();

InterpretResult interpret(const char* source);

void freeVM();
/**
 * @brief 栈中放入值
 *
 * @param value
 */
void push(Value value);
Value pop();

#endif