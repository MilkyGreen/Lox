package com.milkygreen.jlox;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

/**
 * 静态分析器。在parser之后，运行之前，对AST进行一个静态的分析。目前实现了对变量的分析，即使用到一个变量的时候，确定
 * 它是在哪里定义的。
 *
 * 主要做的事情是：标记变量或函数的定义，在使用的时候，从内向外遍历当前scope链，确定在第几层能找到该变量的定义，interpreter
 * 记录下层数。在执行的时候就按记录的层数去找。
 */
public class Resolver implements Expr.Visitor<Void>,Stmt.Visitor<Void>{

    private final Interpreter interpreter;

    // 作用域，每进入一个块就会push一个作用域，出来块之后pop掉。目的是提前确定使用一个变量的时候，应该向上找几层
    private final Stack<Map<String, Boolean>> scopes = new Stack<>();
    // 标记当前是否处于一个函数中，仅对return起作用
    private FunctionType currentFunction = FunctionType.NONE;

    private enum FunctionType {
        NONE,
        FUNCTION,
        INITIALIZER,
        METHOD
    }

    // 标记当前是否处于一个类中
    private ClassType currentClass = ClassType.NONE;

    private enum ClassType {
        NONE,
        CLASS,
        SUBCLASS
    }

    private LoopType currentLoop = LoopType.NONE;

    private enum LoopType {
        NONE,
        LOOP
    }

    Resolver(Interpreter interpreter) {
        this.interpreter = interpreter;
    }

