// SPDX-License-Identifier: GPL-2.0-only
/*
 * part of caltimist - calculates project-/worktime and vacation using iCalendar data
 * Copyright (C) 2023 Thomas Pöhnitzsch <thpo+caltimist@dotrc.de>
 */

#include <string.h>
#include <errmsg.h>
#include <str.h>
#include <scan.h>
#include "ics.h"
#include "format.h"

#define V(__l,__fn) do{if(ics_verbosity>=__l){ __fn; }}while(0);
short ics_verbosity=0;

void set_ics_verbosity( short v ) {
    ics_verbosity=v;
}

static time_t begin_year, end_year;
unsigned char workday[366];

struct calendar_context {
    char *user;
    char *subject;
    time_t start;
    //time_t pause;
    time_t end;
    bool dayevent;
    bool recurring_yearly;
    bool onsite;
    struct calendar_context *next_entry;
} *first_entry=NULL, *last_entry=NULL, *incubator=NULL;

#define for_each_calentry(__entry) for (__entry=first_entry; (__entry); (__entry)=(__entry)->next_entry)

struct glue_buffer {
    char *str;
    size_t alloc_len;
} gbuf = { .str = NULL, .alloc_len=0 };

struct stralloc output_line_sa;

struct formats format[] = {
    { "text", text_header, text_timeline, text_footer },
    { "html", html_header, html_timeline, html_footer },
};
struct formats current_format;

static int prepare_new_calentry(char *name)
{
    if (incubator) {
        carp("incubator is in use, cleaning up for new calendar entry");
        if (incubator->subject)
            free(incubator->subject);
        free(incubator);
    }

    incubator = calloc( 1, sizeof(struct calendar_context) );
    if (!incubator) {
        carpsys("calloc");
        return -1;
    }
    incubator->user = name;
    incubator->subject = NULL;
    incubator->start = 0;
    //incubator->pause = 0;
    incubator->end = 0;
    incubator->dayevent = false;
    incubator->recurring_yearly = false;
    incubator->next_entry = NULL;
    return 0;
}

static int emerge_calentry()
{
    struct calendar_context *e,*prev=first_entry;

    if (! first_entry) {
        first_entry = incubator; last_entry = incubator;
        goto done;
    }

    for_each_calentry(e) {

        /* merge overlapping/adjacent vacation events per user */
        if ( str_equal( incubator->user, e->user ) &&
                incubator->dayevent && e->dayevent) {
            if (((e->start <= incubator->start) && (incubator->start <= e->end)) ||
                ((e->start <= incubator->end) && (incubator->end <= e->end))) {
                if ( incubator->end > e->end )
                    e->end=incubator->end;
                if ( incubator->start < e->start )
                    e->start=incubator->start;
                free(incubator->subject);
                free(incubator);
                goto done;
            }
        }

        /* sort */
        if ( incubator->start <= e->start ) {
            incubator->next_entry = e;
            if ( e == first_entry )
                first_entry=incubator;
            else
                prev->next_entry=incubator;
            goto done;
        }

        prev=e;
    }

    prev->next_entry = incubator;
    last_entry = incubator;
done:
    incubator=NULL;
    return 0;
}

static struct tm get_period_boundaries(const short year, const short month, time_t *begin, time_t *end)
{
    struct tm b,e;
    time_t t;

    t=time(NULL);
    localtime_r(&t, &b);

    b.tm_sec=0;
    b.tm_min=0;
    b.tm_hour=0;
    b.tm_mday=1;
    if ( -1 != year )
        b.tm_year=year-1900;
    b.tm_mon=(0<month)?month-1:(!month)?0:b.tm_mon;
    if ( begin )
        *begin=mktime(&b);

    e=b;
    e.tm_mon=(0<month)?month:(!month)?12:e.tm_mon+1;
    if ( end )
        *end=mktime(&e);

    return b;
}

