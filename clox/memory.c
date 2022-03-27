#include <stdlib.h>

#include "memory.h"

/**
 * @brief 对一个数组（指针）进行扩容
 * 
 * @param pointer 数组的第一个元素指针
 * @param oldSize 旧的长度
 * @param newSize 新的长度
 * @return void* 
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    // 如果新长度是0，代表需要回收
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  // 重新申请空间
  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

