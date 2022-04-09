#include <stdarg.h>
#include <stdio.h>

#include <string.h>
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

// 虚拟机对象
VM vm;

/**
 * @brief 重置栈。把栈顶移向数组头部，相当于清空栈
 *
 */
static void resetStack() {
    vm.stackTop = vm.stack;
}

/**
 * @brief 处理运行异常
 *
 * @param format 错误信息
 * @param ...
 */
static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // 打印行数
    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    // 重置栈
    resetStack();
}

void initVM() {
    vm.objects = NULL;
    initTable(&vm.strings); // 初始化字符串缓存哈希表
    resetStack();
}

void freeVM() {
    freeTable(&vm.strings);
    // 释放所有对象占用的内存
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;  // 栈顶指针位置的值置为value
    vm.stackTop++;         // 栈顶指针向后移动一位
}

Value pop() {
    vm.stackTop--;        // 指针回一位
    return *vm.stackTop;  // 获取值
}

// 从栈中peek第distance个元素
static Value peek(int distance) {
    // -1是因为stackTop总是指向下一个空位置
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// 字符串连接操作
static void concatenate() {
    // 先取出两个字符串对象
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());
    // 计算新的长度
    int length = a->length + b->length;
    // 开辟新的字符数组空间
    char* chars = ALLOCATE(char, length + 1);
    // 依次将两个字符串拷贝到新字符串
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    // 用新字符串生成ObjString
    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)  // 宏定义：读取一个指令，ip++
#define READ_CONSTANT()         \
    (vm.chunk->constants.values \
         [READ_BYTE()])  // READ_BYTE()获取常量在数组中的索引，再从常量池中读取常量

// 二元操作的宏。先出栈两个元素，再执行op。
// 定义为do while 是为了方便后面加分号
#define BINARY_OP(valueType, op)                          \
    do {                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers.");    \
            return INTERPRET_RUNTIME_ERROR;               \
        }                                                 \
        double b = AS_NUMBER(pop());                      \
        double a = AS_NUMBER(pop());                      \
        push(valueType(a op b));                          \
    } while (false)

    // 依次执行chunk中的指令
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION

        // 执行前打印出执行栈中的所有元素，方便调试
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");

        // debug模式每次输出要执行的指令。 ip存的是指针地址，vm.ip -
        // vm.chunk->code 代表下一个指令在code数组中的offset
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                printValue(constant);
                printf("\n");
                push(constant);
                break;
            }
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    // 字符串相加
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError(
                        "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
                case OP_SUBTRACT:
                    BINARY_OP(NUMBER_VAL, -);
                    break;
                case OP_MULTIPLY:
                    BINARY_OP(NUMBER_VAL, *);
                    break;
                case OP_DIVIDE:
                    BINARY_OP(NUMBER_VAL, /);
                    break;
                case OP_NOT:
                    push(BOOL_VAL(isFalsey(pop())));
                    break;
                case OP_NEGATE:
                    if (!IS_NUMBER(peek(0))) {
                        runtimeError("Operand must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(NUMBER_VAL(-AS_NUMBER(pop())));
                    break;
                case OP_RETURN: {
                    printValue(pop());
                    printf("\n");
                    return INTERPRET_OK;
                }
            }
        }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
    }
}

/**
 * @brief 解释执行代码
 *
 * @return InterpretResult
 */
InterpretResult interpret(const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}