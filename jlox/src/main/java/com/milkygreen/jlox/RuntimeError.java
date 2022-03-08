package com.milkygreen.jlox;

/**
 * lox运行时异常
 */
public class RuntimeError extends RuntimeException{

    final Token token;

    RuntimeError(Token token, String message) {
        super(message);
        this.token = token;
    }

}
