/*
 * Component Name: EIF
 *
 * $Date: 1996/12/10 20:50:46 $
 *
 * $Source: 
 *
 * $Revision: 1.8 $
 *
 * Description:
 *      TEC agent communication library for non-TME agents
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#if defined(NETWARE)
# include <nwthread.h>
# include <nwfile.h>
# include <share.h>
# define ftruncate chsize
# define sleep(sec) delay((sec) * 1000)

/* open() chokes on OS/2 if file is already open.  must
 * use sopen() to open more than one at once.
 */
# define open(file, mode, perm) sopen((file), (mode), SH_DENYNO, perm)

#endif /* defined(NETWARE) */

#ifdef WIN32
#       include <direct.h>
#       include <winsock.h>

#       define close(x) CloseHandle(x)
#else
#  ifdef OS2
#	include <stdlib.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netdb.h>
#	include <signal.h>
#	include <io.h>
#	include <direct.h>
#	include <share.h>
#  else
#       include <limits.h>
#       include <stdlib.h>
#       include <unistd.h>
#       include <ctype.h>
#  endif
#endif

#ifndef MAXPATHLEN
#  ifdef PATH_MAX
#     define MAXPATHLEN PATH_MAX
#  else
#     ifdef FILENAME_MAX
#        define MAXPATHLEN FILENAME_MAX
#     endif
#  endif
#endif

#include "agent_comm.h"
#include "slist.h"
#include "tec_local.h"
#include "eif_common.h"

#if defined(WIN32)
#       define read _hread
#       define write _hwrite
#       define close CloseHandle
#       define mkdir _mkdir
#       define fstat _fstat
#       define strdup _strdup
#else
#       if !defined(AIX) && !defined(OS2)
                extern char *strdup(const char *);
#       endif

#       if !defined(SVR4) && HP_UX != 10 && !defined(OS2) && !defined(NETWARE)
                extern int ftruncate(int fildes, off_t length);
#       endif
#       if defined(OS2)
		/* open() chokes on OS/2 if file is already open.  must
		 * use sopen() to open more than one at once.
		 */
#		define open(file, mode, perm) sopen((file), O_BINARY | (mode), SH_DENYNO, perm)
#               define ftruncate chsize
#               define HFILE    int
#               define DWORD  int
#               define HANDLE  int
#               define MAX_PATH _MAX_PATH
#       endif
#endif

extern void fatal_memory_error();

#if defined(__hpux)
        int MkDirR(char *Dir);
#else
        static int MkDirR(char *Dir);
#endif

static BufferConfig     default_buffer_config = { 0 };

/* Define a default buffer path for Windows platform */									

#ifdef WIN32																			
#define WIN_BUFFER_PATH "%SYSTEMROOT%\\SYSTEM32\\DRIVERS\\ETC\\TIVOLI\\TEC\\CACHE.DAT";  
#endif																					

#if defined(NETWARE)
/*
** Cache file path.
*/
#define BUFFER_PATH "SYS:ETC\\TIVOLI\\TEC\\CACHE.DAT"

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


/*
 * pc_truncate
 *
 * Purpose: 
 *    Truncate a (buffer) file on a PC
 *
 * Description: 
 *      Open, change size, close
 *
 * Returns: 
 *      Return value from _chsize, -1 on error
 *
 * Notes/Dependencies: 
 *
 */

#if defined(WIN32)

static int
pc_truncate(char *path, long size)
{
        int fh;
        int result = -1;

        if( (fh = _open( path, _O_RDWR | _O_CREAT, _S_IREAD | _S_IWRITE ))  != -1 ){
                result = _chsize( fh, size );
                _close( fh );
        }
        return result;
}

#endif

/*
 * trunc_at_top
 *
 * Purpose: 
 *    Truncate a (buffer) file by removing the beginning and keeping the most
 *    recent events.
 *
 * Description: 
 *
 * Returns: 
 *      Nothing
 *
 * Notes/Dependencies: 
 *
 */

