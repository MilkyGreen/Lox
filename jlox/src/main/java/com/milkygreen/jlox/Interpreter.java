package com.milkygreen.jlox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * 解释器，核心执行类型，负责对lox语言的执行
 * 实现了Visitor，以Visitor模式来为不同的表达式类型提供计算能力。
 */
public class Interpreter implements Expr.Visitor<Object>,
                                    Stmt.Visitor<Void> {

    // 全局上下文
    final Environment globals = new Environment();
    // 当前上下文
    private Environment environment = globals;

    // Resolver通过静态分析，记录的每个变量应该在哪一层的上下文中查找
    private final Map<Expr, Integer> locals = new HashMap<>();


    Interpreter() {
        // 在这里定义 native 函数，放在 globals 作用域里
        globals.define("clock", new LoxCallable() {
            @Override
            public int arity() { return 0; }

            @Override
            public Object call(Interpreter interpreter,
                               List<Object> arguments) {
                return (double)System.currentTimeMillis() / 1000.0;
            }

            @Override
            public String toString() { return "<native fn>"; }
        });
    }


    /**
     * 解释执行
     * @param statements
     */
    void interpret(List<Stmt> statements) {
        try {
            // 逐行执行 statement
            for (Stmt statement : statements) {
                execute(statement);
            }
        } catch (RuntimeError error) {
            Lox.runtimeError(error);
        }
    }

    /**
     * 执行 Stmt
     * @param stmt
     */
    private void execute(Stmt stmt) {
        stmt.accept(this);
    }

    /**
     * 执行 Expr
     * @param expr
     * @return
     */
    private Object evaluate(Expr expr) {
        return expr.accept(this);
    }

    /**
     * 记录变量和定义它的上下文层级
     * @param expr
     * @param depth
     */
    void resolve(Expr expr, int depth) {
        locals.put(expr, depth);
    }

    /**
     * 字符串打印表达式结果
     * @param object
     * @return
     */
    private String stringify(Object object) {
        if (object == null) return "nil";

        if (object instanceof Double) {
            String text = object.toString();
            if (text.endsWith(".0")) {
                text = text.substring(0, text.length() - 2);
            }
            return text;
        }

        return object.toString();
    }

    /**
     * 执行二元表达式，先计算子表达式的值，再根据符号进行操作
     * @param expr
     * @return
     */
    @Override
    public Object visitBinaryExpr(Expr.Binary expr) {
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case GREATER:
                checkNumberOperands(expr.operator, left, right);
                return (double)left > (double)right;
            case GREATER_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double)left >= (double)right;
            case LESS:
                checkNumberOperands(expr.operator, left, right);
                return (double)left < (double)right;
            case LESS_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double)left <= (double)right;
            case MINUS:
                checkNumberOperands(expr.operator, left, right);
                return (double)left - (double)right;
            case PLUS:
                if (left instanceof Double && right instanceof Double) {
                    return (double)left + (double)right;
                }
                if (left instanceof String && right instanceof String) {
                    return (String)left + (String)right;
                }
                throw new RuntimeError(expr.operator,
                        "Operands must be two numbers or two strings.");
            case SLASH:
                checkNumberOperands(expr.operator, left, right);
                return (double)left / (double)right;
            case STAR:
                checkNumberOperands(expr.operator, left, right);
                return (double)left * (double)right;
            case BANG_EQUAL:
                return !isEqual(left, right);
            case EQUAL_EQUAL:
                return isEqual(left, right);
        }

        // Unreachable.
        return null;
    }

    /**
     * 执行函数调用
     * @param expr
     * @return
     */
    @Override
    public Object visitCallExpr(Expr.Call expr) {
        // 函数对象
        Object callee = evaluate(expr.callee);

        // 解析实际入参列表
        List<Object> arguments = new ArrayList<>();
        for (Expr argument : expr.arguments) {
            arguments.add(evaluate(argument)); // 执行每个表达式
        }

        if (!(callee instanceof LoxCallable)) {
            throw new RuntimeError(expr.paren,
                    "Can only call functions and classes.");
        }
        LoxCallable function = (LoxCallable)callee;
        if (arguments.size() != function.arity()) {
            throw new RuntimeError(expr.paren, "Expected " +
                    function.arity() + " arguments but got " +
                    arguments.size() + ".");
        }
        return function.call(this, arguments);
    }

    /**
     * 检查两个操作元是否是数字
     * @param operator
     * @param left
     * @param right
     */
    private void checkNumberOperands(Token operator,
                                     Object left, Object right) {
        if (left instanceof Double && right instanceof Double){
            return;
        }
        throw new RuntimeError(operator, "Operands must be numbers.");
    }

    /**
     * 检查operand是否是数字
     * @param operator
     * @param operand
     */
    private void checkNumberOperand(Token operator, Object operand) {
        if (operand instanceof Double){
            return;
        }
        throw new RuntimeError(operator, "Operand must be a number.");
    }

    /**
     * 判断是否相等，基于Java的 equals() 方法
     * @param a
     * @param b
     * @return
     */
    private boolean isEqual(Object a, Object b) {
        if (a == null && b == null){
            return true;
        }
        if (a == null){
            return false;
        }
        return a.equals(b);
    }

    /**
     * 括号表达式，计算括号内部的表达式即可
     * @param expr
     * @return
     */
    @Override
    public Object visitGroupingExpr(Expr.Grouping expr) {
        return evaluate(expr.expression);
    }

    /**
     * 字面值直接返回
     * @param expr
     * @return
     */
    @Override
    public Object visitLiteralExpr(Expr.Literal expr) {
        return expr.value;
    }

    /**
     * 执行逻辑表达式
     * 结构类似二元操作，不过使用了单独的类Logical
     * @param expr
     * @return
     */
    @Override
    public Object visitLogicalExpr(Expr.Logical expr) {
        Object left = evaluate(expr.left); // 先执行左边

        if (expr.operator.type == TokenType.OR) {
            // or 操作
            if (isTruthy(left)){
                return left;
            }else{
                return evaluate(expr.right);
            }
        } else {
            // and 操作
            if (!isTruthy(left)){
                return left;
            }else{
                return evaluate(expr.right);
            }
        }
    }

    /**
     * 一元表达式，先计算右边，再根据符号操作
     * @param expr
     * @return
     */
    @Override
    public Object visitUnaryExpr(Expr.Unary expr) {
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case BANG:
                return !isTruthy(right);
            case MINUS:
                checkNumberOperand(expr.operator, right);
                return -(double)right;
        }

        // Unreachable.
        return null;
    }

    /**
     * 执行变量
     * @param expr
     * @return
     */
    @Override
    public Object visitVariableExpr(Expr.Variable expr) {

        return lookUpVariable(expr.name, expr);

//        return this.environment.get(expr.name); // 将变量的值从上下文中取出
    }

    /**
     * 查找变量的值
     * @param name
     * @param expr
     * @return
     */
    private Object lookUpVariable(Token name, Expr expr) {
        // 先取层级
        Integer distance = locals.get(expr);
        if (distance != null) {
            return environment.getAt(distance, name.lexeme);
        } else {
            // 没有层级的是全局变量
            return globals.get(name);
        }
    }

    /**
     * 执行变量重置
     * @param expr
     * @return
     */
    @Override
    public Object visitAssignExpr(Expr.Assign expr) {
        Object value = evaluate(expr.value); // 先执行右边的表达式
//        environment.assign(expr.name, value); // 把新值放到当前环境中

        Integer distance = locals.get(expr);
        if (distance != null) {
            environment.assignAt(distance, expr.name, value);
        } else {
            globals.assign(expr.name, value);
        }

        return value;
    }

    /**
     * true false的判断
     * null或者false的时候返回false,其他情况为true
     * @param object
     * @return
     */
    private boolean isTruthy(Object object) {
        if (object == null){
            return false;
        }
        if (object instanceof Boolean){
            return (boolean)object;
        }
        return true;
    }



    /**
     * 执行代码块
     * @param statements 声明列表
     * @param environment 代码块对应的上下文环境
     */
    void executeBlock(List<Stmt> statements,
                      Environment environment) {
        // 代码块有可能是嵌套很多层的，当前的 this.environment 是上一层的，先记录一下，等执行完之后需要还原，类似回溯。
        Environment previous = this.environment;
        try {
            this.environment = environment; // this.environment设置成自己的

            for (Stmt statement : statements) { // 逐行执行语句
                execute(statement);
            }
        } finally {
            this.environment = previous; // 还原执行前的 environment
        }
    }

    /**
     * 执行代码块类型Stmt
     * @param stmt
     * @return
     */
    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        executeBlock(stmt.statements, new Environment(environment));
        return null;
    }

    /**
     * 执行Expression类型的Stmt
     * @param stmt
     * @return
     */
    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        evaluate(stmt.expression); // 直接执行里面的表达式
        return null;
    }

    /**
     * 执行函数定义语句
     * @param stmt
     * @return
     */
    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        // 创建函数对象，放到上下文环境中
        LoxFunction function = new LoxFunction(stmt, environment);
        environment.define(stmt.name.lexeme, function);
        return null;
    }

    /**
     * 执行if语句
     * @param stmt
     * @return
     */
    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        if (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.thenBranch);
        } else if (stmt.elseBranch != null) {
            execute(stmt.elseBranch);
        }
        return null;
    }

    /**
     * 执行打印类型的stmt
     * @param stmt
     * @return
     */
    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
    }

    /**
     * 执行return语句
     * @param stmt
     * @return
     */
    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        Object value = null;
        if (stmt.value != null){
            value = evaluate(stmt.value);
        }
        throw new Return(value);
    }

    /**
     * 执行变量定义Stmt
     * @param stmt
     * @return
     */
    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        Object value = null;
        if (stmt.initializer != null) {
            value = evaluate(stmt.initializer); // 执行initializer的表达式，或者值
        }

        environment.define(stmt.name.lexeme, value); // 将变量和值放到上下文中
        return null;
    }

    /**
     * 执行 while循环
     * @param stmt
     * @return
     */
    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        // 就用java的while循环执行
        while (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.body);
        }
        return null;
    }
}
