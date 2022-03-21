package com.milkygreen.jlox;

import java.util.HashMap;
import java.util.Map;

/**
 * 类实例，代表一个实例化的类
 */
public class LoxInstance {

    // 字段。字段可以在运行时随意增加和改变
    private final Map<String, Object> fields = new HashMap<>();

    // 实例来自于哪个类
    private final LoxClass klass;

    LoxInstance(LoxClass klass) {
        this.klass = klass;
    }

    @Override
    public String toString() {
        return klass.name + " instance";
    }

    /**
     * getter方法。
     * 从实例中获取字段或方法。比如 instance.name
     * @param name
     * @return
     */
    Object get(Token name) {
        // 先尝试找字段
        if (fields.containsKey(name.lexeme)) {
            return fields.get(name.lexeme);
        }
        // 尝试找方法
        LoxFunction method = klass.findMethod(name.lexeme);
        if (method != null){
            return method.bind(this);   // 方法上下文中新增this变量，指向当前实例
        }
        throw new RuntimeError(name,
                "Undefined property '" + name.lexeme + "'.");
    }

    /**
     * setter方法。这里只能set字段
     * @param name
     * @param value
     */
    void set(Token name, Object value) {
        fields.put(name.lexeme, value);
    }

}
