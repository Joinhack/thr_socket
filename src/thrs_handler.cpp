#include <stdio.h>
#include <stdlib.h>
#include "mysql_inc.h"
#include "common.h"
#include "clist.h"
#include "cstr.h"
#include "spinlock.h"
#include "log.h"
#include "dict.h"
#include "thrs_handler.h"
#include "jmalloc.h"
#include "thr_socket_svr.h"

static THD* thd_create(char* db, const void *stack_bottom,
		bool writeable) {
	THD *thd = NULL;
	my_thread_init();
	thd = new THD();
	if (thd == NULL) {
		my_thread_end();
		return NULL;
	}
	thd->thread_stack = (char*) stack_bottom;
	DEBUG("THRS:thread_stack = %p sizeof(THD)=%zu sizeof(mtx)=%zu \n",
			thd->thread_stack, sizeof(THD), sizeof(LOCK_thread_count));
	thd->store_globals();
	thd->system_thread = static_cast<enum_thread_type>(1 << 30UL);
	const NET v = { 0 };
	thd->net = v;
	if (writeable) {
		//for write
#if MYSQL_VERSION_ID >= 50505
		thd->variables.option_bits |= OPTION_BIN_LOG;
#else
		thd->options |= OPTION_BIN_LOG;
#endif
	}
	//for db
	safeFree(thd->db);
	thd->db = db;
	my_pthread_setspecific_ptr(THR_THD, thd);
	return thd;
}

static void thd_destroy(THD *thd) {
	my_pthread_setspecific_ptr(THR_THD, 0);
	delete thd;
	--thread_count;
	my_thread_end();
}


static void opentabs_key_destroy(void *c) {
	cstr s = (cstr)c;
	cstr_destroy(s);
}

static unsigned int hash(const void *key) {
	cstr cs = (cstr)key;
	return dict_generic_hash(cs, cstr_used(cs));
}

static int opentabs_key_compare(const void *k1, const void *k2) {
	size_t len;
	cstr s1 = (cstr)k1;
	cstr s2 = (cstr)k2;
	len = cstr_used(s1);
	if(len != cstr_used(s2))
		return 0;
	return memcmp(k1, k2, len) == 0;
}

static void* opentabs_key_dup(const void *k) {
	return cstr_dup((cstr)k);
}

dict_opts opentabs_opts = {
	hash,
	opentabs_key_dup,
	NULL,
	opentabs_key_compare,
	opentabs_key_destroy,
	NULL,
};

void* db_ctx_init(void *p) {
	db_ctx *ctx = (db_ctx*)jmalloc(sizeof(db_ctx));
	ctx->opentabs = dict_create(&opentabs_opts);
	ctx->thd = thd_create(my_strdup("TDR_SOCKET",MYF(0)), p, 1);
	return ctx;
}

void* db_ctx_uninit(void *p) {
	dict_entry *entry = NULL;
	db_ctx *ctx = (db_ctx *)p;
	thd_destroy(ctx->thd);
	dict_destroy(ctx->opentabs);
	return NULL;
}

void thrs_close_table(db_ctx *ctx) {
	close_thread_tables(ctx->thd);
#if MYSQL_VERSION_ID >= 50505
	ctx->thd->mdl_context.release_transactional_locks();
#endif
	dict_clear(ctx->opentabs);
}


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
	int rs = 0;
	fields = cstr_split((char*)fieldstr, cstr_used(fieldstr), ",", 1, &field_num);
	if(fields == NULL ) {
		TRACE("split fields error\n");
		return -1;
	}
	if(len <= field_num) {
		TRACE("so many fields\n");
		rs = -1;
		goto end;
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
			rs = -1;
			goto end;
		}
		result[i] = j;
	}
	rs = field_num;
end:
	for(i = 0; i < field_num; i++) {
		cstr_destroy(fields[i]);
	}
	jfree(fields);
  return rs;
}

unsigned long long thrs_insert_inner(TABLE *table, cstr *fields, size_t fieldnum, int *seq, int seqlen) {
	size_t i;
	handler *const hnd = table->file;
	uchar *const buf = table->record[0];
	empty_record(table);
	memset(buf, 0, table->s->null_bytes); /* clear null flags */
	TRACE("fieldnum:%d seqlen:%d\n", fieldnum, seqlen);
	const size_t n = min(fieldnum, seqlen);

  for (size_t i = 0; i < n; ++i) {
		uint32_t fn = seq[i];
		Field *const fld = table->field[fn];
		TRACE("field %s:%s\n",fld->field_name, fields[i]);
		if (fields[i] == NULL) {
			fld->set_null();
		} else {
			fld->store(fields[i], cstr_used(fields[i]), &my_charset_bin);
		}
  }
	table->next_number_field = table->found_next_number_field;
	const int r = hnd->ha_write_row(buf);
	const ulonglong insert_id = table->file->insert_id_for_cur_row;
	TRACE("ha_write_now result:%d table->next_number_field:%llu\n", r, insert_id);
	table->next_number_field = 0;
	if (r == 0 && table->found_next_number_field != 0) {
	  return insert_id;
	}
	if (r != 0) {
	  return 1;
	}
	return 0;
}

