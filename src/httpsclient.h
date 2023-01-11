#ifndef HTTPSCLIENT_H
#define HTTPSCLIENT_H
#include "config.h"

void set_httpsclient_verbosity( short );
int fetch_calendar( char *, const char *, const struct general_context *, int(*)(char *,char *) );
#endif
