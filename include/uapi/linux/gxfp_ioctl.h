#ifndef __UAPI_GXFP_IOCTL_H
#define __UAPI_GXFP_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define GXFP_IOCTL_MAGIC 'G'

/*
 * 4-byte MP header + payload must fit GXFP_TX_BUFFER_SIZE(512).
 */
#define GXFP_IOCTL_TX_PAYLOAD_MAX 508u

/* Maximum protocol payload bytes exported via read(2) records. */
#define GXFP_IOCTL_TAP_PAYLOAD_MAX (128u * 1024u)

/* 
 * Stream tap (read/poll) record header.
 * Followed by `len` bytes of protocol payload.
 */
struct gxfp_tap_hdr {
	__u32 len;       /* protocol payload bytes */
	__u32 type;      /* MP type */
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
