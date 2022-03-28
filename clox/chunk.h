#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// 指令枚举
typedef enum {
    OP_CONSTANT, // 常量
    OP_ADD, // 加法操作
    OP_SUBTRACT, // 减法
    OP_MULTIPLY, // 乘法
    OP_DIVIDE, // 除法
    OP_NEGATE, // 负号
    OP_RETURN, // 返回
} OpCode;

// 代表串指令
typedef struct {
    int count;      // 指令数组实际使用的空间
    int capacity;   // 指令数组当前总容量
    uint8_t* code;  // 指令数组。实际是第一个指令的指针
    int* lines;  // 代码行数数组。 lines[i] 代码 code[i] 指令所在的行数
    ValueArray constants;  // 常量池，该chunk中包含的所有常量
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