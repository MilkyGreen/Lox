package com.milkygreen.jlox;

import java.util.ArrayList;
import java.util.Arrays;
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
            // 一次解析一个statement
            statements.add(declaration());
        }

        return statements;
    }

    /**
     * 解析声明
     * 分为变量声明、函数声明和其他声明
     *
     * @return
     */
    private Stmt declaration() {
        try {
            if (match(CLASS)){
                // 类声明
                return classDeclaration();
            }
            if (match(FUN)) {
                // 函数声明
                return function("function");
            }
            if (match(VAR)){
                // 变量声明
                return varDeclaration();
            }
            // 其他声明
            return statement();
        } catch (ParseError error) {
            // 如果编译报错，需要跳过当前语句，记录一下错误然后继续编译，尽量一次性抛出更多的编译错误
            synchronize();
            return null;
        }
    }

    /**
     * 类声明
     * @return
     */
    private Stmt classDeclaration() {
        Token name = consume(IDENTIFIER, "Expect class name.");

        Expr.Variable superclass = null;
        if (match(LESS)) {  // 有父类
            consume(IDENTIFIER, "Expect superclass name.");
            superclass = new Expr.Variable(previous());
        }

        consume(LEFT_BRACE, "Expect '{' before class body.");

        // 类里面定义的是一个个方法，解析成方法列表
        List<Stmt.Function> methods = new ArrayList<>();
        while (!check(RIGHT_BRACE) && !isAtEnd()) {
            methods.add(function("method"));
        }

        consume(RIGHT_BRACE, "Expect '}' after class body.");

        return new Stmt.Class(name, superclass, methods);
    }

    /**
     * 函数声明
     * @param kind 函数的类型，有函数和方法
     * @return
     */
    private Stmt.Function function(String kind) {
        // 函数名
        Token name = consume(IDENTIFIER, "Expect " + kind + " name.");
        // 左括号
        consume(LEFT_PAREN, "Expect '(' after " + kind + " name.");
        // 参数定义
        List<Token> parameters = new ArrayList<>();
        if (!check(RIGHT_PAREN)) {
            do {
                if (parameters.size() >= 255) {
                    error(peek(), "Can't have more than 255 parameters.");
                }
                parameters.add(consume(IDENTIFIER, "Expect parameter name."));
            } while (match(COMMA));
        }
        consume(RIGHT_PAREN, "Expect ')' after parameters.");

        // 方法体类似一个block
        consume(LEFT_BRACE, "Expect '{' before " + kind + " body.");
        List<Stmt> body = block();
        return new Stmt.Function(name, parameters, body);
    }

    /**
     * 处理其他声明
     * @return
     */
    private Stmt statement() {
        if (match(FOR)) {
            // for循环
            return forStatement();
        }
        if (match(IF)) {
            // if表达式
            return ifStatement();
        }
        if (match(PRINT)){
            return printStatement();    // lox里规定print也算一个声明
        }
        if (match(RETURN)){
            return returnStatement();
        }
        if (match(BREAK)){
            return breakStatement();
        }
        if (match(WHILE)) {
            // while循环
            return whileStatement();
        }
        if (match(LEFT_BRACE)) {
            return new Stmt.Block(block()); // '{' 开头代表找到了一个代码块
        }

        return expressionStatement();   // 普通表达式
    }

    /**
     * 返回值声明
     * return后面跟一个表达式
     * @return
     */
    private Stmt returnStatement() {
        Token keyword = previous();
        Expr value = null;
        if (!check(SEMICOLON)) {
            // return 后面可以跟一个表达式值，也可以直接跟分号，表达式返回空
            value = expression();
        }

        consume(SEMICOLON, "Expect ';' after return value.");
        return new Stmt.Return(keyword, value);
    }

    private Stmt breakStatement() {
        Token keyword = previous();
        consume(SEMICOLON, "Expect ';' after break value.");
        return new Stmt.Break(keyword);
    }

    /**
     * for循环
     * 所有的for循环都可以用while循环来表示，因此把for当成while的语法糖，遇到for最后转换成while循环来处理。
     * @return
     */
    private Stmt forStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'for'.");

        Stmt initializer;   // 初始化
        if (match(SEMICOLON)) {
            initializer = null; // 没有初始化
        } else if (match(VAR)) {
            initializer = varDeclaration(); // 定义了一个初始化变量
        } else {
            initializer = expressionStatement(); // 没有定义新的，执行一个表达式
        }

        Expr condition = null; // 循环条件
        if (!check(SEMICOLON)) {
            condition = expression(); // 有循环条件
        }
        consume(SEMICOLON, "Expect ';' after loop condition.");

        Expr increment = null; // 递增条件
        if (!check(RIGHT_PAREN)) {
            increment = expression(); // 递增条件也是个表达式
        }
        consume(RIGHT_PAREN, "Expect ')' after for clauses.");

        Stmt body = statement(); // for循环的代码

        // 递增每次都在循环体之后执行，因此把循环体和递增放到一个block里面，每次都会一次执行。
        if (increment != null) {
            body = new Stmt.Block(
                    Arrays.asList(
                            body,
                            new Stmt.Expression(increment)));
        }

        // 如果没有条件表达式，则是true，一直循环
        if (condition == null) {
            condition = new Expr.Literal(true);
        }
        // 构造while循环
        body = new Stmt.While(condition, body);

        // 如果有初始化表达式，会放在while之前组成一个新的代码块，这样会在while之前执行一次，而且新增的变量对循环体里可见
        if (initializer != null) {
            body = new Stmt.Block(Arrays.asList(initializer, body));
        }
        // 最后for循环就变成了一个包含了while的代码块
        return body;
    }

    /**
     * while循环
     * @return
     */
    private Stmt whileStatement() {
        // 后面是一对括号，里面是条件表达式
        consume(LEFT_PAREN, "Expect '(' after 'while'.");
        Expr condition = expression();      // while括号里面的条件表达式
        consume(RIGHT_PAREN, "Expect ')' after condition.");
        // 循环代码体
        Stmt body = statement();

        return new Stmt.While(condition, body);
    }


    /**
     * if else 声明
     * @return
     */
    private Stmt ifStatement() {
        // 先跟一对括号，里面是条件表达式
        consume(LEFT_PAREN, "Expect '(' after 'if'.");
        Expr condition = expression();
        consume(RIGHT_PAREN, "Expect ')' after if condition.");
        // if如果为true，执行thenBranch
        Stmt thenBranch = statement();
        Stmt elseBranch = null;
        if (match(ELSE)) {
            // 如果有else，解析else语句
            elseBranch = statement();
        }

        return new Stmt.If(condition, thenBranch, elseBranch);
    }

    /**
     * 打印类型声明
     * @return
     */
    private Stmt printStatement() {
        // 要返回的值，是一个表达式
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
        // 从优先级最低的赋值开始解析
        return assignment();
    }

    /**
     * 赋值操作
     * @return
     */
    private Expr assignment() {
        // 先用更高级的OR解析左半边
        Expr expr = or();

        if (match(EQUAL)) {
            // 如果存在 = 说明是一个赋值表达式，继续解析右边

            Token equals = previous();

            // 等号的右边，也是个表达式，用同优先级的assignment解析出来
            Expr value = assignment();

            if (expr instanceof Expr.Variable) {
                // 左边如果是一个变量，是变量赋值
                Token name = ((Expr.Variable)expr).name;
                return new Expr.Assign(name, value);
            } else if (expr instanceof Expr.Get) {
                // 左边如果是一个字段的get，则是对象字段的赋值，（set方法其实是左边先出现get，后面跟 = value）
                Expr.Get get = (Expr.Get)expr;
                return new Expr.Set(get.object, get.name, value);
            }

            error(equals, "Invalid assignment target.");
        }else if(match(PLUS_PLUS)){
            // 右边跟的 ++, 是一个 +1操作
            Token line = previous();
            if (expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable)expr).name;
                return new Expr.Assign(name, new Expr.Binary(expr,new Token(PLUS,"+",null,line.line),new Expr.Literal(1d)));
            }else{
                error(line, "Invalid ++ target.");
            }
        }else if(match(MINUS_MINUS)){
            // 左边跟的 -- ，是一个-1操作
            Token line = previous();
            if (expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable)expr).name;
                return new Expr.Assign(name, new Expr.Binary(expr,new Token(MINUS,"-",null,line.line),new Expr.Literal(1d)));
            }else{
                error(line, "Invalid ++ target.");
            }
        }

        return expr;
    }

    /**
     * 逻辑操作 ||
     * @return
     */
    private Expr or() {
        // 先解析左边的 and
        Expr expr = and();

        while (match(OR)) {
            // 如果中间是 and，继续解析后面
            Token operator = previous();
            // 右边也用and方法解析
            Expr right = and();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
    }

    /**
     * 逻辑操作 &&
     * @return
     */
    private Expr and() {
        // 先解析左边的 equality，即 == 表达式
        Expr expr = equality();

        while (match(AND)) {
            // 如果中间有and，继续解析右边的 equality
            Token operator = previous();
            Expr right = equality();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
    }


    /**
     * 处理等于、不等于级别的表达式
     * @return
     */
    private Expr equality() {
        Expr expr = comparison(); // 先让更高级大小比较表达式

        while (match(BANG_EQUAL, EQUAL_EQUAL)) {
            // 如果中间有 = 或者 != ，继续解析右边的 comparison 表达式
            Token operator = previous(); // 操作符
            Expr right = comparison();
            expr = new Expr.Binary(expr, operator, right); // 定义一个Binary类型的表达式，用自己的操作符操作左右两个子表达式
        }

        return expr;
    }

    /**
     * 比较类型的表达式，如 < <= > >=
     * @return
     */
    private Expr comparison() {
        // 先解析左边的加减表达式
        Expr expr = term();

        while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
            // 如果有比较符号，继续解析右边的表达式
            Token operator = previous();
            Expr right = term();
            // 组成一个二元表达式
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * 加、减类型表达式
     * @return
     */
    private Expr term() {
        // 先解析左边的 乘除 表达式（优先级更高）
        Expr expr = factor();

        while (match(MINUS, PLUS)) {
            // 如果中间有 + 或 - ，继续解析后面
            Token operator = previous();
            Expr right = factor();
            // 组成一个二元表达式
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * 乘除类表达式
     * @return
     */
    private Expr factor() {
        // 解析左边一元表达式
        Expr expr = unary();

        while (match(SLASH, STAR)) {
            // 如果中间有 * 或者 / ，继续解析右边的一元表达式
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
        // 一元表达式由 - 或者 ! 开头
        if (match(BANG, MINUS)) {
            Token operator = previous();
            // 后面还是可能跟一个一元表达式，比如 --1，就是 1 ，这个是允许的
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }
        // 如果不是 - ！ 开头，说明不是一元表达式，则按高一级函数调用的表达式去解析
        return call();
    }

    /**
     * 函数调用
     * @return
     */
    private Expr call() {
        // 解析可能的函数名称
        Expr expr = primary();

        while (true) {
            if (match(LEFT_PAREN)) {
                // 如果后面紧跟一个左括号，则是一个函数调用表达式
                expr = finishCall(expr);
            } else if (match(DOT)) {
                // 出现点，代表是一个get表达式，如 person.name，或者 Person().son.name,前面可以出现多个方法调用和get
                // 最后一个点的后面跟的IDENTIFIER才是要赋值的字段
                Token name = consume(IDENTIFIER,
                        "Expect property name after '.'.");
                expr = new Expr.Get(expr, name);
            } else {
                break;
            }
        }

        return expr;
    }

    /**
     * 解析函数的调用表达式
     * @param callee 被调用的函数
     * @return
     */
    private Expr finishCall(Expr callee) {
        // 解析实际入参列表
        List<Expr> arguments = new ArrayList<>();
        if (!check(RIGHT_PAREN)) {
            do {
                if (arguments.size() >= 255) {
                    error(peek(), "Can't have more than 255 arguments.");
                }
                arguments.add(expression()); // 每个入参都是一个表达式
            } while (match(COMMA));
        }

        Token paren = consume(RIGHT_PAREN,
                "Expect ')' after arguments.");

        return new Expr.Call(callee, paren, arguments);
    }

    /**
     * primary类型表达式指不可再拆分解析的字面token，字符串、数字、true、false、nil、括号 等
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

        if (match(SUPER)) {
            // super关键字，它后面只能出现方法调用
            Token keyword = previous();
            consume(DOT, "Expect '.' after 'super'.");
            // 解析方法名
            Token method = consume(IDENTIFIER, "Expect superclass method name.");
            // 返回一个父类方法调用表达式
            return new Expr.Super(keyword, method);
        }

        if (match(THIS)){
            // this关键字表达式
            return new Expr.This(previous());
        }

        if (match(IDENTIFIER)) {
            return new Expr.Variable(previous());
        }

        if (match(LEFT_PAREN)) {
            // 括号，代表一个处在括号内的表达式，把里面的表达式解析一下，去掉两个括号
            // 括号本身代表了一个最高优先级，因此里面的表达式会被优先解析
            Expr expr = expression();
            consume(RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }

        throw error(peek(), "Expect expression.");
    }

    /**
     * 判断tokenType是否符合预期，如果符合，消费掉这个token
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
