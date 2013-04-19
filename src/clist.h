#ifndef CLIST_H
#define CLIST_H

#include "common.h"

typedef struct clist_item {
	struct clist_item *prev;
	struct clist_item *next;
	void *data;
} clist_item;

typedef struct {
	clist_item *head;
	size_t count;
} clist;

clist *clist_create();
void clist_destroy(clist *cl);
void *clist_rpop(clist *cl);
void clist_lpush(clist *cl, void *data);
void *clist_lpop(clist *cl);
void clist_rpush(clist *cl, void *data);

//return removed count.
int clist_walk_remove(clist *cl, int (*cb)(void *, void *priv), void *priv);

CINLINE size_t clist_len(clist *cl) {
	return cl->count;
}

#endif /* end define common list **/
