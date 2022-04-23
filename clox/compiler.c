#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;   // 当前的token
    Token previous;  // 上一个token。只要记住两个token就可以了
    bool hadError;   // 是否遇到了编译错误
    bool
        panicMode;  // 是否处于panic模式（遇到语法错误，直到一个语句结束会退出panic模式，忽略中间的错误）
} Parser;

/**
 * @brief 定义一个函数类型 ParseFn ，代表一个token对应的前缀或中缀parser函数
 *
 */
typedef void (*ParseFn)(bool canAssign);

// token优先级。在解析一个token的时候如果后面有更高优先级的token，则要和后面的token组成新的表达式。
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

// ParseRule代表一种token对应的前缀、中缀parser函数和优先级
typedef struct {
    // 前缀解析函数。指一个token作为表达式前缀的时候的解析函数。如
    // ！作为前缀可以组成否定表达式，prefix函数是 unary，一元表达式
    ParseFn prefix;
    // 中缀解析函数。指一个token作为表达式中缀的时候的解析函数。如
    // +，作为中缀可以组成一个二元表达式，对应的infix函数就是binary
    ParseFn infix;
    // 优先级。
    Precedence precedence;
} ParseRule;

// 本地变量结构
typedef struct {
    Token name;  // 变量token名称
    int depth;   // 作用域深度。0代表全局变量
    bool isCaptured; // 是否被其他函数当做了闭包变量使用
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

// 函数类型
typedef enum {
    TYPE_FUNCTION,  // 函数
    TYPE_SCRIPT     // 全局（整个lox脚本也认为是一个函数）
} FunctionType;

// 正在执行的Compiler
typedef struct Compiler {
    // Compiler栈，指向上一层函数的Compiler。每解析到一个函数会新建一个专用compiler，记录上一层的，杰希望之后会还原。
    struct Compiler* enclosing;

    ObjFunction* function;  // 函数对象
    FunctionType type;      // 函数类型

    Local locals[UINT8_COUNT];  // 本地变量列表
    int localCount;             // 本地变量数量
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;  // 作用域深度
} Compiler;

Parser parser;

// 当前Compiler对象引用
Compiler* current = NULL;

// 获取当前的指令数组
static Chunk* currentChunk() {
    // 先获取当前函数，再获取函数的指令数组。每遇到一个新的函数时，就会开辟一个新的chunk
    return &current->function->chunk;
}

/**
 * @brief 打印语法错误
 *
 * @param token 错误的token
 * @param message 错误信息
 */
static void errorAt(Token* token, const char* message) {
    if (parser.panicMode)  // 处于panic模式下的错误忽略掉
        return;
    parser.panicMode = true;  // 进入Panic
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

/**
 * @brief 每次从源码中scanner出一个token
 *
 */
static void advance() {
    parser.previous = parser.current;  // 记录上一个token

    // 循环扫描直到获得一个不是错误的token
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);  // 处理错误
    }
}

/**
 * @brief 按期望消费一个token，不符合期望会报错
 *
 * @param type
 * @param message
 */
static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

// 检查当前token是否是期望的类型
static bool check(TokenType type) {
    return parser.current.type == type;
}

// 检查当前token是否是期望的类型，如果符合消费掉
static bool match(TokenType type) {
    if (!check(type))
        return false;
    advance();
    return true;
}

// 写入一个指令到当前的chunk中
static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

// 连续写入两个指令
static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

// 设置循环指令和循环跳回的位置
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

// 写入跳转和跳转数量的指令
static int emitJump(uint8_t instruction) {
    // 跳转指令
    emitByte(instruction);
    // 后面用两个byte代表要跳转的步数，最多 0xffff
    // 这里的步数只是先占位，具体多少要等到后面的代码编译完才知道。
    emitByte(0xff);
    emitByte(0xff);
    // 返回跳转步数在数组中的索引，第一个占位数字的位置，后面会直接在这里赋值
    return currentChunk()->count - 2;
}

