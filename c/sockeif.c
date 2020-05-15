/*
 * Component Name: EIF sockets library
 *
 * $Date: 1998/09/03 16:25:32 $
 *
 * $Source: /net/nfs1/projects/tivoli/cvsroot/tecad_nw4/src/eif/sockeif.c,v $
 *
 * $Revision: 1.4 $
 *
 * Description: This library implements a few IPC functions for the TEC/EIF
 *  so that we don't have to ship the entire ADE.  This file must NOT be 
 *  linked with ADE libraries, as the data structures are not compatible.  
 *  These functions can be used to talk to ADE-type entities, as the protocol
 *  across the wire is consistent.
 *
 * (C) Copyright 1990-1994 Tivoli Systems, Inc. 
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of Tivoli Systems, Inc.
 *
 * =======================
 * NOTE: Set tab stops = 3
 * =======================
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#if defined(NETWARE)
#   include <stdio.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netdb.h>
#else  /* !defined(NETWARE) */
#	include <sys/socket.h>
#	include <netdb.h>
#	include <netinet/in.h>
#	if !defined(OS2)
#		include <netinet/tcp.h>
#	else
#		define TCP_NODELAY     0x01    /* from netinet/tcp.h */
#		include <signal.h>
#		include <nerrno.h>
#		include <utils.h>
#		define EINTR SOCEINTR
#	endif
#endif /* defined(NETWARE) */
#else  /* defined(WIN32) */
#	include <winsock.h>
#endif /* !defined(WIN32) */


#if !defined(OS2)
#	include <errno.h>
#	include <unistd.h>
#endif

#include "eif.h"

static int
set_ipc_flags(int fd)
{
#if !defined(NETWARE)
	struct protoent	*proto_info;
	int					true_flag;
	int					rc;

	/*
	 * Need prototype number for TCP before setting TCP_NODELAY option.
	 * Then set the flag.
	 * Finally, get the flag to confirm it was changed.
	 */
	if ((proto_info = getprotobyname("tcp")) != NULL) {
		true_flag = 1;

#		ifdef WIN32
			rc = setsockopt((SOCKET) fd, proto_info->p_proto, TCP_NODELAY,
								 (char FAR *) &true_flag, sizeof(true_flag));
#		else
			rc = setsockopt(fd, proto_info->p_proto, TCP_NODELAY,
								 (void *) &true_flag, sizeof(true_flag));
#		endif
	}
	else {
		rc = -1;
	}

	return(rc);
#else  /* defined(NETWARE) */
    return -1;
#endif /* !defined(NETWARE) */
}

/* $Id: sockeif.c,v 1.4 1998/09/03 16:25:32 joe Exp $ */
/*
 * Description:  create ipc client handle
 *
 * Returns: handle on success, NULL on failure
 *
 * Notes/Dependencies: 
 */

eipc_handle_p_t
eipc_create_remote_client(unsigned addr, unsigned short port, eipc_error_t *error)
{
	eipc_handle_p_t     handle;
	struct sockaddr_in server;
	int ret;

	handle = (eipc_handle_p_t) malloc(sizeof(eipc_handle_t));
#if defined(NETWARE)
    if (handle == NULL) {
        return NULL;
    }
#endif /* defined(NETWARE) */

	/*
	 * Don't let SIGALRM break this
	 */
	errno = EINTR;
	handle->fd = -1;

	while ((errno == EINTR) && (handle->fd == -1)) {
	   errno = 0;
	   handle->fd = socket(AF_INET, SOCK_STREAM, 0);
	}

	if (handle->fd < 0) {
		free(handle);
		*error = E_SCALL;
		return 0;
	}

	set_ipc_flags(handle->fd);

	memset((char*)&server,0,sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = port;
	server.sin_addr.s_addr = addr;

	/*
	 * Don't let SIGALRM break this
	 */
	errno = EINTR;
	ret = -1;

	while ((errno == EINTR) && (ret == -1)) {
	   errno = 0;
	   ret = connect(handle->fd, (struct sockaddr*)&server, sizeof(server));
	}

	if (ret < 0) {
#		if defined(PC) && !defined(NETWARE)
#			if !defined(OS2)
				*error = (WSAGetLastError() == WSAECONNREFUSED) ? E_IPC_BROKEN
																				: E_SCALL;
				closesocket(handle->fd);
				free(handle);
#			else
				*error = (errno == ECONNREFUSED) ? E_IPC_BROKEN : E_SCALL;
				soclose(handle->fd);
				free(handle);
#			endif
#		else
			*error = (errno == ECONNREFUSED) ? E_IPC_BROKEN : E_SCALL;
			close(handle->fd);
			free(handle);
#		endif

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
        shutdown( ipch->fd, 2);
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
#if !defined(PC) || defined(NETWARE)
        close(ipch->fd);
#else
#	ifndef OS2
		closesocket(ipch->fd);
#	else
		soclose(ipch->fd);
#	endif
#endif
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
    rv = send(fd, buf, len, 0);
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
eipc_send(eipc_handle_p_t handle, char *data, long len, long type, long id,
                                                          eipc_error_t *error)
{
    eipc_msg_hdr_t hdr;
    int           dlen;
    int           slen;

    /*
    ** Put this check in to avoid sending headers without messages.
    ** T/EC 3.6 server seems to have difficulty dealing with it
    ** when in connection-oriented (CO) mode.
    */
    if (len < 1) {
        return 0;
    }

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
     * HDR_DATA_LEN
     */
    if (len - dlen > 0) {
        if (do_send(handle->fd, data + dlen, len - dlen, error) != 0) {
            return -1;
        }
    }

    return 0;
}
