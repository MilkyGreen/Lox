package com.milkygreen.jlox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * 解释器，负责对lox语言的执行
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
     * getter表达式
     * @param expr
     * @return
     */
    @Override
    public Object visitGetExpr(Expr.Get expr) {
        // getter的主体必须是一个类实例
        Object object = evaluate(expr.object);
        if (object instanceof LoxInstance) {
            // 从实例中查找出字段的值
            return ((LoxInstance) object).get(expr.name);
        }

        throw new RuntimeError(expr.name,
                "Only instances have properties.");
    }

    /**
     * setter表达式
     * @param expr
     * @return
     */
    @Override
    public Object visitSetExpr(Expr.Set expr) {
        Object object = evaluate(expr.object);
        // getter的主体必须是一个类实例
        if (!(object instanceof LoxInstance)) {
            throw new RuntimeError(expr.name,
                    "Only instances have fields.");
        }

        Object value = evaluate(expr.value);
        ((LoxInstance)object).set(expr.name, value);
        return value;
    }

    /**
     * super关键字
     * @param expr
     * @return
     */
    @Override
    public Object visitSuperExpr(Expr.Super expr) {
        int distance = locals.get(expr);    // super当做变量来解析，查找目标和当前上下文的距离
        LoxClass superclass = (LoxClass)environment.getAt(distance, "super");   // 找出父类
        // 在super的下一层可以找到子类实例。
        // 因为super表达式一定处于一个方法的调用过程中，而每个方法的在调用之前都是绑定了自己的this的。
        // 且super所在的上下文位于this上下文上一层的位置。具体可见类声明方法。
        LoxInstance object = (LoxInstance)environment.getAt(distance - 1, "this");
        // 找到父类的方法
        LoxFunction method = superclass.findMethod(expr.method.lexeme);
        if (method == null) {
            throw new RuntimeError(expr.method, "Undefined property '" + expr.method.lexeme + "'.");
        }
        // 父类的方法绑定this为当前的实例
        return method.bind(object);
    }

    /**
     * this表达式
     * @param expr
     * @return
     */
    @Override
    public Object visitThisExpr(Expr.This expr) {
        // this已经放入的上下文中，直接从里面查找
        return lookUpVariable(expr.keyword, expr);
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
     * 执行类声明
     * @param stmt
     * @return
     */
    @Override
    public Void visitClassStmt(Stmt.Class stmt) {
        Object superclass = null;
        if (stmt.superclass != null) {
            // 父类必须是一个class
            superclass = evaluate(stmt.superclass);
            if (!(superclass instanceof LoxClass)) {
                throw new RuntimeError(stmt.superclass.name,"Superclass must be a class.");
            }
        }

        // 将类声明放入上下文，此时值是null，代表还没解析完
        environment.define(stmt.name.lexeme, null);

        // 如果有父类，需要把super关键字放入上下文中。这样每个方法的上下文路径中都会有super
        if (stmt.superclass != null) {
            environment = new Environment(environment);
            environment.define("super", superclass);
        }

        // 依次解析方法列表，需要区分是否是init
        Map<String, LoxFunction> methods = new HashMap<>();
        for (Stmt.Function method : stmt.methods) {
            LoxFunction function = new LoxFunction(method, environment, method.name.lexeme.equals("init"));
            methods.put(method.name.lexeme, function);
        }
        // 新建class对象
        LoxClass klass = new LoxClass(stmt.name.lexeme, (LoxClass)superclass, methods);

        if (superclass != null) {
            // 还原上下文。因为只有类里面才能用这个super，外面的不能用
            environment = environment.enclosing;
        }

        // 更新上下文
        environment.assign(stmt.name, klass);
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
        LoxFunction function = new LoxFunction(stmt, environment,false);
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