// 写入一个return指令
static void emitReturn() {
    // 默认return nil
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

// 向chunk写入一个常量值
static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// 设置之前的跳转占位符的步数
// 此时要被跳过的代码已经被编译成了指令，跳过的步数即这些指令的数量
static void patchJump(int offset) {
    // 当前的指令数量减去被跳过代码编译前的指令数量，就是要被跳过的指令数。当然还要被两个占位符去掉，因为offset占位符的索引开始位置。
    // jump就是要被跳过的指令数
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    // 把指令数写入之前的两个占位符中。由于可能超过8bit，把它拆分成两部分。
    // 先存高的8位，右移8位后低的8位会丢弃掉，跟 0xff（11111111）与操作
    // 后8位的话直接跟0xff 与操作。因为0xff只有8位，高的8位自然会被丢弃
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

/**
 * @brief 初始化Compiler
 *
 * @param compiler
 * @param type 函数类型
 */
static void initCompiler(Compiler* compiler, FunctionType type) {
    // 上级Compiler
    compiler->enclosing = current;

    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        // 当前函数名称，需要从源码字符串拷贝一下
        current->function->name =
            copyString(parser.previous.start, parser.previous.length);
    }

    // 当前compiler的第一个Local留给VM自己用
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->isCaptured = false;
    local->name.length = 0;
}

// 结束编译，返回函数对象
static ObjFunction* endCompiler() {
    // 所有函数后面都默认返回nil。如果前面的body里面已经有return,
    // 则会在执行中跳过默认的return nil
    emitReturn();
    ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL
                                             ? function->name->chars
                                             : "<script>");
    }
#endif
    current = current->enclosing;
    return function;
}

// 开启一个block
static void beginScope() {
    // 当前的作用域深度+1
    current->scopeDepth++;
}

// 结束一个block
static void endScope() {
    // 作用域 -1
    current->scopeDepth--;

    // 作用域结束后，该作用域里面的本地变量需要从运行栈中清除 POP
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
        // 如果被函数当做闭包变量来用，不能直接pop
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

// 因为递归调用顺序的问题，这些函数提前定义
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// 判断两个token是否同名
static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// 获取变量在compiler列表中的索引。从后往前找，即先找最近的作用域。
// 这个索引在执行时就代表了变量值在栈中的索引，在运行时不再通过变量名称获取值，而是直接用这个索引在栈中取值
static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

// 函数的upvalues数组中新增一个Upvalue，返回索引。如果存在返回旧索引
static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

// 尝试解析闭包变量。依次向上级函数寻找本地变量
static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL)
        return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

// 将本地变量加入到变量列表中
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    // 深度先设置为 -1，表明还未定义，只是先声明了一下。防止 var a = a + 1;
    // 这种语法
    local->depth = -1;
    local->isCaptured = false;
}

// 声明变量
static void declareVariable() {
    if (current->scopeDepth == 0) {
        // 全局变量不处理
        return;
    }

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        // 检测相同作用域（depth相等）中是否有相同变量
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    // 加入到本地变量列表中
    addLocal(*name);
}

// 定义变量名称，把字符串名称放入常量池，后面只使用常量池索引
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    // 变量声明，会将本地变量放入数组、校验是否有重名
    declareVariable();
    if (current->scopeDepth > 0) {
        // 本地变量不用放入常量池
        return 0;
    }

    return identifierConstant(&parser.previous);
}

// 设置本地变量的深度
static void markInitialized() {
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// 变量定义
static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        // 本地变量的声明不往chunk里放任何值，它由变量的初始化表达式来直接表示。
        // 如 var a = 1 + 2; 执行到这个表达式的时候，栈顶就是 3
        // ，这个就代表了变量a的值。后面用到变量a的时候来栈顶取就可以了。
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

// 解析函数调用入参
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            // 解析入参
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

// 逻辑操作符 and
// and有短路特性，即一旦出现false，就可以整体跳过后面的条件。
// 所以用一个有条件跳过指令可以实现，如果出现false，跳过整个条件表达式
static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    // 将true pop出去。
    emitByte(OP_POP);
    // 继续编译后面的表达式
    parsePrecedence(PREC_AND);
    // 设置跳过的步数
    patchJump(endJump);
}

