#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "mysql_inc.h"
#include "thr_socket_svr.h"

static thr_socket_svr *svr = NULL;

static pthread_t mainloop_thrid = 0;

static int thr_socket_plugin_init(void *p) {
	pthread_attr_t  thr_attr;
	if(pthread_attr_init(&thr_attr) < 0) {
		fprintf(stderr, "init thread attr error\n");
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
	{"threads", (char*)&threads, SHOW_INT},
	{"used_mem", (char*)show_thrs_mem, SHOW_FUNC},
	{ NullS, NullS, SHOW_LONG }
};

static MYSQL_SYSVAR_INT(listen_port, port, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"thr socket listen port", NULL,NULL, 8889, 1024, 65535, 0);

static MYSQL_SYSVAR_INT(backend_thread_num, threads, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
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