static unsigned short workdays_in_period( const time_t begin, const time_t end )
{
    size_t i;
    unsigned short v=0;
    struct tm b,e;
    localtime_r(&begin,&b);
    localtime_r(&end,&e);

    for ( i = b.tm_yday; i<=e.tm_yday; ++i )
        if ( (workday[i] > 0) && (workday[i]<6) )
            v++;

    V(3,
        buffer_puts(buffer_2,"period ");
        buffer_puts(buffer_2, ctime(&begin));
        buffer_puts(buffer_2," up to ");
        buffer_puts(buffer_2, ctime(&end));
        buffer_puts(buffer_2," has ");
        buffer_putulong(buffer_2, e.tm_yday+1-b.tm_yday);
        buffer_puts(buffer_2," days, with ");
        buffer_putulong(buffer_2,v);
        buffer_putsflush(buffer_2," of them being counted as workdays\n");
     );
    return v;
}

void init_holiday_list(const short year)
{
    size_t i;
    struct tm t;
    time_t e;

    t = get_period_boundaries(year, 0, &begin_year, &end_year);

    for (i=0; i<sizeof(workday); i++)
        workday[i]=(i+t.tm_wday)%7;

    e = end_year-1;
    if (localtime(&e)->tm_yday != 365)
        workday[365] = 8;
}

static int flag_holiday()
{
    struct tm b,e;
    size_t i;

    if ( !incubator )
        return -1;

    if ( !incubator->dayevent ) {
        carp("error: holiday is not a dayevent");
        goto cleanup;
    }

    if ( ! incubator->recurring_yearly &&
            ((incubator->start >= end_year) || (incubator->end < begin_year)) )
        goto cleanup;

    localtime_r(&incubator->start, &b);
    localtime_r(&incubator->end, &e);

    if ( b.tm_yday >= e.tm_yday && !( (e.tm_yday==0) && (b.tm_year+1==e.tm_year) ) ) {
        carp("holiday has begin after end");
        goto cleanup;
    }

    for (i=b.tm_yday; i<e.tm_yday; ++i) {
        V(2,
            buffer_puts(buffer_2,"day ");
            buffer_putulong(buffer_2,i);
            buffer_puts(buffer_2," (wday=");
            buffer_putulong(buffer_2,workday[i]);
            buffer_puts(buffer_2,") of the year marked as holiday (");
            buffer_puts(buffer_2,incubator->subject);
            buffer_putsflush(buffer_2,")\n");
         );
        workday[i]=7;
    }

cleanup:
    if (incubator->subject)
        free(incubator->subject);
    free(incubator);
    incubator=NULL;
    return 0;
}

static time_t str2time_t( const char *ts, const bool dayevent )
{
    unsigned long x;
    time_t buf;
    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    size_t o=0,l;

    // yyyymmddThhmmssZ
    if ( str_len(ts) < (sizeof("yyyymmdd")-1) )
        return -1;
    l=sizeof("yyyy")-1;
    scan_ulongn(ts, l, &x);
    t.tm_year=(int)(x-1900);
    o+=l;
    l=sizeof("mm")-1;
    scan_ulongn(ts+o, l, &x);
    t.tm_mon=(int)(x-1);
    o+=l;
    l=sizeof("dd")-1;
    scan_ulongn(ts+o, l, &x);
    t.tm_mday=(int)(x);
    if ( ! dayevent && ( str_len(ts) == sizeof("yyyymmddThhmmssZ")-1 ) ) {
        o+=l+(sizeof("T")-1);
        l=(sizeof("hh")-1);
        scan_ulongn(ts+o, l, &x);
        t.tm_hour=(int)(x);
        o+=l;
        l=(sizeof("mm")-1);
        scan_ulongn(ts+o, l, &x);
        t.tm_min=(int)(x);
        o+=l;
        l=(sizeof("ss")-1);
        scan_ulongn(ts+o, l, &x);
        t.tm_sec=(int)(x);
        buf = mktime(&t);
        if ( ts[o+l]=='Z' ) {
            if ( t.tm_zone )
                buf+= t.tm_gmtoff;
            else
                buf+= timezone;
        }
        return buf;
    } else {
        return mktime(&t);
    }
}

