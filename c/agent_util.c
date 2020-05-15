/*
 * Component Name:
 *      agent_util.c
 *
 * $Date: 1996/10/22 19:08:39 $
 *
 * $Source: /tivoli/homes/1/tgreenbe/convert/tec/eif/agent_util.c,v $
 *
 * $Revision: 1.19 $
 *
 * Description:
 *      common functions for TME and non-TME agent libraries
 *
 * Note:
 *      Tab stops = 3
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef PC
#	include <sys/time.h>
#	include <sys/socket.h>
#else
#  ifdef OS2
#	include <sys/socket.h>
#	include <netinet\in.h>
#	include <netdb.h>
#	include <signal.h>
#  elif defined(NETWARE)
#   include <sys/socket.h>
#  else
#	include <winsock.h>
#  endif
#endif

#include "slist.h"
#include "tec_local.h"
#include "agent_comm.h"
#include "eif_common.h"
#include "eifdebug.h"

#if defined(PC) && !defined(OS2) && !defined(NETWARE)
#	define strdup _strdup
#	define stat  _stat
#endif

#if defined(NETWARE)
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
#if defined(isspace)
#undef isspace
#endif /* defined(isspace) */
/*
** Default configuration file path.
*/
#define CONFIG_PATH "SYS:ETC\\TIVOLI\\TECADEIF.CFG"

/*
** For ConsolePrintf...
*/
#include <nwconio.h>

#endif /* defined(NETWARE) */

typedef struct {
	char *keyword;
	char *value;
} tec_env_t;

static void tec_env_delete(tec_env_t *item);
static int tec_env_search(tec_env_t *item, char *keyword);
static int tec_add_filter(char *line, int filter_type);

AgentConfig     real_default_agent_info = { NULL };
AgentConfig	*default_agent_info = (AgentConfig *)&real_default_agent_info;

/*
 * Data structure used for storing slot value for filtering
 */

typedef struct {
	char *name;
	char *value;
} ev_slot;

/*
 * This list of filters is a slist of slists.  Each slist in the slist is a 
 * list of ev_slots.  Got that?
 */

static SL_t *tec_event_filters = NULL;
static SL_t *tec_event_cache_filters = NULL;

/*
 * Callback functions for filter list
 */
static int	tec_filter_walk(SL_t *filter, SL_t *event);
static void	tec_filter_delete(SL_t *filter);
static void	tec_event_delete(ev_slot *ep);

/*
 * Callback functions for filter list
 */
static int      tec_filter_walk(SL_t *filter, SL_t *event);
static void     tec_filter_delete(SL_t *filter);
static void     tec_event_delete(ev_slot *ep);

#if defined(WIN32) || defined(NETWARE)
/*
 * NT pre_filter
 */
static  void		free_nt_prefilter(NT_pre_filter *);
#if defined(_WIN32)
static  int		prefilter_matching(char *, char *, int, char *, NT_pre_filter *);
#elif defined(NETWARE)
static int prefilter_matching(char*, int, int, int, int, NT_pre_filter*);
#endif
static  int		tec_make_nt_prefilter(char *, NT_pre_filter *);
static  NT_pre_filter   *alloc_new();
 
static  NT_pre_filter   *listEnd = NULL;
static  NT_pre_filter   *listHead = NULL;
static  char		preFilterMode[24];

#if defined(_WIN32)
int     nt_prefiltering(char *, char *, int, char *);
#elif defined(NETWARE)
int nt_prefiltering(char*, int, int, int, int);
#endif
#endif

void
fatal_memory_error()
{
	fprintf(stderr, "Memory allocation failure, exiting...\n");
	exit(1);
}

/*
 * Purpose: 
 *    Read a configuration file line.
 *
 * Description: 
 * 	Keeps reading into a buffer until a newline is encountered.
 * 	Splits line into keywork and value.
 * 	Uses internal, static buffer if the line fits within it.
 *
 * Returns: 
 * 	Return value is NULL when the end of file is reached.
 * 	Keywork and value pointers are returned through the parameter list.
 *
 * Notes/Dependencies: 
 */