static void binary(bool canAssign) {
    // 获取当前token
    TokenType operatorType = parser.previous.type;
    // 获取该token的parse规则
    ParseRule* rule = getRule(operatorType);
    // 先解析右半部分的表达式（左半部分已经解析过了）
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:  // !=
            emitBytes(OP_EQUAL,
                      OP_NOT);  // 不等没有定义专门的OP指令，可以用 = ! 来操作
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:        // >=
            emitBytes(OP_LESS, OP_NOT);  // >= 使用 !< 操作
            break;
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(OP_GREATER, OP_NOT);  // <= 使用 !> 操作
            break;
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
        default:
            return;  // Unreachable.
    }
}

// 函数调用解析。
// 函数调用分为
// 函数名+一对括号，写按标识符解析函数名，然后获得括号的中缀表达式方法，即call
static void call(bool canAssign) {
    // 解析入参表达式
    uint8_t argCount = argumentList();
    // 增加一个函数调用指令和参数数量
    emitBytes(OP_CALL, argCount);
}

/**
 * @brief 解析字面量token，放入chunk
 *
 */
static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        default:
            return;  // Unreachable.
    }
}

// 处理括号
static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

// 处理一个数字类型token
static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// or逻辑，任意一个true就可以跳过后面的条件
// 所以一个有条件跳转之后加一个无条件跳转。
// 先判断是否是false，有的话跳过无条件跳转指令，继续往后执行。
// 如果有true则会执行到无条件跳转，跳过后面所有的条件。
static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    // false时跳转的位置
    patchJump(elseJump);
    // 把前面的false pop出来，已经没用了。
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    // 设置结束跳转的位置
    patchJump(endJump);
}

// 处理一个String类型的token
static void string(bool canAssign) {
    // 从源码字符串拷贝一份，生成字符串对象
    emitConstant(OBJ_VAL(
        copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * @brief 处理变量token
 *
 * @param name 变量名称
 * @param canAssign 当前的表达式中是否支持变量赋值（大部分不支持）
 */
static void namedVariable(Token name, bool canAssign) {
    // 先获取变量名称的在常量池中的索引，作为指令。（为了保持执行栈的大小，不直接把变量名放入栈中）
    uint8_t getOp, setOp;
    // 查看是否是本地变量
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        // 本地变量，arg代表变量在本地变量数组中的索引，运行时会是变量值在栈中的索引。
        // 比如，var a = 1 + 2;  a == 3 ;
        // 指令列表是 1 2 + OP_GET_LOCAL 3 ==
        // 执行到OP_GET_LOCAL的时候的栈：3 。这时stack[0]
        // 就是当前scope中第一个变量的值，本地变量的名称甚至不用出现。
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        // 查看是不是闭包变量
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        // 全局变量，arg是变量在常量池中的索引，运行时需要先获取变量名称，然后去全局变量哈希表中查找值
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        // 如果可以赋值且后面跟等号，则需要解析后面变量值的表达式，然后放一个OP_SET指令
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        // 不然就是一个变量获取
        emitBytes(getOp, (uint8_t)arg);
    }
}

// 处理变量token（非定义）
static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

// 处理一元操作
static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
        default:
            return;  // Unreachable.
    }
}

// token类型和所属的前缀，中缀解析方法，优先级对应关系
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

