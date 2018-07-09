#ifndef _ABI_UW7_ACL_H
#define _ABI_UW7_ACL_H

#ident "%W% %G%"

/*
 * UnixWare 7 ACL bits (unused so far).
 */

enum {
	GETACL =	1,
	SETACL =	2,
	GETACLCNT =	3,
};

struct uw7_acl {
	int		a_type;
	uid_t		a_id;
        u_int16_t	a_perm;
};


/* int uw7_acl(char * path, int cmd, int nentries, struct acl * aclp); */

#endif /* _ABI_UW7_ACL_H */