static void
trunc_at_top(char* path, int offset)
{
        int     n;
        int     size = 0;
        char    buf[4096];
        
#ifndef WIN32

        int fd_to = open(path, O_RDWR, 0);
        int fd_from = open(path, O_RDONLY, 0);

        if (fd_from < 0 || fd_to < 0)
                return;

        lseek(fd_from, offset, SEEK_SET);

        while ((n = read(fd_from, buf, 4096))) {
                write(fd_to, buf, n);
                size += n;
        }

        close(fd_from);
        ftruncate(fd_to, (off_t) size);
        close(fd_to);

#else

        SECURITY_ATTRIBUTES  sa;
        HANDLE fd_from;
        HANDLE fd_to;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        fd_from = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                                                FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                                OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

        fd_to = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                                         FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                         OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

        if (fd_from == INVALID_HANDLE_VALUE || fd_to == INVALID_HANDLE_VALUE) {
                return;
        }

        _llseek((HFILE)fd_from, offset, FILE_BEGIN);

        while ((n = _hread((HFILE) fd_from, buf, 4096))) {
                _hwrite((HFILE) fd_to, buf, n);
                size += n;
        }

        _llseek((HFILE)fd_to, size, FILE_BEGIN);
        SetEndOfFile(fd_to);
        CloseHandle(fd_from);
        CloseHandle(fd_to);

#endif

}

/*
 * init_buffer_config
 *
 * Purpose: 
 *    Initializes some configuration parameters for buffering.
 *
 * Description: 
 *
 * Returns: 
 *      Nothing
 *
 * Notes/Dependencies: 
 *
 */

static void
init_buffer_config(BufferConfig *buffer_info)
{
        char    *env;

#ifdef WIN32
        static char     exp[MAXPATHLEN];
#endif
#ifdef OS2
	char *new_fn, *ptr;
#endif

        buffer_info->initialized = 1;
        buffer_info->do_buffer = !tec_agent_match("BufferEvents", "NO");

        /*
         * Event max size in K
         */
        buffer_info->event_max_size = 4096;

        if ((env = tec_agent_getenv("EventMaxSize")) != NULL) {
                if (isdigit(*env))
                        buffer_info->event_max_size = atol(env);
        }

        /*
         * Event adapter buffer file's max size in K
         */
        buffer_info->max_size = 64 * 1024;

        if ((env = tec_agent_getenv("BufEvtMaxSize")) != NULL) {
                if (isdigit(*env))
                        buffer_info->max_size = atol(env) * 1024;
        }

        /*
         * Size, in K, to shrink adapter file's buffer when MaxSize exceeded
         */
        buffer_info->shrink_size = 8 * 1024;

        if ((env = tec_agent_getenv("BufEvtShrinkSize")) != NULL) {
                if (isdigit(*env))
                        buffer_info->shrink_size = atol(env) * 1024;
        }
        if(buffer_info->shrink_size < buffer_info->event_max_size)
                buffer_info->shrink_size = buffer_info->event_max_size;
                
        /*
         * If BufferFlushRate is not specified, then we will use 0 which means
         * don't wait at all
         */
        buffer_info->flush_rate = 0;

        if ((env = tec_agent_getenv("BufferFlushRate")) != NULL) {
                if (isdigit(*env))
                        buffer_info->flush_rate = (short) atoi(env);
        }

        /*
         * Get the size of the buffer file blocks to read.
         * This MUST be large enough for at least one event.
         */
        buffer_info->read_blk_len = 64 * 1024;

        if ((env = tec_agent_getenv("BufEvtRdBlkLen")) != NULL) {
                if (isdigit(*env))
                        buffer_info->read_blk_len = atol(env);
        }

        if ((env = tec_agent_getenv("BufEvtPath")) != NULL) {
                buffer_info->buffer_path = env;

#		ifdef OS2
		        /* Change '\' to '/' in filename */
		        new_fn = strdup(env);
		        for (ptr = new_fn; *ptr != NULL; ptr++)
               			if (*ptr == '\\') *ptr = '/';
			buffer_info->buffer_path = new_fn;
#		endif
        }
        else {
#               ifndef WIN32
#if !defined(NETWARE)
                        buffer_info->buffer_path = "/etc/Tivoli/tec/cache";
#else  /* defined(NETWARE) */
        buffer_info->buffer_path = BUFFER_PATH;
#endif /* !defined(NETWARE) */
#               else
                     buffer_info->buffer_path = WIN_BUFFER_PATH;	
#               endif
        }
#       ifdef WIN32																		
               if (ExpandEnvironmentStrings(buffer_info->buffer_path, exp,MAXPATHLEN)) 
                       buffer_info->buffer_path = strdup(exp);							
#       endif																			

}