// partt parser
// 1 + 2 * 3
// 首先遇到1，按数字解析，放入chunk。后面遇到更大优先级的+，继续获取+的中缀方法binary，执行binary，
// 看看后面是否有更高优先级的token，有的话放入chunk，最后放入自己的操作符。
// chunk里的元素应该是: 1 2 3 * + 。 放入栈中的之后的顺序是：3 2 1, 计算是先取*
// 取出2和3，相乘放6进入栈，再取6和1，相加
static void parsePrecedence(Precedence precedence) {
    advance();  // 前进一个token
    // 获取上个一个(当前操作的)token的前缀parse方法。任何一个token只少属于一个前缀表达式
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }
    // 执行前缀parser。如果优先级小于等于PREC_ASSIGNMENT，则支持变量赋值
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    // 后面一个token如果优先级更高，则和前面处理过的那些token共同组成一个中缀表达式。while是为了后面有多个高优先级中缀表达式，比如：1
    // + 2 * 3 / 6  解析成指令：1 2 3 * 6 / + 执行步骤1：栈：3 2 1   指令： * 6
    // 、 + 执行步骤2：栈：6 1     指令： 6 / + 执行步骤3：栈：6 6 1   指令: / +
    // 执行步骤4：栈：1 1     指令: +
    // 执行步骤5：栈：2       指令:
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();  // 消费一个token
        // 获取中缀解析方法
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        // 执行中缀解析
        infixRule(canAssign);
    }

    // 变量赋值不能还留在最后
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

// 解析表达式
static void expression() {
    // 先从优先级最低的 = 操作开始
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

// 创建函数对象
static void function(FunctionType type) {
    // 创建新的compiler绑定到函数
    Compiler compiler;
    initCompiler(&compiler, type);
    // 开启一个作用域
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    // 出差函数入参
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            // 入参数量+1
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            // 解析一个入参，声明为本地变量。（函数的入参就是函数方法体内的本地变量）
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    // 函数代码块
    block();

    // 返回函数对象
    ObjFunction* function = endCompiler();
    // 函数对象放入常量池。每个函数都被包装成一个闭包函数
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    // 后面跟闭包变量
    for (int i = 0; i < function->upvalueCount; i++) {
        // 是否是上级函数的变量，或者上上级的
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        // 变量的索引
        emitByte(compiler.upvalues[i].index);
    }
}

// 函数声明
static void funDeclaration() {
    // 函数名称
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    // 创建函数对象
    function(TYPE_FUNCTION);
    // 函数放入全局变量字典中
    defineVariable(global);
}

// 变量定义
static void varDeclaration() {
    // 获取变量名的常量池索引
    uint8_t global = parseVariable("Expect variable name.");

    // 后面有等于号，变量有初始化值，否则是空。先把变量值放入栈中
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    // 定义变量指令到chunk中
    defineVariable(global);
}

// 处理表达式声明。（就是表达式）
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
    // 表达式是逐行执行的，上一行的结果并不对下一行有影响。一个表达式一定会有一个结果值，执行完之后需要把结果pop，清空栈，为下一行执行做准备。
    // 对后面代码的影响只能通过变量来做。
    // 比如： 1+1;
    // 执行完完之后会在栈里放一个2，但是下一个行是不需要这个2的，需要弹出。
}

