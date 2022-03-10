package com.milkygreen.jlox;

import java.util.List;

/**
 * 解释器，核心执行类型，负责对lox语言的执行
 * 实现了Visitor，以Visitor模式来为不同的表达式类型提供计算能力。
 */
public class Interpreter implements Expr.Visitor<Object>,
                                    Stmt.Visitor<Void> {

    // 全局上下文
    private Environment environment = new Environment();

    /**
     * 执行传入的表达式
     * @param statements
     */
    void interpret(List<Stmt> statements) {
        try {
            // 逐行执行声明
            for (Stmt statement : statements) {
                execute(statement);
            }
        } catch (RuntimeError error) {
            Lox.runtimeError(error);
        }
    }

    /**
     * 通过 visitor 模式执行不通Stmt对应的操作
     * @param stmt
     */
    private void execute(Stmt stmt) {
        stmt.accept(this);
    }

    /**
     * 对指定表达式执行计算
     * @param expr
     * @return
     */
    private Object evaluate(Expr expr) {
        return expr.accept(this);
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
     * 二元表达式，先计算子表达式的值，再根据符号进行操作
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
        return this.environment.get(expr.name); // 将表达式的值从上下文中取出
    }

    /**
     * 执行变量重置
     * @param expr
     * @return
     */
    @Override
    public Object visitAssignExpr(Expr.Assign expr) {
        Object value = evaluate(expr.value); // 先执行右边的表达式
        environment.assign(expr.name, value); // 把新值放到当前环境中
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
}
