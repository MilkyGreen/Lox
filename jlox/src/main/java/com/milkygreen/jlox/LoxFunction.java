package com.milkygreen.jlox;

import java.util.List;

/**
 * 函数
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

    /**
     * 是否是一个类的构造方法(init)
     */
    private final boolean isInitializer;

    LoxFunction(Stmt.Function declaration, Environment closure,
                boolean isInitializer) {
        this.isInitializer = isInitializer;
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
            // 初始化方法中也允许return，不过只会return类的实例
            if (isInitializer){
                return closure.getAt(0, "this");
            }

            // 对于函数的return，按照一个异常来处理，这样可以很方便的跳过后面的代码直接返回
            return returnValue.value;
        }
        // 如果是构造方法需要return 新建的类实例
        if (isInitializer){
            return closure.getAt(0, "this");
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

    /**
     * 函数绑定到一个类实例上，则该函数就是这个实例的方法了
     * @param instance 类实例
     * @return
     */
    LoxFunction bind(LoxInstance instance) {
        // 创建一个新的上下文，增加一个this "变量"，指向类实例
        Environment environment = new Environment(closure);
        environment.define("this", instance);
        // 返回一个包装过的新函数，可以调用this变量
        return new LoxFunction(declaration, environment,isInitializer);
    }

}
