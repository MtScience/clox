#include <string.h>  /* For memcmp */
#include <assert.h>  /* For assert */

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"


static constexpr double tableMaxLoad = 0.75;


/* Local functions */

static Entry* findEntry(Entry entries[static 1], uint32_t capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = nullptr;

    for (;;) {
        Entry* entry = &entries[index];
        if (!entry->key) {
            if (isNil(entry->value)) {
                return tombstone ? tombstone : entry;
            } else if (!tombstone) {
                tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}


static void adjustCapacity(GC* gc, Table table[static 1], uint32_t capacity) {
    Entry* entries = ALLOCATE(gc, Entry, capacity);

    for (uint32_t i = 0; i < capacity; i++) {
        entries[i].key = nullptr;
        entries[i].value = nilVal();
    }

    table->count = 0;
    for (uint32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (!entry->key) {
            continue;
        }

        Entry* dest = findEntry(entries, capacity, entry->key);
        *dest = (Entry){
            .key = entry->key,
            .value = entry->value,
        };
        table->count++;
    }

    FREE_ARRAY(gc, Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}


/* Public functions */

void initTable(Table table[restrict static 1]) {
    *table = (Table){
        .count = 0,
        .capacity = 0,
        .entries = nullptr,
    };
}


void freeTable(GC* gc, Table table[restrict static 1]) {
    FREE_ARRAY(gc, Entry, table->entries, table->capacity);
    initTable(table);
}


bool tableSet(GC* gc, Table table[static 1], ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * tableMaxLoad) {
        uint32_t capacity = growCapacity(table->capacity);
        adjustCapacity(gc, table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->key == nullptr;
    if (isNewKey && isNil(entry->value)) {
        table->count++;
    }

    *entry = (Entry){
        .key = key,
        .value = value,
    };

    return isNewKey;
}


bool tableGet(Table table[static 1], ObjString* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (!entry->key) {
        return false;
    }

    *value = entry->value;
    return true;
}


void tableAddAll(GC* gc, Table from[static 1], Table to[static 1]) {
    for (uint32_t i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key) {
            tableSet(gc, to, entry->key, entry->value);
        }
    }
}


bool tableDelete(Table table[static 1], ObjString* key) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (!entry->key) {
        return false;
    }

    *entry = (Entry){
        .key = nullptr,
        .value = boolVal(true),
    };
    return true;
}


ObjString* tableFindString(Table table[static 1], char const* chars, uint32_t length, uint32_t hash) {
    if (table->count == 0) {
        return nullptr;
    }

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (!entry->key) {
            if (isNil(entry->value)) {
                return nullptr;
            }
        } else if (
            entry->key->length == length &&
            entry->key->hash == hash &&
            !memcmp(entry->key->chars, chars, length)
        ) {
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}


void markTable(GC* gc, Table table[static 1]) {
    for (uint32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject(gc, (Obj*)entry->key);
        markValue(gc, entry->value);
    }
}


void tableRemoveWhite(Table table[static 1]) {
    for (uint32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}
