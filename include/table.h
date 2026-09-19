#ifndef clox_table_h
#define clox_table_h


#include "common.h"
#include "value.h"


typedef struct {
    ObjString* key;
    Value value;
} Entry;


typedef struct {
    uint32_t count;
    uint32_t capacity;
    Entry* entries;
} Table;


void initTable(Table* restrict table);
void freeTable(Table* restrict table);

bool tableSet(Table* table, ObjString* key, Value value);
bool tableGet(Table* table, ObjString* key, Value* value);
void tableAddAll(Table* from, Table* to);
bool tableDelete(Table* table, ObjString* key);
ObjString* tableFindString(Table* table, char const* chars, uint32_t length, uint32_t hash);


#endif
