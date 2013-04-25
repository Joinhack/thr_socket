#include <stdio.h>
#include <stdlib.h>
#include "mysql_inc.h"
#include "common.h"
#include "cstr.h"
#include "spinlock.h"
#include "log.h"
#include "thrs_handler.h"


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


