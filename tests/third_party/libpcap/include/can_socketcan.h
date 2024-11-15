/*-
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lib_pcap_can_socketcan_h
#define lib_pcap_can_socketcan_h

#include <pcap/pcap-inttypes.h>

/*
 * SocketCAN header for CAN and CAN FD frames, as per
 * Documentation/networking/can.rst in the Linux source.
 */
typedef struct {
	uint32_t can_id;
	uint8_t payload_length;
	uint8_t fd_flags;
	uint8_t reserved1;
	uint8_t reserved2;
} pcap_can_socketcan_hdr;

/* Bits in the fd_flags field */
#define CANFD_BRS   0x01 /* bit rate switch (second bitrate for payload data) */
#define CANFD_ESI   0x02 /* error state indicator of the transmitting node */
#define CANFD_FDF   0x04 /* mark CAN FD for dual use of CAN format */

/*
 * SocketCAN header for CAN XL frames, as per Linux's can.h header.
 * This is different from pcap_can_socketcan_hdr; the flags field
 * overlaps with the payload_length field in pcap_can_socketcan_hdr -
 * the payload_length field in a CAN or CAN FD frame never has the
 * 0x80 bit set, and the flags field in a CAN XL frame always has
 * it set, allowing code reading the frame to determine whether
 * it's CAN XL or not.
 */
typedef struct {
	uint32_t priority_vcid;
	uint8_t flags;
	uint8_t sdu_type;
	uint16_t payload_length;
	uint32_t acceptance_field;
} pcap_can_socketcan_xl_hdr;

/* Bits in the flags field */
#define CANXL_SEC   0x01 /* Simple Extended Context */
#define CANXL_XLF   0x80 /* mark to distinguish CAN XL from CAN/CAN FD frames */

#endif
