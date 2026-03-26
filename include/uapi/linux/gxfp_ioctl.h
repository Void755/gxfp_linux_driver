#ifndef __UAPI_GXFP_IOCTL_H
#define __UAPI_GXFP_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define GXFP_IOCTL_MAGIC 'G'

/*
 * 4-byte MP header + payload must fit GXFP_TX_BUFFER_SIZE(512).
 */
#define GXFP_IOCTL_TX_PAYLOAD_MAX 508u

/* Maximum TAP payload bytes exported via read(2) records. */
#define GXFP_IOCTL_TAP_PAYLOAD_MAX (128u * 1024u)

/* 
 * Stream tap (read/poll) record header.
 * Followed by `len` bytes of raw upstream packet.
 */
struct gxfp_tap_hdr {
	__u32 len;       /* payload bytes following this header */
	__u32 type;      /* mp_flags>>4 for MP packets */
	__u32 mp_len;    /* MP length field */
	__u32 _rsvd0;
	__u64 ts_ns;     /* ktime_get_ns() */
	__u8 head16[16]; /* first 16 bytes of payload */
};

/*
 * write(2) packet header.
 * Userspace writes: [gxfp_tx_pkt_hdr][payload bytes]
 */
struct gxfp_tx_pkt_hdr {
	__u8 mp_flags;
	__u8 _pad0;
	__u16 payload_len;
	__u32 flags;
};

#define GXFP_IOCTL_FLUSH_RXQ _IO(GXFP_IOCTL_MAGIC, 0x11)

#endif /* __UAPI_GXFP_IOCTL_H */
