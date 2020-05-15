/*****************************************************************************/
/*                                                                           */
/*  FILE: slist.h                                                            */
/*                                                                           */
/*  FUNCTION: Header file for T/EC Adapter structured list utilities.        */
/*                                                                           */
/*****************************************************************************/

#ifndef _SLIST_H
#define _SLIST_H

/**********************************/
/*** Constant Macro Definitions ***/
/**********************************/

#define BLOCK_SIZE	5
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif /* TRUE */

/************************/
/*** Enumerated types ***/
/************************/

/***********************/
/*** Function Macros ***/
/***********************/

/***********************************/
/*** Class/Structure Definitions ***/
/***********************************/

struct SL {
  void **list;
  int  nbr;
  int  last_pos;
};

/****************/
/*** Typedefs ***/
/****************/

typedef struct SL SL_t;

/**********************/
/*** Global Externs ***/
/**********************/

/***************************/
/*** Function Prototypes ***/
/***************************/

extern SL_t * SL_Create (  /* */ );
extern int    SL_InsLast ( /* SL_t * handle, void *elm */ );
extern void * SL_Find (    /* SL_t * handle, 
			      void * cmp_data, 
			      int (*cmp_func)() */ );
extern int    SL_FindPos (    /* SL_t * handle, 
			      void * cmp_data, 
			      int (*cmp_func)() */ );
extern int    SL_FindReplace ( /* SL_t * handle, 
				  void * replace_data, 
				  int (*cmp_func)(),
				  void (*del_func)() */ );
extern void * SL_GetPos (  /* SL_t * handle, int pos */ );
extern void   SL_DelAll (  /* SL_t * handle, void ( *del_func)() */ );
extern int    SL_Walk (    /* SL_t * handle, int ( *func)()*/ );
extern int    SL_Walk2 (    /* SL_t * handle, int ( *func)(), void *arg */ );
extern int    SL_NbrElm (  /* SL_t * handle */ );

#if defined(__STDC__) || defined(STDC_SRC_COMPATIBLE)
extern int SL_WalkVa(SL_t *, int (*func)(), ...);
#else
extern int SL_WalkVa ();
#endif

#endif /* _SLIST_H */


