#ifndef eif_common_h
#define eif_common_h

/*
 * Component Name:  EIF
 *
 * Dependencies:
 *
 * Description:
 *        Common header file for TME agents
 *
 * (C) COPYRIGHT TIVOLI Systems 1997
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems
 */

#include "slist.h"
#include "tec_local.h"

typedef unsigned char	eif_boolean;

typedef struct {
	char			*location[TECEIF_MAX_SERVERS];
	FILE			*test_out;        /* Output stream for test mode               */
	int			max_locations;
	int			retry_window;
	eif_boolean	in_flush_events;
	eif_boolean	test_mode;        /* Using test mode?                          */
	eif_boolean	dont_filter;      /* If set, tec_put_event() doesn't filter    */
} tec_handle_common_t;

typedef struct {
	SL_t	*tec_env_list;
	int	filter_in;
} AgentConfig;

typedef struct {
	char				*buffer_path;
	unsigned long	max_size;
	unsigned long	shrink_size;
	unsigned long	read_blk_len;
	unsigned long	event_max_size;
	int				flush_rate;
	eif_boolean		initialized;
	eif_boolean		do_buffer;
} BufferConfig;

#define tec_disable_auto_filter(_h) ((tec_handle_common_t *)(_h))->dont_filter=1

#ifndef E_RESERVED
#	define E_RESERVED 21
#endif

#ifndef E_HOST
#	define E_HOST	(E_RESERVED + 27) /* host inaccessible or host failure */
#endif

EXTERN_DECL(int common_agent_handle_init, (tec_handle_common_t*, char*, char*));
EXTERN_DECL(int common_agent_get_retry, (tec_handle_t));

EXTERN_DECL(int tec_agent_match, (char *, char *));

#endif