int filter_project_calentries( const char *project )
{
    struct calendar_context *e, *prev=first_entry, *next;

    for(e=first_entry; e;) {
        next=e->next_entry;
        if ( e->dayevent || !str_start(e->subject, project) ) {
            if ( e == first_entry )
                first_entry=next;
            else
                prev->next_entry=next;
            if ( e == last_entry )
                last_entry = prev;
            if (e->subject)
                free(e->subject);
            free(e);
        } else {
            prev=e;
        }
        e=next;
    }
    return 0;
}

static int parse_ics_line( char *user )
{

    V(4,
            buffer_puts(buffer_2, "ICS: ");
            buffer_puts(buffer_2, gbuf.str);
            buffer_putsflush(buffer_2, "\n");
    );
    if ( str_start( gbuf.str, "BEGIN:VEVENT" ) )
        prepare_new_calentry(user);
    if ( str_start( gbuf.str, "END:VEVENT" ) ) {
        if ( user )
            emerge_calentry();
        else
            flag_holiday();
    }

    if ( str_start( gbuf.str, "SUMMARY:" ) ) {
        if ( !(incubator->subject = calloc( str_len(gbuf.str)-(sizeof("SUMMARY:")-1)+1, sizeof(char) ))) {
            carpsys("calloc");
            return -1;
        }
        str_copy( incubator->subject, gbuf.str+(sizeof("SUMMARY:")-1) );
    }

    if ( str_start( gbuf.str, "LOCATION:" ) ) {
        incubator->onsite = true;
    }

    if ( str_start( gbuf.str, "DTSTART:" ) ) {
        incubator->start=str2time_t( gbuf.str+(sizeof("DTSTART:")-1), false );
    }
    if ( str_start( gbuf.str, "DTSTART;VALUE=DATE:" ) ) {
        incubator->start=str2time_t( gbuf.str+(sizeof("DTSTART;VALUE=DATE:")-1), true );
        incubator->dayevent = true;
    }
    if ( str_start( gbuf.str, "RRULE:FREQ=YEARLY" ) ) {
        incubator->recurring_yearly = true;
    }
    if ( str_start( gbuf.str, "DTEND:" ) ) {
        incubator->end=str2time_t( gbuf.str+(sizeof("DTEND:")-1), false );
    }
    if ( str_start( gbuf.str, "DTEND;VALUE=DATE:" ) ) {
        incubator->end=( str2time_t( gbuf.str+(sizeof("DTEND;VALUE=DATE:")-1), true ) );
        incubator->dayevent = true;
    }

    return 0;
}

static int stream2lines( char *buf, char *user )
{
    bool line_complete;
    do {
        size_t eol = str_chr(buf, '\n');
        size_t current_len = gbuf.str?str_len(gbuf.str):0;
        size_t required = ( current_len + eol + 1);

        line_complete = (eol<str_len(buf))?true:false;
        buf[eol] = '\0';
        if ( eol && buf[eol-1] == '\r' ) { buf[eol-1] = '\0'; required--; }

        if (  required > gbuf.alloc_len ) {
            gbuf.str = realloc( gbuf.str, required );
            if ( ! gbuf.str )
                return -1;
            gbuf.alloc_len = required;
        }
        fmt_str( gbuf.str + current_len, buf );
        if ( required )
            gbuf.str[required-1]='\0';

        if (line_complete) {
            parse_ics_line(user);
            gbuf.str[0]='\0';
            int i = 0;
            do {
                buf[i]=buf[i+eol+1];
            } while (buf[i++] != '\0');

        }
    } while (line_complete);

    return 0;
}

int ics_parser( char *buf, char *user )
{
    if ( stream2lines(buf, user) )
        return -1;
    return 0;
}

