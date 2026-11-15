#include <fcntl.h>
#include <linux/a.out.h>


int
main(int argc, char *argv[])
{
	while (--argc) {
		struct exec e;
		int d;

		argv++;

		if (!(d = open(argv[0], O_RDWR))) {
			perror(argv[0]);
			continue;
		}
		if (read(d, &e, sizeof(e)) != sizeof(e)) {
			close(d);
			perror(argv[0]);
			continue;
		}
		if ((N_MAGIC(e) == OMAGIC || N_MAGIC(e) == NMAGIC
		|| N_MAGIC(e) == ZMAGIC || N_MAGIC(e) == QMAGIC)
		&& N_MACHTYPE(e) == 0) {
			/* Machine type should be 100 for Linux. */
			N_SET_MACHTYPE(e, 100);
			if (lseek(d, 0, 0) != 0
			|| write(d, &e, sizeof(e)) != sizeof(e))
				perror(argv[0]);
		}
		close(d);
	}

	exit (0);
}
