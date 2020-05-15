#ifndef _eifdebug_h
#define _eifdebug_h

#include <stdio.h>
#include <stddef.h>

#define DEBUGFILE_EP "/tmp/.tivoli/ep_eif_debug.log"
#define DEBUGFILE_GW "/tmp/.tivoli/gw_eif_debug.log"
#define DEBUGFILE_AGENT_COMM "/tmp/.tivoli/agent_comm_debug.log"

#ifdef DEBUG

#define FILEOPEN(f) \
	FILE* fp = fopen(f,"a"); \
	if(fp) setbuf(fp, NULL);  

#define FILECLOSE(f) \
	if(f) fclose(f);

#define tecadINITIAL(f) \
	{ FILEOPEN(f) \
		if(fp) { \
			fprintf(fp,"INITIALIZED TecGatewayProgram built: %s:%s\n\n",__DATE__,__TIME__); \
			FILECLOSE(fp); \
		} \
	}

#define tecadDEBUG(f) \
	{ \
		FILEOPEN(f) \
		if(fp) fprintf(fp,	"DEBUG in %s:%i \n", \
				__FILE__, __LINE__); \
		FILECLOSE(fp)  \
	}

#define tecadDEBUG_T(f,v) \
	{ \
		FILEOPEN(f) \
		if(fp) fprintf(fp,"DEBUG in %s:%i -- %s \n", \
				__FILE__,__LINE__,v); \
		FILECLOSE(fp) \
	}

#define tecadDEBUG_S(f,v) \
	{ \
		if(v) { \
			FILEOPEN(f) \
			if(fp) fprintf(fp,"DEBUG in %s:%i -- " #v "=%s \n", \
					__FILE__,__LINE__,v); \
			FILECLOSE(fp) \
		} \
	}
 		
#define tecadDEBUG_I(f,v) \
	{ \
		FILEOPEN(f) \
		if(fp) fprintf(fp,"DEBUG in %s:%i -- " #v "=%i \n", \
				__FILE__,__LINE__,v); \
		FILECLOSE(fp) \
	}

#define tecadDEBUG_L(f,v) \
	{ \
		FILEOPEN(f) \
		if(fp) fprintf(fp,"DEBUG in %s:%i -- " #v "=%lu \n", \
				__FILE__,__LINE__,v); \
		FILECLOSE(fp) \
	}

#define tecadDEBUG_C(f,v) \
	{ \
		FILEOPEN(f) \
		if(fp) fprintf(fp,"DEBUG in %s:%i -- " #v "=%c \n", \
				__FILE__,__LINE__,v); \
		FILECLOSE(f) \
	}

#define tecadASSERT(f,cond) \
	{ \
		FILEOPEN(f) \
		((cond) || \
		if(fp) fprintf(f,"Assertion (" # cond ") failed (%s,%i)\n" \
				__FILE__, __LINE__), exit(1)); \
		FILECLOSE(f) \
	}

#else
#  define tecadINITIAL(_f)       ;
#  define tecadDEBUG(_f)         ;
#  define tecadDEBUG_T(_f, _s)   ;
#  define tecadDEBUG_S(_f, _s)   ;
#  define tecadDEBUG_I(_f, _i)   ;
#  define tecadDEBUG_L(_f, _l)   ;
#  define tecadDEBUG_C(_f, _c)   ;
#  define tecadASSERT(_f, _cond) ;
#endif

#endif
