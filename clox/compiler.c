#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;   // 当前的token
    Token previous;  // 上一个token
    bool hadError;   // 是否遇到了错误
    // 是否处于panic模式（遇到语法错误，直到一个语句结束会推出panic模式，忽略中间的错误）
    bool panicMode;
} Parser;

/**
 * @brief 定义一个函数类型 ParseFn ，代表一个token对应的前缀或中缀parser函数
 * 
 */
typedef void (*ParseFn)(bool canAssign);

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
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;

// 正在编译中的chunk
Chunk* compilingChunk;

// 获取当前chunk
static Chunk* currentChunk() {
    return compilingChunk;
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
 * @brief 每次从源码中scanner出一个token，转成指令放入chunk
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
// 写入一个return指令
static void emitReturn() {
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

// 结束编译
static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
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

static void binary(bool canAssign) {
    // 获取当前token
    TokenType operatorType = parser.previous.type;
    // 获取该token的parse规则
    ParseRule* rule = getRule(operatorType);
    // 先试图解析更高一层的规则
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
    uint8_t arg = identifierConstant(&name);

    if (canAssign && match(TOKEN_EQUAL)) {
        // 如果可以赋值且后面跟等号，则需要解析后面变量值的表达式，然后放一个OP_SET_GLOBAL指令
        expression();
        emitBytes(OP_SET_GLOBAL, arg);
    } else {
        // 不然就是一个变量获取
        emitBytes(OP_GET_GLOBAL, arg);
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
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
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
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
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
// chunk里的元素应该是: 1 2 3 * + 。 放入栈中的之后的顺序是：3 2 1, 计算是先取 +
// ，然后取* 取出2和3，相乘放6进入栈，再取6和1，相加
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

    // 后面一个token如果优先级更高，则和前面处理过的那些token共同组成一个中缀表达式
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

// 定义变量名称，把字符串名称放入常量池，后面只使用常量池索引
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

// 变量定义
static void defineVariable(uint8_t global) {
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

// 解析表达式
static void expression() {
    // 先从优先级最低的 = 操作开始
    parsePrecedence(PREC_ASSIGNMENT);
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
    // 定义变量指令到栈中
    defineVariable(global);
}

// 处理表达式声明。（就是表达式）
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP); // 表达式是逐行执行的，上一行的结果并不对下一行有影响。一个表达式执行完之后需要把结果pop。对后面代码的影响只能通过变量来做。
}

// 打印 Statement
static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
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
    if (match(TOKEN_VAR)) {
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
    } else {
        expressionStatement();
    }
}

/**
 * @brief 编译源代码
 *
 * @param source
 */
bool compile(const char* source, Chunk* chunk) {
    initScanner(source);  // 先交给scanner进行token识别
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    // 循环处理declaration。代码就是由多个declaration组成的
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    endCompiler();
    return !parser.hadError;
}