// for循环比while循环复杂的多....
// for循环分为四个部分，变量初始化、条件判断、变量递增、循环代码
// 关键的在于变量递增，因为我们的编译器是一次性从前往后编译代码的，因此变量递增的指令会出现在循环代码指令的前面
// 但是运行时需要先执行循环代码，再执行变量递增。跟指令的顺序是不一致的。
// 解决方法是多跳几次：
// 1、执行完条件判断后，跳过变量递增，直接执行循环代码。
// 2、循环代码执行完后跳转到变量递增，执行变量递增
// 3、变量递增执行完后跳转到条件判断
// 。。。。。
// 重复上面的步骤直到条件为false,跳出整个循环
static void forStatement() {
    // 因为可能有变量定义，开启一个作用域
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        // 有var，定义变量
        varDeclaration();
    } else {
        // 变量已经在外边定义，这里只赋值
        expressionStatement();
    }

    // 循环开始的位置，一轮结束之后要跳回来
    int loopStart = currentChunk()->count;

    // 条件判断 -1 代表没有条件判断，可以一直循环
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        // 条件表达式
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // 有条件跳转指令，如果条件为false需要跳出整个循环
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        // 条件值(true)pop出去。
        emitByte(OP_POP);  // Condition.
    }

    // 如果有变量递增表达式
    if (!match(TOKEN_RIGHT_PAREN)) {
        // 执行到这里时需要跳到代码体，跳过变量递增。
        int bodyJump = emitJump(OP_JUMP);
        // 变量递增指令开始的位置，循环体结束之后会跳转到这里
        int incrementStart = currentChunk()->count;
        // 变量递增表达式
        expression();
        // 把变量值pop出去
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        // 递增之后跳转的循环的开始
        emitLoop(loopStart);
        loopStart = incrementStart;
        // 统计变量递增的指令数量
        patchJump(bodyJump);
    }
    // 循环体
    statement();
    // 如果有变量递增，此时的loopStart是变量递增的位置，如果没有就是条件判断
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP);  // Condition.
    }

    endScope();
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    // 先compile条件表达式
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    // 有条件跳转指令。如果前面的表达式是true,会继续按顺序执行指令。如果是false，会跳过一些指令。具体跳过多少指令要等
    // if后面的代码编译完了才知道，这里只是先占个位，后面会把要跳过的指令数量填在thenJump的位置。
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    // 如果没被跳过，会执行到这里，把条件的值pop出来，已经没有用了。
    emitByte(OP_POP);
    // true是执行的代码块。
    statement();
    // else部分的跳过指令。如果是true才会执行到这里，那么else肯定就不能执行了，需要无条件的跳过。
    int elseJump = emitJump(OP_JUMP);
    // 这里是条件为false时要跳转到的位置，后面直接执行else的代码
    patchJump(thenJump);
    // 把栈里的false pop出来
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) {
        // 如果有 else的token才解析
        statement();
    }
    // 设置else跳过的指令数量，else代码块到这里就结束了
    patchJump(elseJump);
}

// 打印 Statement
static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

// return 声明
static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON)) {
        // 没有返回值的话直接return
        emitReturn();
    } else {
        // 解析返回表达式，然后返回
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

// while循环，循环在条件为false时，指令需要跳出来。
// 结束一次循环时，指令需要跳回循环开始的地方，重新执行循环的指令，当然这时里面的变量的值可能会变化。
static void whileStatement() {
    // 循环开始的指令索引，当执行完一轮时需要跳转回来。
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    // 条件表达式
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    // 如果为false，直接跳转到循环结束
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    // 这一轮的条件（true） pop出去
    emitByte(OP_POP);
    // 循环体
    statement();
    // 跳回开始的指令位置
    emitLoop(loopStart);
    // 设置循环结束的指令位置
    patchJump(exitJump);
    // 把false pop出去
    emitByte(OP_POP);
}

// 从异常模式中恢复编译
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:;  // Do nothing.
        }

        advance();
    }
}

// 处理declaration
static void declaration() {
    if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        // 变量定义声明
        varDeclaration();
    } else {
        // statement
        statement();
    }

    if (parser.panicMode)
        synchronize();
}

// 处理statement
static void statement() {
    if (match(TOKEN_PRINT)) {
        // 打印statement
        printStatement();
    } else if (match(TOKEN_IF)) {
        // if
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        // while循环
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        // for循环
        forStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        // 大括号开头表明遇到了一个block
        beginScope();
        block();
        endScope();
    } else {
        // 表达式
        expressionStatement();
    }
}

/**
 * @brief 编译源代码
 *
 * @param source
 */
ObjFunction* compile(const char* source) {
    initScanner(source);  // 先交给scanner进行token识别
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    // 循环处理declaration。代码就是由多个declaration组成的
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}