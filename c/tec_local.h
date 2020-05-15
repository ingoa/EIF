/*
 * Component Name: EIF
 *
 * Dependencies:
 *
 * $Date: 1996/10/22 19:08:41 $
 *
 * $Source: /tivoli/homes/1/tgreenbe/convert/tec/eif/tec_local.h,v $
 *
 * $Revision: 1.8 $
 *
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

#if !defined(EXTERN_DECL)
#	if defined(__cplusplus)
#		define EXTERN_DECL(entry, args) extern "C" entry args
#		define VOID_DECL void
#	elif defined(__STDC__)
#		define EXTERN_DECL(entry, args) extern entry args
#		define VOID_DECL void
#	else
#		if !defined(const)
#			define const
#		endif

#		define EXTERN_DECL(entry, args) extern entry()
#		define VOID_DECL
#	endif
#endif

#ifndef PC
#	include <netinet/in.h>

#	ifdef AIX    /* the AIX rpc stuff are wrapped by #ifdef _SUN */
#		define _SUN
#		include <netdb.h>
#		undef _SUN
#	else
#		include <netdb.h>
#	endif

#	ifdef SVR4
#		ifndef DGUX
#			include <rpc/rpcent.h>   /* for RPC definitions on svr4 */
#		endif
#	endif
#endif

#include <stdlib.h>

#ifdef WIN32
#	ifndef S_ISBLK
#		define	S_ISBLK(a)	(a&0)
#	endif
#
#	ifndef S_ISDIR
#		define	S_ISDIR(a)	(a&_S_IFDIR)
#	endif
#
#	ifndef S_ISCHR
#		define	S_ISCHR(a)	(a&_S_IFCHR)
#	endif
#
#	ifndef S_ISFIFO
#		define	S_ISFIFO(a)	(a&_S_IFIFO)
#	endif
#
#	ifndef S_ISREG
#		define	S_ISREG(a)	(TRUE)/*(a&_S_IFREG)*/
#	endif
#
#	ifndef S_IRUSR
#		define	S_IRUSR		0
#	endif
#
#	ifndef S_IXUSR
#		define	S_IXUSR		0
#	endif
#
#endif

#define AGENT_PREFIX		"agent"
#define TME_AGENT_PREFIX	"tme_agent"
#define CONNECTIONLESS_AGENT_PREFIX	"conn_agent"

#define MAX_PENDING		5

#define TEC_SERVER_NAME		"tec_server"
#define TEC_SERVER_NUMBER	100033057
#define TEC_VERSION_NUMBER	1

#if !defined(OS2)

EXTERN_DECL(unsigned short pmap_getport, (struct sockaddr_in *addr, unsigned long prognum,
		unsigned long versnum, int protocol));

EXTERN_DECL(short pmap_set, (unsigned long prognum, unsigned long versnum, int protocol,
		unsigned short port));

EXTERN_DECL(short pmap_unset, (unsigned long prognum, unsigned long versnum));

#endif

EXTERN_DECL(int tec_filter_event, (char *event, int filter_type));

EXTERN_DECL(int common_agent_init, (char *cfgfile));

#ifdef PC
	EXTERN_DECL(int MPinit, (int));
#endif

/* In the CO mode, once we get a connection, we will send at most
   MAX_EVENTS_ON_SCHANNEL events on a secondary connection. 
   Then we come back to check the server hosts in the order of
   their listing in the conf file, and possibly get a connection
   to the primary.
   */
#define MAX_EVENTS_ON_SCHANNEL   64

/* maximum number of alternate servers */
#define TECEIF_MAX_SERVERS   8

/* number of seconds to try to re-establish a dropped connection */
#define RETRY_WINDOW        120

extern char** tec_split_entries(char* env, int* num);