static char *
filter_events(char *message, int msg_len)
{
        char    *new_msg;
        char    *nptr;
        char    *mptr = message;
        char    *eptr;
        char    orig_char;
        char    different = 0;

        if (!msg_len)
                return(NULL);

        nptr = new_msg = (char *) malloc((size_t) msg_len + 1);

        if (!new_msg)
                return(NULL);

        /*
         * Break up 'message' into individual events in case there are more than one
         */
        while ((eptr = strchr(mptr, TECAD_EVENT_END_CHAR)) != NULL) {
                orig_char = *(++eptr);
                *eptr = '\0';

                if (!tec_filter_event(mptr, FILTER_EVENT_CACHE_TYPE)) {
                        strcpy(nptr, mptr);
                        nptr += (eptr - mptr);
                }
                else
                        different = 1;

                *eptr = orig_char;
                mptr = eptr;
        }

        /*
         * If the new message is not different than the first, return the input event
         * If the new message is empty, return NULL.
         * Otherwise, return the new message.
         */
        if (!different) {
                if (new_msg != NULL)
                        free(new_msg);

                new_msg = message;
        }
        else if (nptr == new_msg) {
                if (new_msg != NULL)
                        free(new_msg);

                new_msg = NULL;
        }

        return(new_msg);
}

/*
 * tec_buffer_event_to_file
 *
 * Purpose: 
 *    Buffers an event or events to a specific file.
 *
 * Description: 
 *
 * Returns: 
 *      The number of bytes written.
 *
 * Notes/Dependencies: 
 *
 */


