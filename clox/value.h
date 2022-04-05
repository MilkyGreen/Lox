#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// value的类型枚举
typedef enum {
  VAL_BOOL,
  VAL_NIL, 
  VAL_NUMBER,
} ValueType;

// Value代表lox中的一个值
typedef struct {
  ValueType type; // 值类型
  union {
    bool boolean;
    double number;
  } as;  // 真实的值，使用union类型节省空间，union中的字段一次只会使用一个。
} Value;

// 判断Value是否是指定的类型
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

// 从Value中取真实的值
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

// 将值包装成Value
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

// 定义值的动态数组
typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
// 下面是动态数组的初始化、写入、释放、打印等方法
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif