/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define C_CFISH_HASH
#define CFISH_USE_SHORT_NAMES

#include <string.h>
#include <stdlib.h>

#include "Clownfish/Class.h"

#include "Clownfish/Hash.h"
#include "Clownfish/String.h"
#include "Clownfish/Err.h"
#include "Clownfish/VArray.h"
#include "Clownfish/Util/Memory.h"

// TOMBSTONE is shared across threads, so it must never be incref'd or
// decref'd.
static String *TOMBSTONE;

#define HashEntry cfish_HashEntry

typedef struct HashEntry {
    String  *key;
    Obj     *value;
    int32_t  hash_sum;
} HashEntry;

// Return the entry associated with the key, if any.
static CFISH_INLINE HashEntry*
SI_fetch_entry(Hash *self, String *key, int32_t hash_sum);

// Double the number of buckets and redistribute all entries.
static CFISH_INLINE HashEntry*
SI_rebuild_hash(Hash *self);

void
Hash_init_class() {
    TOMBSTONE = Str_newf("[HASHTOMBSTONE]");
}

Hash*
Hash_new(uint32_t capacity) {
    Hash *self = (Hash*)Class_Make_Obj(HASH);
    return Hash_init(self, capacity);
}

Hash*
Hash_init(Hash *self, uint32_t capacity) {
    // Allocate enough space to hold the requested number of elements without
    // triggering a rebuild.
    uint32_t requested_capacity = capacity < INT32_MAX ? capacity : INT32_MAX;
    uint32_t threshold;
    capacity = 16;
    while (1) {
        threshold = (capacity / 3) * 2;
        if (threshold > requested_capacity) { break; }
        capacity *= 2;
    }

    // Init.
    self->size         = 0;

    // Derive.
    self->capacity     = capacity;
    self->entries      = (HashEntry*)CALLOCATE(capacity, sizeof(HashEntry));
    self->threshold    = threshold;

    return self;
}

void
Hash_Destroy_IMP(Hash *self) {
    if (self->entries) {
        Hash_Clear(self);
        FREEMEM(self->entries);
    }
    SUPER_DESTROY(self, HASH);
}

void
Hash_Clear_IMP(Hash *self) {
    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *const limit = entry + self->capacity;

    // Iterate through all entries.
    for (; entry < limit; entry++) {
        if (!entry->key) { continue; }
        if (entry->key == TOMBSTONE) {
            entry->key = NULL;
            continue;
        }
        DECREF(entry->key);
        DECREF(entry->value);
        entry->key       = NULL;
        entry->value     = NULL;
        entry->hash_sum  = 0;
    }

    self->size = 0;
    // All tombstones were removed, reset threshold.
    self->threshold = (self->capacity / 3) * 2;
}

static void
S_do_store(Hash *self, String *key, Obj *value, int32_t hash_sum,
           bool incref_key) {
    HashEntry *entry = SI_fetch_entry(self, key, hash_sum);
    if (entry) {
        DECREF(entry->value);
        entry->value = value;
        return;
    }

    HashEntry *entries = self->size >= self->threshold
                         ? SI_rebuild_hash(self)
                         : (HashEntry*)self->entries;
    uint32_t       tick = hash_sum;
    const uint32_t mask = self->capacity - 1;

    while (1) {
        tick &= mask;
        HashEntry *entry = entries + tick;
        if (entry->key == TOMBSTONE || !entry->key) {
            if (entry->key == TOMBSTONE) {
                // Take note of diminished tombstone clutter.
                self->threshold++;
            }
            entry->key       = incref_key
                               ? (String*)INCREF(key)
                               : key;
            entry->value     = value;
            entry->hash_sum  = hash_sum;
            self->size++;
            break;
        }
        tick++; // linear scan
    }
}

void
Hash_Store_IMP(Hash *self, String *key, Obj *value) {
    S_do_store(self, key, value, Str_Hash_Sum(key), true);
}

void
Hash_Store_Utf8_IMP(Hash *self, const char *key, size_t key_len, Obj *value) {
    StackString *key_buf = SSTR_WRAP_UTF8((char*)key, key_len);
    S_do_store(self, (String*)key_buf, value, SStr_Hash_Sum(key_buf), true);
}

Obj*
Hash_Fetch_Utf8_IMP(Hash *self, const char *key, size_t key_len) {
    StackString *key_buf = SSTR_WRAP_UTF8(key, key_len);
    return Hash_Fetch_IMP(self, (String*)key_buf);
}

static CFISH_INLINE HashEntry*
SI_fetch_entry(Hash *self, String *key, int32_t hash_sum) {
    uint32_t tick = hash_sum;
    HashEntry *const entries = (HashEntry*)self->entries;
    HashEntry *entry;

    while (1) {
        tick &= self->capacity - 1;
        entry = entries + tick;
        if (!entry->key) {
            // Failed to find the key, so return NULL.
            return NULL;
        }
        else if (entry->hash_sum == hash_sum
                 && entry->key != TOMBSTONE
                 && Str_Equals(key, (Obj*)entry->key)
                ) {
            return entry;
        }
        tick++;
    }
}

Obj*
Hash_Fetch_IMP(Hash *self, String *key) {
    HashEntry *entry = SI_fetch_entry(self, key, Str_Hash_Sum(key));
    return entry ? entry->value : NULL;
}

