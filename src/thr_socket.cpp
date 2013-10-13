#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "mysql_inc.h"
#include "thr_socket_svr.h"
#include "thrs_handler.h"

void pong(cio *io) {
	reply_cstr(io, (cstr)shared.pong->priv);
}

static inline cstr get_table_key(cstr db, cstr tab) {
	cstr key = cstr_dup(db);
	key = cstr_ncat(key, ".", 1);
	key = cstr_ncat(key, tab, cstr_used(tab));
	return key;
}

static TABLE* open_table(cio* io) {
	thr_socket_svr *svr = (thr_socket_svr*)io->priv;
	db_ctx *ctx = (db_ctx*)io->thr_priv;
	THD *thd = ctx->thd;
	cstr db = (cstr)io->argv[1]->priv;
	cstr tab = (cstr)io->argv[2]->priv;
	cstr tab_key = get_table_key(db, tab);
	TABLE *table = NULL;
	dict_entry *entry = NULL;
	entry = dict_find(ctx->opentabs, tab_key);
	if(entry != NULL)
		table = (TABLE*)entry->value;
	if(table == NULL) {
		table = thrs_open_table(thd, db, tab, 1);
		if(table != NULL)
			dict_add(ctx->opentabs, tab_key, table);
	}
	cstr_destroy(tab_key);
	return table;
}

void open_table_command(cio* io) {
	TABLE *table = NULL;
	table = open_table(io);
	if(table != NULL)
		reply_cstr(io, (cstr)shared.ok->priv);
	else
		reply_cstr(io, (cstr)shared.err->priv);
}

void insert_command(cio *io) {
	int seq[1024];
	int rs, filenum;
	cstr *fields;
	size_t size, i, j;
	cstr val;
	TABLE *tab = NULL;
	MYSQL_LOCK *lock;
	db_ctx *ctx = (db_ctx*)io->thr_priv;
	tab = open_table(io);
	if(tab == NULL) {
		reply_cstr(io, (cstr)shared.err->priv);
		return;
	}
	filenum = thrs_parse_fields(tab, (cstr)io->argv[3]->priv, seq, sizeof(seq));
	if(rs == -1) {
		reply_cstr(io, (cstr)shared.err->priv);
		thrs_close_table(ctx);
		return;
	}
	lock = thrs_lock_tables(ctx, 1);
	for(j = 4; j < io->argc; j++) {
		val = (cstr)io->argv[j]->priv;
		fields = cstr_split(val, cstr_used(val), ",", 1, &size);
		rs = thrs_insert_inner(tab, fields, size, seq, filenum);
		for(i = 0; i < size; i++) {
			cstr_destroy(fields[i]);
		}
		jfree(fields);
		//if thrs_insert_inner return not 0, will be rollback;
		if(rs)
			break;
	}
	thrs_unlock_table(ctx, lock, 1, rs);
	thrs_close_table(ctx);
	reply_len(io, rs);
}

void get_command(cio *io) {
	int seq[1024];
	db_ctx *ctx = (db_ctx*)io->thr_priv;
	TABLE *tab;
	tab = open_table(io);
	MYSQL_LOCK *lock;
	int rs;
	int idx;
	if(tab == NULL) {
		reply_cstr(io, (cstr)shared.err->priv);
		return;
	}
	idx = thrs_open_index(tab, (cstr)io->argv[4]->priv);
	if(idx < 0) {
		DEBUG("error open idx\n");
		reply_cstr(io, (cstr)shared.err->priv);
		return;
	}
	lock = thrs_lock_tables(ctx, 0);
  KEY& kinfo = tab->key_info[idx];
  uchar *const key_buf = (uchar*)jmalloc(sizeof(uchar) * kinfo.key_length);
  size_t kplen_sum = prepare_keybuf((cstr*)&io->argv[6]->priv, 1, key_buf, tab, kinfo);
  tab->read_set = &tab->s->all_set;
  handler *const hnd = tab->file;
  hnd->init_table_handle_for_HANDLER();
  hnd->ha_index_or_rnd_end();
  hnd->ha_index_init(idx, 1);
  const key_part_map kpm = (1U << cstr_used((cstr)io->argv[6]->priv)) - 1;
  rs = hnd->index_read_map(tab->record[0], key_buf, kpm, HA_READ_KEY_EXACT);
  rs = hnd->index_next_same(tab->record[0], key_buf, kplen_sum);
  TRACE("rs %d HA_ERR_RECORD_DELETED:%d HA_ERR_KEY_NOT_FOUND:%d, HA_ERR_END_OF_FILE:%d\n", rs, HA_ERR_RECORD_DELETED, HA_ERR_KEY_NOT_FOUND, HA_ERR_END_OF_FILE);
  
  if(rs != 0 && rs != HA_ERR_RECORD_DELETED && rs != HA_ERR_KEY_NOT_FOUND
				&& rs != HA_ERR_END_OF_FILE) {

  } else {
  	rs = thrs_parse_fields(tab, (cstr)io->argv[3]->priv, seq, sizeof(seq));
	  for (size_t i = 0; i < rs; ++i) {
	  	char rwpstr_buf[64];
	  	String rwpstr(rwpstr_buf, sizeof(rwpstr_buf), &my_charset_bin);
	    int fn = seq[i];
	    Field *const fld = tab->field[fn];
			TRACE("fld=%p %zu\n", fld, fn);
			if (fld->is_null()) {
			/* null */
			} else {
				fld->val_str(&rwpstr, &rwpstr);
				const size_t len = rwpstr.length();
				if (len != 0) {
					/* non-empty */
					const std::string s(rwpstr.ptr(), rwpstr.length());
					TRACE("buf %s\n", s.c_str());
				} else {
				/* empty */
					static const char empty_str[] = "";
				}
			}
		}
	}
	hnd->ha_index_or_rnd_end();
  jfree(key_buf);
	thrs_unlock_table(ctx, lock, 0, 0);
	thrs_close_table(ctx);
	reply_cstr(io, (cstr)shared.err->priv);
}

