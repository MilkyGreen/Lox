#include <stdio.h>

#include "debug.h"
#include "value.h"

/**
 * @brief 可视化打印chunk
 * 
 * @param chunk 指针
 * @param name  名称
 */
void disassembleChunk(Chunk *chunk, const char *name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;)
    {
        // 依次打印每个指令。由于不同指令所占的空间不用，offset需要具体的指令返回，而不是 ++
        offset = disassembleInstruction(chunk, offset);
    }
}

/**
 * @brief 打印简单指令
 * 
 * @param name 
 * @param offset 
 * @return int 
 */
static int simpleInstruction(const char *name, int offset)
{
    // 直接将指令名称打印出来。只占一个空间，因此offset+1，
    printf("%s\n", name);
    return offset + 1;
}

/**
 * @brief 打印常量指令
 * 
 * @param name 指令名称
 * @param chunk  对象指针
 * @param offset 指令索引
 * @return int 
 */
static int constantInstruction(const char *name, Chunk *chunk,
                               int offset)
{
    // 常量指令后面跟的是常量值在constants里的索引
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);  // 先打印名称和值的索引位置
    printValue(chunk->constants.values[constant]); // 打印常量值
    printf("'\n");
    return offset + 2; // 指令+常量值，因此+2
}

/**
 * @brief 打印指令
 * 
 * @param chunk 指针
 * @param offset  指令所在位置
 * @return int 
 */
int disassembleInstruction(Chunk *chunk, int offset)
{
    printf("%04d ", offset);

    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1])
    {
        // 如果这个指令和上个指令行数一样，行数省略成 |
        printf("   | ");
    }
    else
    {
        // 打印行数
        printf("%4d ", chunk->lines[offset]);
    }

    // 从code中从获取指令，根据不同的指令执行不同的打印函数
    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_CONSTANT:
        return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}