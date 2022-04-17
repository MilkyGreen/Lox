#include <stdarg.h>
#include <stdio.h>

#include <string.h>
#include <time.h>
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

// 虚拟机对象
VM vm;

// clockNative函数
static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

/**
 * @brief 重置栈。把栈顶移向数组头部，相当于清空栈
 *
 */
static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
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

    //打印函数调用链上的所有帧栈
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    // 重置栈
    resetStack();
}

// 定义navtive函数
static void defineNative(const char* name, NativeFn function) {
    // push然后pop是确保GC不会在这期间回收这些字符串
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() {
    vm.objects = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);              // 初始化字符串缓存哈希表
    defineNative("clock", clockNative);  // 定义一个native函数
    resetStack();
}

void freeVM() {
    freeTable(&vm.globals);
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

// 执行函数调用。
// 函数的执行其实还是通过run()
// 方法来执行，只是我们把当前的函数帧改为目标函数的执行帧。
// 执行结束之后会再恢复当前的函数帧。
static bool call(ObjFunction* function, int argCount) {
    // 实际入参和函数定义不符
    if (argCount != function->arity) {
        runtimeError("Expected %d arguments but got %d.", function->arity,
                     argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    // 新的帧
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;        // 帧绑定函数
    frame->ip = function->chunk.code;  // 帧绑定函数的指令数组
    frame->slots =
        vm.stackTop - argCount -
        1;  // vm的栈始终是一个，新的函数帧的栈从函数对象位置开始，后面跟的是函数的入参
    return true;
}

// 执行函数调用
static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_FUNCTION:
                return call(AS_FUNCTION(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break;  // Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
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

// 运行指令
static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

// 从当前函数的frame指令数组中，读取一个byte
#define READ_BYTE() (*frame->ip++)

// 连线读取两个byte组成一个short
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

// 从当前frame中读取一个常量
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())

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
        disassembleInstruction(&frame->function->chunk,
                               (int)(frame->ip - frame->function->chunk.code));
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
            case OP_POP:
                pop();
                break;
            case OP_GET_LOCAL: {
                // 本地变量取值，下一个指令就是本地变量值在栈中的索引
                uint8_t slot = READ_BYTE();
                // 将值push进栈
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                // 本地变量赋值，下一个指令就是本地变量值在栈中的索引
                uint8_t slot = READ_BYTE();
                // 将索引位置的值修改。这里用peek是因为表达式后面统一都跟了一个pop，会统一把值弹出，清空栈。
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                // 获取变量名
                ObjString* name = READ_STRING();
                Value value;
                // 如果是没取到值，报错
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                // 把值放到栈中
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                // 获取变量名称
                ObjString* name = READ_STRING();
                // 将变量放入全局变量集合
                tableSet(&vm.globals, name, peek(0));
                // 把变量的value pop()出来，后面会通过变量名从全局变量中获取
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                // 读取变量名
                ObjString* name = READ_STRING();
                // 试图赋值，如果是第一次赋值，说名变量还没定义过，需要报错
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                // 赋值不对栈产生任何影响。栈里的值会在expressionStatement()
                // 加的POP指令被pop出来
                break;
            }
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
                case OP_PRINT: {
                    printValue(pop());
                    printf("\n");
                    break;
                }
                case OP_JUMP: {
                    // 无条件跳转，读取要跳转的指令数量，ip直接 +
                    uint16_t offset = READ_SHORT();
                    frame->ip += offset;
                    break;
                }
                case OP_JUMP_IF_FALSE: {
                    // 有条件跳转，如果是false，跳过一定数量的ip
                    uint16_t offset = READ_SHORT();
                    if (isFalsey(peek(0)))
                        frame->ip += offset;
                    break;
                }
                case OP_LOOP: {
                    // 循环跳转，即向前跳转一定的指令步数
                    uint16_t offset = READ_SHORT();
                    frame->ip -= offset;
                    break;
                }
                case OP_CALL: {
                    // 获取入参数量
                    int argCount = READ_BYTE();
                    // 从栈中获函数对象，执行函数
                    if (!callValue(peek(argCount), argCount)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    // callValue() 会创建一个新的frame，将它赋给当前frame，这样下一轮loop就会执行函数中的指令
                    frame = &vm.frames[vm.frameCount - 1];
                    break;
                }
                case OP_RETURN: {
                    // 获取返回值
                    Value result = pop();
                    // 函数帧减一
                    vm.frameCount--;
                    if (vm.frameCount == 0) {
                        // 如果已经退到最后，说明脚本执行结束了
                        pop();
                        return INTERPRET_OK;
                    }
                    // 丢弃执行完的函数帧在栈上的窗口，回到函数调用前的位置
                    vm.stackTop = frame->slots;
                    // 结果放入栈中
                    push(result);
                    // 恢复caller的frame，继续回到函数调用之后的指令。
                    // 每个函数都会return，会在编译阶段统一在函数后面增加return nil指令。
                    // 如果函数指定的return ，那么隐含的return nil则会因为frame的改变被跳过
                    frame = &vm.frames[vm.frameCount - 1];
                    break;
                }
            }
        }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
    }
}

/**
 * @brief 解释执行代码
 *
 * @return InterpretResult
 */
InterpretResult interpret(const char* source) {
    // 编译源码，返回顶级函数
    ObjFunction* function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    // 将函数对象放入执行栈
    push(OBJ_VAL(function));
    call(function, 0);
    return run();
}