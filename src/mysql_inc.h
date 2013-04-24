#ifndef MYSQL_INC_H
#define MYSQL_INC_H

#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H
#endif

#define MYSQL_DYNAMIC_PLUGIN
#define MYSQL_SERVER 1

#include <my_config.h>

#include <mysql_version.h>

#if MYSQL_VERSION_ID >= 50505
#include <my_pthread.h>
#include <sql_priv.h>
#include "sql_class.h"
#include "unireg.h"
#include "key.h" // key_copy()
#include <my_global.h>
#include <mysql/plugin.h>
#include <transaction.h>
#include <sql_base.h>

#else
#include "mysql_priv.h"
#endif

#undef min
#undef max

#endif