struct timeslotinfo tsi;

static time_t slice_timeslots( const struct calendar_context *e, const time_t begin_month, const time_t end_month )
{
    time_t start_ts=(e->start<begin_month)?begin_month:e->start,
           end_ts=(e->end>end_month)?end_month:e->end,
           diff;
    struct tm eod, s_tm, e_tm;

    V(3,
        buffer_puts(buffer_2,"event start: ");
        buffer_puts(buffer_2, ctime(&e->start));
        buffer_puts(buffer_2,"event end:   ");
        buffer_puts(buffer_2, ctime(&e->end));
        buffer_flush(buffer_2);
    );

    diff = end_ts - start_ts;
    if (e->dayevent)
        return diff;

    localtime_r(&start_ts, &s_tm);
    localtime_r(&end_ts, &e_tm);
    eod = s_tm;
    eod.tm_sec=0;
    eod.tm_min=0;
    eod.tm_hour=0;
    eod.tm_mday+=1;

    tsi.mday=s_tm.tm_mday;
    tsi.mon=s_tm.tm_mon+1;
    tsi.shour=s_tm.tm_hour;
    tsi.smin=s_tm.tm_min;

    if (s_tm.tm_yday < e_tm.tm_yday) {
        tsi.ehour=24;
        tsi.emin=0;
        tsi.workhours_ch=( mktime(&eod) - start_ts )/(60*60/100);
        current_format.timeline();
        tsi.shour=0;
        tsi.smin=0;
        tsi.workhours_ch=(24*100);
        while (e_tm.tm_yday > eod.tm_yday) {
            tsi.mday=eod.tm_mday++;
            tsi.mon=eod.tm_mon+1;
            mktime(&eod);
            current_format.timeline();
        }
        if (end_ts > mktime(&eod)) {
            tsi.mday=e_tm.tm_mday;
            tsi.mon=e_tm.tm_mon+1;
            tsi.ehour=e_tm.tm_hour;
            tsi.emin=e_tm.tm_min;
            tsi.workhours_ch=( end_ts - mktime(&eod) )/(60*60/100);
            current_format.timeline();
        }
    } else {
        tsi.ehour=e_tm.tm_hour;
        tsi.emin=e_tm.tm_min;
        tsi.workhours_ch=( end_ts - start_ts )/(60*60/100);
        current_format.timeline();
    }
    return diff;
}

