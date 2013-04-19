#ifndef DICT_H
#define DICT_H

typedef struct dict_entry {
	void *key;
	void *value;
	int ref;
	struct dict_entry *next;
} dict_entry;

typedef struct dict_table {
	dict_entry **entries;
	unsigned int size;
	unsigned int mask;
	unsigned int used;
} dict_table;

typedef struct dict_opts {
	unsigned int (*hash)(const void *key);
	void *(*key_dup)(const void *key);
	void *(*value_dup)(const void *key);
	int (*key_compare)(const void *k1, const void *k2);
	void (*key_destroy)(void *key);
	void (*value_destroy)(void *v);
} dict_opts;

typedef struct dict {
	dict_opts *opts;
	//when in rehash step 0 is used for old dict_table, 1 is used for new hashtable
	dict_table dt[2];
	int rehashidx;
	int iterators;
} dict;

typedef struct dict_iterator {
	dict *d;
	int dt_idx;
	int idx;
	dict_entry *entry;
} dict_iterator;

dict *dict_create(dict_opts *opts);

void dict_destroy(dict *d);

unsigned int dict_generic_hash(const char *buf, size_t len);

int dict_rehash(dict *d, int n);

int dict_expand(dict *d, unsigned int size);

int dict_add(dict *d, void *key, void *val);

dict_iterator *dict_get_iterator(dict *d);

dict_entry *dict_iterator_next(dict_iterator *iter);

void dict_iterator_destroy(dict_iterator *iter);

int dict_replace(dict *d, void *key, void *val);

int dict_del(dict *d, void *key);

dict_entry *dict_find(dict *d, void *key);

int dict_del_no_free(dict *d, void *key);

#define DICT_USED(d) (d->dt[0].used + d->dt[1].used)

#define DICT_IS_REHASHING(d) (d->rehashidx != -1)

#define DICT_CAP(d) (d->dt[1].size + d->dt[0].size)

#define DICT_SET_KEY(d, entry, key) \
	if(d->opts->key_dup) \
		entry->key = d->opts->key_dup(key);\
	else \
		entry->key = (key)

#define DICT_SET_VALUE(d, entry, val) \
	if(d->opts->value_dup) \
		entry->value = d->opts->value_dup(val);\
	else \
		entry->value = (val)

#define DICT_KEY_DESTROY(d, entry) \
	if(d->opts->key_destroy) \
		d->opts->key_destroy(entry->key)

#define DICT_KEY_COMPARE(d, k1, k2) d->opts->key_compare?d->opts->key_compare(k1, k2):k1 == k2


#define DICT_VALUE_DESTROY(d, entry) \
	if(d->opts->value_destroy) \
		d->opts->value_destroy(entry->value)

#define DICT_HASH(d, key) d->opts->hash(key)

#define DICT_INIT_SIZE 1<<3

#endif /**end define dict*/
