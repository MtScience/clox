#ifndef clox_table_h
#define clox_table_h


#include "common.h"
#include "value.h"


/**
 * \struct Entry table.h
 * \brief Hash table entry type.
 *
 * A `struct` that represents a hash table entry.
 */
typedef struct {
    ObjString* key;    /**< Entry key string. */
    Value      value;  /**< Entry value. */
} Entry;


/**
 * \struct Table table.h
 * \brief Hash table type.
 *
 * A ``struct` that represents a hash table. Specialized to use
 * `ObjString`s as keys and `Value`s for values.
 */
typedef struct {
    uint32_t count;     /**< Number of entries in the table. */
    uint32_t capacity;  /**< Maximum number of entries the table can hold. */
    Entry*   entries;   /**< List of entries. */
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
