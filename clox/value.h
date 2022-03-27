#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// 代表代码中的一个常量值
typedef double Value;

// 定义值的动态数组
typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

// 下面是动态数组的初始化、写入、释放、打印等方法
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif