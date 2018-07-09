#ifndef __TLI_H
#define __TLI_H


struct t_info {
	long addr;
	long options;
	long tsdu;
	long etsdu;
	long connect;
	long discon;
	long servtype;
};


/* Error codes used by TLI transport providers. */
#define	TBADADDR		1
#define	TBADOPT			2
#define	TACCES			3
#define TBADF			4
#define TNOADDR			5
#define TOUTSTATE	        6
#define TBADSEQ		        7
#define TSYSERR			8
#define TLOOK		        9
#define TBADDATA	       10
#define TBUFOVFLW	       11
#define TFLOW		       12
#define	TNODATA		       13
#define TNODIS		       14
#define TNOUDERR	       15
#define TBADFLAG	       16
#define TNOREL		       17
#define TNOTSUPPORT	       18
#define TSTATECHNG	       19


/* User level states (maintained internally by libnsl_s). */
#define T_UNINIT	0
#define T_UNBND		1
#define T_IDLE		2
#define T_OUTCON	3
#define T_INCON		4
#define T_DATAXFER	5
#define T_OUTREL	6
#define T_INREL		7
#define T_FAKE		8
#define T_HACK		12

/* Kernel level states of a transport end point. */
#define TS_UNBND	0	/* unbound */
#define	TS_WACK_BREQ	1	/* waiting for T_BIND_REQ ack  */
#define TS_WACK_UREQ	2	/* waiting for T_UNBIND_REQ ack */
#define TS_IDLE		3	/* idle */
#define TS_WACK_OPTREQ	4	/* waiting for T_OPTMGMT_REQ ack */
#define TS_WACK_CREQ	5	/* waiting for T_CONN_REQ ack */
#define TS_WCON_CREQ	6	/* waiting for T_CONN_REQ confirmation */
#define	TS_WRES_CIND	7	/* waiting for T_CONN_IND */
#define TS_WACK_CRES	8	/* waiting for T_CONN_RES ack */
#define TS_DATA_XFER	9	/* data transfer */
#define TS_WIND_ORDREL	10	/* releasing read but not write */
#define TS_WREQ_ORDREL	11      /* wait to release write but not read */
#define TS_WACK_DREQ6	12	/* waiting for T_DISCON_REQ ack */
#define TS_WACK_DREQ7	13	/* waiting for T_DISCON_REQ ack */
#define TS_WACK_DREQ9	14	/* waiting for T_DISCON_REQ ack */
#define TS_WACK_DREQ10	15	/* waiting for T_DISCON_REQ ack */
#define TS_WACK_DREQ11	16	/* waiting for T_DISCON_REQ ack */
#define TS_NOSTATES	17


/* Messages used by "timod". */
#define	T_CONN_REQ	0
#define T_CONN_RES	1
#define T_DISCON_REQ	2
#define T_DATA_REQ	3
#define T_EXDATA_REQ	4
#define T_INFO_REQ	5
#define T_BIND_REQ	6
#define T_UNBIND_REQ	7
#define T_UNITDATA_REQ	8
#define T_OPTMGMT_REQ   9
#define T_ORDREL_REQ	10

#define T_CONN_IND	11
#define T_CONN_CON	12
#define T_DISCON_IND	13
#define T_DATA_IND	14
#define T_EXDATA_IND	15
#define T_INFO_ACK	16
#define T_BIND_ACK	17
#define T_ERROR_ACK	18
#define T_OK_ACK	19
#define T_UNITDATA_IND	20
#define T_UDERROR_IND	21
#define T_OPTMGMT_ACK   22
#define T_ORDREL_IND    23

/* Flags used from user level library routines. */
#define T_MORE		0x0001
#define T_EXPEDITED	0x0002
#define T_NEGOTIATE	0x0004
#define T_CHECK		0x0008
#define T_DEFAULT	0x0010
#define T_SUCCESS	0x0020
#define T_FAILURE	0x0040
#define T_CURRENT	0x0080
#define T_PARTSUCCESS	0x0100
#define T_READONLY	0x0200
#define T_NOTSUPPORT	0x0400


struct T_conn_req {
	long	PRIM_type;	/* T_CONN_REQ */
	long	DEST_length;
	long	DEST_offset;
	long	OPT_length;
	long	OPT_offset;
};

struct T_conn_res {
	long	PRIM_type;	/* T_CONN_RES */
	void	*QUEUE_ptr;
	long	OPT_length;
	long	OPT_offset;
	long	SEQ_number;
};

struct T_discon_req {
	long	PRIM_type;	/* T_DISCON_REQ */
	long	SEQ_number;
};

struct T_data_req {
	long	PRIM_type;	/* T_DATA_REQ */
	long	MORE_flag;
};

struct T_exdata_req {
	long	PRIM_type;	/* T_EXDATA_REQ */
	long	MORE_flag;
};

struct T_info_req {
	long	PRIM_type;	/* T_INFO_REQ */
};

struct T_bind_req {
	long	PRIM_type;	/* T_BIND_REQ */
	long	ADDR_length;
	long	ADDR_offset;
	unsigned long CONIND_number;
};

struct T_unbind_req {
	long	PRIM_type;	/* T_UNBIND_REQ */
};

struct T_unitdata_req {
	long	PRIM_type;	/* T_UNITDATA_REQ */
	long	DEST_length;
	long	DEST_offset;
	long	OPT_length;
	long	OPT_offset;
};

struct T_optmgmt_req {
	long	PRIM_type;	/* T_OPTMGMT_REQ */
	long	OPT_length;
	long	OPT_offset;
	long	MGMT_flags;
};

struct T_ordrel_req {
	long	PRIM_type;	/* T_ORDREL_REQ */
};


struct T_conn_ind {
	long	PRIM_type;	/* T_CONN_IND */
	long	SRC_length;
	long	SRC_offset;
	long	OPT_length;
	long    OPT_offset;
	long    SEQ_number;
};

struct T_conn_con {
	long	PRIM_type;	/* T_CONN_CON */
	long	RES_length;
	long	RES_offset;
	long	OPT_length;
	long    OPT_offset;
};

struct T_discon_ind {
	long	PRIM_type;	/* T_DISCON_IND */
	long	DISCON_reason;
	long    SEQ_number;
};

struct T_data_ind {
	long 	PRIM_type;	/* T_DATA_IND */
	long	MORE_flag;
};

struct T_exdata_ind {
	long	PRIM_type;	/* T_EXDATA_IND */
	long	MORE_flag;
};

/* information acknowledgment */

struct T_info_ack {
	long	PRIM_type;	/* T_INFO_ACK */
	long	TSDU_size;
	long	ETSDU_size;
	long	CDATA_size;
	long	DDATA_size;
	long	ADDR_size;
	long	OPT_size;
	long    TIDU_size;
	long    SERV_type;
	long    CURRENT_state;
	long    PROVIDER_flag;
};

struct T_bind_ack {
	long		PRIM_type;	/* T_BIND_ACK */
	long		ADDR_length;
	long		ADDR_offset;
	unsigned long	CONIND_number;
};

struct T_error_ack {
	long 	PRIM_type;	/* T_ERROR_ACK */
	long	ERROR_prim;
	long	TLI_error;
	long	UNIX_error;
};

struct T_ok_ack {
	long 	PRIM_type;	/* T_OK_ACK */
	long	CORRECT_prim;
};

struct T_unitdata_ind {
	long	PRIM_type;	/* T_UNITDATA_IND */
	long	SRC_length;
	long	SRC_offset;
	long	OPT_length;
	long	OPT_offset;
};

struct T_uderror_ind {
	long	PRIM_type;	/* T_UDERROR_IND */
	long	DEST_length;
	long	DEST_offset;
	long	OPT_length;
	long	OPT_offset;
	long	ERROR_type;
};

struct T_optmgmt_ack {
	long	PRIM_type;	/* T_OPTMGMT_ACK */
	long	OPT_length;
	long	OPT_offset;
	long    MGMT_flags;
};

struct T_ordrel_ind {
	long	PRIM_type;	/* T_ORDREL_IND */
};


union T_primitives {
	long			type;
	struct T_conn_req	conn_req;
	struct T_conn_res	conn_res;
	struct T_discon_req	discon_req;
	struct T_data_req	data_req;
	struct T_exdata_req	exdata_req;
	struct T_info_req	info_req;
	struct T_bind_req	bind_req;
	struct T_unbind_req	unbind_req;
	struct T_unitdata_req	unitdata_req;
	struct T_optmgmt_req	optmgmt_req;
	struct T_ordrel_req	ordrel_req;
	struct T_conn_ind	conn_ind;
	struct T_conn_con	conn_con;
	struct T_discon_ind	discon_ind;
	struct T_data_ind	data_ind;
	struct T_exdata_ind	exdata_ind;
	struct T_info_ack	info_ack;
	struct T_bind_ack	bind_ack;
	struct T_error_ack	error_ack;
	struct T_ok_ack		ok_ack;
	struct T_unitdata_ind	unitdata_ind;
	struct T_uderror_ind	uderror_ind;
	struct T_optmgmt_ack	optmgmt_ack;
	struct T_ordrel_ind	ordrel_ind;
};


/* The t_opthdr structure defines the layout of options in a T_OPTMGMT_*
 * data buffer. This is specified in the X/Open specs but does not
 * appear to exist in SCO 3.2.x, SCO OS5, Interactive SVR4 or UnixWare 1.x.
 * There are programs that make options request however.
 * The older TLI uses struct opthdr which is different and incompatible
 * (see below).
 */
struct t_opthdr {
	unsigned long len;	/* *Total* length including header */
	unsigned long level;
	unsigned long name;
	unsigned long status;
	char value[0];		/* and onwards... */
};

struct opthdr {
	long level;
	long name;
	long len;		/* Length of option value */
	char value[0];		/* and onwards... */
};


struct T_primsg {
	struct T_primsg *next;
	unsigned char pri;
	unsigned char band;
	int length;
	long type;
};

#define XTI_MAGIC 638654838

struct T_private {
	int magic;
	long state;
	int offset;
	struct T_primsg *pfirst, *plast;
};

#define Priv(file)	((struct T_private *)(file->private_data))

extern int timod_ioctl(struct pt_regs *, int, unsigned int, void *, int, int *);
extern int timod_putmsg(int, struct inode *, int, struct pt_regs *);
extern int timod_getmsg(int, struct inode *, int, struct pt_regs *);
extern int timod_update_socket(int, struct file *, struct pt_regs *);

#ifndef SOCKSYS_MAJOR
#define SOCKSYS_MAJOR  30
#endif

#endif /* __TLI_H */
