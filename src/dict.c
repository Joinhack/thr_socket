#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "jmalloc.h"
#include "dict.h"

#define DICT_TRY_REHASH(d) if(DICT_IS_REHASHING(d) && d->iterators == 0) dict_rehash(d, 1)

#define ENTRY_DECR(e) if(--e->ref == 0) jfree(e)
#define ENTRY_INCR(e) e->ref++

static unsigned int _pow_size(unsigned int size) {
	unsigned int s = DICT_INIT_SIZE;
	if(s > INT_MAX) return INT_MAX;
	while(s < size) s<<=1;
	return s;
}

static void _table_reset(dict_table *table) {
	table->entries = NULL;
	table->size = 0;
	table->mask = 0;
	table->used = 0;
}

static void _table_clear(dict *d, dict_table *table) {
	size_t i;
	dict_entry *entry, *next;
	for(i = 0; i < table->size; i++) {
		entry = table->entries[i];
		while(entry) {
			next = entry->next;
			DICT_KEY_DESTROY(d, entry);
			DICT_VALUE_DESTROY(d, entry);
			ENTRY_DECR(entry);
			table->used--;
			entry = next;
		}
	}
	if(table->entries)
		jfree(table->entries);
	_table_reset(table);
}

dict *dict_create(dict_opts *opts) {
	dict *d = jmalloc(sizeof(struct dict));
	_table_reset(&d->dt[0]);
	_table_reset(&d->dt[1]);
	d->rehashidx = -1;
	d->opts = opts;
	d->iterators = 0;
	return d;
}

void dict_destroy(dict *d) {
	d->rehashidx = -1;
	_table_clear(d, &d->dt[0]);
	_table_clear(d, &d->dt[1]);
	jfree(d);
}

/**
 *if is rehashing move data from old to new hash table
 */
int dict_rehash(dict *d, int n) {
	dict_entry *entry, *next;
	if(!DICT_IS_REHASHING(d)) return 0;
	while(n--) {
		//if finish the rehash move the dt[1] to dt[0]
		if(d->dt[0].used == 0) {
			jfree(d->dt[0].entries);
			d->dt[0] = d->dt[1];
			_table_reset(&d->dt[1]);
			d->rehashidx = -1;
			return 0;
		}

		while(d->dt[0].entries[d->rehashidx] == NULL) d->rehashidx++;
		entry = d->dt[0].entries[d->rehashidx];
		while(entry) {
			unsigned int idx; 
			next = entry->next;
			idx = DICT_HASH(d, entry->key)&d->dt[1].mask;
			//if have the enetry, push it.
			entry->next = d->dt[1].entries[idx];
			d->dt[1].entries[idx] = entry;
			d->dt[1].used++;
			d->dt[0].used--;
			entry = next;
		}
		d->dt[0].entries[d->rehashidx++] = NULL;
	}
	return 1;
}

static int dict_try_expand(dict *d) {
	if(DICT_IS_REHASHING(d)) return 0;
	if(d->dt[0].size == 0) return dict_expand(d, DICT_INIT_SIZE);
	if(d->dt[0].used >= d->dt[0].size)
		return dict_expand(d, d->dt[0].used*2);
	return 0;
}

/**
 *expand the dict capacity.
 */
int dict_expand(dict *d, unsigned int size) {
	unsigned int realsize;
	size_t msize;
	dict_table dt;
	//if dict is rehashing or new size less than dict capacity, don't epxand.
	if(DICT_IS_REHASHING(d) || d->dt[0].size > size)
		return -1;
	realsize = _pow_size(size);
	dt.size = realsize;
	dt.mask = realsize - 1;
	dt.used = 0;
	msize = realsize*sizeof(struct dict_entry *);
	dt.entries = jmalloc(msize);
	memset(dt.entries, 0, msize);
	if(d->dt[0].entries == NULL) {
		d->dt[0] = dt;
		return 0;
	}
	d->dt[1] = dt;
	d->rehashidx = 0;
	return 0;
}

//find the index of bucket, if key exist return -1.
static int _dict_key_index(dict *d, const void *key) {
	int i, idx;
	dict_entry *entry;
	unsigned int hash;
	if(dict_try_expand(d) < 0) return -1;
	hash = DICT_HASH(d, key);
	for(i = 0; i <=1; i++) {
		idx = hash&d->dt[i].mask;
		entry = d->dt[i].entries[idx];
		while(entry) {
			if(DICT_KEY_COMPARE(d, entry->key, key))
				return -1;
			entry = entry->next;
		}
		//if is rehashing, should choose dt[1]
		if(!DICT_IS_REHASHING(d)) break;
	}
	return idx;
}

