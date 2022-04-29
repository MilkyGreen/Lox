#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// 指令枚举。所有的源码最终会被编译为一个个指令，VM按照顺序执行每个指令就代表了代码的执行。
typedef enum {
    OP_CONSTANT,  // 常量，后面通常会跟一个内存地址，执行常量的值。
    OP_NIL,    // 空值
    OP_TRUE,   // true
    OP_FALSE,  // false
    OP_POP,  // 从栈中取值。VM在运行代码的时候回维护一个栈，pop代表从栈顶取一个值出来
    OP_GET_LOCAL,   // 本地变量取值，后面通常跟本地变量的索引
    OP_SET_LOCAL,   // 本地变量赋值，后面通常跟本地变量的索引
    OP_GET_GLOBAL,  // 全局变量获取，后面通常跟全局变量的名称
    OP_DEFINE_GLOBAL,  // 全局变量定义，后面通常跟全局变量的名称
    OP_SET_GLOBAL,  // 全局变量设置，后面通常跟全局变量的名称
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_EQUAL,          //  =
    OP_GET_SUPER,
    OP_GREATER,        // >
    OP_LESS,           // <
    OP_ADD,            // 加法操作
    OP_SUBTRACT,       // 减法
    OP_MULTIPLY,       // 乘法
    OP_DIVIDE,         // 除法
    OP_NOT,            // !
    OP_NEGATE,         // 负号
    OP_PRINT,          // 打印
    OP_JUMP,           // 无条件跳转，会跳过一些指令
    OP_JUMP_IF_FALSE,  // 有条件跳转,当遇到false时会跳过一些指令
    OP_LOOP,           // 循环指令，会跳到之前的指令再执行
    OP_CALL,           // 函数调用
    OP_INVOKE,          // 方法直接调用
    OP_SUPER_INVOKE,    // 父类方法直接调用
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,  // 返回
    OP_CLASS,
    OP_INHERIT,
    OP_METHOD
} OpCode;

// 代表一段被编译后的代码
typedef struct {
    int count;     // 指令数量
    int capacity;  // 指令数组当前总容量（需要适时扩容数组）
    uint8_t* code;  // 指令数组。代码全部被编译成了这一串指令。
    int* lines;     // 代码行数数组。 lines[i] 代码 code[i]
                    // 指令在源码中的行数，方便异常信息打印。
    ValueArray
        constants;  // 常量数组，为了保持code数组精简，代码中的常量都被按顺序存在constants里面，code里存放的常量值的是constants数组的索引。
} Chunk;

/**
 * @brief 初始化一个chunk
 *
 * @param chunk Chunk的引用
 */
void initChunk(Chunk* chunk);

/**
 * @brief 向chunk中写入一个指令
 *
 * @param chunk  Chunk指针
 * @param byte  指令
 * @param line  代码行数
 */
void writeChunk(Chunk* chunk, uint8_t byte, int line);

/**
 * @brief 回收一个chunk
 *
 * @param chunk
 */
void freeChunk(Chunk* chunk);

/**
 * @brief 向chunk中新增常量值
 *
 * @param chunk 指针
 * @param value  常量值
 * @return int 常量在constants中的索引
 */
int addConstant(Chunk* chunk, Value value);

#endif