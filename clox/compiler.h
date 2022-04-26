#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

// 编译源码，返回函数对象
// 最外层的脚本代码也认为是一个函数，没有入参也没有返回值。执行的时候统一当做函数来执行
ObjFunction* compile(const char* source);

void markCompilerRoots();

#endif