int dict_add(dict *d, void *key, void *val) {
	int htidx;
	int idx;
	dict_entry *entry;
	//try to reash, if is rehashing move data from old to new hash table
	DICT_TRY_REHASH(d);
	if((idx = _dict_key_index(d, key)) < 0) return -1;
	htidx = DICT_IS_REHASHING(d)?1:0;
	entry = jmalloc(sizeof(struct dict_entry));
	entry->ref = 1;
	DICT_SET_KEY(d, entry, key);
	DICT_SET_VALUE(d, entry, val);
	entry->next = d->dt[htidx].entries[idx];
	d->dt[htidx].used++;
	d->dt[htidx].entries[idx] = entry;
	return 1;
}

int dict_replace(dict *d, void *key, void *val) {
	dict_entry *entry;
	if((entry = dict_find(d, key)) != NULL) {
		DICT_VALUE_DESTROY(d, entry);
		DICT_SET_VALUE(d, entry, val);
		return 0;
	} else
		return dict_add(d, key, val);
}

dict_entry *dict_find(dict *d, void *key) {
	int i, idx;
	dict_entry *entry;
	unsigned int hash;
	if(d->dt[0].size == 0) return NULL;
	DICT_TRY_REHASH(d);
	hash = DICT_HASH(d, key);
	for(i = 0; i <=1; i++) {
		idx = hash&d->dt[i].mask;
		entry = d->dt[i].entries[idx];
		while(entry) {
			if(DICT_KEY_COMPARE(d, entry->key, key)) {
				return entry;
			}
			entry = entry->next;
		}
		//if is rehashing, should choose dt[1]
		if(!DICT_IS_REHASHING(d)) break;
	}
	return NULL;
}

static int dict_del_internal(dict *d, void *key, int flag) {
	int i, idx;
	dict_entry *entry, *prev;
	unsigned int hash;
	if(d->dt[0].size == 0) return 0;
	DICT_TRY_REHASH(d);
	hash = DICT_HASH(d, key);
	for(i = 0; i <= 1; i++) {
		idx = hash&d->dt[i].mask;
		entry = d->dt[i].entries[idx];
		prev = NULL;
		while(entry) {
			if(DICT_KEY_COMPARE(d, entry->key, key)) {
				if(flag) {
					DICT_KEY_DESTROY(d, entry);
					DICT_VALUE_DESTROY(d, entry);
				}
				if(prev)
					prev->next = entry->next;
				else
					d->dt[i].entries[idx] = entry->next;
				ENTRY_DECR(entry);
				d->dt[i].used--;
				return 1;
			}
			prev = entry;
			entry = entry->next;
		}
		//if is rehashing, should choose dt[1]
		if(!DICT_IS_REHASHING(d)) break;
	}
	return 0;
}

int dict_del(dict *d, void *key) {
	return dict_del_internal(d, key, 1);
}

int dict_del_no_free(dict *d, void *key) {
	return dict_del_internal(d, key, 0);
}

//copy it from redis
unsigned int dict_generic_hash(const char *buf, size_t len) {
	unsigned int hash = 5381;

	while (len--)
		hash = ((hash << 5) + hash) + (*buf++); /* hash * 33 + c */
	return hash;
}

dict_iterator *dict_get_iterator(dict *d) {
	dict_iterator *iter;
	iter = jmalloc(sizeof(struct dict_iterator));
	iter->dt_idx = 0;
	iter->idx = -1;
	iter->d = d;
	iter->entry = NULL;
	return iter;
}

dict_entry *dict_iterator_next(dict_iterator *iter) {
	dict *d = iter->d;
	while(1) {
		if(iter->entry == NULL) {
			if(iter->dt_idx == 0 && iter->idx == -1)
				d->iterators++;
			iter->idx++;
			if(iter->idx >= d->dt[iter->dt_idx].size) {
				if(iter->dt_idx == 0 && DICT_IS_REHASHING(d)) {
					iter->dt_idx++;
					iter->idx = 0;
				} else
					break;
			}
			iter->entry = d->dt[iter->dt_idx].entries[iter->idx];
		} else {
			ENTRY_DECR(iter->entry);
			iter->entry = iter->entry->next;
		}
		if(iter->entry) {
			ENTRY_INCR(iter->entry);
			return iter->entry;
		}
	}
	return NULL;
}

void dict_iterator_destroy(dict_iterator *iter) {
	if(!(iter->idx == -1 && iter->dt_idx == 0))
		iter->d->iterators--;
	if(iter->entry)
		ENTRY_DECR(iter->entry);
	jfree(iter);
}