int cal_statistics( struct config_context *cfgctx )
{
    struct user_context *user = NULL;
    struct project_context *project= NULL;
    struct program_args *pa = &(cfgctx->prog_arg);
    struct calendar_context *e;
    time_t begin_month, end_month;
    struct tm t;

    memset(&tsi, 0, sizeof(struct timeslotinfo));

    if ( pa->user ) {
        struct user_context *ucntx;
        for_each_user(cfgctx, ucntx) {
            if (str_equal(ucntx->name,pa->user)) {
                user = ucntx;
                break;
            }
        }
    }

    if ( pa->format ) {
        size_t i; bool format_found=false;
        for (i=0; i<(sizeof(format)/sizeof(format[0]));i++) {
            if (str_equal( pa->format, format[i].name )) {
                current_format = format[i];
                format_found=true;
            }
        }
        if (!format_found) exit(EXIT_FAILURE);
    } else
        current_format = format[0];

    t = get_period_boundaries(pa->year, pa->month, &begin_month, &end_month);
    tsi.mon=(pa->month)?t.tm_mon+1:1;
    tsi.allyear=(pa->month)?false:true;
    tsi.year=t.tm_year+1900;

    V(4,
        buffer_puts(buffer_2,"begin of month: ");
        buffer_puts(buffer_2, ctime(&begin_month));
        buffer_puts(buffer_2,"end of month: ");
        buffer_puts(buffer_2, ctime(&end_month));
        buffer_puts(buffer_2,"begin of year: ");
        buffer_puts(buffer_2, ctime(&begin_year));
        buffer_puts(buffer_2,"end of year: ");
        buffer_puts(buffer_2, ctime(&end_year));
        buffer_putnlflush(buffer_2);
    );

    if (user) {
        tsi.userlimit=true;
        tsi.user = user->name;
    } else
        tsi.userlimit=false;

    if ( pa->project ) {
        tsi.projectlimit=true;
        tsi.project=pa->project;
        for_each_project(cfgctx, project) {
            if ( str_equal( project->name, pa->project ) ) {
                tsi.centihourlyrate_onsite = project->onsite;
                tsi.centihourlyrate_remote = project->remote;
                break;
            }
        }
    }
    stralloc_init(&output_line_sa);
    current_format.header();

    for (e=first_entry;e;) {
        tsi.onsite = e->onsite;
        tsi.user = e->user;
        tsi.project = e->subject;

        if ( (e->start < end_month) && (e->end > begin_month) ) {
            time_t t = slice_timeslots(e, begin_month, end_month);
            if ( 0>t )
                return -1;

            if (e->dayevent)
                tsi.vmonth += workdays_in_period(
                            (e->start<begin_month)?begin_month:e->start,
                            ((e->end>end_month)?end_month:e->end)-1 );
            else {
                if ( tsi.onsite )
                    tsi.worksum_onsite_ch += t/(60*60/100);
                else
                    tsi.worksum_remote_ch += t/(60*60/100);
            }
        }
        if ( (e->dayevent) && (e->start < end_year) && (e->end > begin_year) )
            tsi.vyear += workdays_in_period(
                            (e->start<begin_year)?begin_year:e->start,
                            ((e->end>end_year)?end_year:e->end)-1 );

        struct calendar_context *t = e->next_entry;
        if (e->subject) free(e->subject);
        free(e);
        e=t;
    }
    if ( user ) {
        unsigned short vday_hours = (unsigned short) (( user->monthhours *
                    12.0 / workdays_in_period(begin_year, end_year-1)) +.5);
        V(3,
            buffer_puts(buffer_2, "vacation day in work hours: ");
            buffer_putulong(buffer_2, vday_hours);
            buffer_putnlflush(buffer_2);
        );
        tsi.worktbd_ch=(tsi.worksum_onsite_ch + tsi.worksum_remote_ch +
                ((tsi.vmonth*vday_hours) - (user->monthhours*((pa->month)?1:12)))*100);
        tsi.vleft=user->vacation-tsi.vyear;
    }
    current_format.footer();

    stralloc_free(&output_line_sa);
    free(gbuf.str);
    return 0;
}

#ifdef UNITTEST
#include <assert.h>

int main( int argc, char *argv[] )
{
#define ICSDATA "foo\r\nbar\r\n\r\nBEGIN:VEVENT\r\nDTSTART:19700101T100000Z"\
    "\r\nDTEND:19700101T123456Z\r\nSUMMARY:testevent\r\nEND:VEVENT\r\n"

    char *ics_data=calloc(str_len(ICSDATA)+1,sizeof(char));
    char *ics_user="testuser";
    memset(ics_data, 0, str_len(ICSDATA+1));
    str_copy(ics_data, ICSDATA);
    ics_parser(ics_data, ics_user);
    assert(str_equal(first_entry->user,"testuser"));
    assert(str_equal(first_entry->subject,"testevent"));
    assert(first_entry->start==(10*60*60));
    assert(first_entry->end==((((12*60)+34)*60)+56));
    free(ics_data);

    init_holiday_list(2020);
    assert(workday[0] == 3);
    assert(workday[365] == 4);
    init_holiday_list(2021);
    assert(workday[365] == 8);

    struct tm x,y={.tm_year=70, .tm_mon=0,.tm_mday=1};
    x=y; y.tm_mday=4;
    unsigned short v = workdays_in_period( mktime(&x), mktime(&y) );
    assert(v == 2);

    exit(EXIT_SUCCESS);
}
#endif
