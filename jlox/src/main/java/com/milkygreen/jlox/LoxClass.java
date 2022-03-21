package com.milkygreen.jlox;

import java.util.List;
import java.util.Map;

/**
 * lox里面的类。
 * lox里新建类实例的方法是 类名() ,所以实现了LoxCallable接口
 * lox类的字段是动态的，可以在运行时任意增加，所以这里只有固定的方法集合，字段集合在LoxInstance里面定义
 */
public class LoxClass implements LoxCallable{

    // 类名
    final String name;
    // 类的方法集合
    private final Map<String, LoxFunction> methods;
    // 父类
    final LoxClass superclass;

    LoxClass(String name, LoxClass superclass,
             Map<String, LoxFunction> methods) {
        this.superclass = superclass;
        this.name = name;
        this.methods = methods;
    }

    @Override
    public String toString() {
        return name;
    }

    /**
     * 一个类被call时，就是要实例化对象
     * @param interpreter 解释器实例
     * @param arguments 入参
     * @return
     */
    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        LoxInstance instance = new LoxInstance(this);   // 新建一个实例
        LoxFunction initializer = findMethod("init");   // 查找有没有init方法
        if (initializer != null) {
            // 先把this关键字指向上面的新实例，然后调用init方法
            initializer.bind(instance).call(interpreter, arguments);
        }
        return instance;    // 返回新实例
    }

    @Override
    public int arity() {
        LoxFunction initializer = findMethod("init");
        if (initializer == null) {
            return 0;
        }
        return initializer.arity(); // 如果有init方法，返回其参数数量
    }

    /**
     * 查询类的方法
     * @param name 方法名
     * @return
     */
    LoxFunction findMethod(String name) {
        if (methods.containsKey(name)) {
            return methods.get(name);
        }

        if (superclass != null) {
            return superclass.findMethod(name);
        }

        return null;
    }
}