Obj*
Hash_Delete_IMP(Hash *self, String *key) {
    HashEntry *entry = SI_fetch_entry(self, key, Str_Hash_Sum(key));
    if (entry) {
        Obj *value = entry->value;
        DECREF(entry->key);
        entry->key       = TOMBSTONE;
        entry->value     = NULL;
        entry->hash_sum  = 0;
        self->size--;
        self->threshold--; // limit number of tombstones
        return value;
    }
    else {
        return NULL;
    }
}

Obj*
Hash_Delete_Utf8_IMP(Hash *self, const char *key, size_t key_len) {
    StackString *key_buf = SSTR_WRAP_UTF8(key, key_len);
    return Hash_Delete_IMP(self, (String*)key_buf);
}

String*
Hash_Find_Key_IMP(Hash *self, String *key, int32_t hash_sum) {
    HashEntry *entry = SI_fetch_entry(self, key, hash_sum);
    return entry ? entry->key : NULL;
}

VArray*
Hash_Keys_IMP(Hash *self) {
    VArray    *keys        = VA_new(self->size);
    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *const limit = entry + self->capacity;

    for (; entry < limit; entry++) {
        if (entry->key && entry->key != TOMBSTONE) {
            VA_Push(keys, INCREF(entry->key));
        }
    }

    return keys;
}

VArray*
Hash_Values_IMP(Hash *self) {
    VArray    *values      = VA_new(self->size);
    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *const limit = entry + self->capacity;

    for (; entry < limit; entry++) {
        if (entry->key && entry->key != TOMBSTONE) {
            VA_Push(values, INCREF(entry->value));
        }
    }

    return values;
}

bool
Hash_Equals_IMP(Hash *self, Obj *other) {
    Hash    *twin = (Hash*)other;

    if (twin == self)             { return true; }
    if (!Obj_Is_A(other, HASH))   { return false; }
    if (self->size != twin->size) { return false; }

    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *const limit = entry + self->capacity;

    for (; entry < limit; entry++) {
        if (entry->key && entry->key != TOMBSTONE) {
            Obj *other_val = Hash_Fetch(twin, entry->key);
            if (!other_val || !Obj_Equals(other_val, entry->value)) {
                return false;
            }
        }
    }

    return true;
}

uint32_t
Hash_Get_Capacity_IMP(Hash *self) {
    return self->capacity;
}

uint32_t
Hash_Get_Size_IMP(Hash *self) {
    return self->size;
}

static CFISH_INLINE HashEntry*
SI_rebuild_hash(Hash *self) {
    // HashIterator ticks are int32_t.
    if (self->capacity > INT32_MAX / 2) {
        THROW(ERR, "Hash grew too large");
    }

    HashEntry *old_entries = (HashEntry*)self->entries;
    HashEntry *entry       = old_entries;
    HashEntry *limit       = old_entries + self->capacity;

    self->capacity *= 2;
    self->threshold = (self->capacity / 3) * 2;
    self->entries   = (HashEntry*)CALLOCATE(self->capacity, sizeof(HashEntry));
    self->size      = 0;

    for (; entry < limit; entry++) {
        if (!entry->key || entry->key == TOMBSTONE) {
            continue;
        }
        S_do_store(self, entry->key, entry->value, entry->hash_sum, false);
    }

    FREEMEM(old_entries);

    return (HashEntry*)self->entries;
}

String*
Hash_get_tombstone() {
    return TOMBSTONE;
}

Hash*
Hash_Shallow_Copy_IMP(Hash *self) {
    Hash *twin = Hash_new(self->size);

    HashEntry *entries = (HashEntry*)twin->entries;
    memcpy(entries, self->entries, self->capacity * sizeof(HashEntry));
    twin->size      = self->size;
    twin->threshold = self->threshold;

    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *const limit = entry + self->capacity;

    for (; entry < limit; entry++) {
        if (entry->key && entry->key != TOMBSTONE) {
            (void)INCREF(entry->key);
            (void)INCREF(entry->value);
        }
    }

    return twin;
}

/*Hash*
Hash_Clone_IMP(Hash *self) {
    Hash *twin = Hash_new(self->size);

    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *const limit = entry + self->capacity;

    for (; entry < limit; entry++) {
        if (entry->key && entry->key != TOMBSTONE) {
	        String *key = Str_Clone(entry->key);
	        Obj *value = Obj_Clone(entry->value);
	        if (value) {
                Hash_Store(twin, key, value);
	        }
        }
    }

    return twin;
}
*/

Hash*
Hash_Clone_IMP(Hash *self) {
    Hash *twin = Hash_new(self->size);

    HashEntry *entry       = (HashEntry*)self->entries;
    HashEntry *twin_entry  = (HashEntry*)twin->entries;
    HashEntry *const limit = entry + self->capacity;

    for (; entry < limit; entry++ && twin_entry++) {
        if (entry->key) {
            if (entry->key == TOMBSTONE) {
                twin_entry->key = TOMBSTONE;
            } else {
                twin_entry->key = Str_Clone(entry->key);
                if (entry->value) {
                    twin_entry->value = Obj_Clone(entry->value);
                }
            }
            twin_entry->hash_sum = entry->hash_sum;
        }
    }

    twin->size = self->size;
    twin->threshold = self->threshold;

    return twin;
}
