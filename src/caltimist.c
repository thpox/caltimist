// SPDX-License-Identifier: GPL-2.0-only
/*
 * caltimist - calculates project-/worktime and vacation using iCalendar data
 * Copyright (C) 2023 Thomas Pöhnitzsch <thpo+caltimist@dotrc.de>
 */

#include <string.h>
#include <unistd.h>
#include <buffer.h>
#include <scan.h>
#include <str.h>
#include <errmsg.h>
#include "httpsclient.h"
#include "ics.h"

#define V(__l,__fn) do{if(verbosity>=__l){ __fn; }}while(0);
short verbosity=0;

char *PROGNAME;

static void show_help()
{
    buffer_puts(buffer_1,PROGNAME);
    buffer_puts(buffer_1,"\n");
    buffer_puts(buffer_1,"\t-y [>1969]\tyear\n");
    buffer_puts(buffer_1,"\t-m [1-12]\tmonth\n");
    buffer_puts(buffer_1,"\t-u [user]\tuser\n");
    buffer_puts(buffer_1,"\t-p [project]\tproject\n");
    buffer_puts(buffer_1,"\t-o [text|]\toutput format\n");
    buffer_puts(buffer_1,"\t-v\tverbosity\n");
    buffer_puts(buffer_1,"\t-[UP]\tshow user or project list and exit\n");
    buffer_puts(buffer_1,"\t-h\thelp");
    buffer_putnlflush(buffer_1);
}

static int cgi_auth_user(char **user)
{
    *user=getenv("REMOTE_USER");
    if (!*user) {
        carp("CGI has to have REMOTE_USER set");
        return -1;
    }
    return 0;
}

static int handle_request(char **request)
{
    char *method=getenv("REQUEST_METHOD");
    char *contentlength,*t;
    unsigned short l;
    size_t rcnt=0;

    if (!method) {
        return -1;
    } else if (str_equal(method, "GET")) {
        *request=getenv("QUERY_STRING");
        if (!*request) return -1;
    } else if (str_equal(method, "POST")) {
        contentlength=getenv("CONTENT_LENGTH");
        if (!contentlength) return -1;
        scan_ushort(contentlength, &l);
        if ((l <= 0) || !(t=calloc(l, sizeof(char)+1)))
            return -1;
        memset(t,0, sizeof(char)+1);
        *request=t;
        while(l) {
            rcnt=read( 0, t, l);
            if ( (0>=rcnt) || (rcnt>l) ){
                carpsys("read");
                free(*request);
                return -1;
            }
            l-=rcnt;
            t+=rcnt;
        }
        t='\0';
    } else return -1;

    buffer_puts(buffer_1,"Content-Type:text/html;charset=iso-8859-1\n");
    buffer_putnlflush(buffer_1);
    return str_len(*request);
}

static int validate_args( struct program_args *pa )
{
    if ( (-1 > pa->month) || (12 < pa->month) ||
            ((-1 != pa->year) && (1970 > pa->year)) ||
            (pa->show_project && pa->show_user) )
        return -1;
    if ( (pa->year>0) && (pa->month==-1) ) pa->month=0;
    return 0;
}

static int parse_query_string(struct program_args *pa, char *request, const short l)
{
    size_t cur=0,sep=0;

    while (cur<=l) {
        sep=str_chr(request+cur, '&');
        request[sep]='\0';
        if ( (request[cur] == 'y') && (request[cur+1] == '=') )
            scan_short( request+cur+2, &(pa->year));
        else if ( (request[cur] == 'm') && (request[cur+1] == '=') )
            scan_short( request+cur+2, &(pa->month));
        cur+=sep+1;
    }
    return 0;
}

static void free_cfgctx( struct config_context *c )
{
    struct user_context *u, *tu;
    struct project_context *p, *tp;

    if (c->general.user) free(c->general.user);
    if (c->general.password) free(c->general.password);
    if (c->general.public_holidays) free(c->general.public_holidays);
    for (u=c->first_user; u;) {
        if (u->name) free(u->name);
        if (u->cal) free(u->cal);
        tu=u->next_user;
        free(u);
        u=tu;
    }
    for (p=c->first_project; p;) {
        if (p->name) free(p->name);
        tp=p->next_project;
        free(p);
        p=tp;
    }
}

