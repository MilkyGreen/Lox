package com.milkygreen.jlox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

/**
 */
public class Lox {
    private static final Interpreter interpreter = new Interpreter();

    // 是否有编译错误
    static boolean hadError = false;
    // 是否有运行时错误
    static boolean hadRuntimeError = false;

    public static void main(String[] args) throws Exception {
        if (args.length > 1) {
            System.out.println("Usage: jlox [script]");
            System.exit(64);
        } else if (args.length == 1) {
            // 一个参数，直接读取文件
            runFile(args[0]);
        } else {
            // 没有参数，交互式命令行
            runPrompt();
        }
    }

    /**
     * 以文件方式执行源代码
     * @param path 源代码文件路径
     * @throws IOException
     */
    private static void runFile(String path) throws IOException {
        byte[] bytes = Files.readAllBytes(Paths.get(path));
        run(new String(bytes, Charset.defaultCharset()));
        if (hadError) {
            System.exit(65);
        }
        if (hadRuntimeError) {
            System.exit(70);
        }
    }

    /**
     * 以命令行方式交互运行
     * @throws IOException
     */
    private static void runPrompt() throws IOException {
        InputStreamReader input = new InputStreamReader(System.in);
        BufferedReader reader = new BufferedReader(input);
        // 读取输入的代码,执行
        for (;;) {
            System.out.print("> ");
            String line = reader.readLine();
            if (line == null){
                break;
            }
            run(line);
            hadError = false;
        }
    }

    /**
     * 执行源码
     * @param source
     */
    private static void run(String source) {
        // 将源码字符串识别成token列表
        Scanner scanner = new Scanner(source);
        List<Token> tokens = scanner.scanTokens();

        // 将token列表解析成statement列表。所有的代码都是由一个个statement组成的
        Parser parser = new Parser(tokens);
        List<Stmt> statements = parser.parse();

        // 如果存在编译错误直接返回
        if (hadError){
            return;
        }

        // 静态分析
        Resolver resolver = new Resolver(interpreter);
        resolver.resolve(statements);
        if (hadError){
            return;
        }

        // 解释执行
        interpreter.interpret(statements);
    }

    static void error(int line, String message) {
        report(line, "", message);
    }

    private static void report(int line, String where,
                               String message) {
        System.err.println(
                "[line " + line + "] Error" + where + ": " + message);
        hadError = true;
    }

    static void error(Token token, String message) {
        if (token.type == TokenType.EOF) {
            report(token.line, " at end", message);
        } else {
            report(token.line, " at '" + token.lexeme + "'", message);
        }
    }

    static void runtimeError(RuntimeError error) {
        System.err.println(error.getMessage() +
                "\n[line " + error.token.line + "]");
        hadRuntimeError = true;
    }
}
