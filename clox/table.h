#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// 哈希表的Entry定义
typedef struct {
    ObjString* key;  // key，字符串
    Value value;     // value
} Entry;

/**
 * @brief 哈希表定义
 * lox的哈希表采用数组+开放性寻址（open addressing）来实现。
 * 根据key的hash值确定数组索引，每个索引只存一个entry。如果遇到冲突，则继续向期望索引后面遍历，存到遇到的第一个空位置或者墓碑节点。
 * 删除key时，并不真的把entry置为空，而是标记为墓碑节点（不这么做的话无法保证get方法的正确性）。墓碑节点可以被后面的set方法复用。此时count数量并不减少，不然可能会出现全都是墓碑count=0的情况，findEntry永远在找非墓碑空节点。
 * 最大负载为0.75,达到时需要扩容。在扩容时会把墓碑节点的数量去掉。
 *
 */
typedef struct {
    int count;       // 当前Entry个数
    int capacity;    // 当前容量
    Entry* entries;  // Entry数组
} Table;

// 初始化哈希表
void initTable(Table* table);

// 释放哈希表
void freeTable(Table* table);

/**
 * @brief 从哈希表取数据
 *
 * @param table  哈希表引用
 * @param key  key
 * @param value  value引用，如果取到会把值赋给这个引用地址
 * @return true
 * @return false
 */
bool tableGet(Table* table, ObjString* key, Value* value);

/**
 * @brief set方法
 *
 * @param table 哈希表引用
 * @param key key
 * @param value value
 * @return true
 * @return false
 */
bool tableSet(Table* table, ObjString* key, Value value);

/**
 * @brief 删除方法
 *
 * @param table 哈希表引用
 * @param key key
 * @return true
 * @return false
 */
bool tableDelete(Table* table, ObjString* key);

/**
 * @brief 整体拷贝哈希表
 *
 * @param from
 * @param to
 */
void tableAddAll(Table* from, Table* to);

/**
 * @brief
 * 查找字符串的key是否已经存在哈希表中。用来做字符串缓存，确保lox中每个相同字符串都使用一个ObjString，
 * 这样比较时可以直接用等号，避免逐个char对比。
 *
 * @param table 哈希表引用
 * @param chars  字符串
 * @param length  长度
 * @param hash  字符串hash
 * @return ObjString* 字符对象
 */
ObjString* tableFindString(Table* table,
                           const char* chars,
                           int length,
                           uint32_t hash);

#endif