int main( int argc, char *argv[], char *envp[] )
{
    int ret=EXIT_SUCCESS, o;
    size_t pnlen;

    struct user_context *ucntx;
    struct project_context *pcntx;
    struct config_context cfgctx;
    memset( &cfgctx,0, sizeof(struct config_context));

    cfgctx.prog_arg.year=-1;
    cfgctx.prog_arg.month=-1;
    cfgctx.prog_arg.user = NULL;
    cfgctx.prog_arg.project = NULL;
    cfgctx.prog_arg.format = NULL;
    cfgctx.prog_arg.show_user=false;
    cfgctx.prog_arg.show_project=false;

    PROGNAME = argv[0];
#if 0
    for ( pnlen=0; envp[pnlen]; ++pnlen ) {
        carp("ENV ",envp[pnlen]);
    }
#endif

    pnlen = str_len(PROGNAME);
    if ((pnlen > 4) &&
        (PROGNAME[pnlen-4] == '.') &&
        (PROGNAME[pnlen-3] == 'c') &&
        (PROGNAME[pnlen-2] == 'g') &&
        (PROGNAME[pnlen-1] == 'i')) {
        char *request;
        short total;
        if ( cgi_auth_user(&(cfgctx.prog_arg.user)) ||
            (0>(total=handle_request(&request))) )
            exit(EXIT_FAILURE);
        cfgctx.prog_arg.format="html";
        parse_query_string(&cfgctx.prog_arg, request,total);
    }

    while ( ( o = getopt(argc, argv, "y:m:u:p:o:vUPh")) !=-1 ) {
        switch(o) {
        case 'y':
            scan_short(optarg,&(cfgctx.prog_arg.year));
            break;
        case 'm':
            scan_short(optarg,&(cfgctx.prog_arg.month));
            break;
        case 'u':
            cfgctx.prog_arg.user = optarg;
            break;
        case 'p':
            cfgctx.prog_arg.project = optarg;
            break;
        case 'o':
            cfgctx.prog_arg.format = optarg;
            break;
        case 'v':
            verbosity++;
            break;
        case 'U':
            cfgctx.prog_arg.show_user=true;
            break;
        case 'P':
            cfgctx.prog_arg.show_project=true;
            break;
        default:
            ret=EXIT_FAILURE;
        case 'h':
            show_help();
            exit(ret);
        }
    }

    set_config_verbosity(verbosity);
    set_ics_verbosity(verbosity);
    set_httpsclient_verbosity(verbosity);

    if ( validate_args(&(cfgctx.prog_arg)) ||
        parse_config(&cfgctx) )
        die(EXIT_FAILURE,"arguments or config invalid");

    if (cfgctx.prog_arg.show_project) {
        for_each_project(&cfgctx, pcntx) {
            buffer_puts(buffer_1,pcntx->name);
            buffer_putnlflush(buffer_1);
        }
        exit(EXIT_SUCCESS);
    }

    if (cfgctx.prog_arg.show_user) {
        for_each_user(&cfgctx, ucntx) {
            buffer_puts(buffer_1,ucntx->name);
            buffer_putnlflush(buffer_1);
        }
        exit(EXIT_SUCCESS);
    }

    init_holiday_list(cfgctx.prog_arg.year);
    if ( cfgctx.general.public_holidays )
        if ( fetch_calendar( NULL, cfgctx.general.public_holidays, &(cfgctx.general), ics_parser ) ) {
            free_cfgctx(&cfgctx);
            die(EXIT_FAILURE,"failed to fetch public holiday calendar");
        }

    for_each_user(&cfgctx, ucntx) {
        if (cfgctx.prog_arg.user && !str_equal(ucntx->name,cfgctx.prog_arg.user))
            continue;

        if ( fetch_calendar( ucntx->name, ucntx->cal, &(cfgctx.general), ics_parser ) ) {
            free_cfgctx(&cfgctx);
            die(EXIT_FAILURE,"failed to fetch user calendar(s)");
        }
    }

    if ( cfgctx.prog_arg.project )
        filter_project_calentries( cfgctx.prog_arg.project );

    if ( cal_statistics(&cfgctx) ) {
        free_cfgctx(&cfgctx);
        die(EXIT_FAILURE,"issue while printing calendar statistics");
    }

    free_cfgctx(&cfgctx);
    exit(EXIT_SUCCESS);
}
