#ifndef ICS_H
#define ICS_H
#include "config.h"

void set_ics_verbosity( short );
void init_holiday_list( short );
int ics_parser( char *, char * );
int cal_statistics( struct config_context * );
int filter_project_calentries( const char * );
#endif
