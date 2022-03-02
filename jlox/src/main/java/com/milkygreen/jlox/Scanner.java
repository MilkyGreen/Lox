package com.milkygreen.jlox;


import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static com.milkygreen.jlox.TokenType.*;

/**
 * 源代码扫描处理类
 *
 * 主要讲原始的代码字符串识别成token
 */
public class Scanner {

    private final String source;
    private final List<Token> tokens = new ArrayList<>();

    private int start = 0;  // lexeme的开始位置
    private int current = 0; // 当前扫描到的位置
    private int line = 1; // 当前行数

    // 所有的关键字
    private static final Map<String, TokenType> keywords;

    static {
        keywords = new HashMap<>();
        keywords.put("and",    AND);
        keywords.put("class",  CLASS);
        keywords.put("else",   ELSE);
        keywords.put("false",  FALSE);
        keywords.put("for",    FOR);
        keywords.put("fun",    FUN);
        keywords.put("if",     IF);
        keywords.put("nil",    NIL);
        keywords.put("or",     OR);
        keywords.put("print",  PRINT);
        keywords.put("return", RETURN);
        keywords.put("super",  SUPER);
        keywords.put("this",   THIS);
        keywords.put("true",   TRUE);
        keywords.put("var",    VAR);
        keywords.put("while",  WHILE);
    }

    Scanner(String source) {
        this.source = source;
    }

    /**
     * 逐字扫描源代码，解析成 Token 列表
     *
     * @return
     */
    List<Token> scanTokens() {
        while (!isAtEnd()) {
            // 下一个 lexeme（词素）的开始位置
            start = current;
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line));
        return tokens;
    }

    /**
     * 从源代码中识别出下一个token
     */
    private void scanToken() {
        char c = advance();
        switch (c) {
            // 单字符token
            case '(':
                addToken(LEFT_PAREN);
                break;
            case ')':
                addToken(RIGHT_PAREN);
                break;
            case '{':
                addToken(LEFT_BRACE);
                break;
            case '}':
                addToken(RIGHT_BRACE);
                break;
            case ',':
                addToken(COMMA);
                break;
            case '.':
                addToken(DOT);
                break;
            case '-':
                addToken(MINUS);
                break;
            case '+':
                addToken(PLUS);
                break;
            case ';':
                addToken(SEMICOLON);
                break;
            case '*':
                addToken(STAR);
                break;
            // 单或双字符token
            case '!':
                // 判断后面是否跟着等号
                addToken(match('=') ? BANG_EQUAL : BANG);
                break;
            case '=':
                addToken(match('=') ? EQUAL_EQUAL : EQUAL);
                break;
            case '<':
                addToken(match('=') ? LESS_EQUAL : LESS);
                break;
            case '>':
                addToken(match('=') ? GREATER_EQUAL : GREATER);
                break;

            // 长字符
            case '/':
                if (match('/')) {
                    // 斜杠有两种情况，一种是除法，一种是单行注释。
                    // 如果是注释，就需要忽略掉 // 和 /n 之间的字符
                    while (peek() != '\n' && !isAtEnd()) {
                        advance();
                    }
                } else {
                    addToken(SLASH);
                }
                break;
            case ' ':
            case '\r':
            case '\t':
                // 空白、制表等全都忽略
                break;

            case '\n':
                line++; // 行数+1
                break;

                // 处理string
            case '"':
                string();
                break;

            default:
                if (isDigit(c)) {
                    // 处理数字
                    number();
                } else if (isAlpha(c)) {
                    // 其余的一律当成identifier（变量名、方法名、属性名等等）或关键字处理
                    identifier();
                } else {
                    Lox.error(line, "Unexpected character.");
                }
                break;
        }
    }

    private void identifier() {
        while (isAlphaNumeric(peek())){
            advance();
        }

        String text = source.substring(start, current);
        TokenType type = keywords.get(text); // 如果是关键字，按关键字处理
        if (type == null){
            type = IDENTIFIER;
        }
        addToken(type);
    }

    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                c == '_';
    }

    private boolean isAlphaNumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }

    private void number() {
        while (isDigit(peek())){
            advance();
        }

        // 处理小数点
        if (peek() == '.' && isDigit(peekNext())) {
            // 有小数点，且后面跟着数字，需要当成一个整体的数字
            advance();

            while (isDigit(peek())){
                advance();
            }
        }

        addToken(NUMBER,
                Double.parseDouble(source.substring(start, current)));
    }

    // 查询下下一个字符
    private char peekNext() {
        if (current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1);
    }

    /**
     * 处理string类型的token
     */
    private void string() {
        while (peek() != '"' && !isAtEnd()) {
            // 向后找直到下一个双引号
            if (peek() == '\n') {  // string可能是跨行的
                line++;
            }
            advance();
        }

        if (isAtEnd()) { // 缺少后面的双引号
            Lox.error(line, "Unterminated string.");
            return;
        }

        // 走到下一个双引号的位置
        advance();

        // 截取的时候把开始和结束的双引号忽略
        String value = source.substring(start + 1, current - 1);
        addToken(STRING, value);
    }

    /**
     * 当前字符是否符合预期
     *
     * @param expected 预期字符
     * @return
     */
    private boolean match(char expected) {
        if (isAtEnd()){
            return false;
        }
        if (source.charAt(current) != expected) {
            return false;
        }
        current++;
        return true;
    }

    /**
     * 查看下一个字符，但是并不前进
     * @return
     */
    private char peek() {
        if (isAtEnd()){
            return '\0';
        }
        return source.charAt(current);
    }

    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    /**
     * 向前读取一个字符
     *
     * @return
     */
    private char advance() {
        return source.charAt(current++);
    }

    /**
     * 添加一个单字符token
     *
     * @param type
     */
    private void addToken(TokenType type) {
        addToken(type, null);
    }

    /**
     * 添加一个token
     *
     * @param type    Token类型
     * @param literal token的原始字面值
     */
    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current); // 从源代码中截取出识别到的token文本
        tokens.add(new Token(type, text, literal, line));
    }

    /**
     * 是否扫描到了代码的结束位置
     *
     * @return
     */
    private boolean isAtEnd() {
        return current >= source.length();
    }


}
