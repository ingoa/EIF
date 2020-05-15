#ifndef agent_comm_h
#define agent_comm_h

/*
 * Component Name:  EIF
 *
 * Dependencies:
 *
 * Description:
 *        Header file for TME agents
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

#ifndef PC
#	include <sys/time.h>
#endif

#define FILTER				"Filter:"
#define FILTER_CACHE			"FilterCache:"
#define TECAD_FILTERMODE		"FilterMode"
#define FILE_LEN			256
#define PREFILTER			"PreFilter:"
#define TECAD_PFLTMODE			"PreFilterMode"
#define TECAD_EVENT_TO_CATCH		"NumEventsToCatchUp"


#ifdef WIN32
#define EIF_GATEWAY_CONF_FILE	"\\drivers\\etc\\Tivoli\\tec\\tec_gateway.conf"
#elif defined(NETWARE)
#define EIF_GATEWAY_CONF_FILE   "SYS:ETC\\TIVOLI\\TEC\\TECGTWAY.CFG"
#else
#define EIF_GATEWAY_CONF_FILE	"/etc/Tivoli/tec/tec_gateway.conf"
#endif

/*
 * All TEC Events must end with this
 */
#define TECAD_EVENT_DELIMITER			"END\n\001"
#define TECAD_EVENT_DELIMITER_LEN	5
#define TECAD_EVENT_END_CHAR			'\001'


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

#ifndef CODE_ID
#	define CODE_ID(id) static char *_code_id(char *s) {return _code_id(s=id);}
#endif

/*
 * Types of event filters.
 *
 * FILTER_EVENT_TYPE         Filters events into/from being sent to the server
 * FILTER_EVENT_CACHE_TYPE   Filters events into/from being buffered if server
 *                           is unavailable.
 */
typedef enum {
    FILTER_EVENT_TYPE,
    FILTER_EVENT_CACHE_TYPE
} FilterType;

/*
 * Types of connections to the TEC Server.
 *
 * connection_less      A new connection is opened and closed for each event
 * connection_oriented  One open connection is maintained for sending all events
 * use_default          Use the default value specified in the EIF *.conf file
 *                      with the "ConnectionMode" setting.
 */
typedef enum {
	connection_less,
	connection_oriented,
	use_default
} tec_delivery;

typedef void *tec_handle_t;

extern int tec_errno;
extern int TECAD_no_connection;

EXTERN_DECL(tec_handle_t tec_create_handle, (char *, unsigned short, int,
                                             tec_delivery));
     
EXTERN_DECL(long tec_put_event, (tec_handle_t, char *));

EXTERN_DECL(int tec_buffer_event, (char *));

EXTERN_DECL(int tec_flush_events, (tec_handle_t));

EXTERN_DECL(void tec_destroy_handle, (tec_handle_t));

EXTERN_DECL(int tec_agent_init, (char *));

EXTERN_DECL(int tec_method_agent_init, (char *));

EXTERN_DECL(char *tec_agent_getenv, (char *));

EXTERN_DECL(int tec_connection_status, (tec_handle_t));

EXTERN_DECL(int tec_buffer_event_to_file, (char *, char *));

EXTERN_DECL(int tec_flush_events_from_file, (tec_handle_t, char *));

EXTERN_DECL(char *tec_normalize_msg, (char *, unsigned long));

#ifdef WIN32

EXTERN_DECL(long tec_event_buffer_size, (tec_handle_t, char *, HANDLE *));

#else

EXTERN_DECL(long tec_event_buffer_size, (tec_handle_t, char *, int *));

#endif




#ifdef WIN32
/* for NT pre_filter */
#define MAX_EVENT_LOGS   3
#define MAX_EVENT_SRCS  16
#define MAX_EVENT_IDS   16
#define MAX_EVENT_TYPES  6
 
#define KEEP_IT         0
#define DISCARD_IT      1
 
 
typedef struct _NT_pre_filter  {
   int           event_id[MAX_EVENT_IDS+1];
   char *        event_log[MAX_EVENT_LOGS+1];
   char *        event_src[MAX_EVENT_SRCS+1];
   char *        event_type[MAX_EVENT_TYPES+1];
   struct _NT_pre_filter *next;
}  NT_pre_filter;

#elif defined(NETWARE)
/*
** For NetWare 4.x prefilter.
*/

#define MAX_EVENT_SRCS          16
#define MAX_EVENT_IDS           16
#define MAX_EVENT_SEVERITIES    16
#define MAX_EVENT_LOCUSES       16
#define MAX_EVENT_CLASSES       16

#define SUBKEY_SOURCE           "Source"
#define SUBKEY_EVENT_ID         "EventID"
#define SUBKEY_SEVERITY         "Severity"
#define SUBKEY_LOCUS            "Locus"
#define SUBKEY_CLASS            "Class"

#define KEEP_IT                 0
#define DISCARD_IT              1

typedef struct _NW4_pre_filter
{
    char*                   event_src[MAX_EVENT_SRCS + 1];
    int                     event_id[MAX_EVENT_IDS + 1];
    int                     event_severity[MAX_EVENT_SEVERITIES + 1];
    int                     event_locus[MAX_EVENT_LOCUSES + 1];
    int                     event_class[MAX_EVENT_CLASSES + 1];
    struct _NW4_pre_filter* next;
} NT_pre_filter;
 
#endif


#endif /* def agent_comm_h */
