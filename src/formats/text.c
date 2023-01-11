#include "../format.h"

void text_header()
{
    if ( tsi.allyear ) {
        buffer_puts(buffer_1, "1-12/");
    } else {
        buffer_putlong(buffer_1, tsi.mon);
        buffer_puts(buffer_1, "/");
    }
    buffer_putlong(buffer_1, tsi.year);
    if ( tsi.userlimit ) {
        buffer_puts(buffer_1, "\t");
        buffer_puts(buffer_1, tsi.user);
    }
    if ( tsi.projectlimit ) {
        buffer_puts(buffer_1, "\tProjekt ");
        buffer_puts(buffer_1, tsi.project);
    }
    buffer_putnlflush(buffer_1);
}

void text_timeline()
{
    stralloc_zero(&output_line_sa);

    FMT_DATE(output_line_sa, tsi.mday, tsi.mon);
    stralloc_append(&output_line_sa, " ");
    FMT_TIME(output_line_sa, tsi.shour, tsi.smin);
    stralloc_cats(&output_line_sa, " -> ");
    FMT_TIME(output_line_sa, tsi.ehour, tsi.emin);
    stralloc_cats(&output_line_sa, " = ");
    FMT_IND_HOURS(output_line_sa, tsi.workhours_ch);
    if ( tsi.onsite )
        stralloc_cats(&output_line_sa, " | onsite");
    else
        stralloc_cats(&output_line_sa, " | remote");
    if ( !tsi.userlimit )
        stralloc_catm(&output_line_sa, " | ", tsi.user);
    if ( !tsi.projectlimit )
        stralloc_catm(&output_line_sa, " | ", tsi.project);

    buffer_putsaflush(buffer_1, &output_line_sa);
    buffer_putnlflush(buffer_1);
}

void text_footer()
{
    long r, o;

    stralloc_zero(&output_line_sa);

    stralloc_cats(&output_line_sa, "Onsite: ");
    FMT_IND_HOURS(output_line_sa, tsi.worksum_onsite_ch);
    stralloc_cats(&output_line_sa, "\tRemote: ");
    FMT_IND_HOURS(output_line_sa, tsi.worksum_remote_ch);
    if ( tsi.projectlimit ) {
        stralloc_cats(&output_line_sa, "\namount onsite => ");
        o=(tsi.worksum_onsite_ch * tsi.centihourlyrate_onsite)/100;
        FMT_PRICE(output_line_sa, o);
        stralloc_cats(&output_line_sa, "\namount remote => ");
        r=(tsi.worksum_remote_ch * tsi.centihourlyrate_remote)/100;
        FMT_PRICE(output_line_sa, r);
        stralloc_cats(&output_line_sa, "\namount sum => ");
        o+=r;
        FMT_PRICE(output_line_sa, o);
    } else if ( tsi.userlimit ) {
        stralloc_cats(&output_line_sa, "\nworktime balance: ");
        FMT_IND_HOURS(output_line_sa, tsi.worktbd_ch);
        stralloc_cats(&output_line_sa, "\tvacation: ");
        stralloc_catlong(&output_line_sa, tsi.vmonth);
        stralloc_cats(&output_line_sa, "days (left: ");
        stralloc_catlong(&output_line_sa, tsi.vleft);
        stralloc_cats(&output_line_sa, "days)");
    }
    buffer_putsaflush(buffer_1, &output_line_sa);
    buffer_putnlflush(buffer_1);
}
