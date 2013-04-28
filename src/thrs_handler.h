#ifndef THRS_HANDLER_H
#define THRS_HANDLER_H


typedef struct {
	THD *thd;
	struct dict *opentabs;
} thread_priv;

#ifdef __cplusplus
extern "C" {
#endif

TABLE* thrs_open_table(THD *thd, cstr req_db, cstr req_table, const int writeable);

int thrs_parse_fields(TABLE *table, cstr fields, int *result, int len);

unsigned long long thrs_insert_inner(TABLE *table, cstr *fields, size_t fieldnum, int *seq, int seqlen);

MYSQL_LOCK *thrs_lock_tables(thread_priv *thr_priv, int writeable);

int thrs_unlock_table(thread_priv *thr_priv, MYSQL_LOCK* lock, int writeable, int rollback);

void thrs_close_table(thread_priv *thr_priv);
#ifdef __cplusplus
}
#endif
#endif
