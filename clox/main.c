#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

/**
 * @brief 执行交互式命令行
 * 
 */
static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        // 每次读取一行，解释执行
        interpret(line);
    }
}

/**
 * @brief 从path中读取文件内容
 * 
 * @param path 
 * @return char* 
 */
static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file); // 获取文件结尾的位置，来判断文件内容大小
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1); // 开辟文件大小的字符串空间
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    // 将文件读取到字符串中
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0'; // 文件结尾标志

    fclose(file);
    return buffer;
}

/**
 * @brief 执行指定路径上的源代码文件
 * 
 * @param path 
 */
static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

/**
 * @brief 
 * 
 * @param argc  argc至少会等于1，argv[0]是可执行文件名称，后面才是用户传入的参数
 * @param argv 
 * @return int 
 */
int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }
    freeVM();
    return 0;
}
