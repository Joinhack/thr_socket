#include <stdio.h>
#include <stdlib.h>
#include "mysql_inc.h"
#include "common.h"
#include "clist.h"
#include "cstr.h"
#include "spinlock.h"
#include "log.h"
#include "thrs_handler.h"
#include "jmalloc.h"

#define min(a, b) (a<b?a:b)
TABLE* thrs_open_table(THD * thd, cstr req_db, cstr req_table, const int writeable) {
	int refresh = 1;
	TABLE_LIST tables;
	TABLE *table = NULL;
#if MYSQL_VERSION_ID >= 50505
	tables.init_one_table(req_db, cstr_used(req_db),
			req_table, cstr_used(req_table),
			req_table, writeable ? TL_WRITE : TL_READ);
	tables.mdl_request.init(MDL_key::TABLE, req_db, req_table,
			writeable ? MDL_SHARED_WRITE : MDL_SHARED_READ, MDL_TRANSACTION);
	Open_table_context ot_act(thd, 0);
	if (!open_table(thd, &tables, thd->mem_root, &ot_act)) {
		table = tables.table;
	}
#else
	tables.init_one_table(req_db, req_table, writeable ? TL_WRITE : TL_READ);
	table = open_table(thd, &tables, thd->mem_root, &refresh, OPEN_VIEW_NO_PARSE);
#endif
	if (table == NULL) {
		ERROR("can't open table, db:%s table%s\n", req_db, req_table);
		return NULL;
	}
	table->reginfo.lock_type = writeable ? TL_WRITE : TL_READ;
	table->use_all_columns();
	return table;
}


int thrs_parse_fields(TABLE *table, cstr fieldstr, int *result, int len) {
	cstr *fields = NULL;
	size_t field_num;
	size_t i;
	fields = cstr_split((char*)fieldstr, cstr_used(fieldstr), ",", 1, &field_num);
	if(fields == NULL ) {
		TRACE("split fields error\n");
		return -1;
	}
	if(len <= field_num) {
		TRACE("so many fields\n");
		jfree(fields);
		return -1;
	}
	for (i = 0; i < field_num; i++) {
		Field **fld = 0;
		size_t j = 0;
		for (fld = table->field; *fld; fld++, j++) {
			if (memcmp((*fld)->field_name, fields[i], cstr_used(fields[i])) == 0) {
				break;
			}
		}
		if (*fld == 0) {
			TRACE("UNKNOWN FLD %s\n", fields[i]);
			jfree(fields);
			return -1;
		}
		result[i] = j;
	}
	jfree(fields);
  return 0;
}

unsigned long long thrs_insert_inner(TABLE *table, cstr *fields, size_t fieldnum, int *seq, int seqlen) {
	size_t i;
	handler *const hnd = table->file;
	uchar *const buf = table->record[0];
	empty_record(table);
	memset(buf, 0, table->s->null_bytes); /* clear null flags */
	const size_t n = min(fieldnum, seqlen);
  for (size_t i = 0; i < n; ++i) {
		uint32_t fn = seq[i];
		Field *const fld = table->field[fn];
		if (fields[i] == NULL) {
			fld->set_null();
		} else {
			fld->store(fields[i], cstr_used(fields[i]), &my_charset_bin);
		}
  }
	table->next_number_field = table->found_next_number_field;
	const int r = hnd->ha_write_row(buf);
	const ulonglong insert_id = table->file->insert_id_for_cur_row;
	table->next_number_field = 0;
	if (r == 0 && table->found_next_number_field != 0) {
	  return insert_id;
	}
	if (r != 0) {
	  return 1;
	}
	return 0;
}
