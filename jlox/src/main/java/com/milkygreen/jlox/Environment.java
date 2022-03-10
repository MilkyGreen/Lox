package com.milkygreen.jlox;

import java.util.HashMap;
import java.util.Map;

/**
 * 上下文环境，主要用来保存一个作用域中的变量
 */
class Environment {

    /**
     * 父作用域的环境
     */
    final Environment enclosing;

    /**
     * 该作用域里面定义的变量和值
     */
    private final Map<String, Object> values = new HashMap<>();

    Environment() {
        enclosing = null;
    }

    Environment(Environment enclosing) {
        this.enclosing = enclosing;
    }

    /**
     * 定义一个变量
     * @param name 变量名
     * @param value 变量值
     */
    void define(String name, Object value) {
        values.put(name, value);
    }

    /**
     * 查询一个变量的值
     * @param name 变量名
     * @return 值
     */
    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            return values.get(name.lexeme);
        }

        // 如果在该作用域没找到，向父作用域查找
        if (enclosing != null) {
            return enclosing.get(name);
        }

        throw new RuntimeError(name,
                "Undefined variable '" + name.lexeme + "'.");
    }

    /**
     * 给变量重新复制
     * @param name 变量名
     * @param value 变量的新值
     */
    void assign(Token name, Object value) {
        if (values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }
        // 当前作用域没有的话交给上层操作
        if (enclosing != null) {
            enclosing.assign(name, value);
            return;
        }

        throw new RuntimeError(name,
                "Undefined variable '" + name.lexeme + "'.");
    }
}
