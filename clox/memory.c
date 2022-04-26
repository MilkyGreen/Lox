#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

// 每次GC之后，下载GC阈值为当前的两倍
#define GC_HEAP_GROW_FACTOR 2

/**
 * @brief 对一个数组（指针）进行扩容
 *
 * @param pointer 数组的第一个元素指针
 * @param oldSize 旧的长度
 * @param newSize 新的长度
 * @return void*
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    // 更新全局对象大小
    vm.bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
    }

    // 如果达到了GC触发条件
    if (vm.bytesAllocated > vm.nextGC) {
        collectGarbage();
    }

    // 如果新长度是0，代表需要回收
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }
    // 重新申请空间
    void* result = realloc(pointer, newSize);
    if (result == NULL)
        exit(1);
    return result;
}

void markObject(Obj* object) {
    if (object == NULL) {
        return;
    }

    if (object->isMarked) {
        // 如果已经被标记个了，说明是循环引用，不继续处理。
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    // 动态扩容grayStack数组
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack =
            (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if (vm.grayStack == NULL)
            exit(1);
    }

    // 放入遍历中的对象数组
    vm.grayStack[vm.grayCount++] = object;

    object->isMarked = true;
}

void markValue(Value value) {
    if (IS_OBJ(value))
        markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

// GC遍历对象的引用
static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            // 闭包对象引用了一个函数对象
            markObject((Obj*)closure->function);
            // 闭包变量数组
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            // 分别标记函数名称和常量池里的对象
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE:
            // closed字段引用了变量值
            markValue(((ObjUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

// 释放对象内存空间
static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_STRING: {
            //直接转成ObjString类型
            ObjString* string = (ObjString*)object;
            // 释放字符串所在的heap空间
            FREE_ARRAY(char, string->chars, string->length + 1);
            // 释放对象本身
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

// 标记存活对象
static void markRoots() {
    // 将执行当中的栈中的值依次标记
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // vm当前的frames帧栈里存的closure对象也需要标记
    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    // vm当前保存的闭包变量
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL;
         upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    // 全局变量
    markTable(&vm.globals);

    // 编译阶段也可能触发GC，标记编译过程中的对象
    markCompilerRoots();
}

// GC标记间接引用的对象
static void traceReferences() {
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

// 擦除未标记对象
static void sweep() {
    Obj* previous = NULL;
    // 这里是所有对象链表
    Obj* object = vm.objects;
    while (object != NULL) {
        if (object->isMarked) {
            // 改为未标记，为下次GC做准备。这次的遍历不会再到这个对象了，不影响后的遍历。
            object->isMarked = false;

            // 被标记的跳过
            previous = object;
            object = object->next;
        } else {
            // 未被标记的清除内存
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

// 执行垃圾回收
void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif
    // 从根对象开始标记存活对象
    markRoots();
    // 标记间接引用对象
    traceReferences();

    // 字符串常量池需要单独处理。等上面的对象标记完之后，把哈希表中未标记的字符串删除。
    tableRemoveWhite(&vm.strings);

    // 擦除未标记对象
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

// 释放vm中所有的对象空间
void freeObjects() {
    // 遍历链表依次释放每个对象
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.grayStack);
}
