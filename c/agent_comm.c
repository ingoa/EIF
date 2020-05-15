/*
 * Component Name: EIF
 *
 * $Date: 1996/12/09 23:07:57 $
 *
 * $Source: 
 *
 * $Revision: 1.35 $
 *
 * Description:
 *      TEC agent communication library for non-TME agents
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef DEBUG
#	ifndef SOLARIS2
#		include <arpa/inet.h>
#	else
#		include <sys/socket.h>
#		include <netinet/in.h>
#		include <arpa/inet.h>
#	endif
#endif

static int send_events = 1;

#ifdef HP_UX
#	include <unistd.h>
#endif

#ifndef WIN32
#ifdef OS2
#	include <stdlib.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netdb.h>
#	include <signal.h>
#	include <utils.h>
#	ifndef u_long
#		define u_long unsigned long
#	endif
#	ifndef u_short
#		define u_short unsigned short
#	endif
#	ifndef u_int
#		define u_int unsigned short
#	endif
	extern unsigned short pmap_getport(struct sockaddr_in *, 
		u_long, u_long, u_int);
#elif defined(NETWARE)
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <netdb.h>
#   include <nwthread.h>
#else
#	include <sys/socket.h>
#	include <sys/signal.h>
/*
 * This bizarre define for AIX is to force the inclusion
 * of the declaration for struct rpcent and the function
 * getrpcbyname from header file netdb.h
 */
#	ifdef AIX
#		define _SUN
#	endif

#	include <netdb.h>

#	ifdef AIX
#		undef _SUN
#	endif

#endif
#else
#	include <winsock.h>
#	include <rpc/rpc.h>

#	define strdup _strdup
#endif

#include "tec_local.h"
#include "agent_comm.h"
#include "eif_common.h"
#include "eif.h"
#include "slist.h"

#if defined(NETWARE)
#define sleep(sec) delay((sec) * 1000)
#endif /* defined(NETWARE) */
int TECAD_no_connection = FALSE; /* did the last tec_put_event() fail? */
int tec_errno = 0;

char                    *last_msg=NULL;

/* Internal structure for a non-TME TEC handle */

typedef struct {
	tec_handle_common_t common;  /* Fields common to all EIF libraries         */
	int current;
	long ip_addr[TECEIF_MAX_SERVERS];   /* TEC server IP address */

	/* 
	 * once a handle is created, the `port' and `ipch' may change
	 * when we reconnect possibly to another host.
	 */
	unsigned short port[TECEIF_MAX_SERVERS];
   char use_srvport[TECEIF_MAX_SERVERS]; /* 1/0 depending on whether or not
														 the port comes from ServerPort 
														 config entry */

	tec_delivery   type;  /* connection-less or connection-oriented */

	/* IPC handle for sending events. 
	   In the CO mode, when the primary connection is broken, this 
	   will be set to some secondary server, if any. NULL means no 
	   connection.
	   */
	eipc_handle_p_t ipch; 
	
	long           ev_count;  /* number of events sent on `ipch' - used only in CO mode */
} tec_handle_s_t;

/*
 * Purpose: 
 *    SIGPIPE signal handler
 *
 * Description: 
 *    
 * Returns: 
 *
 * Notes/Dependencies: 
 */

#if !defined(NETWARE)
#ifdef AIX

static void
catch_pipe(int sig)

#else

static void
catch_pipe(int sig, int code, struct sigcontext *scp, char *addr)

#endif

{
#	ifdef PC
		abort();
#	else
		signal(SIGPIPE, catch_pipe);
#	endif
}
#endif /* NETWARE */

/*
 * Purpose: 
 *    Acquire the port the T/EC Server is listening to
 *
 * Description: 
 *      Uses portmapper to locate the server port.
 *
 * Returns: 
 *    Port Numver 
 *
 * Notes/Dependencies: 
 */

#if 0
/* I'm removing this, since we're no longer compiling with Watcom - wpw */

#ifdef OS2
 /* The IBM rpc libraries have an unusual calling
  * convention. The first three paramaters are passed
  * in registers, then all others are passed on the
  * stack. The stack pointer is also decremented as
  * if all of the params were passed on the stack.
  * This causes a misaligment when compiling with
  * Watcom. The following assembly compensates for
  * this problem.
  */
extern load_registers(struct sockaddr_in *, u_long, u_long, u_int);
#pragma aux load_registers = \
	"mov eax, [esp]" \
	"mov edx, +4[esp]" \
	"mov ecx, +8[esp]";
