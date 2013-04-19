#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "mysql_inc.h"
#include "thr_socket.h"

static int threads = 10;

static int port = 9999;

void pong(cio *io);

struct shared_obj {
	obj *err;
	obj *pong;
	obj *ok;
	obj *cmd;
	obj *nullbulk;
};

static struct shared_obj shared;

typedef void (*cmd_call)(cio *io);

struct command {
	char *name;
	cmd_call call;
	int argc;
};

//use the lower-case for command
struct command commands[] = {
	{"ping", pong, 1}
};


void shared_obj_create() {
	shared.err = cstr_obj_create("-ERR\r\n");
	shared.pong = cstr_obj_create("+PONG\r\n");
	shared.ok = cstr_obj_create("+OK\r\n");
	shared.nullbulk = cstr_obj_create("$-1\r\n");
}

void pong(cio *io) {
	reply_cstr(io, (cstr)shared.pong->priv);
}

void command_key_destroy(void *c) {
	cstr s = (cstr)c;
	cstr_destroy(s);
}

static unsigned int hash(const void *key) {
	cstr cs = (cstr)key;
	return dict_generic_hash(cs, cstr_used(cs));
}

int command_key_compare(const void *k1, const void *k2) {
	size_t len;
	cstr s1 = (cstr)k1;
	cstr s2 = (cstr)k2;
	len = cstr_used(s1);
	if(len != cstr_used(s2))
		return 0;
	return memcmp(k1, k2, len) == 0;
}

dict_opts command_opts = {
	.hash = hash,
	NULL,
	NULL,
	command_key_compare,
	command_key_destroy,
	NULL,
};

int process_commond(cio *io) {
	char buf[1024];
	thr_socket_svr *svr = (thr_socket_svr*)io->priv;
	dict_entry *cmd_entry;
	struct command *cmd;
	if(strcasecmp((cstr)io->argv[0]->priv, "quit") == 0) {
		io->flag |= IOF_CLOSE_AFTER_WRITE;
		reply_obj(io, shared.ok);
		return 0;
	}
	memset(buf, 0, sizeof(buf));
	cstr_tolower((cstr)io->argv[0]->priv);
	cmd_entry = dict_find(svr->commands, (cstr)io->argv[0]->priv);
	memset(buf, 0, sizeof(buf));
	if(cmd_entry != NULL) {
		cmd = (struct command*)cmd_entry->value;
		if(cmd->argc >= 0) {
			if(cmd->argc != -1 && cmd->argc != io->argc) {
				snprintf(buf,sizeof(buf), "-ERR wrong number arguments for command '%s'\r\n", (cstr)io->argv[0]->priv);
				return reply_str(io, buf);
			}
		}
		cmd->call(io);
		return 0;
	}
	snprintf(buf,sizeof(buf), "-ERR unknown command '%s'\r\n", (cstr)io->argv[0]->priv);
	return reply_str(io, buf);
}

int create_tcp_server() {
	int fd;
	char buff[1024];
	fd = cnet_tcp_server("0.0.0.0", port, buff, sizeof(buff));
	if(fd < 0) {
		ERROR("%s", buff);
		return -1;
	}
	cio_set_noblock(fd);
	return fd;
}

static void destroy_thr_server(thr_socket_svr *svr) {
	if(svr->in_fd > 0)
		close(svr->in_fd);
	if(svr->logfd > 0)
		close(svr->logfd);
	if(svr->thr_pool)
		cthr_pool_destroy(svr->thr_pool);
	if(svr->evts)
		cevents_destroy(svr->evts);
	if(svr->commands)
		dict_destroy(svr->commands);
	jfree(svr);
}

static void regist_commands(thr_socket_svr *svr) {
	size_t i;
	struct command *cmd;
	for(i = 0; i < sizeof(commands)/sizeof(struct command); i++) {
		cmd = &commands[i];
		dict_add(svr->commands, cstr_new(cmd->name, strlen(cmd->name)), cmd);
	}
}

