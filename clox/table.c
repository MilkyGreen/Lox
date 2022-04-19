#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// 最大负载因子。负载因子=count/cap
// 达到这个值说明需要扩容了，不然后面冲突的可能性会比较大。
#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    // 释放entry数组
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

/**
 * @brief 根据key查找Entry
 *
 * @param entries  Entry数组
 * @param capacity  当前容量
 * @param key key
 * @return Entry*
 */
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // 计算数组中的索引
    uint32_t index = key->hash % capacity;
    // 墓碑节点
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // 找到了一个空节点。如果前面遇到过墓碑节点，返回墓碑节点复用。没有当前的空节点
                return tombstone != NULL ? tombstone : entry;
            } else {
                // 记录遇到的第一个墓碑节点
                if (tombstone == NULL)
                    tombstone = entry;
            }
        } else if (entry->key == key) {
            // 找到了
            return entry;
        }
        // 如果上面没找到合适的，继续向数组下一个位置遍历。%
        // capacity是为了遍历到末尾之后从头开始
        index = (index + 1) % capacity;
    }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0)
        return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;
    // 把传入的引用内存数据改成entry里面的
    *value = entry->value;
    return true;
}

/**
 * @brief 调整哈希表大小
 *
 * @param table 哈希表引用
 * @param capacity 新容量
 */
static void adjustCapacity(Table* table, int capacity) {
    // 开辟新容量的数组空间
    Entry* entries = ALLOCATE(Entry, capacity);
    // 初始化每个entry
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }
    // count先归零。新的哈希表要去除之前的墓碑节点
    table->count = 0;
    // 依次把老数组的entry放入新数组
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL)
            // 可能是个空节点或者墓碑节点，都跳过
            continue;
        // 找到新节点，把值拷贝过去
        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }
    // 释放老的数组空间
    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    // 如果元素数量就要超过阈值，要先扩容
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    // 是否是新的key
    bool isNewKey = entry->key == NULL;
    // 如果key和value都是空，则count++。 墓碑节点的vlaue不为空，也是算count的
    if (isNewKey && IS_NIL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0)
        return false;

    // Find the entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    // 将要被删除的节点标记为墓碑节点（key=null，value=true），供后面复用.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table,
                           const char* chars,
                           int length,
                           uint32_t hash) {
    if (table->count == 0)
        return NULL;
    // 计算索引位置
    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            // 如果找到了非墓碑空节点，停止遍历.
            if (IS_NIL(entry->value))
                return NULL;
                // 比较长度、hash、char数组，都相当时任务是相同字符串
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}