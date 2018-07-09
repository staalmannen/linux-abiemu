#ifndef _ABI_MAP_H
#define _ABI_MAP_H

#ident "%W% %G%"

/*
 *  Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */


struct map_segment {
	int	start, end;
	u_char	*map;
};


static __inline u_short
map_flags(u_short f, u_short map[])
{
        u_short m, r = 0;
        int i;

        for (i = 0, m = 1; i < 16; i++) {
                if (f & m)
                        r |= map[i];
		m <<= 1;
	}

        return r;
}

static __inline long
map_bitvec(u_long vec, long map[])
{
	u_long newvec = 0, m = 1;
	int i;

	for (i = 1; i <= 32; i++) {
		if ((vec & m) && map[i] != -1 && map[i] != 0)
			newvec |= (1 << (map[i] - 1));
		m <<= 1;
	}

	return newvec;
}

static __inline u_long
map_sigvec_from_kernel(sigset_t vec, u_long map[])
{
	u_long newvec = 0;
	int i;

	for (i = 1; i <= 32; i++) {
		if (sigismember(&vec, i) && map[i] != -1 && map[i] != 0)
			newvec |= (1 << (map[i] - 1));
	}

	return newvec;
}

static __inline sigset_t
map_sigvec_to_kernel(u_long vec, u_long map[])
{
	sigset_t newvec;
	u_long m = 1;
	int i;

	sigemptyset(&newvec);
	for (i = 1; i <= 32; i++) {
		if ((vec & m) && map[i] != -1)
			sigaddset(&newvec, map[i]);
		m <<= 1;
	}

	return newvec;
}

static __inline int
map_value(struct map_segment *m, int val, int def)
{
	struct map_segment *seg;

	/*
	 * If no mapping exists in this personality just return the
	 * number we were given.
	 */
	if (!m)
		return val;

	/*
	 * Search the map looking for a mapping for the given number.
	 */
	for (seg = m; seg->start != -1; seg++) {
		if (seg->start <= val && val <= seg->end) {
			/*
			 * If the start and end are the same then this
			 * segment has one entry and the map is the value
			 * it maps to. Otherwise if we have a vector we
			 * pick out the relevant value, if we don't have
			 * a vector we give identity mapping.
			 */
			if (seg->start == seg->end)
				return (int)seg->map;
			else
				return (seg->map ? seg->map[val-seg->start] : val);
		}
	}

	/* Number isn't mapped. Returned the requested default. */
	return def;
}

#endif /* ABI_MAP_H */
