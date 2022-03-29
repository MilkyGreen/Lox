#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

/**
 * @brief Scanner负责将源码字符串识别为各种token
 * 
 */
typedef struct {
    const char* start; // 当前token的开始指针
    const char* current; // 当前扫描到的位置
    int line; // 行数
} Scanner;

Scanner scanner;

/**
 * @brief 初始化扫描器
 * 
 * @param source 源码字符串指针
 */
void initScanner(const char* source) {
    // 开始指向字符串开头
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// 是否到达结尾
static bool isAtEnd() {
    return *scanner.current == '\0';
}

// 获取当前字符，索引加一
static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

// 查看当前字符，索引不变
static char peek() {
    return *scanner.current;
}

// 查看下一个字符，索引不变
static char peekNext() {
    if (isAtEnd())
        return '\0';
    return scanner.current[1];
}

/**
 * @brief 下一个字符是否期望字符
 * 
 * @param expected 
 * @return true 
 * @return false 
 */
static bool match(char expected) {
    if (isAtEnd())
        return false;
    if (*scanner.current != expected)
        return false;
    // 符合期望，消费掉，索引加一
    scanner.current++;
    return true;
}

/**
 * @brief 根据type创建token
 * 
 * @param type 
 * @return Token 
 */
static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

/**
 * @brief 代码中的跳过空白、注释、换行等字符。
 * 每次获取下一个token之前调用
 * 
 */
static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++; // 行数加一
                advance();
                break;
            case '/':
                // 如果两个斜杆，后面的都算注释，跳过，直到换行
                if (peekNext() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd())
                        advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/**
 * @brief 判断是否是关键字
 * 
 * @param start 字符串开始
 * @param length 字符串长度
 * @param rest  期望的字符串
 * @param type  期望类型
 * @return TokenType 
 */
static TokenType checkKeyword(int start,
                              int length,
                              const char* rest,
                              TokenType type) {
    
    // 当前字符串长度和内容等于期望字符串，判断为关键字。否则就是token
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

/**
 * @brief 判断当前token是不是关键字。关键字是有限的，因此利用类似字典树的方法判断
 * 
 * @return TokenType 
 */
static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a':
            return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

/**
 * @brief 获取 identifier类型token
 * 
 * @return Token 
 */
static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek()))
        advance();
    return makeToken(identifierType());
}

// 数字类型token
static Token number() {
    while (isDigit(peek()))
        advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the ".".
        advance();

        while (isDigit(peek()))
            advance();
    }

    return makeToken(TOKEN_NUMBER);
}

// 字符串类型token
static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n')
            scanner.line++;
        advance();
    }

    if (isAtEnd())
        return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    return makeToken(TOKEN_STRING);
}

// 扫描一个token
Token scanToken() {
    // 先跳过前面的空白
    skipWhitespace();
    // 记录开始位置
    scanner.start = scanner.current;

    if (isAtEnd())
        return makeToken(TOKEN_EOF);

    // 每次取一个字符判断
    char c = advance();
    if (isAlpha(c))
        return identifier();
    if (isDigit(c))
        return number();

    switch (c) {
        case '(':
            return makeToken(TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(TOKEN_LEFT_BRACE);
        case '}':
            return makeToken(TOKEN_RIGHT_BRACE);
        case ';':
            return makeToken(TOKEN_SEMICOLON);
        case ',':
            return makeToken(TOKEN_COMMA);
        case '.':
            return makeToken(TOKEN_DOT);
        case '-':
            return makeToken(TOKEN_MINUS);
        case '+':
            return makeToken(TOKEN_PLUS);
        case '/':
            return makeToken(TOKEN_SLASH);
        case '*':
            return makeToken(TOKEN_STAR);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string();
    }

    return errorToken("Unexpected character.");
}