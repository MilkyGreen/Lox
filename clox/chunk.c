#include "chunk.h"
#include <stdlib.h>
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(
        &chunk->constants);  // 将constants字段的指针传入函数中，进行初始化
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    // 判断capacity是否足够放入新指令
    if (chunk->capacity < chunk->count + 1) {
        // 如果不够，需要扩容数组。新的capacity是旧的两倍，创建一个新code数组，将旧数据拷贝过去。
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        // 新code数组
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        // lines和code是长度相同的，也要做相同操作
        chunk->lines =
            GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void freeChunk(Chunk* chunk) {
    // 释放code数组
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    // 释放lines数组
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    // 释放常量
    freeValueArray(&chunk->constants);
    // chunk置零
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value) {
    // 向数组中写入常量值
    writeValueArray(&chunk->constants, value);
    // 返回改常量在数组中的索引
    return chunk->constants.count - 1;
}