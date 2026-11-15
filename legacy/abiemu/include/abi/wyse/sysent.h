/*
 * Copyright (c) 2001 Christoph Hellwig.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _ABI_WYSE_SYSENT_H
#define _ABI_WYSE_SYSENT_H

#ident "%W% %G%"

/*
 * External function declarations for the Wyse V/386 syscall table.
 */

struct sockaddr;


/* ptrace.c */
extern int wyse_ptrace(int, int, u_long, u_long);

/* socket.c */
extern int wyse_gethostname(char *, int);
extern int wyse_getdomainname(char *, int);
extern int wyse_wait3(int *);
extern int wyse_socket(int, int, int);
extern int wyse_setsockopt(int, int, int, char *, int);
extern int wyse_getsockopt(int, int, int, char *, int *);
extern int wyse_recvfrom(int, void *, size_t, unsigned,
		struct sockaddr *, int *);
extern int wyse_recv(int, void *, size_t, unsigned);
extern int wyse_sendto(int, void *, size_t, unsigned,
		struct sockaddr *, int);
extern int wyse_send(int, void *, size_t, unsigned);

/* syslocal.c */
extern int wyse_syslocal(struct pt_regs *);

#endif /* _ABI_WYSE_SYSENT_H */
