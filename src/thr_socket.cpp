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
	printf("%s\n", key);
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
	tab = open_table(io);
	printf("%p\n", tab);
	rs = thrs_parse_fields(tab, (cstr)io->argv[3]->priv, seq, sizeof(seq));
	if(rs == -1) {
		reply_cstr(io, (cstr)shared.err->priv);
		return;
	}
	val = (cstr)io->argv[4]->priv;
	fields = cstr_split(val, cstr_used(val), ",", 1, &size);
	thrs_insert_inner(tab, fields, size, seq, rs);
	for(i = 0; i < size; i++) {
		cstr_destroy(fields[i]);
	}
	jfree(fields);
	reply_cstr(io, (cstr)shared.ok->priv);
}

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

void* thr_priv_init(void *p) {
	thread_priv *thr_priv = (thread_priv*)jmalloc(sizeof(thread_priv));
	thr_priv->opentabs = dict_create(&opentabs_opts);
	thr_priv->thd = thd_create(my_strdup("TDR_SOCKET",MYF(0)), p, 1);
	return thr_priv;
}

void* thr_priv_uninit(void *p) {
	dict_entry *entry = NULL;
	thread_priv *thr_priv = (thread_priv *)p;
	thd_destroy(thr_priv->thd);
	dict_destroy(thr_priv->opentabs);
	return NULL;
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
