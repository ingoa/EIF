#ifndef eif_h
#define eif_h

/*
 * Component Name: EIF
 *
 * $Date: 1995/10/02 19:50:17 $
 *
 * $Source: /tivoli/homes/1/tgreenbe/convert/tec/eif/eif.h,v $
 *
 * $Revision: 1.3 $
 *
 * Description:
 *      common functions for TME and non-TME agent libraries
 *
 * (C) COPYRIGHT TIVOLI Systems, Inc. 1994
 * Unpublished Work
 * All Rights Reserved
 * Licensed Material - Property of TIVOLI Systems, Inc.
 */

/* $Id: eif.h,v 1.3 1995/10/02 19:50:17 hemanta Exp $ */

#define E_RESERVED 21

#define E_BAD_HANDLE    (E_RESERVED+8)  /* bad handle specified */
#define E_SCALL         (E_RESERVED+17) /* system call failed */
#define E_IPC_BROKEN    (E_RESERVED+46) /* IPC shutdown */
#define E_IPC_CORRUPT   (E_RESERVED+52) /* IPC message corrupted */
#undef E_HOST
#define E_HOST          (E_RESERVED+27) /* host inaccessible or host failure */

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*
 * The eipc prefix means EIF IPC. TME libraries contain symbols
 * with the ipc prefix already.
 */

typedef struct eipc_handle {
    int fd;
} eipc_handle_t, *eipc_handle_p_t;


typedef int eipc_error_t;

#define HDR_DATA_LEN 2000
#define TOKEN "<START>>"

typedef struct eipc_msg_hdr {
    char     token[8];
    unsigned msg_id;
    unsigned msg_from;
    unsigned msg_to;
    unsigned msg_type;
    unsigned ipc_msg_type;
    unsigned msg_len;
    unsigned hdr_data_len;
    char     hdr_data[HDR_DATA_LEN];
} eipc_msg_hdr_t, *eipc_msg_hdr_p_t;

extern eipc_handle_p_t eipc_create_remote_client(unsigned, unsigned short, eipc_error_t *);
extern void eipc_shutdown( eipc_handle_p_t );
extern void eipc_destroy_handle( eipc_handle_p_t );
extern int  eipc_send( eipc_handle_p_t, char *, long, long, long, eipc_error_t *);

#endif  /* eif_h */
