package com.milkygreen.jlox;

import java.util.List;

/**
 * 函数（不同与类里的方法）
 */
public class LoxFunction implements LoxCallable {

    // 函数的声明
    private final Stmt.Function declaration;

    /**
     * 闭包上下文
     * lox是支持闭包的，也就是说一个函数可以定义在另一个函数里面，且执行的时候能方法父级函数内的变量。
     * 因此在解释执行一个函数的定义的时候，需要记住当前的上下文环境。
     * 当函数被call的时候，以闭包上下文为父级上下文，以便能访问定义时的上下变量。
     */
    private final Environment closure;

    LoxFunction(Stmt.Function declaration, Environment closure) {
        this.closure = closure;
        this.declaration = declaration;
    }

    @Override
    public Object call(Interpreter interpreter,
                       List<Object> arguments) {
        // 以闭包上下文作为父级上下文
        Environment environment = new Environment(closure);

        // 将声明参数和调用参数绑定，放到上下文中
        for (int i = 0; i < declaration.params.size(); i++) {
            environment.define(declaration.params.get(i).lexeme, arguments.get(i));
        }

        try {
            // 方法体当做代码块来执行
            interpreter.executeBlock(declaration.body, environment);
        } catch (Return returnValue) {
            // 对于函数的return，按照一个异常来处理，这样可以很方便的跳过后面的代码直接返回
            return returnValue.value;
        }
        return null;
    }

    @Override
    public int arity() {
        return declaration.params.size();
    }

    @Override
    public String toString() {
        return "<fn " + declaration.name.lexeme + ">";
    }

}
