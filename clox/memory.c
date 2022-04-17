#include <stdlib.h>

#include "memory.h"
#include "vm.h"

/**
 * @brief 对一个数组（指针）进行扩容
 *
 * @param pointer 数组的第一个元素指针
 * @param oldSize 旧的长度
 * @param newSize 新的长度
 * @return void*
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
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

// 释放对象内存空间
static void freeObject(Obj* object) {
    switch (object->type) {
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
    }
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
}
