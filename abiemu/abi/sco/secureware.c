/*
 * Copyright (c) 1994 Mike Jagdis.
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

#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>

/*
 * SecureWare, Inc. provided the C2 security subsystem used on SCO Unix.
 * This is not that package. This does not even attempt to emulate
 * that package. This emulates just enough of the "obvious" bits to
 * allow some programs to get a bit further. It is not useful to
 * try to implement C2 security in an emulator. Nor is it particularly
 * useful to run SCO's secure admin programs on Linux anyway...
 */
enum {
	SW_GETLUID =	0x01,
	SW_SETLUID =	0x02,
};

int
sw_security(int cmd, void *p1, void *p2, void *p3, void *p4, void *p5)
{
	switch (cmd) {
	case SW_GETLUID:
		/*
		 * We want the login user id. We don't have it
		 * specifically so we'll just use the real uid
		 * instead - it should be good enough.
		 */
		return (current->uid);
	case SW_SETLUID:
		/*
		 * Strictly we should only be able to call setluid()
		 * once but we can't enforce that. We have the choice
		 * between having it always succeed or always fail.
		 * Since setluid() should only ever be invoked by
		 * things like login processes we always fail it.
		 */
		return -EPERM;
	}

	printk(KERN_ERR "%s: unsupported security call cmd=%d\n", __FILE__, cmd);
	return -EINVAL;
}