static thr_socket_svr *create_thr_server() {
	thr_socket_svr *svr;
	
	shared_obj_create();
	svr = jmalloc(sizeof(thr_socket_svr));
	memset(svr, 0, sizeof(thr_socket_svr));
	
	svr->logfd = fileno(stdout);
	log_init(svr->logfd);

	svr->commands = dict_create(&command_opts);
	regist_commands(svr);

	//TODO: set size from config
	svr->thr_pool = cthr_pool_create(threads);
	if(svr->thr_pool == NULL) {
		destroy_thr_server(svr);
		return NULL;
	}
	svr->evts = cevents_create();
	INFO("server used %s for event\n", svr->evts->impl_name);
	svr->last_info_time = 0;

	svr->in_fd = create_tcp_server();
	if(svr->in_fd <= 0) {
		destroy_thr_server(svr);
		return NULL;
	}
	svr->running = 0;
	return svr;
}

void *process_event(void *priv) {
	int rs;
	cevents *cevts = (cevents*)priv;
	cevent_fired fired;
	cevent *evt;
	while(1) {
		if(cevents_pop_fired(cevts, &fired) == 0)
			return NULL;
		evt = cevts->events + fired.fd;
		if(fired.mask & CEV_PERSIST) {
			cio *io = (cio*)evt->priv;
			io->mask = fired.mask;
			process_commond(io);
		} else {
			if(fired.mask & CEV_READ) {
				evt->read_proc(cevts, fired.fd, evt->priv, fired.mask);
			}
			if(fired.mask & CEV_WRITE) {
				evt->write_proc(cevts, fired.fd, evt->priv, fired.mask);
			}
		}
	}
	return NULL;
}


void *mainLoop(void *p) {
	thr_socket_svr *svr = (thr_socket_svr*)p;
	int ev_num, i, ret;
	svr->running = 1;
	while(svr->running) {
		ev_num = cevents_poll(svr->evts, 10);
		if(ev_num > 0) {
			//if connections less than limited, use the main thread process or use multi thread process. I think this value should from config.
			if(((int)svr->connections) > 10) {
				for(i = 0; i < ev_num; i++) {
					//all threads is working.
					if(cthr_pool_run_task(svr->thr_pool, process_event, svr->evts) == -1) {
						break;
					}
				}
			} else {
				process_event(svr->evts);
			}
		}
		if(svr->last_info_time + 2 <= svr->evts->poll_sec) {
				svr->last_info_time = svr->evts->poll_sec;
				INFO("total connections:%lu, memory used:%lu\n", svr->connections, used_mem());
		}
	}
	return NULL;
}

int server_init(thr_socket_svr *svr) {
	svr->connections = 0;
	cevents_add_event(svr->evts, svr->in_fd, CEV_READ|CEV_PERSIST, tcp_accept_event_proc, svr);
	return 0;
}

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
		ERROR("create thread error\n");
		return 1;
	}
	INFO("thr socket started.\n");
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


static long long thrs_used_mem;

static int show_thrs_mem(void *thd, struct st_mysql_show_var *var, char *buff) {
	var->type = SHOW_LONGLONG;
	thrs_used_mem = used_mem();
	var->value = (char *) &thrs_used_mem;
	return 0;
}

static struct st_mysql_show_var thr_status_variables[] = {
	{"threads", (char*)&threads, SHOW_INT},
	{"used_mem", (char*)show_thrs_mem, SHOW_FUNC},
	{ NullS, NullS, SHOW_LONG }
};

static MYSQL_SYSVAR_INT(listen_port, port, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"thr socket listen port", NULL,NULL, 9999, 1024, 65535, 0);

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
	0
};

int _mysql_plugin_interface_version_= MYSQL_PLUGIN_INTERFACE_VERSION;
int _mysql_sizeof_struct_st_plugin_= sizeof(struct st_mysql_plugin);

struct st_mysql_plugin _mysql_plugin_declarations_[]= {
{
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
