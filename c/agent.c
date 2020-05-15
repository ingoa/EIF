/*
 * Component Name: EIF
 *
 * $Date: 1998/07/22 20:37:24 $
 *
 * $Source: /net/nfs1/projects/tivoli/cvsroot/tecad_nw4/src/eif/agent.c,v $
 *
 * $Revision: 1.5 $
 *
 * Description:
 *      TEC agent communication library for non-TME agents
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

/* test "agent" for either a TME or non-TME connection.  Link with the
 * appropriate libraries to build a TME version or non-TME version
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#if defined(SOLARIS2)
	extern int getopt(int, char *const *, const char *);
#endif

#include "agent_comm.h"

#if defined(NETWARE)
#include "getopt.h"
extern void joe_cleanup_heap();
void __WATCOM_Prelude() {}

/*
** With more recent NetWare SDKs, stat() and fstat() are macro aliases
** to stat_411() and fstat_411().  Since these symbols don't exist for
** NetWare 4.10, undefine these macros to force referencing the real
** stat() and fstat() symbols.
*/
#if defined(stat)
#undef stat
#endif /* defined(stat) */
#if defined(fstat)
#undef fstat
#endif /* defined(fstat) */

/*
** With more recent NetWare SDKs, ctype functions are macro aliases that
** reference the symbol __ctype which is not present in NetWare 4.10.
** Undefine these macros and force referencing the actual ctype function
** symbols.
*/
#if defined(isdigit)
#undef isdigit
#endif /* defined(isdigit) */

#endif /* defined(NETWARE) */

static char s[2048];

void
print_usage_and_exit(char **argv)
{
    fprintf(stderr, "Usage: agent [-l cfgfile] [-c] [-p port] [server_host]\n");
    exit(1);
}


int
main(int argc, char *argv[])
{

#if defined(PC) && !defined(NETWARE)
#	define exit return
#endif

    int c;
    extern int     optind;
    extern char    *optarg;
    char           *location = NULL;
    char           cfg_file_name[FILE_LEN];
    char           *cfg_file = NULL;
    unsigned short port = 0;
    tec_handle_t   th;
    extern int     tec_errno;
    int            connectionless = 0;
    int            oneway = 0;
    tec_delivery   type = use_default;
    struct         stat   sbuf;
    int            rc;

#if defined(NETWARE)
	atexit(&joe_cleanup_heap);
#endif /* defined(NETWARE) */

    while ((c = getopt(argc, argv, "Ccl:p:")) != -1) {
        switch (c) {
            case 'l':
                    /* Specifies location of the agent configuration file */
                strcpy(cfg_file_name, optarg);
                if (stat(cfg_file_name, &sbuf) != 0) {
                        /* not existant */
                    printf("%s: Not existant\n", cfg_file_name);
                    print_usage_and_exit(argv);
                }
                cfg_file = cfg_file_name;
                break;
            case 'p':
                    /* Specifies the port to connect to on the Server */
                if (!isdigit(*optarg)) {
                    print_usage_and_exit(argv);
                }
                port = (unsigned short) atol(optarg);
                break;
            case 'C':
                oneway++;
            case 'c':
                connectionless++;
                type = connection_less;
                break;
            default:
                print_usage_and_exit(argv);
                break;
        }
    }

    if (optind + 1 == argc) {
        location = argv[optind];
    }

        /* If cfg_file is NULL, the agent_init will look for
           /etc/Tivoli/tecad_eif.conf, if this is unavailable, all 
           parameters to tec_create_handle must be provided */

    tec_agent_init(cfg_file);

    if ((th = tec_create_handle(location, port, oneway, type)) == NULL) {
        fprintf(stderr, "tec_create_handle failed, errno = %d\n", tec_errno);
        exit(1);
    }

	/*
	 * Try to flush any buffered events before processing new ones.
	 * Note: If the TEC Server or Network is down all buffered
	 *   events will still try to be sent; there is no abort - yet.
	 */
	tec_flush_events(th);

        /* test version - read lines from stdin and send them as "events" */
    while (fgets(s, 2048, stdin) != NULL) {

            /* keep the newline and adda ^A to the end, this makes
               the message "<msg>\n\001" */
        s[strlen(s)] = '\001';
        s[strlen(s)+1] = '\0';

            /* tec_put_event will send the event to the selected server */
        rc = tec_put_event(th, s);
        if (rc == -1) {
            fprintf(stderr, "tec_put_event failed, errno = %d\n", tec_errno);
            exit(1);
        }
    }

        /* eof - close the connection and exit */
    tec_destroy_handle(th);
    exit(0);

#if defined(NETWARE)
    return 0;
#endif /* defined(NETWARE) */
}  /* main */
