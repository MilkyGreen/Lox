package com.milkygreen.jlox;

/**
 * 函数或方法的return语句
 * 直接将return定义为一个异常，可以方便的直接抛到函数最外层
 */
public class Return extends RuntimeException {

    // 返回值
    final Object value;

    Return(Object value) {
        super(null, null, false, false);
        this.value = value;
    }
}
