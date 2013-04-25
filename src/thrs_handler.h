#ifndef THRS_HANDLER_H
#define THRS_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif


TABLE* thrs_open_table(THD * thd, cstr req_db, cstr req_table, const int writeable);


#ifdef __cplusplus
}
#endif
#endif
