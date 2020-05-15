/*
 * Component Name: EIF TLI library
 *
 * $Date: 1996/07/25 18:41:04 $
 *
 * $Source: /tivoli/homes/1/tgreenbe/convert/tec/eif/tlieif.c,v $
 *
 * $Revision: 1.6 $
 *
 * Description: This library implements a few IPC functions for the TEC/EIF
 *  so that we don't have to ship the entire ADE.  This file must NOT be 
 *  linked with ADE libraries, as the data structures are not compatible.  
 *  These functions can be used to talk to ADE-type entities, as the protocol
 *  across the wire is consistent.
 *
 *  This is a very simple library, with the bare bones TLI calls needed.
 *  It is intended to be used as a reference point when porting the EIF
 *  library to a TLI platform.  I have seen a lot of variation among TLI
 *  behaviors in the various platform ports, so there will be some need to
 *  beef up error handling, etc. for the behavior on a particular platform.
 *
 * (C) Copyright 1990-1994 Tivoli Systems, Inc. 
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of Tivoli Systems, Inc.
 */


#define TLI_TCP_DEVICE "/dev/tcp"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <tiuser.h>
#include <errno.h>
#include <sys/socket.h>

#ifdef SOLARIS2
#	include <string.h>
#	include <unistd.h>
#endif

#include "eif.h"

#ifdef NCR
extern int t_errno;
#endif

/*
 * Description:  wrappers around TLI functions that handle common errors
 *
 * Returns: passes through TLI function return value
 *
 * Notes/Dependencies:  Currently handled: EINTR
 */

#define GENERATE_WRAPPER(wrap_decl, body)                   \
        static int wrap_decl                                \
        { int rv, try = 0;                                  \
                                                            \
          label:                                            \
          try++;                                            \
          rv = body;                                        \
          if ((rv == -1) && (errno == EINTR) && (try < 10)) \
              goto label;                                   \
          return rv;                                        \
        } 

#define GROUP(foo) foo

GENERATE_WRAPPER( GROUP(tli_look(int fd)), GROUP(t_look(fd)))

GENERATE_WRAPPER( GROUP(tli_rcvdis(int fd,  struct t_discon *dis)),
                   GROUP(t_rcvdis(fd, dis)))

GENERATE_WRAPPER( GROUP(tli_snddis( int fd, struct t_call *call)),
                GROUP(t_snddis(fd, call)) )

GENERATE_WRAPPER( GROUP(tli_bind(int fd, struct t_bind *req,
        struct t_bind *ret)), GROUP(t_bind(fd, req, ret)))

GENERATE_WRAPPER( GROUP(tli_unbind(int fd)),
                    GROUP( t_unbind(fd)) )

GENERATE_WRAPPER( GROUP(tli_sndrel(int fd)),
                GROUP(t_sndrel(fd)) )

GENERATE_WRAPPER( GROUP(tli_rcvrel( int fd)),
                GROUP(t_rcvrel(fd)) )

GENERATE_WRAPPER( GROUP(tli_connect(int fd, struct t_call *in,
        struct t_call *out)), GROUP(t_connect(fd, in, out)) )

GENERATE_WRAPPER( GROUP(tli_rcvconnect( int fd, struct t_call *call)),
        GROUP(t_rcvconnect(fd, call)) )

GENERATE_WRAPPER( GROUP(tli_listen( int fd, struct t_call *call)),
        GROUP(t_listen(fd, call)) )

GENERATE_WRAPPER( GROUP(tli_accept( int fd, int fd2,
    struct t_call *call)), GROUP(t_accept(fd, fd2, call)) )

GENERATE_WRAPPER( GROUP(tli_close(int fd)), GROUP(t_close(fd)) )

GENERATE_WRAPPER( GROUP(tli_open(char *path, int flag, struct t_info *info)),
        GROUP(t_open(path, flag, info)) )

GENERATE_WRAPPER( GROUP(tli_sync(int fd)), GROUP(t_sync(fd)) )

/* $Id: tlieif.c,v 1.6 1996/07/25 18:41:04 jmills Exp $ */
/*
 * Description:  create ipc client handle
 *
 * Returns: handle on success, NULL on failure
 *
 * Notes/Dependencies: 
 */
