// SPDX-License-Identifier: GPL-2.0-only
/*
 * part of caltimist - calculates project-/worktime and vacation using iCalendar data
 * Copyright (C) 2023 Thomas Pöhnitzsch <thpo+caltimist@dotrc.de>
 */

#include "config.h"
#include <fmt.h>
#include <buffer.h>
#include <errmsg.h>
#include <str.h>
#include <ctype.h>
#include <errno.h>
#include <open.h>
#include <stralloc.h>
#include <unistd.h>
#include <limits.h>
#include <scan.h>

#define PATH_SEPARATOR '/'

#define V(__l,__fn) do{if(config_verbosity>=__l){ __fn; }}while(0);
short config_verbosity=0;

void set_config_verbosity( short v ) {
    config_verbosity=v;
}

static char* get_rc_file()
{
    char *home=NULL, *basename=NULL, *rcfile=NULL;
    size_t pos;

    if ( !(home=getenv("HOME")) ) {
        carpsys("no home found in environment");
        return NULL;
    }

    pos = str_rchr( PROGNAME, PATH_SEPARATOR );
    if ( PATH_SEPARATOR == PROGNAME[pos] )
        basename = PROGNAME+ sizeof(char)*(pos+1);
    else
        basename = PROGNAME;

    rcfile = calloc( str_len(home)+1+str_len(basename)+4, sizeof(char) );
    if (!rcfile) {
        carpsys("calloc");
        return NULL;
    } else {
        size_t i=0;
        i=fmt_str(rcfile, home);
        rcfile[i++]=PATH_SEPARATOR;
        rcfile[i++]='.';
        fmt_str(rcfile+i, basename);
        // progname.cgi == progname -> .prognamerc
        i+=str_rchr(basename, '.');
        i+=fmt_str(rcfile+i, "rc");
        rcfile[i]=0;
    }

    return rcfile;
}

static int prepare_new_user(struct config_context *cfgctx, char *name)
{
    struct user_context *user = calloc( 1, sizeof(struct user_context) );
    if (!user) {
        carpsys("calloc");
        return -1;
    }
    user->name = name;
    user->vacation = 0;
    user->monthhours = 0;
    user->next_user = NULL;
    if (!cfgctx->first_user) {
        cfgctx->first_user = user; cfgctx->last_user = user;
    } else {
        cfgctx->last_user->next_user = user; cfgctx->last_user = user;
    }
    return 0;
}

static int prepare_new_project(struct config_context *cfgctx, char *name)
{
    struct project_context *project = calloc( 1, sizeof(struct project_context) );
    if (!project) {
        carpsys("calloc");
        return -1;
    }
    project->name = name;
    project->onsite = 0;
    project->remote = 0;
    project->next_project = NULL;
    if (!cfgctx->first_project) {
        cfgctx->first_project = project; cfgctx->last_project = project;
    } else {
        cfgctx->last_project->next_project = project; cfgctx->last_project = project;
    }
    return 0;
}

static int get_float_as_centiushort( unsigned short *v, const char *floatstring ) {
    double f=0.0;

    if ( !scan_double(floatstring, &f) ) {
        carp("no float found");
        return -1;
    }
    if ( (f*100.0+.5)>USHRT_MAX) {
        carp("parsed float does not fit in ushort max");
        return -1;
    }
    *v = (unsigned short) (f*100.0+.5);

    return 0;
}

static int get_string_value( char **var, const char *strval )
{
    size_t len=str_len(strval);
    *var=calloc( len+1, sizeof(char) );
    if (!*var) {
        carpsys("calloc");
        return -1;
    }
    fmt_str(*var,strval);
    (*var)[len]=0;

    return 0;
}

