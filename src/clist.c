#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "jmalloc.h"
#include "clist.h"

static clist_item *clist_item_create() {
	clist_item *l_item;
	size_t len = sizeof(struct clist_item);
	l_item = jmalloc(len);
	if(l_item == NULL)
		return NULL;
	l_item->next = NULL;
	l_item->prev = NULL;
	l_item->data = NULL;
	return l_item;
}

static void clist_item_destroy(clist_item *item) {
	jfree(item);
}

clist *clist_create() {
	clist *cl;
	size_t len = sizeof(clist);
	cl = jmalloc(len);
	cl->head = NULL;
	cl->count = 0;
	return cl;
}


#define POP(cl, item) \
if(--cl->count) { \
	item->prev->next = item->next; \
	item->next->prev = item->prev; \
} else { \
	cl->head = NULL; \
} \
clist_item_destroy(item)


void *clist_rpop(clist *cl) {
	void *data;
	clist_item *item;
	if(cl->count == 0) {
		return NULL;
	}
	item = cl->head->prev;
	data = item->data;
	POP(cl, item);
	return data;
}

void *clist_lpop(clist *cl) {
	void *data;
	clist_item *item;
	if(cl->count == 0) {
		return NULL;
	}
	item = cl->head;
	data = item->data;
	cl->head = item->next;
	POP(cl, item);
	return data;
}

int clist_walk_remove(clist *cl, int (*cb)(void *, void *priv), void *priv) {
	int count = 0;
	clist_item *item, *next;
	item = cl->head;
	while(item != NULL || cl->count != count) {
		if(!cb(item->data, priv)) {
			next = item->next;
			if(--cl->count) {
				item->prev->next = next;
				next->prev = item->prev;
			} else {
				cl->head = NULL;
			}
			count++;
			if(next == item) {
				clist_item_destroy(item);
				break;
			}
			clist_item_destroy(item);
		} else {
			next = item->next;
		}
		item = next;
	}
	return count;
}

#define PUSH(cl, item) \
if(cl->head != NULL) { \
	hprev = cl->head->prev; \
	item->next = cl->head; \
	item->prev = hprev; \
	cl->head->prev = item; \
	hprev->next = item; \
} else { \
	item->prev = item; \
	item->next = item; \
	cl->head = item; \
}

void clist_lpush(clist *cl, void *data) {
	clist_item *item, *hprev;
	item = clist_item_create();
	item->data = data;
	PUSH(cl, item);
	cl->head = item;
	cl->count++;
}

void clist_rpush(clist *cl, void *data) {
	clist_item *item, *hprev;
	item = clist_item_create();
	item->data = data;
	PUSH(cl, item);
	cl->count++;
}

void clist_destroy(clist *cl) {
	while(cl->count > 0) {
		clist_rpop(cl);
	}
	jfree(cl);
}
