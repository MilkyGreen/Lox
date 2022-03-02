package com.milkygreen.jlox;

import java.util.List;

/**
 * 虚拟语法树
 */
abstract class Expr {
    /**
     * Visitor模式接口，方便新增语法类和处理方法
     * @param <R>
     */
    interface Visitor<R> {
        R visitBinaryExpr(Binary expr);

        R visitGroupingExpr(Grouping expr);

        R visitLiteralExpr(Literal expr);

        R visitUnaryExpr(Unary expr);
    }

    /**
     * 二元语法，一个操作符和两个表达式
     */
    static class Binary extends Expr {
        Binary(Expr left, Token operator, Expr right) {
            this.left = left;
            this.operator = operator;
            this.right = right;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitBinaryExpr(this);
        }

        final Expr left;
        final Token operator;
        final Expr right;
    }

    /**
     * 分组语法，括号等
     */
    static class Grouping extends Expr {
        Grouping(Expr expression) {
            this.expression = expression;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitGroupingExpr(this);
        }

        final Expr expression;
    }

    /**
     * 字面值。不需要再计算的最终值
     */
    static class Literal extends Expr {
        Literal(Object value) {
            this.value = value;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitLiteralExpr(this);
        }

        final Object value;
    }

    /**
     * 一元操作，一个操作符和一个表达式，如 -1  !true
     */
    static class Unary extends Expr {
        Unary(Token operator, Expr right) {
            this.operator = operator;
            this.right = right;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitUnaryExpr(this);
        }

        final Token operator;
        final Expr right;
    }

    /**
     * 基础类的accept,用于接收visitor，来实现各自的操作
     * @param visitor
     * @param <R>
     * @return
     */
    abstract <R> R accept(Visitor<R> visitor);
}