static int parse_line(struct config_context *cfgctx, stralloc *sa)
{
    size_t numspace=0;
    int ret=0;
    char *line = sa->s;
    size_t len = sa->len;

    for (size_t i=0; i < len; ++i) {
        if (!isspace(line[i]))
            line[numspace++] = line[i];
    }
    line[numspace]=0;

    if (!numspace) return 0;

    if (str_equal(line, "[General]")) {
        cfgctx->active_context = (1 << GENERALCTX);
    }
    else if (str_equal(line, "[User]")) {
        cfgctx->active_context = (1 <<  USERCTX);
    }
    else if (str_equal(line, "[Projects]")) {
        cfgctx->active_context = (1 << PROJECTCTX);
    }
    else if ( (cfgctx->active_context ) &&  numspace>2 \
            && line[0]=='{' && line[numspace-1]=='}') {
        //chop the brackets
        line[numspace-1]=0;line++;

        char *name = calloc( numspace-1, sizeof(char) );
        if (!name) {
            carpsys("calloc");
            return -1;
        }
        fmt_str( name, line);

        if ( cfgctx->active_context & (1 << USERCTX) )
            if ( 0>prepare_new_user(cfgctx, name) )
                return -1;

        if ( cfgctx->active_context & (1 << PROJECTCTX) )
            if ( 0>prepare_new_project(cfgctx, name) )
                return -1;
    }
#define  if_ctx_value(__scope,__param) if ( (cfgctx->active_context & (1 << __scope)) &&  str_start(line, __param"=" ) )
    else if_ctx_value(GENERALCTX, "user") { ret=get_string_value( &(cfgctx->general.user), line+sizeof("user")); }
    else if_ctx_value(GENERALCTX, "password") { ret=get_string_value( &(cfgctx->general.password), line+sizeof("password")); }
    else if_ctx_value(GENERALCTX, "public_holidays") { ret=get_string_value( &(cfgctx->general.public_holidays), line+sizeof("public_holidays")); }
    else if_ctx_value(USERCTX, "cal") { ret=get_string_value( &(cfgctx->last_user->cal), line+sizeof("cal")); }
    else if_ctx_value(USERCTX, "vacation") { ret=(scan_ushort( line+sizeof("vacation"), &cfgctx->last_user->vacation )?0:-1); }
    else if_ctx_value(USERCTX, "monthhours") { ret=(scan_ushort( line+sizeof("monthhours"), &cfgctx->last_user->monthhours )?0:-1); }
    else if_ctx_value(PROJECTCTX, "onsite" ) { ret=get_float_as_centiushort( &cfgctx->last_project->onsite, line+sizeof("onsite") ); }
    else if_ctx_value(PROJECTCTX, "remote" ) { ret=get_float_as_centiushort( &cfgctx->last_project->remote, line+sizeof("remote") ); }
    else {
        carp("ERROR parsing line:\t", line);
    }

    return ret;
}

int parse_config( struct config_context *cfgctx )
{
    char *rcfile = NULL;
    int f=0, ret=0;
    buffer b;
    char buf[1024];
    size_t l;
    struct user_context *user;
    struct project_context *project;
    stralloc line;

    stralloc_init(&line);

    rcfile = get_rc_file();
    if (!rcfile) { ret=-1; goto cleanup; }

    f = open_read(rcfile);
    if (-1 == f) {
        carp(rcfile);
        carpsys("open_read");
        ret=-1;
        goto cleanup;
    }

    buffer_init(&b,read,f,buf,1024);
    for (;;) {
        stralloc_zero(&line);
        l=buffer_getline_sa(&b,&line);
        if (0>l)
            carpsys("buffer_getline_sa");
        if (0>=l)
            break;
        stralloc_chomp(&line);

        if (0>parse_line(cfgctx, &line)) { ret=-1; goto cleanup; }
    }

    V(1,
        carp("Config:");
        for_each_user(cfgctx, user) {
            carp("user: ",user->name);
            buffer_puts(buffer_2,"\tvacation: ");
            buffer_putlong(buffer_2,user->vacation);
            buffer_puts(buffer_2,"\thours: ");
            buffer_putlong(buffer_2,user->monthhours);
            buffer_puts(buffer_2,"\tcal: ");
            buffer_puts(buffer_2,user->cal);
            buffer_putnlflush(buffer_2);
        }

        for_each_project(cfgctx, project) {
            carp("project: ",project->name);
            buffer_puts(buffer_2,"\tonsite: ");
            buffer_putlong(buffer_2,project->onsite/100);
            buffer_puts(buffer_2,".");
            buffer_putlong(buffer_2,project->onsite%100);
            buffer_puts(buffer_2,"\tremote: ");
            buffer_putlong(buffer_2,project->remote/100);
            buffer_puts(buffer_2,".");
            buffer_putlong(buffer_2,project->remote%100);
            buffer_putnlflush(buffer_2);
        }
     );

cleanup:
    stralloc_free(&line);
    if (rcfile) free(rcfile);
    if (0 < f) close(f);
    return ret;
}

#ifdef UNITTEST
#include <assert.h>

char *PROGNAME;
int main(int argc, char *argv[])
{
    char *rcfile;
    size_t p;
    unsigned short v;

    PROGNAME = argv[0];
    rcfile = get_rc_file();
    p=str_rchr(rcfile,PATH_SEPARATOR);
    assert(str_equal(rcfile+p, "/.test_configrc"));
    assert(0==get_float_as_centiushort(&v, "0.999"));
    assert(100==v);
    assert(0==get_float_as_centiushort(&v, "12.234"));
    assert(1223==v);
    return 0;
}
#endif
