package com.milkygreen.jlox;

/**
 * break声明。以异常的方式中断循环
 */
public class Break extends RuntimeException {

    Break() {
        super(null, null, false, false);

    }
}
