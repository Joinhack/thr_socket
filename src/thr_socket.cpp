#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
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
	thread_priv *thr_priv = (thread_priv*)io->thr_priv;
	THD *thd = thr_priv->thd;
	cstr db = (cstr)io->argv[1]->priv;
	cstr tab = (cstr)io->argv[2]->priv;
	cstr tab_key = get_table_key(db, tab);
	TABLE *table = NULL;
	dict_entry *entry = NULL;
	entry = dict_find(thr_priv->opentabs, tab_key);
	if(entry != NULL)
		table = (TABLE*)entry->value;
	if(table == NULL) {
		table = thrs_open_table(thd, db, tab, 1);
		if(table != NULL)
			dict_add(thr_priv->opentabs, tab_key, table);
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
	int rs;
	cstr *fields;
	size_t size, i;
	cstr val;
	TABLE *tab = NULL;
	MYSQL_LOCK *lock;
	thread_priv *thr_priv = (thread_priv*)io->thr_priv;
	tab = open_table(io);
	if(tab == NULL) {
		reply_cstr(io, (cstr)shared.err->priv);
		return;
	}
	rs = thrs_parse_fields(tab, (cstr)io->argv[3]->priv, seq, sizeof(seq));
	if(rs == -1) {
		reply_cstr(io, (cstr)shared.err->priv);
		thrs_close_table(thr_priv);
		return;
	}
	val = (cstr)io->argv[4]->priv;
	fields = cstr_split(val, cstr_used(val), ",", 1, &size);
	lock = thrs_lock_tables(thr_priv, 1);
	rs = thrs_insert_inner(tab, fields, size, seq, rs);
	thrs_unlock_table(thr_priv, lock, 1, rs);
	thrs_close_table(thr_priv);
	for(i = 0; i < size; i++) {
		cstr_destroy(fields[i]);
	}
	jfree(fields);
	reply_cstr(io, (cstr)shared.ok->priv);
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
