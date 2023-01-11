#include "../format.h"

void html_header()
{
    if ( tsi.allyear ) {
        buffer_puts(buffer_1, "1-12/");
    } else {
        buffer_putlong(buffer_1, tsi.mon);
        buffer_puts(buffer_1, "/");
    }
    buffer_putlong(buffer_1, tsi.year);
    if ( tsi.userlimit ) {
        buffer_puts(buffer_1, "&nbsp;");
        buffer_puts(buffer_1, tsi.user);
    }
    buffer_putm(buffer_1, "\n",
        "<table>\n\t<tr>\t",
        "<th>Date</th>",
        "<th>Starttime</th>",
        "<th>Endtime</th>",
        "<th>Duration</th>",
        "<th>Location</th>",
        "\t</tr>");
    buffer_putnlflush(buffer_1);
}

void html_timeline()
{
    stralloc_zero(&output_line_sa);

    stralloc_cats(&output_line_sa, "\t<tr>\t<td>");
    FMT_DATE(output_line_sa, tsi.mday, tsi.mon);
    stralloc_cats(&output_line_sa, "</td><td>");
    FMT_TIME(output_line_sa, tsi.shour, tsi.smin);
    stralloc_cats(&output_line_sa, "</td><td>");
    FMT_TIME(output_line_sa, tsi.ehour, tsi.emin);
    stralloc_cats(&output_line_sa, "</td><td>");
    FMT_IND_HOURS(output_line_sa, tsi.workhours_ch);
    if ( tsi.onsite ) {
        stralloc_cats(&output_line_sa, "</td><td>onsite");
    } else {
        stralloc_cats(&output_line_sa, "</td><td>remote");
    }
    stralloc_cats(&output_line_sa, "</td>\t</tr>");

    buffer_putsaflush(buffer_1, &output_line_sa);
    buffer_putnlflush(buffer_1);
}

void html_footer()
{
    stralloc_zero(&output_line_sa);

    stralloc_catm(&output_line_sa, "\t<tr>\t<td colspan=\"5\">", "Onsite: ");
    FMT_IND_HOURS(output_line_sa, tsi.worksum_onsite_ch);
    stralloc_cats(&output_line_sa, "&nbsp;Remote: ");
    FMT_IND_HOURS(output_line_sa, tsi.worksum_remote_ch);
    stralloc_cats(&output_line_sa, "&nbsp;worktime balance: ");
    FMT_IND_HOURS(output_line_sa, tsi.worktbd_ch);
    stralloc_cats(&output_line_sa, "</td>\t</tr>");
    buffer_putsaflush(buffer_1, &output_line_sa);
    buffer_putnlflush(buffer_1);

    stralloc_zero(&output_line_sa);

    stralloc_catm(&output_line_sa, "\t<tr>\t<td colspan=\"5\">", "vacation: ");
    stralloc_catlong(&output_line_sa, tsi.vmonth);
    stralloc_cats(&output_line_sa, "days (left: ");
    stralloc_catlong(&output_line_sa, tsi.vleft);
    stralloc_catm(&output_line_sa, "days)", "</td>\t</tr>\n</table>");

    buffer_putsaflush(buffer_1, &output_line_sa);
    buffer_putnlflush(buffer_1);
}

