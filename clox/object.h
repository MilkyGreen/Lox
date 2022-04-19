#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

// 获取对象的类型
#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
// 是否是个string对象
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
// Object转成ObjString
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
// Object转成字符串（chars）
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

// 对象类型 string instence function 等等
typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
} ObjType;

// Obj类型，代表lox中的一个对象实例
struct Obj {
    ObjType type;      // 对象类型
    struct Obj* next;  // VM中的所有对象链表，方便清除内存
};

// 函数对象
typedef struct {
    Obj obj;
    int arity;        // 函数入参数量
    Chunk chunk;      // 函数的所有指令
    ObjString* name;  // 函数名称
} ObjFunction;

// 定义 NativeFn 函数
typedef Value (*NativeFn)(int argCount, Value* args);

// native函数对象
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

// String类型对象
struct ObjString {
    Obj obj;  // 第一个字段是Obj类型，这样 *ObjString 可以直接转成
              // *Obj，相当于继承的功能
    int length;     // 字符串的长度
    char* chars;    // 开始字符的引用
    uint32_t hash;  // 字符串hash值
};

ObjFunction* newFunction();

ObjNative* newNative(NativeFn function);

// 根据字符引用和长度，生成一个ObjString
ObjString* takeString(char* chars, int length);

// 根据字符引用和长度，拷贝一个ObjString
ObjString* copyString(const char* chars, int length);

// 打印Obj类型的Value
void printObject(Value value);

// 判断Value是否是指定的Obj类型
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif