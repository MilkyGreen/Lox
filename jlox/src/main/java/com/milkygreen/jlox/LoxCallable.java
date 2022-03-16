package com.milkygreen.jlox;

import java.util.List;

/**
 * 方法、函数抽象类
 */
public interface LoxCallable {

    /**
     * 执行调用
     * @param interpreter 解释器实例
     * @param arguments 入参
     * @return
     */
    Object call(Interpreter interpreter, List<Object> arguments);

    /**
     * 获取定义的参数数量
     * @return
     */
    int arity();

}
