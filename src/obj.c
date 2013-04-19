#include <stdlib.h>
#include <string.h>
#include "jmalloc.h"
#include "cstr.h"
#include "obj.h"


obj* obj_create(int type, void *priv) {
	obj *o = jmalloc(sizeof(struct obj));
	o->type = type;
	o->priv = priv;
	o->ref = 1;
	LOCK_INIT(&o->lock);
	return o;
}

obj* dict_obj_create(dict_opts *opts) {
	dict *d = dict_create(opts);
	return obj_create(OBJ_TYPE_DICT, d);
}

obj* cstr_obj_create(const char *s) {
	cstr cs = cstr_new(s, strlen(s));
	obj *o = obj_create(OBJ_TYPE_STR, cs);
	return o;
};

int obj_incr(obj *o) {
	int rs;
	OBJ_LOCK(o);
	o->ref++;
	OBJ_UNLOCK(o);
	return o->ref;
}

int obj_decr(obj *o) {
	OBJ_LOCK(o);
	o->ref--;
	if(o->ref == 0) {
		switch(o->type) {
		case OBJ_TYPE_DICT:
			dict_destroy((dict*)o->priv);
			break;
		case OBJ_TYPE_STR:
			cstr_destroy((cstr)o->priv);
			break;
		}
		OBJ_UNLOCK(o);
		LOCK_DESTROY(&o->lock);
		jfree(o);
		//must be return, the objecj is already released.
		return 1;
	}
	OBJ_UNLOCK(o);
	return 0;
}
