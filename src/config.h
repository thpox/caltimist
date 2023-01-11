#ifndef CONFIG_H
#define CONFIG_H
#include <stdbool.h>
#include <time.h>

void set_config_verbosity( short );

enum config_flags {
    GENERALCTX,
    USERCTX,
    PROJECTCTX
};

struct program_args {
    short year;
    short month;
    char *user;
    char *project;
    char *format;
    bool show_user;
    bool show_project;
};

struct general_context {
    char *user;
    char *password;
    char *public_holidays;
};

struct user_context {
    char *name;
    char *cal;
    unsigned short vacation;
    unsigned short monthhours;
    struct user_context *next_user;
};

struct project_context {
    char *name;
    unsigned short onsite;
    unsigned short remote;
    struct project_context *next_project;
};

struct config_context {
    struct program_args prog_arg;
    struct general_context general;
    struct user_context *first_user, *last_user;
    struct project_context *first_project, *last_project;
    unsigned short active_context;
};

#define for_each_user(__cfgctx,__user) for (__user=(__cfgctx)->first_user; (__user); (__user)=(__user)->next_user)
#define for_each_project(__cfgctx,__project) for (__project=(__cfgctx)->first_project; (__project); (__project)=(__project)->next_project)

extern char *PROGNAME;
int parse_config( struct config_context * );
#endif