static char *
read_conf_line(FILE *fp, char **keyword_ptr, char **value_ptr)
{
	char			*whole_line;
	char			*delimiter;
	int			read_len;
	int			end_of_line;
	static char	workbuf[512];

	*keyword_ptr = *value_ptr = NULL;

	/*
	 * Read until a newline is found.  Concatenate each read into one big line.
	 */
	for (whole_line = NULL, end_of_line = 0; !end_of_line; ) {
		if (fgets((char *) workbuf, sizeof(workbuf), fp) == NULL)
			return(NULL);
		else {
			read_len = strlen(workbuf);

			if (workbuf[read_len - 1] == '\n') {
				workbuf[--read_len] = '\0';
				end_of_line = 1;
			}

			if (whole_line != NULL) {
				whole_line = (char *)realloc((void *) whole_line,
													(size_t)(strlen(whole_line) + read_len));
				strcat(whole_line, workbuf);
			}
			else {
				whole_line = (char *) malloc((size_t) read_len + 1);
				strcpy(whole_line, workbuf);
			}
		}
	}

	/*
	 * Split the line into keyword and value
	 */
	if ((whole_line[0] != '#') && (whole_line[0] != '\n')) {

		/*
		 * Check if this is a filter specification
		 */
		if (strncmp(whole_line, FILTER, strlen(FILTER)) == 0) {
			delimiter = whole_line + strlen(FILTER) - 1;
			*keyword_ptr = whole_line;
			*value_ptr = delimiter + 1;
		}

		/*
		 * Check if this is a cache filter specification
		 */
		else if (strncmp(whole_line, FILTER_CACHE, strlen(FILTER_CACHE)) == 0) {
			delimiter = whole_line + strlen(FILTER_CACHE) - 1;
			*keyword_ptr = whole_line;
			*value_ptr = delimiter + 1;
		}

                /*
                 * Check if this is a pre_filter specification
                 */
                else if (strncmp(whole_line, PREFILTER, strlen(PREFILTER)) == 0) {
                        delimiter = whole_line + strlen(PREFILTER) - 1;
                        *keyword_ptr = whole_line;                        
                        *value_ptr = delimiter + 1;
                }

		/*
		 * All other specifications
		 */
		else if ((delimiter = strchr(whole_line, '=')) != NULL) {
			*delimiter = '\0';
			*keyword_ptr = whole_line;
			*value_ptr = delimiter + 1;
		}
	}

	return(whole_line);
}

/*
 * Purpose: 
 *    initialization for TEC agent
 *
 * Description: 
 *
 * Returns: 
 *
 * Notes/Dependencies: 
 */

