#ifndef FORMAT_H
#define FORMAT_H
#include <stdbool.h>
#include <buffer.h>
#include <fmt.h>
#include <stralloc.h>
#include "formats/text.h"
#include "formats/html.h"

#define DECSEP ","
#define CURSYM "€"
#define FMT_DATE(__o,__d,__m) do { stralloc_catulong0(&__o,__d,2); stralloc_append(&__o,"."); stralloc_catulong0(&__o,__m,2); stralloc_append(&__o,"."); } while(0);
#define FMT_TIME(__o,__h,__m) do { stralloc_catulong0(&__o,__h,2); stralloc_append(&__o,":"); stralloc_catulong0(&__o,__m,2); } while(0);
#define FMT_IND_HOURS(__o,__d) do { stralloc_catlong0(&__o,__d/100,2); stralloc_append(&__o,DECSEP); stralloc_catulong0(&__o,((__d<0)?-1:1)*__d%100,2); stralloc_append(&__o,"h"); } while(0);
#define FMT_PRICE(__o,__p) do { stralloc_catlong(&__o,__p/100); stralloc_append(&__o,DECSEP); stralloc_catulong0(&__o,__p%100,2); stralloc_append(&__o,CURSYM); } while(0);

extern struct stralloc output_line_sa;
extern struct timeslotinfo tsi;

struct formats {
    char *name;
    void (*header)();
    void (*timeline)();
    void (*footer)();
};

struct timeslotinfo {
    char *user;
    bool userlimit;
    char *project;
    bool projectlimit;
    short mday;
    short mon;
    short year;
    short shour;
    short smin;
    short ehour;
    short emin;
    bool allyear;
    bool onsite;
    // vacation
    short vmonth;
    short vyear;
    short vleft;
    // ch: centihours -> to represent industry hours without using float
    long workhours_ch;
    long worksum_onsite_ch;
    long worksum_remote_ch;
    long worktbd_ch;
    short centihourlyrate_onsite;
    short centihourlyrate_remote;
};
#endif