    @Override
    public Void visitBinaryExpr(Expr.Binary expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitCallExpr(Expr.Call expr) {
        resolve(expr.callee);
        for (Expr argument : expr.arguments) {
            resolve(argument);
        }
        return null;
    }

    @Override
    public Void visitGetExpr(Expr.Get expr) {
        resolve(expr.object);
        return null;
    }

    @Override
    public Void visitSetExpr(Expr.Set expr) {
        resolve(expr.value);
        resolve(expr.object);
        return null;
    }

    @Override
    public Void visitSuperExpr(Expr.Super expr) {
        if (currentClass == ClassType.NONE) {
            Lox.error(expr.keyword,"Can't use 'super' outside of a class.");
        } else if (currentClass != ClassType.SUBCLASS) {
            Lox.error(expr.keyword,"Can't use 'super' in a class with no superclass.");
        }

        resolveLocal(expr, expr.keyword);
        return null;
    }

    @Override
    public Void visitThisExpr(Expr.This expr) {
        // this只能在类中出现
        if (currentClass == ClassType.NONE) {
            Lox.error(expr.keyword,
                    "Can't use 'this' outside of a class.");
            return null;
        }
        resolveLocal(expr, expr.keyword);
        return null;
    }

    @Override
    public Void visitGroupingExpr(Expr.Grouping expr) {
        resolve(expr.expression);
        return null;
    }

    @Override
    public Void visitLiteralExpr(Expr.Literal expr) {
        return null;
    }

    @Override
    public Void visitLogicalExpr(Expr.Logical expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary expr) {
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitVariableExpr(Expr.Variable expr) {
        // 如果已经声明同名变量但是还没初始化，说明当前处于该变量的初始化代码中，一个变量不能在初始化代码中引用自己
        if (!scopes.isEmpty() && scopes.peek().get(expr.name.lexeme) == Boolean.FALSE) {
            Lox.error(expr.name, "Can't read local variable in its own initializer.");
        }

        resolveLocal(expr, expr.name);
        return null;
    }

    /**
     * 从scope链中找变量声明
     * @param expr
     * @param name
     */
    private void resolveLocal(Expr expr, Token name) {
        // 依次从近到远遍历stack，找到变量为止
        for (int i = scopes.size() - 1; i >= 0; i--) {
            if (scopes.get(i).containsKey(name.lexeme)) {
                // 交给interpreter记录一下层数
                interpreter.resolve(expr, scopes.size() - 1 - i);
                return;
            }
        }
    }

    @Override
    public Void visitAssignExpr(Expr.Assign expr) {
        // 变量的Assign，先看看value里面有没有用到变量
        resolve(expr.value);
        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        // 代码块先开启一个新scope，逐行分析里面代码，最后退出新scope
        beginScope();
        resolve(stmt.statements);
        endScope();
        return null;
    }

    @Override
    public Void visitClassStmt(Stmt.Class stmt) {
        // 首先表明当前处于一个类中
        ClassType enclosingClass = currentClass;
        currentClass = ClassType.CLASS;

        declare(stmt.name);
        define(stmt.name);

        // 不能继承自己
        if (stmt.superclass != null &&
                stmt.name.lexeme.equals(stmt.superclass.name.lexeme)) {
            Lox.error(stmt.superclass.name,
                    "A class can't inherit from itself.");
        }

        if (stmt.superclass != null) {
            currentClass = ClassType.SUBCLASS;
            resolve(stmt.superclass);
        }

        // 如果有父类，增加super关键字
        if (stmt.superclass != null) {
            beginScope();
            scopes.peek().put("super", true);
        }

        // 类里面属于一个新scope
        beginScope();
        // 把this关键字放进去
        scopes.peek().put("this", true);

        // 解析类里的方法
        for (Stmt.Function method : stmt.methods) {
            FunctionType declaration = FunctionType.METHOD; // 普通的方法是METHOD
            if (method.name.lexeme.equals("init")) {
                // init方法是INITIALIZER
                declaration = FunctionType.INITIALIZER;
            }
            resolveFunction(method, declaration);
        }
        endScope();
        // 父类增加了一层scope，最后关掉
        if (stmt.superclass != null){
            endScope();
        }
        currentClass = enclosingClass;
        return null;
    }

    private void beginScope() {
        scopes.push(new HashMap<String, Boolean>());
    }

    private void endScope() {
        scopes.pop();
    }

    void resolve(List<Stmt> statements) {
        for (Stmt statement : statements) {
            resolve(statement);
        }
    }

    private void resolve(Stmt stmt) {
        stmt.accept(this);
    }

    private void resolve(Expr expr) {
        expr.accept(this);
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        // 声明、定义在前面，再处理方法体。在递归的情况下，一个函数是可以调用自己的
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt,FunctionType.FUNCTION);
        return null;
    }

    /**
     * 处理函数体
     * @param function
     */
    private void resolveFunction(Stmt.Function function, FunctionType type) {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = type; // 表明当前处于一个函数中

        // 开启新的scope
        beginScope();
        // 入参相当于函数中的变量，需要声明和定义
        for (Token param : function.params) {
            declare(param);
            define(param);
        }
        resolve(function.body);
        endScope();
        // 还原函数标记
        currentFunction = enclosingFunction;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        // 条件语句需要走全部分支，这里只是静态分析不是runtime
        resolve(stmt.condition);
        resolve(stmt.thenBranch);
        if (stmt.elseBranch != null){
            resolve(stmt.elseBranch);
        }
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        // 如果return不在一个函数中，则是非法的
        if (currentFunction == FunctionType.NONE) {
            Lox.error(stmt.keyword, "Can't return from top-level code.");
        }

        if (stmt.value != null) {
            // 如果return值不等于空且在INITIALIZER方法中，是非法的
            if (currentFunction == FunctionType.INITIALIZER) {
                Lox.error(stmt.keyword,
                        "Can't return a value from an initializer.");
            }
            resolve(stmt.value);
        }
        return null;
    }

    @Override
    public Void visitBreakStmt(Stmt.Break stmt) {
        if(currentLoop != LoopType.LOOP){
            Lox.error(stmt.keyword,
                    "Can't break a loop outside a loop.");
        }
        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        // 变量声明
        declare(stmt.name);
        if (stmt.initializer != null) {
            // 处理声明的表达式，如果里面使用了自己，是会报错的
            resolve(stmt.initializer);
        }
        define(stmt.name);
        return null;
    }

    /**
     * 声明
     * @param name
     */
    private void declare(Token name) {
        if (scopes.isEmpty()){
            return;
        }

        Map<String, Boolean> scope = scopes.peek();
        // 同一个scope不能重复声明相同变量
        if (scope.containsKey(name.lexeme)) {
            Lox.error(name, "Already a variable with this name in this scope.");
        }
        // false代表仅仅做了声明，还没初始化
        scope.put(name.lexeme, false);
    }

    /**
     * 定义
     * @param name
     */
    private void define(Token name) {
        if (scopes.isEmpty()){
            return;
        }
        // true 表明变量已经可以供别人使用了
        scopes.peek().put(name.lexeme, true);
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        resolve(stmt.condition);
        LoopType enclosingFunction = currentLoop; // 进入了loop里面
        currentLoop = LoopType.LOOP;
        resolve(stmt.body);
        currentLoop = enclosingFunction;
        return null;
    }
}
