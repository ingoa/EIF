/*
 * Component	: slist.c
 *
 * Description	: This file contains the structured list functions
 *			for the T/EC Adapters.
 *
 * Primary Author : Marc Foret
 *
 * Peer Reviewer  : 
 *
 * (C) COPYRIGHT BIM, sa/nv 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - BIM, sa/nv.
 * 
 */

/*****************************************************************************/
/*                            I N T E R F A C E                              */
/*****************************************************************************/

/*** I M P O R T E D ***/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#if defined(__STDC__) || defined(STDC_SRC_COMPATIBLE)
#	include <stdarg.h>
#else
#	include <varargs.h>
#endif

#include "slist.h"

#if defined(NETWARE)
/*
** List traversals can take enough time that it makes the server
** unresponsive - joys of non-preemptive environment.
**
** Use a crude timing mechanism to yield the CPU once in a while.
*/
#include <nwthread.h>
#include <time.h>
#define QUANTUM 3
#define SETUP_TIMERS() \
    clock_t startTick = 0; \
    clock_t stopTick = 0
#define SET() startTick = clock()
#define YIELD() \
    stopTick = clock(); \
    if (stopTick - startTick > QUANTUM) { \
        ThreadSwitchWithDelay(); \
        startTick = clock(); \
    }
#endif /* defined(NETWARE) */


/*
 * This is from tivoli/defines.h but since this is part of the EIF source
 * kit, we don't want to be dependent on ADE
 */
#ifndef CODE_ID
#	define CODE_ID(id) static char *_code_id(char *s) {return _code_id(s=id);}
#endif

CODE_ID("$Id: slist.c,v 1.7 1996/11/14 18:11:29 jmills Exp $")

/*****************************************************************************/
/*                                B O D Y                                    */
/*****************************************************************************/

/*******************/
/*** Definitions ***/
/*******************/


/****************************************/
/*** Defined functions and procedures ***/
/****************************************/

SL_t * SL_Create (  /* */ );
int    SL_InsLast ( /* SL_t * handle, void *elm */ );
void * SL_Find (    /* SL_t * handle, 
			     void * cmp_data, 
			     int (*cmp_func)() */ );
int    SL_FindPos ( /* SL_t * handle, 
			     void * cmp_data, 
			     int (*cmp_func)() */ );
int    SL_FindReplace ( /* SL_t * handle, 
				 void * replace_data, 
				 int (*cmp_func)(),
				 void (*del_func)() */ );
void * SL_GetPos (  /* SL_t * handle, int pos */ );
void   SL_DelAll (  /* SL_t * handle, void ( *del_func)() */ );
int    SL_Walk (    /* SL_t * handle, int ( *func)()*/ );
int    SL_Walk2 (    /* SL_t * handle, int ( *func)(), void *arg */ );
int    SL_NbrElm (  /* SL_t * handle */ );

/* ------------------------------------------------------------------------- */
/*  1.   API Functions                                                       */
/* ------------------------------------------------------------------------- */

/*
 * Name: SL_Create
 *
 * Purpose: Create a Simple List handler.
 *
 * Input: /
 *
 * Ouput/Exceptions: 
 * Returns: return a handle for list manipulations or NULL
 * 		if error.
 *
 */

