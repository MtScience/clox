#ifndef clox_table_h
#define clox_table_h


#include "common.h"
#include "value.h"


typedef struct {
    ObjString* key;
    Value      value;
} Entry;


typedef struct {
    uint32_t count;
    uint32_t capacity;
    Entry*   entries;
} Table;


void initTable(Table table[restrict static 1]);
void freeTable(GC* gc, Table table[restrict static 1]);

bool tableSet(GC* gc, Table table[static 1], ObjString* key, Value value);
bool tableGet(Table table[static 1], ObjString* key, Value* value);
void tableAddAll(GC* gc, Table from[static 1], Table to[static 1]);
bool tableDelete(Table table[static 1], ObjString* key);

ObjString* tableFindString(Table table[static 1], char const* chars, uint32_t length, uint32_t hash);

void markTable(GC* gc, Table table[static 1]);
void tableRemoveWhite(Table table[static 1]);


#endif