int 
tec_buffer_event_to_file(char *message, char *BufEvtPath)
{
#define OFLAG (O_RDWR | O_APPEND | O_CREAT)

#ifdef WIN32
#       define PMODE _O_RDWR
        SECURITY_ATTRIBUTES     sa;
        HANDLE                                  BufEvtFD;
        HANDLE                                  CpFromFD;
        HANDLE                                  CpToFD;
        DWORD                                           derror;
#else
#       if defined(OS2) || defined(NETWARE)
#               define PMODE (S_IREAD | S_IWRITE)
#       else
#               define PMODE (S_IRWXU | S_IRGRP | S_IROTH)
#       endif
        int                                             BufEvtFD;
        int                                             CpFromFD;
        int                                             CpToFD;
#endif

        BufferConfig    *b_info;
        char                            *new_msg;
        char                            *Dir;
        char                            *DirEnd;
        char                            *ShrinkBuf = NULL;
        char                            *ShrinkBufEventStartPtr = NULL;
        char                            *ShrinkBufPtr = NULL;
        char                            *Separator = "\001";
        off_t                           EventSeekPos;
        int                             DirVal = 0;
        int                             BytesRd;
        int                             BytesWrt;
        int                             MsgLen;
        int                             NumBytesToRd;
        int                             RetVal;
        int                             SeparatorLen = strlen(Separator);
        int                             TotBytesRd;
        struct stat             BufEvtStat;

        b_info = &default_buffer_config;

        if (!b_info->initialized)
                init_buffer_config(b_info);

        /*
         * Return if BufferEvents was specified, but is not yes or YES
         */
        if (!b_info->do_buffer)
                return(-1);

        /*
         * Check if event should not be cached because it is filtered.
         * This is different then the 'normal' event filtering and special
         * ONLY for event that are to be cached. It uses a separate filter
         * then the normal event filtering and is meant to be more restrictive.
         * Events may be important enough to send back but not important
         * enough if they are *first cached* !!
         */
        MsgLen = strlen(message);
        new_msg = filter_events(message, MsgLen);

        if (new_msg == NULL)
                return(0);
        else if (new_msg != message) {
                message = new_msg;
                MsgLen = strlen(message);
        }
        else
                new_msg = NULL;

        /*
         * If no buffer path specified, look it up.
         */
        if (BufEvtPath == NULL)
                BufEvtPath = b_info->buffer_path;

        /*
         * create the file's directory if it doesn't exist
         */
        Dir = strdup(BufEvtPath);

#if !defined(NETWARE)
        if ((DirEnd = strrchr(Dir, '/')) != NULL) {
#else  /* defined(NETWARE) */
    if ((DirEnd = strrchr(Dir, '\\')) != NULL) {
#endif /* !defined(NETWARE) */
                *DirEnd = 0;                /* terminate shortened path */

                if ((DirVal = MkDirR(Dir)) != 0) {
                        if (new_msg != NULL) free(new_msg);
                        return(DirVal);
                }
        }
        free(Dir);

#ifndef WIN32
	fprintf(stderr, "Opening cache file: %s\n", BufEvtPath);
        if ((BufEvtFD = open(BufEvtPath, OFLAG, PMODE)) == -1) {
                if (new_msg != NULL) free(new_msg);
                return(-1);
        }

        if (fstat(BufEvtFD, &BufEvtStat) == -1) {
                close(BufEvtFD);
                if (new_msg != NULL) free(new_msg);
                return(-1);
        }

#else

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        BufEvtFD = CreateFile(BufEvtPath, GENERIC_READ | GENERIC_WRITE,
                                                                 FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                                 OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

        if (BufEvtFD == INVALID_HANDLE_VALUE) {
                if (new_msg != NULL) free(new_msg);
                return(-1);
        }

        if ((BufEvtStat.st_size = GetFileSize(BufEvtFD, NULL)) == 0xFFFFFFFF) {
                CloseHandle(BufEvtFD);
                derror = GetLastError();
                if (new_msg != NULL) free(new_msg);
                return(-1);
        }

#endif

        /* If the event buffer cache will overflow then shrink it.
         * Deleting data anywhere in a file is not supported by
         * ftruncate(). The data must be copied down so the end
         * of the file can be deleted.
         * 'BufEvtShrinkSize' is an estimate of how much to shrink
         * the file since this may be in the middle of an event.
         * Search for the start of the next event and start shrinking.
         * BufEvtFD is closed and reopened after the file is shrunk
         * as a precaution since I'm not sure if all OS's handle
         * append mode on a file which other FD's are modifying.
         */
        if ((BufEvtStat.st_size + MsgLen) > b_info->max_size) {
                close(BufEvtFD);

                if (b_info->shrink_size > BufEvtStat.st_size) {

                        /*
                         * Shrinking the whole file so just truncate it
                         */

#ifndef WIN32
                        /*************************************************************/
                        /* Just open & truncate the file instead of using O_TRUNC    */
                        /* option on the OPEN. ftruncate discards the file contents. */
                        /*************************************************************/
                        if ((BufEvtFD = open(BufEvtPath, O_RDWR, 0)) == -1) {
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }

                        if (ftruncate(BufEvtFD,0) == -1) {
                                close(BufEvtFD);
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }
                        close(BufEvtFD);

#else

                        BufEvtFD = CreateFile(BufEvtPath, GENERIC_READ | GENERIC_WRITE,
                                                                                 FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                                                 CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

                        if (BufEvtFD == INVALID_HANDLE_VALUE) {
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }
#endif

                }
                else {
                        /*
                         * event_max_size is if 'BufEvtShrinkSize' is not on an event boundary
                         */
                        if ((ShrinkBuf = (char *) malloc(b_info->shrink_size + b_info->event_max_size )) == NULL) {
                                fatal_memory_error();
                        }

#ifndef WIN32

                        if ((CpToFD = open(BufEvtPath, O_RDWR, 0)) == -1) {
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }
                        if ((CpFromFD = open(BufEvtPath, O_RDWR, 0)) == -1) {
                                close(CpToFD);
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }

#else

                        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
                        sa.lpSecurityDescriptor = NULL;
                        sa.bInheritHandle = TRUE;
                        CpToFD = CreateFile(BufEvtPath, GENERIC_READ | GENERIC_WRITE,
                                                                          FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                                          OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

                        if (CpToFD == INVALID_HANDLE_VALUE) {
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }

                        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
                        sa.lpSecurityDescriptor = NULL;
                        sa.bInheritHandle = TRUE;
                        CpFromFD = CreateFile(BufEvtPath, GENERIC_READ | GENERIC_WRITE,
                                                                                 FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                                                 OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

                        if (CpFromFD == INVALID_HANDLE_VALUE) {
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }

#endif
                        /*
                         * Establish the place to begin shrinking by locating the
                         * start of the next event.
                         *   read() may return fewer bytes then requested so loop to
                         *   read all requested bytes.
                         */
                        NumBytesToRd = b_info->shrink_size + b_info->event_max_size;
                        ShrinkBufPtr = ShrinkBuf;
                        TotBytesRd = 0;

#ifdef WIN32
                        while (((BytesRd = read((HFILE)CpFromFD,ShrinkBufPtr,NumBytesToRd)) >0)
                                         && (BytesRd != NumBytesToRd))
#else
                        while (((BytesRd = read(CpFromFD, ShrinkBufPtr, NumBytesToRd)) > 0)
                                         && (BytesRd != NumBytesToRd))
#endif
                        {
                                TotBytesRd += BytesRd;
                                NumBytesToRd -= BytesRd;
                                ShrinkBufPtr += BytesRd;
                        }

                        if (BytesRd < 0) {
                                close(CpFromFD);
                                close(CpToFD);
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }
                        else {
                                TotBytesRd += BytesRd;

                                if (TotBytesRd <= (b_info->shrink_size)) {
                                        close(CpFromFD);      /* failed to rd enough bytes */
                                        close(CpToFD);        /* to collapse the file so   */
                                        if (new_msg != NULL) free(new_msg);
                                        return(-2);           /* return an error.          */
                                }
                        }

                        ShrinkBufEventStartPtr = strtok(ShrinkBuf+b_info->shrink_size, Separator);
                        ShrinkBufEventStartPtr += (strlen(ShrinkBufEventStartPtr) + SeparatorLen);
                        EventSeekPos = ShrinkBufEventStartPtr - ShrinkBuf;

#ifndef WIN32
                        if (lseek(CpFromFD, EventSeekPos, SEEK_SET) == -1) 
#else
                        if (_llseek((HFILE) CpFromFD, EventSeekPos, FILE_BEGIN) == -1) 
#endif
                        {
                                close(CpFromFD);
                                close(CpToFD);
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }

                        TotBytesRd = 0;
                        ShrinkBufPtr = ShrinkBuf;

#ifdef WIN32
                        while ((BytesRd=read((HFILE)CpFromFD,ShrinkBufPtr,b_info->shrink_size))>0)
#else
                        while ((BytesRd = read(CpFromFD, ShrinkBufPtr, b_info->shrink_size)) > 0)
#endif
                        {
                                TotBytesRd += BytesRd;

#ifdef WIN32
                                if ((BytesWrt=write((HFILE)CpToFD,ShrinkBufPtr,BytesRd)) != BytesRd)
#else
                                if ((BytesWrt = write(CpToFD, ShrinkBufPtr, BytesRd)) != BytesRd)
#endif
                                {
                                        close(CpFromFD);
                                        close(CpToFD);
                                        if (new_msg != NULL) free(new_msg);
                                        return(-1);
                                }
                        }

#ifndef WIN32
                        if ((RetVal = ftruncate(CpToFD, TotBytesRd)) != 0) {
                                close(CpFromFD);
                                close(CpToFD);
                                if (new_msg != NULL) free(new_msg);
                                return(-1);        
                        }
                        close(CpFromFD);
                        close(CpToFD);
#else
                        close(CpFromFD);
                        close(CpToFD);

                        if ((RetVal = pc_truncate(BufEvtPath, TotBytesRd)) != 0) {
                                if (new_msg != NULL) free(new_msg);
                                return(-1);
                        }
#endif
                        free(ShrinkBuf);
                }

#ifndef WIN32

                if ((BufEvtFD = open(BufEvtPath, OFLAG, PMODE)) == -1) {
                        if (new_msg != NULL) free(new_msg);
                        return(-1);
                }

#else

                sa.nLength = sizeof(SECURITY_ATTRIBUTES);
                sa.lpSecurityDescriptor = NULL;
                sa.bInheritHandle = TRUE;

                BufEvtFD = CreateFile(BufEvtPath, GENERIC_READ | GENERIC_WRITE,
                                                                         FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                                                                         OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

                if (BufEvtFD == INVALID_HANDLE_VALUE) {
                        derror = GetLastError();
                        if (new_msg != NULL) free(new_msg);
                        return(-1);
                } 

#endif

        }

        /*
         * Cache event to file
         */
#ifdef WIN32
        _llseek((HFILE) BufEvtFD, 0, FILE_END);
        write((HFILE) BufEvtFD, message, MsgLen);
#else
        write(BufEvtFD, message, MsgLen);
#endif

        close(BufEvtFD);

        if (new_msg != NULL)
                free(new_msg);

        return(MsgLen); 
}

int
tec_buffer_event(char *message)
{
        return(tec_buffer_event_to_file(message, NULL));
}

void
tec_free_func(void *ptr)
{
        if (ptr)
                free(ptr);
}

#ifdef WIN32

long
tec_event_buffer_size(tec_handle_t handle, char *BufEvtPath, HANDLE *fd_ptr)

#else

long
tec_event_buffer_size(tec_handle_t handle, char *BufEvtPath, int *fd_ptr)

#endif

{
        BufferConfig                    *b_info;
        struct stat                             BufEvtStat;

#ifndef WIN32
        int                                             BufEvtFD;
#else
        HANDLE                                  BufEvtFD;
        SECURITY_ATTRIBUTES     sa;
#endif

        if (fd_ptr != NULL)
                *fd_ptr = 0;

        b_info = &default_buffer_config;

        if (!b_info->initialized)
                init_buffer_config(b_info);

        /*
         * If no buffer path specified, look it up.
         */
        if (BufEvtPath == NULL)
                BufEvtPath = b_info->buffer_path;

        BufEvtStat.st_size = -1;

#ifndef WIN32

        if ((BufEvtFD = open(BufEvtPath, O_RDWR, 0)) != -1)
                fstat(BufEvtFD, &BufEvtStat);

#else

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        BufEvtFD = CreateFile(BufEvtPath, GENERIC_READ | GENERIC_WRITE,
                                                                 FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, OPEN_ALWAYS,
                                                                 FILE_ATTRIBUTE_NORMAL, NULL);

        if (BufEvtFD != INVALID_HANDLE_VALUE) {
                if ((BufEvtStat.st_size = GetFileSize(BufEvtFD, NULL)) == 0xFFFFFFFF)
                        BufEvtStat.st_size = -1;
        }

        errno = (int) GetLastError();

#endif

        if (fd_ptr != NULL)
                *fd_ptr = BufEvtFD;
        else
                close(BufEvtFD);

        return(BufEvtStat.st_size);
}

/*
 * The first two input parameters are self-explanatory, while the third one
 * represents a pointer to the appropriate send function which will be used
 * to put events to the TEC server.
 *
 * Flush all events in the event buffer cache (on disk).
 * Try to send each event. If it won't go, then it will
 * will get re-buffered again by tec_buffer_event().
 *
 * As data blocks are read a partial event may be read in.
 * To detect this each potential _next_ event is checked
 * for the 'Separator' string. If the 'Separator' is not
 * found then it may contain a partial event. If the next
 * 'event' is NULL then the read ended on an event boundary
 * otherwise it is a partial event so move it to the front
 * of the buffer and have the read() fill in after it.
 *
 */

int
tec_flush_events_from_file(tec_handle_t handle, char *BufEvtPath)
{
        BufferConfig                    *b_info;
        tec_handle_common_t     *th = (tec_handle_common_t *) handle;
        char                                            *event;
        char                                            *EventCopy = NULL;
        char                                            *NextEvtPtr = NULL;
        char                                            *BufEvtBlk = NULL;
        char                                            *BufEvtBlkPtr = NULL;
        char                                            *PartEvtBuf = NULL;
        char                                            *Separator = "\001";
        char                                            *SeparatorFnd = NULL;
        unsigned long                   RdLen;
        unsigned long                   PartEvtLen;
        unsigned long                   BytesRd;
        unsigned long                   TotBytesRd = 0;
        unsigned long                   EventCopyLen;
        int                                             SeparatorLen = strlen(Separator);
        int                                             nap = 0;
        int                                             sent = 0;
        int                                             EvtCnt = 0;
        long                                            buf_file_size;

#ifndef WIN32
        int                                             BufEvtFD;
#if !defined(OS2) && !defined(NETWARE)
	struct flock buflock;		/* used to lock cache file */
#  endif
#else
        HANDLE                                  BufEvtFD;
	int	buf_size_left;	/* keep track of buffer left to flush */
#endif

        b_info = &default_buffer_config;

        if (!b_info->initialized)
                init_buffer_config(b_info);

        /*
         * If no buffer path specified, look it up.
         */
        if (BufEvtPath == NULL)
                BufEvtPath = b_info->buffer_path;

        buf_file_size = tec_event_buffer_size(handle, BufEvtPath, &BufEvtFD);

        if (buf_file_size < 0) {
                close(BufEvtFD);
                return(errno);
        }

#ifndef WIN32
#if !defined(OS2) && !defined(NETWARE)
	buflock.l_type = F_WRLCK;
	buflock.l_start = 0;
	buflock.l_whence = SEEK_SET;
	buflock.l_len = 0;

	/* Lock the cache file so no one else tries to flush at the same time 
	   If we can't lock it means someone else is flushing, so exit       */
	if (fcntl(BufEvtFD, F_SETLK, &buflock) < 0) {
		close(BufEvtFD);
		return(sent);
	}
#  endif
#else
	buf_size_left = buf_file_size;

	/* Lock the cache file so no one else tries to flush at the same time 
	   If we can't lock it means someone else is flushing, so exit       */
	if (!LockFile(BufEvtFD, 0, 0, buf_size_left, 0)) {
		CloseHandle(BufEvtFD);
		return(sent);
	}
#endif

        if ((BufEvtBlk = (char *) malloc(b_info->read_blk_len + 1)) == NULL)
                fatal_memory_error();

        /*
         * Only now we can set in_flush_events, and do not return from this
         * function without clearing it.
         */
        th->in_flush_events = 1;
        BufEvtBlkPtr = BufEvtBlk;
        RdLen = b_info->read_blk_len;

        /*
         * During a flush, the event server may go down and this would cause
         * the event to reenter the cache. Thus, we read the cache file only
         * to the size before flushing (as in BufEvtStat) and later shrink
         * the file from the beginning to this size. Anything beyond this size
         * must be due to events reentering the cache (or brand new events
         * if we were threaded).
         */
        
#ifdef WIN32
        while ((TotBytesRd < buf_file_size)
                         && (BytesRd = read((HFILE) BufEvtFD, BufEvtBlkPtr, RdLen)) > 0)
#else
        while ((TotBytesRd < buf_file_size)
                         && (BytesRd = read(BufEvtFD, BufEvtBlkPtr, RdLen)) > 0)
#endif
        {
                TotBytesRd += BytesRd;

                /*
                 * Add terminator to make a real string
                 */      
                if (TotBytesRd <= buf_file_size) {
                        BufEvtBlkPtr[BytesRd] = 0;
                }
                else {
                        /* we have read past the original cache size */
                        BufEvtBlkPtr[BytesRd - (TotBytesRd - buf_file_size)] = 0;     
                }
                
                SeparatorFnd = strstr(BufEvtBlk, Separator);
                event = strtok(BufEvtBlk, Separator); /* First event */
                NextEvtPtr = event + strlen(event) + SeparatorLen;

                while (SeparatorFnd != NULL) {
                        EvtCnt++;

                        /* Copy the event putting the 'Separator' back. Putting the
                         * Separator back into 'event' is inadequate because that
                         * does not form an event *string* (the string continues to
                         * the end of the read buffer).   */
                        EventCopyLen = strlen(event) + SeparatorLen + 1;

                        if ((EventCopy = (char *) malloc(EventCopyLen)) == NULL)
                                fatal_memory_error();

                        strcpy(EventCopy, event);
                        strcat(EventCopy, Separator);

                        if (tec_put_event(handle, EventCopy) > 0 )
                                sent++;

#ifdef WIN32
			/* must unlock for NT so trunc_at_top() can open it */
			UnlockFile(BufEvtFD, 0, 0, buf_size_left, 0);
#endif

			trunc_at_top(BufEvtPath, strlen(event) + SeparatorLen);
			/* lock it back up - Unix unlocks in trunc_at_top() */
#ifndef WIN32
#if !defined(OS2) && !defined(NETWARE)
			if (fcntl(BufEvtFD, F_SETLK, &buflock) < 0) {
				close(BufEvtFD);
				return(sent);
			}
#  endif
#else
			buf_size_left = buf_size_left - (strlen(event) + SeparatorLen);

			if (!LockFile(BufEvtFD, 0, 0, buf_size_left, 0)) {
				CloseHandle(BufEvtFD);
				return(sent);
			}
#endif

                        free(EventCopy);

                        SeparatorFnd = strstr(NextEvtPtr, Separator);
                        event = strtok(NextEvtPtr, Separator); /* next event */

                        if (event != NULL)
                                NextEvtPtr = event + strlen(event) + SeparatorLen;

                        /* If rate is defined, and we have sent 'rate' events,
                         * sleep for a minute.  This defines rate events/minute
                         * even though they are sent in bursts
                         */
                        if (b_info->flush_rate)
                                nap = (EvtCnt % b_info->flush_rate);

                        if (b_info->flush_rate != 0  && nap == 0) {
#ifndef WIN32
#ifdef OS2
                                DosSleep(60 * 1000L);
#else
                                sleep(60);
#endif
#else
                                Sleep(60 * 1000);
#endif
                        }
                }

                /*
                 * If a partial event is left in the memory buffer then move it
                 * to the front of the buffer and have the read() fill in after
                 * it.     */
                if (event != NULL) {
                        /*
                         * Create a temp buffer to hold the partial event
                         * before copying back into the real buffer just
                         * in case a small buffer was read/specified and
                         * the memcpy() might overlap.
                         * Init the BufEvtBlkPtr and RdLen correctly.
                         */
                        PartEvtLen = BufEvtBlkPtr + BytesRd - event;

                        if ((PartEvtBuf = (char *) malloc(PartEvtLen)) == NULL)
                                fatal_memory_error();

                        memcpy(PartEvtBuf, event, PartEvtLen);
                        memcpy(BufEvtBlk, PartEvtBuf, PartEvtLen);
                        BufEvtBlkPtr = BufEvtBlk + PartEvtLen;
                        RdLen = b_info->read_blk_len - PartEvtLen;
                        free(PartEvtBuf);
                }
                else {
                        /* reset the buffer ptr and read length */
                        BufEvtBlkPtr = BufEvtBlk;
                        RdLen = b_info->read_blk_len;
                }
        }
        free(BufEvtBlk);

#ifndef WIN32
#  if  !defined(OS2) && !defined(NETWARE)
	buflock.l_type = F_UNLCK;
	fcntl(BufEvtFD, F_SETLKW, &buflock);
#  endif
        close(BufEvtFD);
#else
	UnlockFile(BufEvtFD, 0, 0, buf_size_left, 0);
	CloseHandle(BufEvtFD);
#endif

        th->in_flush_events = 0;

        return(sent);
}


/*
 * tec_flush_events -
 *
 * Description:
 * Input:
 * handle - the event server handle to send the cached
 *           events to.
 */
int
tec_flush_events(tec_handle_t handle)
{
        return(tec_flush_events_from_file(handle, NULL));
}

/*
 * MkDirR
 *
 * Description:
 *      Recursively make the given directory.
 *      If the mkdir() works or the directory exists then return.
 *      If ENOENT is returned by mkdir(), something in the path
 *      doesn't exist, then remove the last node from the path and
 *      call MkDirR() to try and make that directory.
 *
 * Input:
 *      Dir - the directory path to create.
 */
int
MkDirR(char *Dir) {
#if defined(NETWARE)
    /*
    ** Can't get meaningful errno from mkdir(), and can't open()
    ** and stat() a directory...
    **
    ** Unconditionally create the whole directory path, more
    ** elegant solutions notwithstanding...
    */

    char currPath[255];
    char* currElem = NULL;

    currPath[0] = '\0';
    currElem = strtok(Dir, "\\");

    while (currElem != NULL) {
        if (currElem[strlen(currElem) - 1] == ':') {
            strcat(currPath, currElem);
            currElem = strtok(NULL, "\\");
            continue;
        }

        strcat(currPath, currElem);
        strcat(currPath, "\\");
        mkdir(currPath);

        currElem = strtok(NULL, "\\");
    }

    return 0;

#else  /* !defined(NETWARE) */

        char    *DirDup;
        char    *PathEnd;
        int     RetVal = 0;

#if !defined(WIN32) && !defined(OS2)
        if (mkdir(Dir, (S_IRWXU | S_IRGRP | S_IROTH)) != 0) 
#else
        /* Don't try to make a directory that is actually a drive or 
           partition name */
        if (Dir[strlen(Dir)-1] == ':')
                return RetVal;
        if (mkdir(Dir) != 0) 
#endif
        {
                switch (errno) {
                        /*
                         * Part of path doesn't exist: create parent path then create Dir.
                         */
                        case ENOENT:
                                DirDup = strdup(Dir);

                                if ((PathEnd = strrchr(DirDup, '/')) != NULL) {
                                        *PathEnd = 0; /* terminate shortened path */

                                        if (MkDirR(DirDup) == 0) {
#if !defined(WIN32) && !defined(OS2)
                                                if (mkdir(Dir, (S_IRWXU | S_IRGRP | S_IROTH)) != 0)
#else
                                                        if (mkdir(Dir) != 0)
#endif
                                                {
                                                        RetVal = -4;
                                                }
                                        }
                                        else {
                                                RetVal = -2;
                                        }
                                }
                                else {
                                        RetVal = -3; /* mkdir err creating first elem of Dir */
                                }
                                free(DirDup);
                                break;

                        case EEXIST:
                                RetVal = 0;
                                break;

#ifdef OS2
			/* OS/2 returns EACCESS if directory exists */
                        case EACCES:
                                RetVal = 0;
                                break;
#endif

                        default:
                                RetVal = -1;  /* must be some sort of error */
                                break;
                }
        }
        return RetVal;
#endif /* NETWARE */
}