static thr_socket_svr *svr = NULL;

static pthread_t mainloop_thrid = 0;

static int thr_socket_plugin_init(void *p) {
	pthread_attr_t  thr_attr;
	if(pthread_attr_init(&thr_attr) < 0) {
		ERROR("init thread attr error\n");
		return 1;
	}
	svr = create_thr_server();
	if(svr == NULL)
		return 1;
	server_init(svr);
	if(pthread_create(&mainloop_thrid, &thr_attr, mainLoop, svr) < 0) {
		destroy_thr_server(svr);
		svr = NULL;
		return 1;
	}
	return 0;
}

static int thr_socket_plugin_deinit(void *p) {
	void *rs;
	INFO("thr socket server exit\n");
	if(svr == NULL)
		return 1;
	if(svr->running) {
		svr->running = 0;
		pthread_join(mainloop_thrid, &rs);
	}
	destroy_thr_server(svr);
	svr = NULL;
	return 0;	
}

static long long _thrs_used_mem;

static int show_thrs_mem(void *thd, struct st_mysql_show_var *var, char *buff) {
	var->type = SHOW_LONGLONG;
	_thrs_used_mem = thrs_used_mem();
	var->value = (char *) &_thrs_used_mem;
	return 0;
}

static struct st_mysql_show_var thr_status_variables[] = {
	{"threads", (char*)&thread_num, SHOW_INT},
	{"used_mem", (char*)show_thrs_mem, SHOW_FUNC},
	{ NullS, NullS, SHOW_LONG }
};

static MYSQL_SYSVAR_INT(listen_port, port, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"thr socket listen port", NULL,NULL, 8889, 1024, 65535, 0);

static MYSQL_SYSVAR_INT(backend_thread_num, thread_num, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"thr socket backend thread nnum", NULL,NULL, 10, 1, 1024, 0);

static int show_thrs_vars(void *thd, struct st_mysql_show_var *var, char *buff) {
	var->type = SHOW_ARRAY;
	var->value = (char *) &thr_status_variables;
	return 0;
}

struct st_mysql_daemon thr_socket_plugin = { MYSQL_DAEMON_INTERFACE_VERSION };

static struct st_mysql_show_var daemon_thr_socket_status_variables[] = { { "thrs",
		(char*) show_thrs_vars, SHOW_FUNC }, { NullS, NullS, SHOW_LONG } };

static struct st_mysql_sys_var *daemon_thr_socket_system_variables[] = {
	MYSQL_SYSVAR(listen_port),
	MYSQL_SYSVAR(backend_thread_num),
	0
};

int _mysql_plugin_interface_version_= MYSQL_PLUGIN_INTERFACE_VERSION;
int _mysql_sizeof_struct_st_plugin_= sizeof(struct st_mysql_plugin);

struct st_mysql_plugin _mysql_plugin_declarations_[] = {{
	MYSQL_DAEMON_PLUGIN,
	&thr_socket_plugin,
	"thr_socket",
	"joinhack@gmail.com",
	"proxy the handler",
	PLUGIN_LICENSE_GPL, 
	thr_socket_plugin_init, /* Plugin Init */
	thr_socket_plugin_deinit, /* Plugin Deinit */
	0x0005, 
	daemon_thr_socket_status_variables, /* status variables*/
	daemon_thr_socket_system_variables, /* system variables*/
	NULL /* config options*/
}, 0};