static MYSQL_LOCK *_thrs_lock_tables(THD *thd, TABLE** tables, int count, int writeable) {
	MYSQL_LOCK *lock;
	if (!writeable) {
		thd->lex->sql_command = SQLCOM_SELECT;
	}
#if MYSQL_VERSION_ID >= 50505
	lock = thd->lock = mysql_lock_tables(thd, tables, count, 0);
#else
	bool need_reopen = false;
	lock = thd->lock = mysql_lock_tables(thd, tables, count,
			MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN, &need_reopen);
#endif
	if (lock == NULL) {
		ERROR( "lock table failed! thd [%p]\n", thd);
		return NULL;
	}
	if (writeable) {
#if MYSQL_VERSION_ID >= 50505
		thd->set_current_stmt_binlog_format_row();
#else
		thd->current_stmt_binlog_row_based = 1;
#endif
	}

	DEBUG("tdhs_lock_table! table count[%d]\n", count);
	return lock;
}

MYSQL_LOCK *thrs_lock_tables(db_ctx *ctx, int writeable) {
	size_t i;
	MYSQL_LOCK *lock;
	size_t tabnum = DICT_USED(ctx->opentabs);
	TABLE **tables = (TABLE **)jmalloc(tabnum*sizeof(TABLE*));
	dict_iterator *iter = dict_get_iterator(ctx->opentabs);
	dict_entry *entry;
	i = 0;
	while((entry = dict_iterator_next(iter)) != NULL) {
		tables[i] = (TABLE*)entry->value;
	}
	dict_iterator_destroy(iter);
	lock = _thrs_lock_tables(ctx->thd, tables, tabnum, writeable);
	jfree(tables);
	return lock;
}

int thrs_unlock_table(db_ctx *ctx, MYSQL_LOCK* lock, int writeable, int rollback) {
	int rs = 0;
	THD *thd = ctx->thd;
	DEBUG("thrs_unlock_table! lock [%p]\n", *lock);
	if (lock != NULL) {
		if (writeable && DICT_USED(ctx->opentabs) > 0) {
			TABLE* tab;
			dict_iterator *iter = dict_get_iterator(ctx->opentabs);
			dict_entry *entry;
			while((entry = dict_iterator_next(iter)) != NULL) {
				tab = (TABLE*)entry->value;
				query_cache_invalidate3(thd, tab, 1);
				tab->file->ha_release_auto_increment();
			}
			dict_iterator_destroy(iter);
		}
		{
			bool suc = true;
#if MYSQL_VERSION_ID >= 50505
			if(rollback) {
				suc = trans_rollback_stmt(thd);
			} else {
				suc = (trans_commit_stmt(thd) == FALSE);
			}
#else
			suc = (ha_autocommit_or_rollback(thd, rollback) == 0);
#endif
			if (!suc) {
				WARN("commit failed, it's rollback! thd [%p] write_error [%d] \n", thd, rollback);
				rs = -1;
			}
		}
		mysql_unlock_tables(thd, lock);
		thd->lock = NULL;
	}
	return rs;
}

int thrs_open_index(TABLE *table, cstr idx) {
	const char *ptr = idx;
	long long idxnum;
	if(ptr[0] >= '0' && ptr[0] <= '9') {
		if(str2ll(idx, cstr_used(idx), &idxnum) < 0) return -1;
		return idxnum;
	}
	ptr[0] == '\0'? ptr = "PRIMARY": ptr = idx;
	for (uint i = 0; i < table->s->keys; ++i) {
		KEY& kinfo = table->key_info[i];
		if (strncmp(kinfo.name, ptr, cstr_used(idx)) == 0) {
			return idxnum;
		}
	}
	return -1;
}

size_t prepare_keybuf(cstr* keys, const uint32_t key_num, uchar *key_buf, TABLE *table, KEY& kinfo) {
	size_t key_len_sum = 0;
	for (size_t i = 0; i < key_num; i++) {
		const KEY_PART_INFO & kpt = kinfo.key_part[i];
		const cstr key = keys[i];
		if (cstr_used(key) == 0) {
			kpt.field->set_null();
		} else {
			kpt.field->set_notnull();
			kpt.field->store(key, cstr_used(key), &my_charset_bin);
		}
		key_len_sum += kpt.store_length;
		TRACE("key len=%u store len=%zu\n", kpt.length, kpt.store_length);
	}
	key_copy(key_buf, table->record[0], &kinfo, key_len_sum);
	TRACE("keys sum=%zu flen=%u\n", key_len_sum, kinfo.key_length);
	return key_len_sum;
}

