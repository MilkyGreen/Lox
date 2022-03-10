package com.milkygreen.jlox;

import java.util.ArrayList;
import java.util.List;

import static com.milkygreen.jlox.TokenType.*;

/**
 * Parser负责将tokens转换成抽象语法树（Expr）
 */
public class Parser {

    private static class ParseError extends RuntimeException {}

    // 需要处理的token列表
    private final List<Token> tokens;
    // 当前处理到的token位置
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    /**
     * 对tokens进行解析
     * 采用递归下降的解析方法。对不同的操作划分优先级，每个等级可以解析大于等于自身级别的操作。
     * 比如：equality级别小于comparison，因此equality()会优先交给comparison()解析，之后再尝试自己解析。
     * @return List<Stmt>
     */
    List<Stmt> parse() {
        List<Stmt> statements = new ArrayList<>();
        while (!isAtEnd()) {
            statements.add(declaration());
        }

        return statements;
    }

    /**
     * 解析声明
     * 分为变量定义声明和其他声明
     *
     * @return
     */
    private Stmt declaration() {
        try {
            if (match(VAR)){    // var 开头说明定义了一个变量
                return varDeclaration();
            }
            return statement();
        } catch (ParseError error) {
            synchronize();
            return null;
        }
    }

    /**
     * 处理非变量定义声明
     * @return
     */
    private Stmt statement() {
        if (match(PRINT)){
            return printStatement();    // lox里规定print也算一个声明
        }
        if (match(LEFT_BRACE)) {
            return new Stmt.Block(block()); // '{' 开头代表找到了一个代码块
        }

        return expressionStatement();   // 普通表达式类型的声明
    }

    /**
     * 打印类型声明
     * @return
     */
    private Stmt printStatement() {
        Expr value = expression();
        consume(SEMICOLON, "Expect ';' after value.");
        return new Stmt.Print(value);
    }

    /**
     * 处理一个变量定义声明
     * @return
     */
    private Stmt varDeclaration() {
        // var后面必须是一个变量名，IDENTIFIER类型的token
        Token name = consume(IDENTIFIER, "Expect variable name.");
        // 变量可以没有初始表达式
        Expr initializer = null;
        if (match(EQUAL)) {
            initializer = expression(); // 等号后面的属于变量值的表达式
        }
        // 消费最后的分号，分号必须要有
        consume(SEMICOLON, "Expect ';' after variable declaration.");
        return new Stmt.Var(name, initializer);
    }

    /**
     * 普通表达式声明（一个表达式也算一个声明）
     * @return
     */
    private Stmt expressionStatement() {
        Expr expr = expression();
        consume(SEMICOLON, "Expect ';' after expression.");
        return new Stmt.Expression(expr);
    }

    /**
     * 处理代码块
     * @return
     */
    private List<Stmt> block() {
        // 代码块里面有一系列的声明
        List<Stmt> statements = new ArrayList<>();

        // 遇到 '}' 之前一直解析块里的声明
        while (!check(RIGHT_BRACE) && !isAtEnd()) {
            statements.add(declaration());
        }

        consume(RIGHT_BRACE, "Expect '}' after block.");
        return statements;
    }

    /**
     * 从token中识别出表达式
     * @return
     */
    private Expr expression() {
        return assignment();
    }

    private Expr assignment() {
        Expr expr = equality();

        if (match(EQUAL)) {
            Token equals = previous();
            Expr value = assignment();

            if (expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable)expr).name;
                return new Expr.Assign(name, value);
            }

            error(equals, "Invalid assignment target.");
        }

        return expr;
    }



    /**
     * 处理等于、不等于级别的表达式
     * @return
     */
    private Expr equality() {
        Expr expr = comparison(); // 先让更高级的处理

        while (match(BANG_EQUAL, EQUAL_EQUAL)) { // 如果有 = 或者 != ，则需要自己处理
            Token operator = previous(); // 操作符
            Expr right = comparison(); // 操作符后面的表达式，还是交给更高级方法处理
            expr = new Expr.Binary(expr, operator, right); // 定义一个Binary类型的表达式，用自己的操作符操作左右两个子表达式
        }

        return expr;
    }

    /**
     * 比较类型的表达式，如 < <= > >=
     * @return
     */
    private Expr comparison() {
        Expr expr = term();

        while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
            Token operator = previous();
            Expr right = term();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * 加、减类型表达式
     * @return
     */
    private Expr term() {
        Expr expr = factor();

        while (match(MINUS, PLUS)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * 乘除类表达式
     * @return
     */
    private Expr factor() {
        Expr expr = unary();

        while (match(SLASH, STAR)) {
            Token operator = previous();
            Expr right = unary();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * 一元表达式 ，如：-1 、!true
     * @return
     */
    private Expr unary() {
        if (match(BANG, MINUS)) {
            Token operator = previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return primary();
    }

    /**
     * primary类型表达式指不可再解析的字面token，字符串、数字、true、false、nil、括号 等
     * @return
     */
    private Expr primary() {
        if (match(FALSE)){
            return new Expr.Literal(false);
        }
        if (match(TRUE)) {
            return new Expr.Literal(true);
        }
        if (match(NIL)){
            return new Expr.Literal(null);
        }

        if (match(NUMBER, STRING)) {
            return new Expr.Literal(previous().literal);
        }

        if (match(IDENTIFIER)) {
            return new Expr.Variable(previous());
        }

        if (match(LEFT_PAREN)) {
            Expr expr = expression();
            consume(RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }

        throw error(peek(), "Expect expression.");
    }

    /**
     * 判断tokenType是否符合预期
     * @param types 预期类型
     * @return
     */
    private boolean match(TokenType... types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }

        return false;
    }

    /**
     * 判断下一个tokenType是否符合预期
     * @param type
     * @return
     */
    private boolean check(TokenType type) {
        if (isAtEnd()){
            return false;
        }
        return peek().type == type;
    }

    /**
     * tokens当前索引 +1
     * @return
     */
    private Token advance() {
        if (!isAtEnd()){
            current++;
        }
        return previous();
    }

    private boolean isAtEnd() {
        return peek().type == EOF;
    }

    private Token peek() {
        return tokens.get(current);
    }

    private Token previous() {
        return tokens.get(current - 1);
    }

    /**
     * 消费下一个token，如果类型不符，则报错
     * @param type
     * @param message
     * @return
     */
    private Token consume(TokenType type, String message) {
        if (check(type)){
            return advance();
        }
        throw error(peek(), message);
    }

    private ParseError error(Token token, String message) {
        Lox.error(token, message);
        return new ParseError();
    }

    /**
     * parse的过程中如果遇到错误语法，我们希望先跳过，继续执行，最后才把所有语法错误一并向用户抛出，而不是每次只抛一个。
     * 这样就要求前面一个错误的语法尽量不要影响后面正确的语法，因此，在遇到错误的时候，丢弃掉下一个声明语句开始之前的token。
     * 下一个声明的标识有分号和一些关键字，识别这些特殊的token然后做分割即可。
     */
    private void synchronize() {
        advance();

        while (!isAtEnd()) {
            if (previous().type == SEMICOLON){
                return;
            }

            switch (peek().type) {
                case CLASS:
                case FUN:
                case VAR:
                case FOR:
                case IF:
                case WHILE:
                case PRINT:
                case RETURN:
                    return;
            }

            advance();
        }
    }

}
