/*	$OpenBSD: uthread_setsockopt.c,v 1.5 2008/06/03 14:45:05 kurt Exp $	*/
/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_setsockopt.c,v 1.5 1999/08/28 00:03:47 peter Exp $
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int             ret;
	struct fd_table_entry *entry;

	ret = _thread_fd_table_init(fd, FD_INIT_UNKNOWN, NULL);
	if (ret == 0) {
		entry = _thread_fd_table[fd];
		 
		_SPINLOCK(&entry->lock);
		if (entry->state == FD_ENTRY_OPEN) {
			ret = _thread_sys_setsockopt(fd, level, optname, optval, optlen);
		} else {
			ret = -1;
			errno = EBADF;
		}
		_SPINUNLOCK(&entry->lock);
	}

	return ret;
}
#endif
