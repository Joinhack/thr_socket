#ifndef THRS_HANDLER_H
#define THRS_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif


TABLE* thrs_open_table(THD *thd, cstr req_db, cstr req_table, const int writeable);

int thrs_parse_fields(TABLE *table, cstr fields, int *result, int len);

unsigned long long thrs_insert_inner(TABLE *table, cstr *fields, size_t fieldnum, int *seq, int seqlen);

#ifdef __cplusplus
}
#endif
#endif
