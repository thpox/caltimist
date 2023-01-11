// SPDX-License-Identifier: GPL-2.0-only
/*
 * part of caltimist - calculates project-/worktime and vacation using iCalendar data
 * Copyright (C) 2023 Thomas Pöhnitzsch <thpo+caltimist@dotrc.de>
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <buffer.h>
#include <errmsg.h>
#include <str.h>
#include <io.h>
#include <fmt.h>
#ifndef NOSSL
#include <openssl/ssl.h>
#endif
#include <textcode.h>
#include "httpsclient.h"

#define BUFFERSIZE 500

#define V(__l,__fn) do{if(httpsclient_verbosity>=__l){ __fn; }}while(0);
short httpsclient_verbosity=0;

void set_httpsclient_verbosity( short v ) {
    httpsclient_verbosity=v;
}

struct url_parts {
    char *authstring;
    char *hostname;
    char *service;
    char *path;
};

static int show_connection_info( const struct addrinfo *ai )
{
    const char *out_ip = NULL;
    uint16_t port=0;

    if ( ai->ai_family == AF_INET ) {
        char buffer[INET_ADDRSTRLEN];
        struct sockaddr_in *sin;
        sin = (struct sockaddr_in *)ai->ai_addr;

        out_ip = inet_ntop(ai->ai_family, &sin->sin_addr, buffer, sizeof(buffer));
        port = ntohs(sin->sin_port);
    }
    if ( ai->ai_family == AF_INET6 ) {
        char buffer[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *sin;
        sin = (struct sockaddr_in6 *)ai->ai_addr;

        out_ip = inet_ntop(ai->ai_family, &sin->sin6_addr, buffer, sizeof(buffer));
        port = ntohs(sin->sin6_port);
    }
    buffer_puts(buffer_2,"Server: ");
    buffer_puts(buffer_2,out_ip);
    buffer_puts(buffer_2," ");
    buffer_putlong(buffer_2, port);
    buffer_putnlflush(buffer_2);

    return 0;
}

#ifndef NOSSL
static int ssl_context_setup( SSL_CTX **ssl_ctx, SSL **ssl, const int *sock, const char *hostname )
{
    *ssl_ctx = SSL_CTX_new( TLS_client_method() );

    if ( ! *ssl_ctx ) {
        carpsys("SSL_CTX_new");
        return -1;
    }
    SSL_CTX_set_verify( *ssl_ctx, SSL_VERIFY_PEER, NULL );
    if ( ! SSL_CTX_set_default_verify_paths(*ssl_ctx) )
        return -1;

    *ssl = SSL_new( *ssl_ctx );

    if ( ! *ssl ||
         ! SSL_set_fd( *ssl, *sock ) ||
         ! SSL_set_tlsext_host_name( *ssl, hostname ) ||
         ! SSL_set1_host( *ssl, hostname ) )
        return -1;

    int ret=SSL_connect( *ssl );
    if ( 0 > ret ) {
        //int err = SSL_get_error(*ssl, ret);
        carp("SSL_connect");
        return -1;
    }

    V(3,carp("SSL context set up\n"));
    return 0;
}
#endif

static int establish_connection( int *sock, const char *hostname, const char *service )
{
    int gai_result = 0, ret=0;
    struct addrinfo hints, *addr_list;

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    gai_result = getaddrinfo( hostname, service, &hints, &addr_list );
    if ( gai_result ) {
        buffer_puts(buffer_2, "getaddrinfo");
        buffer_putsflush(buffer_2, (gai_result == EAI_SYSTEM)?strerror(gai_result):gai_strerror(gai_result));
        ret=-1;
        goto cleanup;
    }

    for ( struct addrinfo *ai = addr_list; ai; ai = ai->ai_next ) {
        *sock = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
        if ( *sock < 0 ) continue;
        V(2,show_connection_info( ai ));

        if ( connect( *sock, ai->ai_addr, ai->ai_addrlen ) ) {
            carpsys("connect");
            ret=-1;
            goto cleanup;
        }
        break;
    }
    V(2,buffer_putsflush(buffer_2,"connection established\n"));

cleanup:
    freeaddrinfo(addr_list);
    return ret;
}

static int set_global_authstring( struct url_parts *up, const struct general_context *general )
{
    if ( !up->authstring && general->user && general->password ) {
        size_t i=0;
        size_t plen = str_len(general->user)+1+str_len(general->password);
        char *userpass= calloc( plen+1, sizeof(char));;
        if (!userpass) {
            carpsys("calloc");
            return -1;
        }
        i = fmt_str( userpass, general->user );
        i+= fmt_str( userpass+i, ":" );
        i+= fmt_str( userpass+i, general->password );
        userpass[i]=0;
        up->authstring=userpass;
    }
    return 0;
}

static int get_response( int *sock, const struct url_parts *up, char *user, int(*parser)(char*,char*) )
{
    char buf[BUFFERSIZE];
#ifndef NOSSL
    SSL_CTX *ssl_ctx = NULL;
    SSL *ssl = NULL;
#endif
    char *getstrings[]={ "GET "," HTTP/1.1\nHost: ","\nConnection: close\nAuthorization: Basic ","\n\n" };
    ssize_t slen=0, rlen=1;
    bool is_https = str_equal(up->service, "https");
    char *b64auth=NULL, *msg=NULL;

    if ( up->authstring ){
        size_t plen = str_len(up->authstring);
        b64auth = calloc( ((plen+2)/3)*4+1, sizeof(char));;
        if (!b64auth) {
            carpsys("calloc"); goto err;
        }
        plen=fmt_base64(b64auth,up->authstring,plen);
        b64auth[plen]='\0';
    }

    for (size_t i=0;i<4;i++) slen+=str_len(getstrings[i]);
    slen += str_len(up->path) + str_len(up->hostname) + str_len(b64auth);

    msg = calloc( slen+1, sizeof(char));;
    if (!msg) {
        carpsys("calloc"); goto err;
    } else {
        size_t i;
        i = fmt_str( msg, getstrings[0] );
        i+= fmt_str( msg+i, up->path );
        i+= fmt_str( msg+i, getstrings[1] );
        i+= fmt_str( msg+i, up->hostname );
        if ( b64auth ) {
            i+= fmt_str( msg+i, getstrings[2] );
            i+= fmt_str( msg+i, b64auth );
            free(b64auth);
            b64auth=NULL;
        }
        i+= fmt_str( msg+i, getstrings[3] );
    }
    V(3, buffer_putsflush(buffer_2, msg); );

#ifndef NOSSL
    if ( is_https && ssl_context_setup( &ssl_ctx, &ssl, sock, up->hostname ) )
        goto err;

    if ( is_https )
        slen = SSL_write( ssl, msg, slen );
    else
#else
    if ( is_https ) {
        carp("program was compiled with NO SSL support");
        goto err;
    }
#endif
        slen = write( *sock, msg, slen );

    if ( slen != str_len(msg) ) {
            carp("write incomplete");
            goto err;
    }
    if (msg) free(msg);

    while ( 0 < rlen ) {
#ifndef NOSSL
        if ( is_https )
            rlen = SSL_read(ssl, buf, BUFFERSIZE );
        else
#endif
            rlen = read(*sock, buf, BUFFERSIZE);

        buf[rlen]=0;
        parser(buf,user);
    }

#ifndef NOSSL
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    SSL_CTX_free(ssl_ctx);
#endif

    return 0;
err:
    if (msg) free(msg);
    if (b64auth) free(b64auth);
    return -1;
}

static int split_uri( const char *uri, struct url_parts *up )
{
    size_t pos=0, pos2=0, pos3=0, max=str_len(uri)+1;

#define uri_slice(__s,__e,__v) do { if (!((up->__v)=calloc( __e-__s+1, sizeof(char)))) { carpsys("calloc"); return -1; }; strncpy(up->__v,uri+__s,__e-__s);up->__v[__e-__s]='\0'; } while(0);

    pos = str_chr( uri, ':' );
    if ( pos+3>=max || !str_start( uri+pos+1, "//") )
        return -1;
    uri_slice( 0, pos, service);
    pos+=3;
    pos2 = pos+str_chr( uri+pos, '/' );
    pos3 = pos+str_chr( uri+pos, '@' );
    if ( pos2>=max )
        return -1;
    if ( pos3 < pos2 ) {
        uri_slice( pos, pos3, authstring);
        uri_slice( (pos3+1), pos2, hostname);
    } else {
        uri_slice(pos, pos2, hostname);
    }
    uri_slice( pos2, max, path);

    return 0;
}

int fetch_calendar( char *user, const char *cal, const struct general_context *general, int(*cal_parser)(char*,char*) )
{
    int ret=0;
    int sock=0;
    struct url_parts up;
    memset( &up, 0, sizeof(struct url_parts));

    if ( cal ) {
        if ( -1 == split_uri( cal, &up ) ||
             -1 == set_global_authstring( &up, general ) ||
             -1 == establish_connection( &sock, up.hostname, up.service) ||
             -1 == get_response( &sock, &up, user, cal_parser ) ) {
            ret=-1;
        }
        if (up.service) free(up.service);
        if (up.authstring) free(up.authstring);
        if (up.hostname) free(up.hostname);
        if (up.path) free(up.path);
        close(sock);
    }
    return ret;
}

#ifdef UNITTEST
#include <assert.h>

int main( int argc, char *argv[] )
{
    struct url_parts up;
    memset( &up, 0, sizeof(struct url_parts));

    split_uri( "https://usr:PaSs@ho.st.na.me/path/to/cal.ics", &up );

    assert(str_equal(up.service,"https"));
    assert(str_equal(up.authstring,"usr:PaSs"));
    assert(str_equal(up.hostname,"ho.st.na.me"));
    assert(str_equal(up.path,"/path/to/cal.ics"));

    exit(EXIT_SUCCESS);
}
#endif
