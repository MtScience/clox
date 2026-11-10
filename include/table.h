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


/**
 * \brief Initializes a hash table.
 *
 * Takes a newly-created `Table` and fills in its fields, putting the structure
 * in a well-defined state.
 *
 * \param[in,out] table Non-`NULL` pointer to the table to be initialized.
 */
void initTable(Table table[restrict static 1]);

/**
 * \brief Frees a hash table.
 *
 * Takes a `Table` and frees the memory occupied by it.
 *
 * \param[in] gc    Non-`NULL` pointer to the `GC` structure managing the table's
 *                  `Table::entries` field.
 * \param[in] chunk Non-`NULL` pointer to the table to be freed.
 */
void freeTable(GC* gc, Table table[restrict static 1]);


/**
 * \brief Adds or replaces a value in a table.
 *
 * Takes a `Table`, an `ObjString` key and a `Value` and associates the value with the given key
 * in the given table.
 *
 * \param[in] gc    Non-`NULL` pointer to the `GC` structure managing the table's
 *                  `Table::entries` field.
 * \param[in] table Non-`NULL` pointer to the table, in which to create a key-value pair.
 * \param[in] key   Non-`NULL` pointer to the key.
 * \param[in] value The value to associate with the key.
 *
 * \returns `true` if a new pair was added, `false` otherwise.
 */
bool tableSet(GC* gc, Table table[static 1], ObjString* key, Value value);

/**
 * \brief Finds a value in a table.
 *
 * Takes a `Table` and an `ObjString` key and gets the value associated with that key. The value
 * is returned through an argument..
 *
 * \param[in] table Non-`NULL` pointer to the table, in which to find the key.
 * \param[in] key   Non-`NULL` pointer to the key.
 * \param[in] value A pointer to the `Value` in which to put the value associated with the key.
 *
 * \returns `true` if the given key was found in the table, `false` otherwise.
 */
bool tableGet(Table table[static 1], ObjString* key, Value* value);

/**
 * \brief Merges two tables.
 *
 * Takes a source `Table`, and a destination `Table` and adds all the entries in the source table
 * to the destination table using `tableSet()`. The source remains unchanged. Used for inheriting
 * methods.
 *
 * \param[in] gc   Non-`NULL` pointer to the `GC` structure managing the target table's
 *                 `Table::entries` field.
 * \param[in] from Non-`NULL` pointer to the source table.
 * \param[in] to   Non-`NULL` pointer to the destination table.
 */
void tableAddAll(GC* gc, Table from[static 1], Table to[static 1]);

/**
 * \brief Removes a key-value pair from a table.
 *
 * Takes a `Table` and an `ObjString` key and removes the pair that has the given key.

 * \param[in] table Non-`NULL` pointer to the table, in which to create a key-value pair.
 * \param[in] key   Non-`NULL` pointer to the key to be removed.
 *
 * \returns `true` if a pair was found and deleted, `false` otherwise.
 */
bool tableDelete(Table table[static 1], ObjString* key);


/**
 * \brief Finds a key in a table.
 *
 * Takes a `Table`, and C `char` array (not necessarily a proper null-terminated string) and
 * searches for a key that mathces this character array. Used for string interning, where a
 * `Table` is treated as a hash set.
 *
 * \param[in] table  Non-`NULL` pointer to the table, in which to find the key.
 * \param[in] hash   Hash value of the character array/key.
 * \param[in] length Length of the array.
 * \param[in] chars  Array of characters to search for.
 *
 * \returns The pointer to the `ObjString` representing the key if it found in the table,
 *          `NULL` otherwise.
 */
ObjString* tableFindString(Table table[static 1], uint32_t hash, uint32_t length, char const chars[static length]);


/**
 * \brief Marks a table as reachable.
 *
 * Takes a `Table` and iterates over it, marking every key and value as reachable using
 * `markObject()` and `markValue()`.
 *
 * \param[in] gc    Non-`NULL` pointer to the `GC` structure managing the target table's
 *                  `Table::entries` field.
 * \param[in] table Non-`NULL` pointer to the table to be marked.
 */
void markTable(GC* gc, Table table[static 1]);

/**
 * \brief Deletes unreachable pairs from a table.
 *
 * Takes a `Table` and iterates over it, deleting every pair whose key is not marked as
 * reachable. It is supposed to be used as a `GC::cleanupWeakRefs` function.
 *
 * \param[in] gc    Non-`NULL` pointer to the `GC` structure managing the target table's
 *                  `Table::entries` field.
 * \param[in] table Non-`NULL` pointer to the table to be cleaned up.
 */
void tableRemoveWhite(Table table[static 1]);


#endif