SL_t * SL_Create()
{
	SL_t *ptr;
	ptr = (SL_t *) malloc(sizeof(SL_t ));

	if (ptr == NULL)
		return(NULL);

	ptr->list = NULL;
	ptr->nbr = 0;
	ptr->last_pos = 0;

	return(ptr);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_InsLast
 *
 * Purpose: 	Add at the end of the list a new element. 
 *
 * Input: 	handle:the handle of the list.
 *		elm: the element to add in the list.
 *
 * Ouput/Exceptions: MALLOC_ERROR: malloc error.
 * Returns: 	TRUE if success, FALSE otherwise.
 *
 */
 
int
SL_InsLast(handle, elm)
	SL_t *handle;
	void *elm;
{
	/* (re) allocate space */
	if (handle == NULL)
		return(FALSE);

	if (handle->last_pos >= handle->nbr) {
		if (handle->nbr == 0)
		  handle->list = (void *) malloc(sizeof(void *) * BLOCK_SIZE);
		else {
		  handle->list = (void *) realloc( handle->list, 
		    			sizeof(void *) * (handle->nbr+BLOCK_SIZE) );
		}

		/* SL_errno = MALLOC_ERROR */
		if (handle->list == NULL)
		  return(FALSE);

		handle->nbr += BLOCK_SIZE;
	}
	handle->list[handle->last_pos++] = elm;
	
	return(TRUE);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_Find
 *
 * Purpose: 	Find a element in the list. 
 *
 * Input: 	handle:the handle of the list.
 *		cmp_data: the data to compare in the list.
 *		cmp_func:	the comparaison func to apply.
 *
 * Ouput/Exceptions: MALLOC_ERROR: malloc error.
 * Returns: 	elem if cmp_data success, NULL otherwise.
 *
 */

void * 	SL_Find (handle, cmp_data, cmp_func)
	SL_t * handle;
	void * cmp_data;
	int (*cmp_func)();
{
#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */

	int i;
	void **list;

	/* SL_errno = INVALID_STRUCTUR */
	if ((handle == NULL) || (handle->nbr == 0) || (handle->last_pos == 0))
		return(NULL);

	/* SL_errno = INCOMPATIBLE_PARAMETER */
	if (cmp_func == NULL)
		return(NULL);

	list = handle->list;
#if defined(NETWARE)
    SET();
#endif /* defined(NETWARE) */


	for (i = 0; i < handle->last_pos; i++) {
		if (cmp_func(list[i], cmp_data) == TRUE) 
		  return(list[i]);
#if defined(NETWARE)
        YIELD();
#endif /* defined(NETWARE) */
	}
	return(NULL);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_FindPos
 *
 * Purpose: 	Find the position of a element in the list. 
 *
 * Input: 	handle:the handle of the list.
 *		cmp_data: the data to compare in the list.
 *		cmp_func:	the comparaison func to apply.
 *
 * Ouput/Exceptions: MALLOC_ERROR: malloc error.
 * Returns: 	pos if cmp_data success, -1 otherwise.
 *
 */

int SL_FindPos (handle, cmp_data, cmp_func)
	SL_t * handle;
	void * cmp_data;
	int (*cmp_func)();
{
#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */

	int i;
	void **list;

	/* SL_errno = INVALID_STRUCTUR */
	if ((handle == NULL) || (handle->nbr == 0) || (handle->last_pos == 0))
		return(-1);

	/* SL_errno = INCOMPATIBLE_PARAMETER */
	if (cmp_func == NULL)
		return(-1);

	list = handle->list;
#if defined(NETWARE)
    SET();
#endif /* defined(NETWARE) */


	for (i = 0; i < handle->last_pos; i++) {
		if (cmp_func(list[i], cmp_data) == TRUE) 
		  return(i);
#if defined(NETWARE)
        YIELD();
#endif /* defined(NETWARE) */

	}
	return(-1);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_FindReplace
 *
 * Purpose: 	Find and Replace an element in the list. 
 *
 * Input: 	handle:		the handle of the list.
 *		replace_data: 	the data to compare/replace in the list.
 *		cmp_func:	the comparison func to apply.
 *		free_func: 	a free function of the data.
 *
 * Ouput/Exceptions: MALLOC_ERROR: malloc error.
 * Returns: 	TRUE if success, FALSE otherwise.
 *
 */

int SL_FindReplace (handle, replace_data, cmp_func, free_func)
	SL_t * handle;
	void * replace_data;
	int (*cmp_func)();
	void (*free_func)();
{
	int i;
	void **list;
#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */


	/* SL_errno = INVALID_STRUCTUR */
	if ((handle == NULL) || (handle->nbr == 0))
		return(FALSE);

	/* SL_errno = INCOMPATIBLE_PARAMETER */
	if (cmp_func == NULL)
		return(FALSE);

	list = handle->list;
#if defined(NETWARE)
    SET();
#endif /* defined(NETWARE) */


	for (i = 0; i < handle->last_pos; i++) {
		if (cmp_func(list[i], replace_data) == TRUE) {
		  free_func(list[i]);
		  list[i] = replace_data;
		  return(TRUE);
		}
#if defined(NETWARE)
        YIELD();
#endif /* defined(NETWARE) */
	}
	return(FALSE);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_GetPos
 *
 * Purpose: 	Return a pointer to the n-th element of the list. 
 *
 * Input: 	handle:the handle of the list.
 *		pos: the position in the list.
 *
 * Ouput/Exceptions: 
 *
 * Returns: 	elem if exist, NULL otherwise.
 *
 */

void * 	SL_GetPos (handle, pos) 
	SL_t * handle;
	int pos;
{
	if ((handle == NULL) || (pos >= handle->last_pos))
		return(NULL);

	return(handle->list[pos]);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_DelAll
 *
 * Purpose: 	Destroys a list:. 
 *
 * Input: 	handle:the handle of the list.
 *		free_func: a free function of the data.
 *
 * Ouput/Exceptions: 
 *
 * Returns: 	/ 
 *
 */

void SL_DelAll (  handle, free_func)
	SL_t * handle;
	void ( *free_func)();
{
	int i;
#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */


	if (handle == NULL)
		return;

	if (handle->list != NULL) {
#if defined(NETWARE)
        SET();
#endif /* defined(NETWARE0 */

		for (i = 0; i < handle->last_pos; i++) {
		  free_func( handle->list[i]);
		  handle->list[i] = NULL;
#if defined(NETWARE)
          YIELD();
#endif /* defined(NETWARE) */
		}
		free(handle->list);
		handle->list = NULL;
	}
	free(handle);
	return;
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_Walk
 *
 * Purpose: 	For each element in the list, call (func) with argument
 *		data: element in the list and the position in the list.
 *
 * Input: 	handle:the handle of the list.
 *		func: the func to call for each element in the list.
 *
 * Ouput/Exceptions: if the func return 0 stops the walk with no error.
 *		if the func return -1 stops the walk with error.
 *
 *
 * Returns: 	FALSE if error, TRUE otherwise.
 *
 */

int    SL_Walk(handle, func)
	SL_t *handle;
	int  (*func)();
{ 
#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */

	int i, ret;
	void **list;

	/* SL_errno = INVALID_STRUCTUR */
	if ((handle == NULL) || (handle->nbr == 0) || (handle->last_pos == 0))
		return(FALSE);

	/* SL_errno = INCOMPATIBLE_PARAMETER */
	if (func == NULL)
		return(FALSE);

	list = handle->list;
#if defined(NETWARE)
    SET();
#endif /* defined(NETWARE) */


	for (i = 0; i < handle->last_pos; i++) {
		if ((ret = func(list[i], i + 1)) == 0)
		  return(TRUE);
		else if (ret == -1) 
		  return(FALSE);
#if defined(NETWARE)
        YIELD();
#endif /* defined(NETWARE) */
	}
	return(FALSE);
}


/*
 * Name: SL_Walk2
 *
 * Purpose: 	For each element in the list, call (func) with argument
 *		data: element in the list and the position in the list.
 *
 * Input: 	handle:the handle of the list.
 *		func: the func to call for each element in the list.
 *
 * Ouput/Exceptions: if the func return 0 stops the walk with no error.
 *		if the func return -1 stops the walk with error.
 *
 *
 * Returns: 	FALSE if error, TRUE otherwise.
 *
 */

int    SL_Walk2(handle, func, arg)
	SL_t *handle;
	int  (*func)();
	void *arg;
{ 
	int i, ret;
	void **list;
#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */


	/* SL_errno = INVALID_STRUCTUR */
	if ((handle == NULL) || (handle->nbr == 0) || (handle->last_pos == 0))
		return(FALSE);

	/* SL_errno = INCOMPATIBLE_PARAMETER */
	if (func == NULL)
		return(FALSE);

	list = handle->list;
#if defined(NETWARE)
    SET();
#endif /* defined(NETWARE) */


	for (i = 0; i < handle->last_pos; i++) {
		if ((ret = func(list[i], arg)) == 0)
		  return(TRUE);
		else if (ret == -1) 
		  return(FALSE);
#if defined(NETWARE)
        YIELD();
#endif /* defined(NETWARE) */

	}
	return(FALSE);
}


/* ------------------------------------------------------------------------- */

/*
 * Name: SL_WalkVa
 *
 * Purpose: 	Return a pointer to the n-th element of the list. 
 *
 * Input: 	handle:the handle of the list.
 *		func: the func to call for each element in the list.
 *		...: Addition args to pass with the func. in the list.
 *
 * Ouput/Exceptions: if the func return 0 stops the walk with no error.
 *		if the func return -1 stops the walk with error.
 *
 *
 * Returns: 	FALSE if error, TRUE otherwise.
 *
 */

#if defined(__STDC__) || defined(STDC_SRC_COMPATIBLE)

int
SL_WalkVa(SL_t *handle, int (*func)(), ...)

#else

int
SL_WalkVa(handle, func, va_alist)
	SL_t *handle;
	int  (*func)();
	va_dcl
#endif
{ 
	int		i, ret;
	void		**list;
	va_list	args;

#if defined(NETWARE)
    SETUP_TIMERS();
#endif /* defined(NETWARE) */

#if defined(__STDC__) || defined(STDC_SRC_COMPATIBLE)
	va_start(args, func);
#else
	va_start(args);
#endif

	/* SL_errno = INVALID_STRUCTUR */
	if ((handle == NULL) || (handle->nbr == 0) || (handle->last_pos == 0))
		return(FALSE);

	/* SL_errno = INCOMPATIBLE_PARAMETER */
	if (func == NULL)
		return(FALSE);

	list = handle->list;
#if defined(NETWARE)
    SET();
#endif /* defined(NETWARE) */


	for (i = 0; i < handle->last_pos; i++) {
		if ((ret = func(list[i], args)) == 0) {
			va_end(args);
			return(TRUE);
		}
		else if (ret == -1)  {
			va_end(args);
			return(FALSE);
		}
#if defined(NETWARE)
        YIELD();
#endif /* defined(NETWARE) */

	}

   va_end(args);

	return(FALSE);
}

/* ------------------------------------------------------------------------- */

/*
 * Name: SL_NbrElm
 *
 * Purpose: 	Return  the number element of the list. 
 *
 * Input: 	handle:the handle of the list.
 *
 * Ouput/Exceptions:
 *
 *
 * Returns: 	FALSE if error, number element otherwise.
 *
 */

int
SL_NbrElm(handle)
	SL_t * handle;
{
	if (handle != NULL)
		return(handle->last_pos);
	else
		return(0);
}

/*****************************************************************************/
/*                        E N D   O F   F I L E                              */
/*****************************************************************************/