int 
common_agent_init(char *cfgfile)
{
	AgentConfig	*agent_info;
	FILE        *fp;
	char        *conf_line;
	char        *keyword;
	char        *value;
	tec_env_t   *entry;
	struct stat sbuf;
	char        cfg_file[FILE_LEN];
#if !defined(NETWARE)
	char        default_cfg[] = "/etc/Tivoli/tecad_eif.conf";
#else
    char        default_cfg[] = CONFIG_PATH;
#endif
	int         line_no = 1;
#if defined(WIN32) || defined(NETWARE)
	NT_pre_filter        *p;
#endif

	/*
	 * This design is to allow future changes so that different agent_info
	 * structures can be used within a single process.
	 */
	agent_info = default_agent_info;

	/*
	 * Initialize the env list and the filter list
	 */
	if (agent_info->tec_env_list != NULL)
		SL_DelAll(agent_info->tec_env_list, tec_env_delete);

	if (tec_event_filters != NULL)
		SL_DelAll(tec_event_filters, tec_filter_delete);

	if (tec_event_cache_filters != NULL)
		SL_DelAll(tec_event_cache_filters, tec_filter_delete);

	if ((agent_info->tec_env_list = SL_Create()) == NULL)
		fatal_memory_error();

	/*
	 * Create structured list to store filters
	 */
	if ((tec_event_filters = SL_Create()) == NULL)
		fatal_memory_error();

	if ((tec_event_cache_filters = SL_Create()) == NULL)
		fatal_memory_error();

	if (cfgfile == NULL) {
		if (stat(default_cfg, &sbuf) == 0)
			strcpy(cfg_file, default_cfg);
		else
			return (0);
	}
	else
		strcpy(cfg_file, cfgfile);

	/*
	 * Check if configuration file exists & is a valid file
	 */
	if (stat(cfg_file, &sbuf) != 0) {
		perror("stat failed");
		return(2);
	}

#ifndef OS2
        if (!S_ISREG(sbuf.st_mode)) {
#else
        if ((sbuf.st_mode & S_IFREG) == 0) {
#endif
		printf("%s: Not a regular file\n", cfg_file);
		return(3);
	}

	/*
	 * Open the given configuration file
	 */
	if (!(fp = fopen(cfg_file, "r"))) {
		printf("%s: Can not fopen\n", cfg_file);
		return(4);
	}

	/*
	 * Process file lines
	 */
	while ((conf_line = read_conf_line(fp, &keyword, &value)) != NULL) {
		line_no++;

		/*
		 * Lines begining with '#' are comments
		 */
		if ((keyword != NULL) && (value != NULL)) {
#if defined(WIN32) || defined(NETWARE)
                        /*
                         * Check if this is a prefilter specification
                         */
                        if (strncmp(keyword, PREFILTER, strlen(PREFILTER)) == 0) {
			   p =(NT_pre_filter *)alloc_new();
			   if ( tec_make_nt_prefilter(value, p) == -1)  {
			      free_nt_prefilter(p);
                              printf("syntax error in PreFilter, line %d ignored\n",line_no);
		 	   }
			   else  {
			      if ( listEnd ) {
				 listEnd->next = p;
				 listEnd = p;
			      }
			      else {
			 	 listHead = listEnd = p;
			      }
			      p->next = NULL;
			   }
 			   free((void *) conf_line);
                        }
                
			else
#endif
			/*
			 * Check if this is a filter specification
			 */

			if (strncmp(keyword, FILTER, strlen(FILTER)) == 0) {
				if (tec_add_filter(value, FILTER_EVENT_TYPE) < 0) {
					printf("syntax error in configuration file, line %d ignored\n",
																						 line_no);
				}
				free((void *) conf_line);
			}

			else

			/*
			 * Check if this is a cache filter specification
			 */
			if (strncmp(keyword, FILTER_CACHE, strlen(FILTER_CACHE)) == 0) {
				if (tec_add_filter(value, FILTER_EVENT_CACHE_TYPE) < 0) {
					printf("syntax error in configuration file, line %d ignored\n",
																						 line_no);
				}
				free((void *) conf_line);
			}
			else {

				/*
				 * Create value list entry
				 */
				if ((entry = (tec_env_t *) malloc(sizeof(tec_env_t))) == NULL)
					fatal_memory_error();

				/*
				 * Create entry keyword
				 */
				entry->keyword = keyword;
				entry->value = value;

				/*
				 * Insert entry in list
				 */
				if (SL_InsLast(agent_info->tec_env_list, (void *) entry) == FALSE)
					fatal_memory_error();
			}
		}
		else if (strlen(conf_line)
					&& (conf_line[0] != '#') && (conf_line[0] != '\n'))
		{
			printf("syntax error in configuration file, line %d ignored\n",
																					 line_no);
			free((void *) conf_line);
		}
		else
			free((void *) conf_line);
	}
   

	fclose(fp);

	agent_info->filter_in = tec_agent_match(TECAD_FILTERMODE, "IN");

#if defined(WIN32) || defined(NETWARE)
        /*
         * get PreFilterMode
         */
        value = tec_agent_getenv(TECAD_PFLTMODE);
        if ( value ) {
           strcpy(preFilterMode,value);
        }
        else{
           strcpy(preFilterMode, "OUT");
        }
#endif
	return(0);
}

/*
 * Purpose: 
 *    Initialization for TEC common fields in an EIF handle.
 *
 * Description: 
 *
 * Returns: 
 * 	Success - 0
 * 	Failure - Positive error number
 *
 * Notes/Dependencies: 
 */

int
common_agent_handle_init(tec_handle_common_t	*common_h, char *location,
								 char *default_loc)
{
	char		**hosts;
	char		*env;

	/* rsm */
	static char* temp_hosts[TECEIF_MAX_SERVERS];
	int j;

	common_h->dont_filter = 0;
	common_h->test_mode = FALSE;
	common_h->test_out = NULL;
	common_h->max_locations = 0;
	common_h->location[0] = NULL;
	common_h->in_flush_events = 0;
	common_h->retry_window = 0;

	/*
	 * Initialize server location(s)
	 */
	
	tecadINITIAL(DEBUGFILE_AGENT_COMM)
	tecadDEBUG_T(DEBUGFILE_AGENT_COMM, "entering common_agent_handle_init")	
	if (location == NULL) {
		tecadDEBUG_T(DEBUGFILE_AGENT_COMM, "location is NULL")	
		hosts = tec_split_entries("ServerLocation", &common_h->max_locations);

		if (common_h->max_locations == 0) {
			if (default_loc != NULL) {
				common_h->max_locations = 1;
				common_h->location[0] = strdup(default_loc);
			}
			else {
				/* return(E_HOST); */
				common_h->max_locations = 1;
#if !defined(NETWARE)
				common_h->location[0] = NULL;
#else  /* defined(NETWARE) */
                common_h->location[0] = strdup("127.0.0.1");
                printf("Please set the \"ServerLocation\" parameter and restart the adapter\n");
                ConsolePrintf("Please set the \"ServerLocation\" parameter and restart the adapter\r\n");
#endif /* defined(NETWARE) */

			}
		}
		else{

            /* rsm: we need to make the hosts string into multiple strings so we
                    can move 'em around later */
            for(j = 0; j < common_h->max_locations; j++)
                common_h->location[j] = strdup(hosts[j]);
		}
	}
	else {
		int 	i;

		tecadDEBUG_T(DEBUGFILE_AGENT_COMM, "location is NOT NULL")
		common_h->location[0] = strdup(location);

		tecadDEBUG_S(DEBUGFILE_AGENT_COMM, common_h->location[0])	

		hosts = tec_split_entries("ServerLocation", &common_h->max_locations);
		for (i=1; i<(TECEIF_MAX_SERVERS>common_h->max_locations+1?common_h->max_locations+1:TECEIF_MAX_SERVERS); i++){ 
			common_h->location[i]=strdup(hosts[i-1]);
			tecadDEBUG_S(DEBUGFILE_AGENT_COMM, common_h->location[i])	
		}
		if(common_h->max_locations < TECEIF_MAX_SERVERS)
			 common_h->max_locations++;
	}

	/*
	 * Check for test mode
	 */
	if (tec_agent_match("TestMode", "YES")) {
		common_h->test_mode = TRUE;

		if ((common_h->test_out = fopen(common_h->location[0], "w")) == NULL)
			return(errno);
	}

	/*
	 * Look up the RetryWindow for (re-)connecting to servers.
	 */
	if ((env = tec_agent_getenv("RetryInterval")) != NULL)
		common_h->retry_window = atoi(env);

	if (!common_h->retry_window)
		common_h->retry_window = RETRY_WINDOW;

	return(0);
}

int
common_agent_get_retry(tec_handle_t handle)
{
	tec_handle_common_t *common_h = (tec_handle_common_t *) handle;

	return(common_h->retry_window);
}

/* ------------------------------------------------------------------------- */

static void
tec_env_delete(tec_env_t *item)
{
	if (item != NULL) {
		if (item->keyword != NULL)
			free(item->keyword);

		free(item);
	}
}

/* ------------------------------------------------------------------------- */

char *
tec_agent_getenv(char *keyword)
{
	tec_env_t	*found;
	AgentConfig	*agent_info = default_agent_info;

	found = (tec_env_t *) SL_Find(agent_info->tec_env_list, keyword,
											tec_env_search);

	return((found != NULL) ? found->value : NULL);
}

/* ------------------------------------------------------------------------- */

static int
tec_env_search(tec_env_t *item, char *keyword)
{
	if ((item != NULL) && (item->keyword != NULL)) {
		if (strcmp(keyword, item->keyword))
			return (FALSE);
		else
			return (TRUE);
	}
	else
		return (FALSE);
}



/*
 * Purpose: 
 *    add an event filter to the list of filters
 *
 * Description: 
 *
 * Returns: 
 *    -1 on parse error
 *
 * Notes/Dependencies: 
 */

static int
tec_add_filter(char *line, int filter_type)
{
	ev_slot *ep;
	SL_t    *filter;
	char    *s;

	if ((filter = SL_Create()) == NULL) {
		fatal_memory_error();
	}

	/*
	 * Parse the input line, putting each slot-value pair into the slot list
	 */
	s = strtok(line, "=");

	while (s != NULL) {
		if ((ep = (ev_slot *)malloc(sizeof *ep)) == NULL)
			fatal_memory_error();

		ep->name = strdup(s);
		s = strtok(NULL, ";");

		if (s == NULL)
			return(-1);

		ep->value = strdup(s);

		if (SL_InsLast(filter, (void *)ep) == FALSE)
			fatal_memory_error();

		s = strtok(NULL, "=");
	}

	/*
	 * Filter completed.  Now put the filter into either the normal 'event
	 * filter' or special 'event cache filter' filter list
	 */
	if (SL_NbrElm(filter) > 0) {
		switch (filter_type) {
		  case FILTER_EVENT_CACHE_TYPE:
				if (SL_InsLast(tec_event_cache_filters, (void *)filter) == FALSE)
					fatal_memory_error();

			break;

		  case FILTER_EVENT_TYPE:
		  default:
				if (SL_InsLast(tec_event_filters, (void *)filter) == FALSE)
					fatal_memory_error();

			break;
		}
	}
	return(0);
}


/*
 * A little state machine to break up the event string into
 * slot=value pairs.
 * The character set is: ' = ; other
 */

#define init         0
#define regular      1 
#define inquote      2 
#define outquote     3 
#define accept_it    4

/*
 * Returns next state given current state and next input
 */
static int
next(int curr, char input)
{
	switch(curr) {
		case regular:
			if (input == '=' || input == ';')
				return accept_it;

			return regular;
			break;

		/*
		 * 2 quotes together
		 */
		case outquote:
			switch (input) {
				case  '\'':
					return(inquote);
					break;

				case '=':
				case ';':
					return(accept_it);
					break;

				/*
				 * A quoted name/value not terminated by a = or ;
				 */
				default:
					return(regular);
					break;
			}
			break;

		case inquote:
			return((input == '\'') ? outquote : inquote);
			break;

		/*
		 * If the thing is quoted, the quote must be the first
		 * char in the slot/value, i. e. "date = '12/12/90; AD'" 
		 * is not the same as "date='12/12/90; AD'".
		 */
		case init:
			/* 
			 * A ; or = in initial state is treated as regular, i. e.
			 * you can have a slotname or value beginnig with ; or = 
			 */
			return((input == '\'') ? inquote : regular);
			break;

		default:
			return(curr);
			break;
	}
}

char *
tec_normalize_msg(char *in_message, unsigned long msg_len)
{
	char	*out_msg, *s, *suffix;

	if (!msg_len)
		return(NULL);

	/*
	 * If it is already OK, just return
	 */
	if (msg_len >= TECAD_EVENT_DELIMITER_LEN) {
		if (!strncmp(&in_message[msg_len - TECAD_EVENT_DELIMITER_LEN],
						 TECAD_EVENT_DELIMITER,
						 (msg_len - TECAD_EVENT_DELIMITER_LEN)))
		{
			return(NULL);
		}
	}

	/*
	 * Have to allocate new space because we can't assume the current message is
	 * in a buffer that has room to grow.
	 */
	out_msg = malloc((size_t) (msg_len + TECAD_EVENT_DELIMITER_LEN + 1));
	strcpy(out_msg, in_message);

	if (msg_len < TECAD_EVENT_DELIMITER_LEN)
		suffix = out_msg;
	else
		suffix = &out_msg[msg_len - TECAD_EVENT_DELIMITER_LEN];

	if ((s = strstr(suffix,"END")) != NULL)
		strcpy(s, TECAD_EVENT_DELIMITER);
	else
		strcat(suffix, TECAD_EVENT_DELIMITER);

	return(out_msg);
}

/*
 * Purpose: 
 *    Determine if an event should be filtered out, based upon the currently
 *		in-effect filters
 *
 * Description: 
 *    The event must match ALL name-value pairs for at least one of the
 *		specified filters
 *
 * Returns: 
 *    1 -> discard the event
 *    0 -> send the event
 *
 * Notes/Dependencies: 
 *    If the input event string doesn't match the expected format, then
 *    tec_filter_event always returns 0.
 */

int
tec_filter_event(char *message, int filter_type)
{
	AgentConfig	*agent_info;
	SL_t			*event;
	ev_slot		*ep;
	char			*msg, *s, *begin;
	int			ret;              /* return value */
	int			state;
	int			type;
	int			no_match;

	agent_info = default_agent_info;

	/*
	 * If filtering IN,  then no matching filter means to discard the event.
	 * If filtering OUT, then no matching filter means to send    the event.
	 */
	no_match = agent_info->filter_in;

	/*
	 * If the appropriate filters are not defined, get out of here now.
	 * The list may exist but be empty, so check by trying to get the 1st elem.
	 */
	switch (filter_type) {
		case FILTER_EVENT_CACHE_TYPE:
			if (SL_GetPos(tec_event_cache_filters, 0) == NULL) {
				return(no_match);
			}

			break;

		case FILTER_EVENT_TYPE:
		default:
			if (SL_GetPos(tec_event_filters, 0) == NULL)
				return(no_match);

			break;
	}

	/*
	 * Convert the event into an SL_t
	 */
	if ((event = SL_Create()) == NULL) {
		printf("memory allocation failure\n");
		exit(1);
	}

	msg = strdup(message);

	/*
	 * Get class
	 */
	if ((s = strchr(msg, ';')) == NULL) {
		free(msg);
		SL_DelAll(event, tec_event_delete);
		return(0);
	}

	*s = '\0';

	if ((ep = (ev_slot *)malloc(sizeof *ep)) == NULL)
		fatal_memory_error();

	ep->name = "Class";
	ep->value = msg;

	if (SL_InsLast(event, (void *)ep) == FALSE)
		fatal_memory_error();

	/*
	 * begin loop for other slots
	 */
	state = init;
	begin = ++s;

	/*
	 * 0 means expecting slotname, 1 means value
	 */
	type = 0;

	while (*s) {
		if (strcmp(s, TECAD_EVENT_DELIMITER) == 0)
			break;

		state = next(state, *s);

		if (state == accept_it) {
			*(s++) = '\0';
			state = init;

			/* allocate link node when we are expecting slot name */
			if (type == 0) {
				if ((ep = (ev_slot *)malloc(sizeof *ep)) == NULL)
					fatal_memory_error();
				else {
					ep->name = begin;
					/* In case the string is bad, we are protected */
					ep->value = "";
				}
			}

			/*
			 * Insert link node when we are expecting slot value.  If we did not
			 * accept on the slotvalue, `ep' would leak, but that can only happen
			 * if the event was not properly formatted, i. e. someone else is
			 * being bad.
			 */
			else {
				ep->value = begin;

				if (SL_InsLast(event, (void *)ep) == FALSE)
					fatal_memory_error();
			}

			begin = s;
			type = (type == 0 ? 1 : 0);
			continue;
		}
		s++;
	}

	/*
	 * Now begin filtering process.  Walk the filter list.  If a successful
	 * match is found, it will return TRUE
	 */
	switch (filter_type) {
		case FILTER_EVENT_CACHE_TYPE:
			ret = SL_Walk2(tec_event_cache_filters, tec_filter_walk, event);
			break;

		case FILTER_EVENT_TYPE:
		default:
			ret = SL_Walk2(tec_event_filters, tec_filter_walk, event);
			break;
	}

	/*
	 * Clean up
	 */
	SL_DelAll(event, tec_event_delete);
	free(msg);

	if (agent_info->filter_in == 1)
		ret = ((ret == 1) ? 0 : 1);

	return(ret);
}



/*
 * Purpose: 
 *    match an event against a single filter from the list of possible filters 
 *
 * Description: 
 *    if the event matches ALL name-value pairs of the filter, that is a 
 *  successful match
 *
 * Returns: 
 *    0 -> successful match, the event will be discarded
 *    1 -> not a successful match
 *
 * Notes/Dependencies: 
 *    if the input event string doesn't match the expected format, then
 *    tec_filter_event always returns 0.
 */

static int
tec_filter_walk(SL_t *filter, SL_t *event)
{
	int     i = 0;
	int     j = 0;
	ev_slot *ep;          /* one slot from the filter */
	ev_slot *evp;         /* one slot from the event */
	int     match;

	/*
	 * A match must be found for EVERY slot before we return 0 (success)
	 */
	while ((ep = SL_GetPos(filter, i++)) != NULL) {

		/*
		 * Look for a match for this filter slot.  If a matched slot is found,
		 * AND the values match, keep going
		 */
		match = FALSE;
		j = 0;

		while ((evp = SL_GetPos(event, j++)) != NULL) {
			if (strcmp(ep->name, evp->name) == 0) {
				match = (strcmp(ep->value, evp->value) == 0) ? TRUE : FALSE;
				break;
			}
		}

		/*
		 * If no match on this slot, return
		 */
		if (!match)
			return(1);
	}

	/*
	 * If we get to here, all slots matched, in which case the event should be
	 * filtered out.
	 */
	return(0);
}


static void
tec_filter_slot_delete(ev_slot *ep)
{
	if (ep->name)
		free(ep->name);

	if (ep->value)
		free(ep->value);

	free(ep);
}

static void 
tec_filter_delete(SL_t *filter)
{
	SL_DelAll(filter, tec_filter_slot_delete);
}


static void 
tec_event_delete(ev_slot *ep)
{
	free(ep);
}

/*
 * Splits a comma separated list of entries. The number of entries is written
 * to num.
 */

char **
tec_split_entries(char* env, int* num)
{
	static char* buf[TECEIF_MAX_SERVERS];
	char* all;
	int i;

	*num = 0;

	if ((all = tec_agent_getenv(env)) == NULL)
		return buf;
	
	/*
	 * Make a copy of the env string that we will hack
	 */
	all = strdup(all);

	for (i = 0; i < TECEIF_MAX_SERVERS; i++) {
		if ((buf[i] = strtok((i == 0) ? all : (char*) 0, ",")) == (char*) 0)
			break;
	}
	
	/*
	 * We ignore entries beyond TECEIF_MAX_SERVERS quietly
	 */
	
	*num = i;
	return buf;
}

int
tec_agent_match(char *env_name, char *match_it)
{
	char				*env_val;
	register char	*e, *m;

	if ((env_val = tec_agent_getenv(env_name)) == NULL)
		return(0);

	for (e = env_val, m = match_it; (*m != '\0'); m++, e++) {
		if ((*e == '\0') || (toupper(*e) != *m))
			return(0);
	}

	return(1);
}


#if defined(WIN32) || defined(NETWARE)


/*
 * Purpose:
 *    allocate memory for a pre filter.
 * 
 */
static NT_pre_filter *
alloc_new () {
   NT_pre_filter *q;
   q = (NT_pre_filter *)malloc(sizeof(NT_pre_filter));
   if ( (q = (NT_pre_filter *)malloc(sizeof(NT_pre_filter))) == NULL)
       fatal_memory_error();
#if defined(_WIN32)
   q->event_id[0] = 0;
   q->event_log[0] = NULL;
   q->event_src[0] = NULL;
   q->event_type[0] = NULL;
#elif defined(NETWARE)
   q->event_src[0] = NULL;
   q->event_id[0] = -1;
   q->event_severity[0] = -1;
   q->event_locus[0] = -1;
   q->event_class[0] = -1;
#endif
   return q;
}

/*
 * Purpose:
 * free nt pre_filter
 * Description:
 */
static void
free_nt_prefilter(NT_pre_filter *p) {
   int i;

#if defined(_WIN32)
   for (i=0; p->event_log[i]; i++)
     free(p->event_log[i]);
   for (i=0; p->event_src[i]; i++)
     free(p->event_src[i]);
   for (i=0; p->event_type[i]; i++)
     free(p->event_type[i]);
#elif defined(NETWARE)
   for (i = 0; p->event_src[i]; ++i) {
       free(p->event_src[i]);
   }
#endif
   free(p);

}
 

/*
 * Purpose:
 * determin if an event should be filtered out,
 * based upon the current prefilter.
 *
 * Description:
 * a match is that the event matches one of logs, one of ids and
 * one of types.
 *
 * Returns:
 * DISCARD_IT=1 -> discard the event
 * KEEP_IT=0 -> keep the event
 *
 */
int
#if defined(_WIN32)
nt_prefiltering(char *log, char *src, int id, char *type)
#elif defined(NETWARE)
nt_prefiltering(char* src, int id, int sev, int loc, int cls)
#endif

{
   NT_pre_filter *p = listHead;
   int no_match = 1;
   int num=0;

   while (p && no_match) {
#if defined(_WIN32)
      no_match = prefilter_matching(log, src, id, type, p);
#elif defined(NETWARE)
      no_match = prefilter_matching(src, id, sev, loc, cls, p);
#endif
      num++;
      p = p->next;
   }
   if ( !no_match ) {
      /* found a match */
      if ( !strcmp(preFilterMode,"IN") || !strcmp(preFilterMode,"in")) {
          return KEEP_IT;
      }
      else {
          return DISCARD_IT;
      }
   }
   else {
      /* found no match */
      if ( !strcmp(preFilterMode,"IN") || !strcmp(preFilterMode,"in")) {
          return DISCARD_IT;
      }
      else {
          return KEEP_IT;
      }
   }    
}

/*
 * Purpose:
 * find a match with the pre_filter 
 * Returns:
 * 1 -> no match 
 * 0 -> match
 *
 */
static int
#if defined(_WIN32)
prefilter_matching(char *log, char *src, int id, char *type, NT_pre_filter *p)
#elif defined(NETWARE)
prefilter_matching(char* src, int id, int sev, int loc, int cls, NT_pre_filter* p)
#endif
{
   int i;

#if defined(_WIN32)
   for (i=0; p->event_id[i]; i++) {
       if (id == p->event_id[i])
          break; 
   }
   if ( i && !p->event_id[i] ) {
      return 1;
   }

   for (i=0;  p->event_log[i]; i++) {
      if (!strcmp(log,p->event_log[i]))
          break;
   }
   if ( i && !p->event_log[i] ) {
      return 1;
   }

   for (i=0;  p->event_src[i]; i++) {
      if (!strcmp(src,p->event_src[i]))
          break;
   }
   if ( i && !p->event_src[i] ) {
      return 1;
   }
 
   for (i=0;  p->event_type[i]; i++) {
      if (!strcmp(type,p->event_type[i]))
         break;
   }
   if ( i && !p->event_type[i] ) {
      return 1;
   } 
#elif defined(NETWARE)
   for (i = 0; p->event_src[i]; ++i) {
       if (!strcmp(src, p->event_src[i])) {
           break;
       }
   }
   if (i && !p->event_src[i]) {
       return 1;
   }

   for (i = 0; p->event_id[i] != -1; ++i) {
       if (id == p->event_id[i]) {
           break;
       }
   }
   if (i && p->event_id[i] == -1) {
       return 1;
   }

   for (i = 0; p->event_severity[i] != -1; ++i) {
       if (sev == p->event_severity[i]) {
           break;
       }
   }
   if (i && p->event_severity[i] == -1) {
       return 1;
   }

   for (i = 0; p->event_locus[i] != -1; ++i) {
       if (loc == p->event_locus[i]) {
           break;
       }
   }
   if (i && p->event_locus[i] == -1) {
       return 1;
   }

   for (i = 0; p->event_class[i] != -1; ++i) {
       if (cls == p->event_class[i]) {
           break;
       }
   }
   if (i && p->event_class[i] == -1) {
       return 1;
   }
#endif

   return 0;

}

/*
 * Purpose:
 * make a NT prefilter 
 * 
 * Description:
 * read in a line contains a NT prefilter info.
 * and make a NT prefilter.
 *
 * Returns:
 * -1 -> syntax error.
 *  0 -> OK. 
 *
 */
static int
tec_make_nt_prefilter(char *line, NT_pre_filter *p)
{
   char *s;
   char *str;
   char workline[512];
   int  i;
   int  current_strlen;
   int  processed_strlen = 0;
   int  total_strlen;




   total_strlen=strlen(line);
   strncpy(workline,line,total_strlen);
   workline[total_strlen]='\0';


   while ( processed_strlen < total_strlen ) {
      s=strtok(workline,";");
      current_strlen = strlen(s);
      str=strtok(s,"=");

#if defined(_WIN32)
      if (!strcmp(str,"Log")) {
         str=strtok(NULL,",");
         i = 0;
         while (str != NULL &&
                (!strcmp(str,"Application") ||
                !strcmp(str,"Security")     ||
                !strcmp(str,"System"))) {
            if (i<MAX_EVENT_LOGS) {
               p->event_log[i] = (char *)malloc(strlen(str)+1);
               strcpy(p->event_log[i],str);
               str=strtok(NULL,",");
               i++;
            }
            else {
               fprintf(stderr, "PreFilter: # of Log exceeds the :%d\n",MAX_EVENT_LOGS);
               return -1;
            }
         }
         p->event_log[i] = NULL;
      }

      else
      if (!strcmp(str,"EventType")) {
         str=strtok(NULL,",");
         i = 0;
         while (str != NULL &&
               (!strcmp(str,"Error")       ||
                !strcmp(str,"Information") ||
                !strcmp(str,"Warning")     ||
                !strcmp(str,"AuditSuccess")||
                !strcmp(str,"AuditFailure")||
                !strcmp(str,"Unknown"))) {
            if ( i < MAX_EVENT_TYPES ) {
               p->event_type[i] = (char *)malloc(strlen(str)+1);
               strcpy(p->event_type[i],str);
               str=strtok(NULL,",");
               i++;
            }
            else {
               fprintf(stderr, "PreFilter: # of EventType exceeds the :%d\n",MAX_EVENT_TYPES);
               return -1;
            }
         }
         p->event_type[i] = NULL;
      }

      else
      if (str != NULL && !strcmp(str,"EventId")) {
         str=strtok(NULL,",");
         i = 0;
         while ( str != NULL ) {
            if ( i < MAX_EVENT_IDS ) {
               p->event_id[i] = atoi(str);
               str=strtok(NULL,",");
               i++;
            }
            else {
               fprintf(stderr, "PreFilter: # of EventId exceeds the :%d\n",MAX_EVENT_IDS);
               return -1;
            }
         }
         p->event_id[i] = 0;
      }
      else
      if (str != NULL && !strcmp(str,"Source")) {
         str=strtok(NULL,",");
         i = 0;
         while ( str != NULL ) {
            if ( i < MAX_EVENT_SRCS ) {
               p->event_src[i] = (char *)malloc(strlen(str)+1);
               strcpy(p->event_src[i],str);
               str=strtok(NULL,",");
               i++;
            }
            else {
               fprintf(stderr, "PreFilter: # of Source  exceeds the :%d\n",MAX_EVENT_SRCS);
               return -1;
            }
         }
         p->event_src[i] = NULL;
      }
#elif defined(NETWARE)
      if (str != NULL && !strcmp(str, SUBKEY_SOURCE)) {
          str = strtok(NULL, ",");
          i = 0;

          while (str != NULL) {
              if (i < MAX_EVENT_SRCS) {
                  p->event_src[i] = (char*) malloc(strlen(str) + 1);
                  strcpy(p->event_src[i], str);
                  str = strtok(NULL, ",");
                  ++i;
              } else {
                  fprintf(stderr, "PreFilter: # of Source exceeds the :%d\n",
                          MAX_EVENT_SRCS);
                  return -1;
              }
          }
          p->event_src[i] = NULL;
      } else if (str != NULL && !strcmp(str, SUBKEY_EVENT_ID)) {
          str = strtok(NULL, ",");
          i = 0;
          while (str != NULL) {
              if (i < MAX_EVENT_IDS) {
                  p->event_id[i] = atoi(str);
                  str = strtok(NULL, ",");
                  ++i;
              } else {
                  fprintf(stderr, "PreFilter: # of EventId exceeds the :%d\n",
                          MAX_EVENT_IDS);
                  return -1;
              }
          }
          p->event_id[i] = -1;
      } else if (str != NULL && !strcmp(str, SUBKEY_SEVERITY)) {
          str = strtok(NULL, ",");
          i = 0;
          while (str != NULL) {
              if (i < MAX_EVENT_SEVERITIES) {
                  p->event_severity[i] = atoi(str);
                  str = strtok(NULL, ",");
                  ++i;
              } else {
                  fprintf(stderr, "PreFilter: # of Severity exceeds the :%d\n",
                          MAX_EVENT_SEVERITIES);
                  return -1;
              }
          }
          p->event_severity[i] = -1;
      } else if (str != NULL && !strcmp(str, SUBKEY_LOCUS)) {
          str = strtok(NULL, ",");
          i = 0;
          while (str != NULL) {
              if (i < MAX_EVENT_LOCUSES) {
                  p->event_locus[i] = atoi(str);
                  str = strtok(NULL, ",");
                  ++i;
              } else {
                  fprintf(stderr, "PreFilter: # of Locus exceeds the :%d\n",
                          MAX_EVENT_LOCUSES);
                  return -1;
              }
          }
          p->event_locus[i] = -1;
      } else if (str != NULL && !strcmp(str, SUBKEY_CLASS)) {
          str = strtok(NULL, ",");
          i = 0;
          while (str != NULL) {
              if (i < MAX_EVENT_CLASSES) {
                  p->event_class[i] = atoi(str);
                  str = strtok(NULL, ",");
                  ++i;
              } else {
                  fprintf(stderr, "PreFilter: # of Class exceeds the :%d\n",
                          MAX_EVENT_CLASSES);
                  return -1;
              }
          }
          p->event_class[i] = -1;
      }
#endif


      else {
         for ( i = 0; i <  current_strlen; i++) {
            if ( !isspace(str[i]) )
            {
               fprintf(stderr, "PreFilter:contains illegal character.\n");
               return -1;
            }
         }
      }

      processed_strlen += current_strlen + 1;
      if ((total_strlen - processed_strlen)<=0)
      {
         return 0;
      }
      strncpy(workline,line + processed_strlen, total_strlen - processed_strlen);
      workline[total_strlen - processed_strlen]='\0';
   }
   return 0;
}


#endif
