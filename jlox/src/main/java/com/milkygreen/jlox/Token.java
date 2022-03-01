package com.milkygreen.jlox;

/**
 * <p>
 */
public class Token {

    // token类型
    final TokenType type;
    // 词素
    final String lexeme;
    // 字面值
    final Object literal;
    // 代码行数
    final int line;

    Token(TokenType type, String lexeme, Object literal, int line) {
        this.type = type;
        this.lexeme = lexeme;
        this.literal = literal;
        this.line = line;
    }

    public String toString() {
        return type + " " + lexeme + " " + literal;
    }

}
