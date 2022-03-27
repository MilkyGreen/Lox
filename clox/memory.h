#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

/**
 * @brief capacity扩容
 * 
 */
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity)*2)

/**
 * @brief 扩容数组，根据类型、指针、新旧容量，重新分配内存
 * 
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount)      \
    (type *)reallocate(pointer, sizeof(type) * (oldCount), \
                       sizeof(type) * (newCount))

/**
 * @brief 释放数组。将新容量设为0
 * 
 */
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/**
 * @brief 扩容数组
 * 
 * @param pointer 原数组指针
 * @param oldSize 原大小
 * @param newSize 新大小
 * @return void* 新指针。 tip: void * 代表无类型指针，它仅仅指向一个内存地址，并不知道这个地址存的什么类型数据。void * 可以转换为其他类型的指针，如int* ，但是之后就不能再转换了。
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif