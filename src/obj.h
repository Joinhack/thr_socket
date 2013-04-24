#ifndef OBJECT_H
#define OBJECT_H

#include "lock.h"
#include "dict.h"

enum obj_types {
	OBJ_TYPE_STR = 0,
	OBJ_TYPE_DICT
};

typedef struct obj {
	int ref;
	int type;
	void *priv;
	LOCK_T lock;
} obj;

#ifdef __cplusplus
extern "C" {
#endif

obj* obj_create(int type, void *priv);

int obj_incr(obj *o);

int obj_decr(obj *o);

obj* dict_obj_create(dict_opts *opts);

obj* cstr_obj_create(const char *s);

#ifdef __cplusplus
}
#endif

#define OBJ_LOCK(o) LOCK(&o->lock)

#define OBJ_UNLOCK(o) UNLOCK(&o->lock)

#endif /*end object head*/
