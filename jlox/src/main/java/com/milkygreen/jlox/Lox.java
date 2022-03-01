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

    static boolean hadError = false;

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
    }

    /**
     * 以命令行方式执行
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

    private static void run(String source) {
        Scanner scanner = new Scanner(source);
        List<Token> tokens = scanner.scanTokens();

        for (Token token : tokens) {
            System.out.println(token);
        }
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
}