eipc_handle_p_t
eipc_create_remote_client(
    unsigned addr,
    unsigned short port,
    eipc_error_t *error)
{
    eipc_handle_p_t     handle;
    struct sockaddr_in server;
    struct t_call      callbuf;
    int                rv;
    int                copy_errno, copy_t_errno;

    handle = (eipc_handle_p_t) malloc(sizeof(eipc_handle_t));

    /* Don't let SIGALRM break this */
    errno = EINTR;
    t_errno = TSYSERR;
    handle->fd = -1;
    while ((errno == EINTR)&&(t_errno == TSYSERR)&&(handle->fd == -1)) {
	errno = 0;
	handle->fd = tli_open(TLI_TCP_DEVICE, O_RDWR, 0);
    }

    if (handle->fd < 0) {
	free(handle);
        *error = E_SCALL;
        return 0;
    }

    /* Don't let SIGALRM break this */
    errno = EINTR;
    t_errno = TSYSERR;
    rv = -1;
    while ((errno == EINTR)&&(t_errno == TSYSERR)&&(rv == -1)) {
	errno = 0;
	rv = tli_bind(handle->fd,0,0);
    }
    if (rv == -1) {
        copy_errno = errno;
        copy_t_errno = t_errno;
    }

    memset((char*)&server,0,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = port;
    server.sin_addr.s_addr = addr;

    callbuf.addr.maxlen = sizeof(server);
    callbuf.addr.len = sizeof(server);
    callbuf.addr.buf = (void*)&server;
    callbuf.opt.len = 0;
    callbuf.udata.len = 0;

    /* Don't let SIGALRM break this */
    errno = EINTR;
    t_errno = TSYSERR;
    rv = -1;
    while ((errno == EINTR)&&(t_errno == TSYSERR)&&(rv == -1)) {
	errno = 0;
	rv = tli_connect(handle->fd, &callbuf, 0);
    }
    if (t_errno == TLOOK) {
        fprintf(stderr, "T_LOOK: event=%d state=%d\n",
                t_look(handle->fd), t_getstate(handle->fd));
    }
    if (rv < 0) {
        copy_errno = errno;
	close(handle->fd);
        free(handle);
        *error = E_SCALL;
        return 0;
    }
    
    return handle;
}

/*
 * Description: graceful shutdown of communications
 *              
 * Returns: void
 *
 * Notes/Dependencies: 
 */
void
eipc_shutdown(eipc_handle_p_t ipch) 
{
    if (ipch->fd >= 0) {
        t_sndrel( ipch->fd);
        t_rcvrel( ipch->fd);
    }
}

/*
 * Description: close and free handle
 *
 * Returns: void
 *
 * Notes/Dependencies: input pointer is freed before return
 */
void
eipc_destroy_handle(eipc_handle_p_t ipch)
{
    if (ipch->fd >= 0) {
        t_close(ipch->fd);
    }
    free(ipch);
}

/*
 * Description: socket-level send function
 *
 * Returns: 0 for success, -1 for failure
 *
 * Notes/Dependencies: 
 */
static int
do_send(int fd, char *buf, int len, eipc_error_t *error)
{
    int rv;
retry:
    rv = t_snd(fd, buf, len, 0);
    if (rv < 0) {
        if (errno == EINTR) {
            goto retry;
        }
        *error = E_SCALL;
        return -1;
    }
    if (rv < len) {
        len -= rv;
        buf += rv;
        goto retry;
    }
    return 0;
}

/*
 * Description:  Send a message
 *
 * Returns: 0 for success, -1 for failure
 *
 * Notes/Dependencies:  Compatible message format with libtmf's and
 *  libwo's ipc_send.
 */

int
eipc_send(
    eipc_handle_p_t handle,
    char *data,
    long len,
    long type,
    long id,
    eipc_error_t *error)
{
    eipc_msg_hdr_t hdr;
    int           dlen;
    int           slen;

    memcpy(hdr.token, TOKEN, 8);
    hdr.msg_id = htonl(id);
    hdr.msg_from = 0;
    hdr.msg_to = 0;
    hdr.msg_type = htonl(type);
    hdr.ipc_msg_type = 0;
    hdr.msg_len = htonl(len);
    if(len < HDR_DATA_LEN) {
        dlen = len;
    } else {
        dlen = HDR_DATA_LEN;
    }
    if((hdr.hdr_data_len = htonl(dlen)) != 0) {
        memcpy(hdr.hdr_data, data, dlen);
    }
    slen = sizeof(eipc_msg_hdr_t) - HDR_DATA_LEN + dlen;
    
        /* send the header (and data if it's short) */
    if (do_send(handle->fd, (char*)&hdr, slen, error) != 0) {
        return -1;
    }

        /* send the rest of the message body (if it was longer than 
           HDR_DATA_LEN */
    if (len - dlen > 0) {
        if (do_send(handle->fd, data + dlen, len - dlen, error) != 0) {
            return -1;
        }
    }

    return 0;
}