#endif /* OS2 */
#endif


static unsigned short
tec_get_port(long server_addr)
{

#if !defined(NETWARE)
	struct rpcent      *rpcinfo;
#endif /* !defined(NETWARE) */
	struct sockaddr_in addr;
	u_long             prognum;
	unsigned short     port = 0;

	/* connect to server via portmapper.  First get rcp program number 
	   being used by the TEC server */
#ifdef PC
	prognum = TEC_SERVER_NUMBER;
#else
	rpcinfo = (struct rpcent *)NULL;
	errno = EINTR;
	while ((errno == EINTR)&&(rpcinfo == (struct rpcent *)NULL)) {
	   errno = 0;
	   rpcinfo = getrpcbyname(TEC_SERVER_NAME);
	}
	prognum = (rpcinfo == NULL) ? TEC_SERVER_NUMBER : rpcinfo->r_number;
#endif

	memset(&addr,'\0',sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = server_addr;
	addr.sin_port = port;

	errno = EINTR;
	while ((errno == EINTR)&&(port == 0)) {
	   errno = 0;
#if 0
#ifdef OS2
           load_registers(&addr, prognum, TEC_VERSION_NUMBER, IPPROTO_TCP);
#endif
#endif
	   port = pmap_getport(&addr, prognum, TEC_VERSION_NUMBER, IPPROTO_TCP);
	}

	if (port == 0) {
		tec_errno = E_SCALL;
		return 0;
	}
	return(htons(port));
}

/* ------------------------------------------------------------------------- */

/*
 * Purpose: 
 *    connect to the TEC server without requiring the TME framework
 *
 * Description: 
 *    currently uses our IPC library.
 *    Uses portmapper to locate the server port.
 *    If location is null, we use the config file to determine
 *    possibly multiple server hosts.
 *
 * Returns: 
 *    IPC handle
 *
 * Notes/Dependencies: 
 */

tec_handle_t
tec_create_handle(char *location, unsigned short port, int oneway,
						tec_delivery type)
{
	tec_handle_s_t *th;
	struct hostent *server_host;
	char           *env;
	int            i;
        int            loc=0;
        int            max;
        char           **ports;

	/* rsm */
	int			   new_max_locations;

	/*
	 * Allocate a new handle
	 */
	th = malloc(sizeof(*th));
	th->current = -1;
	th->ev_count = 0;
	
	if ((tec_errno = common_agent_handle_init(&th->common, location,NULL)) > 0) {
		free(th);
		tec_errno = E_HOST;
		return(NULL);
	}

	/*
	 * If in test mode, we are done
	 */
	if (th->common.test_mode)
		return(th);

	/*
	 * Get the IP address of the server host. If the address is not in the
	 * dotted notation, resolve the name.
	 */
	for (i = 0; i < th->common.max_locations; i++) {
		th->ip_addr[i] = inet_addr(th->common.location[i]);

		/*
		 * On WinSock, INADDR_NONE is really -1
		 */
		if (th->ip_addr[i] == (long) -1) {

			/* rsm: if we can't find the host on the network, we make that
			 *      entry NULL and we'll throw it out a bit later
			 */
			if ((server_host = gethostbyname(th->common.location[i])) == NULL) {
				th->common.location[i] = NULL;
				continue;
			}
			memcpy(&th->ip_addr[i], server_host->h_addr_list[0],
					sizeof(th->ip_addr));
		}
	}


	/* rsm */
	new_max_locations = th->common.max_locations;

	/* rsm: this is where we consolodate the space in the string array */
	for(i = 0; i < th->common.max_locations; i++)
	{
		if(th->common.location[i] != NULL)
		{
			if(i > 0)
			{
				int		k = i;

				while((th->common.location[k - 1] == NULL) && (k > 0))
					k--;

				if(th->common.location[k] == NULL)
				{
					th->common.location[k] = th->common.location[i];
					th->common.location[i] = NULL;
				}

			}
		}
		else
		{
			new_max_locations--;
		}
	}

	/* rsm */
	th->common.max_locations = new_max_locations;

	for (i = 0; i < th->common.max_locations; i++) {
		th->port[i] = 0;
		th->use_srvport[i] = 0;
	}
	
        if (location) loc=1;
                /*
                 * Search for keyword 'ServerPort' in agent config
                 */
        ports = tec_split_entries("ServerPort", &max
);

        for (i = loc; i < max+loc; i++) {
                th->port[i] = htons((unsigned short)atoi(ports[i-loc]));

                        /*
                         * A zero port still means use the portmapper
                         */
                if (th->port[i])
                        th->use_srvport[i] = 1;
        }

        if ( port != 0 ) {
		/*
		 * Do not use portmapper, since we are given a port
		 */
		th->port[0] = htons(port);
		th->use_srvport[0] = 1;
	}

	/*
	 * If not found, 0 in config file or there is more than one server
	 * (but input `port' covers only the first) use portmapper 
	 */
	for (i = 0; i < th->common.max_locations; i++) {
		if (th->port[i] == 0)
			th->port[i] = tec_get_port(th->ip_addr[i]);
	}

	if (type == use_default) {

		/*
		 * Use connection_less, except if CO found in config file
		 */
		th->type = connection_less;

		if ((env = tec_agent_getenv("ConnectionMode")) != NULL) {
			if ((!strcmp(env, "co")) || (!strcmp(env, "CO"))
				 || (!strcmp(env, "connection_oriented")))
			{
				th->type = connection_oriented;
			}
		}
	}
	else
		th->type = type;

	/*
	 * If CO mode, create an IPC client
	 */
	if (th->type == connection_oriented) {
		for (i = 0; i < th->common.max_locations; i++) {
			th->ipch = eipc_create_remote_client(th->ip_addr[i], th->port[i], &tec_errno);
			if (th->ipch != NULL) {
				th->current = i;
				break;
			}
		}

		/*
		 * Set up a signal handler for SIGPIPE, for recovery from broken
		 * connection
		 */
#ifndef PC
		signal(SIGPIPE, catch_pipe);
#endif
	}
	else
		th->ipch = NULL;

	return ((tec_handle_t)th);
}

/* ------------------------------------------------------------------------- */

/*
 * Toggle sending
 */


int
tec_toggle_send()
{
  if (send_events)
    send_events = 0;
  else
    send_events = 1;

  return(send_events);
}

/*
 * Purpose: 
 *    send an event to the TEC server
 *
 * Description: 
 *    non-TME version
 *
 * Returns: 
 *    number of bytes written
 *
 * Notes/Dependencies: 
 */

long
tec_put_event(tec_handle_t handle, char *message)
{
	unsigned long  length;
	tec_handle_s_t *th = (tec_handle_s_t *) handle;
	char				*new_msg = message;
	int				free_new_msg = 0;
	int				destroy_handle = 0;
	int				i;
  struct hostent	*server_host = NULL;

	/* rsm: variables used to figure out the size of the buffer */
	long                buffer_size;
	char                *env = NULL;

	length = (unsigned long) strlen(message);

	/*
	 * This stuff was already done before this event was put into the buffer
	 */
	if (!th || !th->common.in_flush_events) {

		/*
		 * Make sure the event has the right termination
		 */
		if ((new_msg = tec_normalize_msg(message, length)) != NULL) {
			free_new_msg = 1;
			length = (unsigned long) strlen(new_msg);
		}
		else
			new_msg = message;

		/*
		 * Check if event should be discarded due to filtering
		 */
		if (!th->common.dont_filter
			 && tec_filter_event(new_msg, FILTER_EVENT_TYPE))
		{
			if (free_new_msg)
				free(new_msg);

			return(0);
		}
	}

	if (!send_events)
	{
		i = tec_buffer_event(new_msg);
		if (free_new_msg)
			free(new_msg);
		return(i);
	}

	/*
	 * Create a handle if necessary
	 */
	if (th == NULL) {
		th = (tec_handle_s_t *) tec_create_handle(NULL, 0, 0, connection_less);
		destroy_handle = 1;
	}

	/*
	 * If we are processing a real event, assume the connection is ok.  If it's
	 * not, this will be set to TRUE at the end right before calling
	 * tec_buffer_event()
	 */
	if (length != 0)
		TECAD_no_connection = FALSE;

	/*
	 * if we are in test mode, then just dump the stuff to the output file and
	 * return
	 */
	if (th->common.test_mode) {
		fwrite(new_msg, sizeof (char), length, th->common.test_out);
		putc('\n', th->common.test_out);
		fflush(th->common.test_out);

		if (free_new_msg)
			free(new_msg);

		if (destroy_handle)
			tec_destroy_handle((tec_handle_t) th);

		return(length);
	}

	if(!th->common.in_flush_events)
	{
		buffer_size = tec_event_buffer_size(NULL, NULL, NULL);

		if(buffer_size < 0) {    
		    /* This probably means that the buffer doesn't exist,
		     *	and it will be created later on, so let's consider
		     *	it a buffer of size 0 */

		    buffer_size = 0;
		}
	}

	if (th->type == connection_oriented) {
		int rc=0;
		if (th->ipch != NULL && 
		   (th->current == 0 || th->ev_count < MAX_EVENTS_ON_SCHANNEL)) 
		{

			int rc;

			/* rsm: Check if the cache file is bigger than 0.  If not, 
			   we won't bother to do anything except send the new event in */
			if((buffer_size > 0) && (!th->common.in_flush_events)) {
				/* rsm: Try to send a NULL event.  If it succeeds, 
				   then flush any buffered events */
				rc = eipc_send(th->ipch, "", 0, 0, 0, &tec_errno);


				if(rc == 0)
				    tec_flush_events(handle);
			}

			/* Now send the new event */
			rc = eipc_send(th->ipch, new_msg, length, 0, 0, &tec_errno);
			if(rc == 0) {
				th->ev_count++;

				if (last_msg) free(last_msg);
				last_msg=(char*)malloc(length+1);
				strcpy(last_msg,new_msg);

                        	if (free_new_msg)
                        	      free(new_msg);

				if (destroy_handle)
				tec_destroy_handle((tec_handle_t) th);

				return(length);
			}
		}

		if (th->ipch) {
			/* The old connection is broken. We wait for the retry_window 
				in hope of getting back a connection to the same host or 
				possibly a more primary server before switching to a secondary.
				This takes care of quick server restarts to load a new rulebase 
				for example.
				*/

			long wait_until = time(NULL) + th->common.retry_window;

			/* We cannot just sleep on retry_window since alarms or
				other signals will wake us up.
				*/
			while (time(NULL) < wait_until) {
#if defined(OS2) || defined(NETWARE)
			        sleep(5);
#else
#ifndef PC
				sleep(5);
#else
#ifndef OS2
				Sleep(5000);
#else
				Sleep(5);
#endif
#endif
#endif
			}

			eipc_destroy_handle(th->ipch);
			th->ipch = NULL;
			th->current = -1;
			th->ev_count = 0;
		}

		/* try another host/port; here we get a new port immediately and not
			try to reuse the old one first (unlike connectionless mode). */
		for (i = 0; i < th->common.max_locations; i++) {
			if (th->use_srvport[i] == 0) {
				th->port[i] = tec_get_port(th->ip_addr[i]);
			}
			if (th->port[i] != 0) {
				th->ipch = eipc_create_remote_client(th->ip_addr[i], th->port[i], 
								 &tec_errno);
			}

			/* If we can't create the ipc connection, re-resolve name to IP address,
			 * in case host's IP address has changed. If the ipc connection still
			 * can't be established after re-resolving the name to an address, then
			 * we will move on to the next TEC server in the conf file, etc.
			 */
			if (th->ipch == NULL) {

				th->ip_addr[i] = inet_addr(th->common.location[i]);
				if (th->ip_addr[i] == (long) -1) {
					server_host = gethostbyname(th->common.location[i]);
					if(server_host != NULL) {
						memcpy(&th->ip_addr[i], server_host->h_addr_list[0], sizeof(th->ip_addr[0]));
						if (th->use_srvport[i] == 0) {
							th->port[i] = tec_get_port(th->ip_addr[i]);
						}
 	      		if (th->port[i] != 0) {
							th->ipch = eipc_create_remote_client(th->ip_addr[i], th->port[i], &tec_errno);
 		     		}
#ifdef DEBUG
			  			printf("re-resolved host name to IP address connection-oriented\n");
			  			printf("host name is: %s \n", th->common.location[i]);
		  				printf("new ip address is: %s \n", 
										inet_ntoa(*(struct in_addr*)(server_host->h_addr_list[0])));
#endif
					}
				}
#ifdef DEBUG
				else {
			  	printf("host address was provided in numerical format - connection oriented\n");
					printf("host name is: %s \n", th->common.location[i]);
				}
#endif
		  }

			if (th->ipch != NULL) {

				int rc;

				/* if the send succeeds, then flush any buffered
					events and return */
                   if((buffer_size > 0) && (!th->common.in_flush_events)) {
                    /* rsm: Try to send a NULL event.  If it succeeds, 
					 * then flush any buffered events */
                    rc = eipc_send(th->ipch, "", 0 /*length*/, 0, 0, &tec_errno);

                    if(rc == 0) {
			tec_flush_events(handle);
                    }
                }

                /* Now send the new event */
                rc = eipc_send(th->ipch, message, length, 0, 0, &tec_errno);
                if(rc == 0) {

					th->current = i;			
					th->ev_count++;

					if (last_msg) free(last_msg);
					last_msg=(char*)malloc(length+1);
					strcpy(last_msg,new_msg);

                        		if (free_new_msg)
                        		      free(new_msg);

					if (destroy_handle)
						tec_destroy_handle((tec_handle_t) th);


					return(length);
				}
				else {
					eipc_destroy_handle(th->ipch);
					th->ipch = NULL;
					th->current = -1;
					th->ev_count = 0;
				}
			}
		}

	}
	else {
		/*
		 * We are connectionless, so create a new connection to the server.
		 * If the connection fails, it could have been because the server was
		 * restarted and the old port information is bogus, so try to get a
		 * new port, and try to get a connection again
		 */
		for (i = 0; i < th->common.max_locations; i++) {
			/*
			 * try the old port first, more than likely the port is the same.
			 * Contacting portmapper on every put would be expensive.
			 */
			th->ipch = eipc_create_remote_client(th->ip_addr[i], th->port[i], &tec_errno);
			if (th->ipch == NULL) {
			  if (th->use_srvport[i] == 0) {
				 th->port[i] = tec_get_port(th->ip_addr[i]);
			  }
			  if (th->port[i] != 0) {
				 th->ipch = eipc_create_remote_client(th->ip_addr[i], th->port[i], &tec_errno);
			  }
			}

			/* If we can't create the ipc connection, re-resolve name to IP address,
			 * in case host's IP address has changed. If the ipc connection still
			 * can't be established after re-resolving the name to an address, then
			 * we will move on to the next TEC server in the conf file, etc.
			 */
			if (th->ipch == NULL) {
				th->ip_addr[i] = inet_addr(th->common.location[i]);
				if (th->ip_addr[i] == (long) -1) {
					server_host = gethostbyname(th->common.location[i]);
					if(server_host != NULL) {
						memcpy(&th->ip_addr[i], server_host->h_addr_list[0], sizeof(th->ip_addr[0]));
						if (th->use_srvport[i] == 0) {
							th->port[i] = tec_get_port(th->ip_addr[i]);
						}
       			if (th->port[i] != 0) {
							th->ipch = eipc_create_remote_client(th->ip_addr[i], th->port[i], &tec_errno);
      			}
#ifdef DEBUG
			  		printf("re-resolved host name to IP address - connectionless\n");
			 			printf("host name is: %s \n", th->common.location[i]);
		 		 		printf("new ip address is: %s \n", 
										inet_ntoa(*(struct in_addr*)(server_host->h_addr_list[0])));
#endif
					}
				}
#ifdef DEBUG
				else {
			  	printf("host address was provided in numerical format - connectionless \n");
					printf("host name is: %s \n", th->common.location[i]);
				}
#endif
			  }

			/* if we finally got a connection, then send the data */
			if (th->ipch != NULL) {

			    int rc;

			    if((buffer_size > 0) && (!th->common.in_flush_events)) {
				/* rsm: we have a buffer to work with and we're 
				 * not currently flushing the buffer through the 
				 * recursive call from tec_flush_events()
				 */

				/* rsm: send a NULL event in order to test for 
				 * the viability of the connection */
				rc = eipc_send(th->ipch, "", 0, 0, 0, &tec_errno);

#if 0
				eipc_shutdown(th->ipch);
				eipc_destroy_handle(th->ipch);
				th->ipch = NULL;
#endif

				/* if the send succeeded, then flush any buffered events */
				if (rc == 0) {
					int sent;
					sent = tec_flush_events(handle);
				}
			    }

                if (th->ipch == NULL)
                {
			    /* rsm: Send our new event now that the buffer's 
					     * been sent successfully */
			    for (i = 0; i < th->common.max_locations; i++) {
				/* try the old port first, more than likely the port is
				 * the same. Contacting portmapper on every put would be
				 * expensive.
				 */

				th->ipch = eipc_create_remote_client(th->ip_addr[i],
								     th->port[i],
								     &tec_errno);
				if (th->ipch == NULL) {
				    if (th->use_srvport[i] == 0) {
					th->port[i] = tec_get_port(th->ip_addr[i]);
				    }

				    if (th->port[i] != 0) {
					th->ipch = eipc_create_remote_client(th->ip_addr[i],
									     th->port[i],
									     &tec_errno);
				    }
				}

				/* rsm: my only concern here is that the return 
				 * value from eipc_send isn't checked before new_msg 
				 * is freed and gotten rid of -- could this be 
				 * a problem? */
				if(th->ipch != NULL)
				{
				    rc =  eipc_send(th->ipch, new_msg, length, 0, 0, &tec_errno);
				    eipc_shutdown(th->ipch);
				    eipc_destroy_handle(th->ipch);
				    th->ipch = NULL;

				    if (free_new_msg)
					    free(new_msg);
				    if (destroy_handle)
					    tec_destroy_handle((tec_handle_t) th);

				}
			    }
                }
                else
                {
				    rc =  eipc_send(th->ipch, new_msg, length, 0, 0, &tec_errno);
                    if (!th->common.in_flush_events)
                    {
				        eipc_shutdown(th->ipch);
				        eipc_destroy_handle(th->ipch);
				        th->ipch = NULL;
                    }

				    if (free_new_msg)
					    free(new_msg);
				    if (destroy_handle)
					    tec_destroy_handle((tec_handle_t) th);
                }

			    return(length);
			}
		}
	}

	/*
	 * if we got here, then we have not sent the message to the Event Server,
	 * so we buffer it for sending later
	 */

	/*
	 * failed to create a connection for a real message, set to TRUE
	 */
	if (length != 0)
	   TECAD_no_connection = TRUE;

        if (last_msg) {
			i = tec_buffer_event(last_msg);
			free(last_msg);
			last_msg=NULL;
        }

	i = tec_buffer_event(new_msg);

	if (free_new_msg)
		free(new_msg);

	if (destroy_handle)
		tec_destroy_handle((tec_handle_t) th);

	return(i);
}

/* this is defined here so that the EXTERN_DECL from agent_comm.h isn't undefined */
long
tec_put_event_to_location(tec_handle_t handle, char *message) { return(0); }

/* ------------------------------------------------------------------------- */

/*
 * Purpose: 
 *    destroy TEC handle - non-TME version
 *
 * Description: 
 *
 * Returns: 
 *
 * Notes/Dependencies: 
 */

void
tec_destroy_handle(tec_handle_t handle)
{
	tec_handle_s_t *th;

	th = (tec_handle_s_t *)handle;

	if (th->type == connection_oriented) {
		if (th->ipch)  {
			eipc_shutdown(th->ipch);
			eipc_destroy_handle(th->ipch);
		}
	}
	/* only the first location was allocated and divided up */
	free(th->common.location[0]);
	free(th);
}

int
tec_connection_status(tec_handle_t t_handle)
{
	tec_handle_s_t *th = (tec_handle_s_t *) t_handle;

	return(((th->type == connection_less) || (th->ipch != NULL)) ? 1 : 0);
}



/* ------------------------------------------------------------------------- */

/*
 * Purpose: 
 *    initialization for non-TME TEC agent
 *
 * Description: 
 *
 * Returns: 
 *
 * Notes/Dependencies: 
 */

int
tec_agent_init(char *cfgfile)
{
	return(common_agent_init(cfgfile));
}

int
tec_method_agent_init(char *cfgfile)
{
	return(common_agent_init(cfgfile));
}

/*
   These three functions (tec_is_secure/wrapper_thread_delay/wrapper_timeout)
   have been created in both libtec.a & libteceif.a so they can be called
   from liblogfile.a.  It is not until the Logfile Adapter is linked
   that we know if we will be secure or unsecure, so these functions are
   needed in both libraries or else the unsecure adapter won't compile.

   10/6/97 - Sean Starke
*/

/* Are we secure or unsecure? */
int
tec_is_secure()
{
  return FALSE;
}

/* Wrapper function for tmf_thread_delay().  This will not be called
   when running the unsecure adapter, but must exist so it will compile.
   When secure adapter calls it the arguments are just passed on. */
int
wrapper_thread_delay(struct timeval *timespan)
{
	return 0;
}

/* Wrapper function for tmf_timeout().  This will not be called
   when running the unsecure adapter, but must exist so it will compile.
   When secure adapter calls it the arguments are just passed on. */
int
wrapper_timeout(void (*func)(), void *data, struct timeval *timer)
{
	return 0;
}
