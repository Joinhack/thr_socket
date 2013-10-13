#ifndef THRS_HANDLER_H
#define THRS_HANDLER_H


typedef struct {
	THD *thd;
	struct dict *opentabs;
} db_ctx;

#ifdef __cplusplus
extern "C" {
#endif

TABLE* thrs_open_table(THD *thd, cstr req_db, cstr req_table, const int writeable);

int thrs_parse_fields(TABLE *table, cstr fields, int *result, int len);

unsigned long long thrs_insert_inner(TABLE *table, cstr *fields, size_t fieldnum, int *seq, int seqlen);

int thrs_open_index(TABLE *table, cstr field);

MYSQL_LOCK *thrs_lock_tables(db_ctx *ctx, int writeable);

int thrs_unlock_table(db_ctx *ctx, MYSQL_LOCK* lock, int writeable, int rollback);

void thrs_close_table(db_ctx *ctx);

size_t prepare_keybuf(cstr* keys, const uint32_t key_num, uchar *key_buf, TABLE *table, KEY& kinfo);
#ifdef __cplusplus
}
#endif
#endif
