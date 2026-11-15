/*
 * Copyright (c) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 * Copyright (c) 2001  Christoph Hellwig (hch@caldera.de)
 */

#ident "%W% %G%"

/*
 * This file is based upon code written by Al Longyear for the COFF file
 * format which is in turn based upon code written by Eric Youngdale for
 * the ELF object file format. Any errors are most likely my own however.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/a.out.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/binfmts.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/personality.h>
#include <linux/file.h>
#include <linux/slab.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/xout.h>

#include <asm/uaccess.h>
#include <asm/desc.h>


MODULE_DESCRIPTION("Support for the Microsoft a.out (x.out) binary format");
MODULE_AUTHOR("Mike Jagdis, Christoph Hellwig");
MODULE_LICENSE("GPL");


/*
 * This is the name of the overlay library used for handling
 * Xenix/286 binaries.  Usually there is no need to change this.
 */
#define _PATH_X286EMUL "/usr/lib/x286emul"

/*
 * If you compile with XOUT_DEBUG defined you will get additional
 * debugging messages from the x.out module.
 */
#undef XOUT_DEBUG

/*
 * If you compile with XOUT_SEGMENTS defined the loader will take care
 * to set up the LDT as would "real" Xenix. This shouldn't be necessary
 * for most programs but it is just possible that something out there
 * makes assumptions about its environment and uses segment overrides.
 *
 * The default is not to bother setting up the LDT unless we need support
 * for Xenix 286 binaries.
 */
#undef XOUT_SEGMENTS

/*
 * Xenix 286 requires segment handling.
 */
#if defined(CONFIG_BINFMT_XOUT_X286)
#define XOUT_SEGMENTS
#endif

/*
 * Be verbose if XOUT_DEBUG is defined.
 */
#if defined(XOUT_DEBUG)
#define dprintk(x...)	printk(x)
#else
#define dprintk(x...)
#endif


static int	xout_load_binary(struct linux_binprm *, struct pt_regs *);
static int	xout_load_library(struct file *);

static struct linux_binfmt xout_format = {
	NULL, THIS_MODULE, xout_load_binary, xout_load_library, NULL, PAGE_SIZE
};

#if defined(XOUT_DEBUG) && defined(XOUT_SEGMENTS)

static u_long __gdt[2];

/*
 * This is borrowed (with minor variations since we are in kernel mode)
 * from the DPMI code for DOSEMU. I don't claim to understand LDTs :-).
 */
void
print_desc(int which)
{
	u_long			base_addr, limit;
	u_long			*lp;
	int			count, type, dpl, i;

	if (which) {
		lp = (u_long *)((struct desc_struct*)
				(current->mm->context.segments));
		count = LDT_ENTRIES;
		printk(KERN_DEBUG "xout: LDT\n");
	} else {
		/* joerg from david bruce */
		asm volatile ("sgdt __gdt+2");
		lp = (u_long *)__gdt[1];
		count = 8;
		printk(KERN_DEBUG "xout: GDT\n");
	}

	if (!lp)
		return;

	printk(KERN_DEBUG "XOUT: SLOT  BASE/SEL    LIM/OFF     TYPE  DPL  ACCESSBITS\n");
	for (i=0; i < count; i++, lp++) {
		/* First 32 bits of descriptor */
		base_addr = (*lp >> 16) & 0x0000FFFF;
		limit = *lp & 0x0000FFFF;
		lp++;

		/* First 32 bits of descriptor */
		base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
		limit |= (*lp & 0x000F0000);
		type = (*lp >> 10) & 7;
		dpl = (*lp >> 13) & 3;
		if ((base_addr > 0) || (limit > 0 )) {
			printk(KERN_DEBUG "XOUT: %03d   0x%08lx  0x%08lx  0x%02x  %03d %s%s%s%s%s%s%s\n",
				i,
				base_addr, limit, type, dpl,
				(*lp & 0x100) ? " ACCS'D" : "",
				(*lp & 0x200) ? " R&W" : " R&X",
				(*lp & 0x8000) ? " PRESENT" : "",
				(*lp & 0x100000) ? " USER" : "",
				(*lp & 0x200000) ? " X" : "",
				(*lp & 0x400000) ? " 32" : "",
				(*lp & 0x800000) ? " PAGES" : "");
		}
	}
}
#endif


static u_long *
xout_create_tables(char *p, struct linux_binprm *bprm, int ibcs)
{
        int				argc = bprm->argc, envc = bprm->envc;
        u_long				*argv,*envp;
        u_long				*sp;

        sp = (u_long *) ((-(u_long)sizeof(char *)) & (u_long) p);
        sp -= envc+1;
        envp = sp;
        sp -= argc+1;
        argv = sp;
        if (!ibcs) {
                sp--;
                put_user(envp, sp);
                sp--;
                put_user(argv, sp);
        }
        sp--;
        put_user(argc, sp);
        current->mm->arg_start = (u_long) p;
        while (argc-->0) {
                put_user(p, argv); argv++;
                p += strlen_user(p);
        }
        put_user(NULL,argv);
        current->mm->arg_end = current->mm->env_start = (u_long) p;
        while (envc-->0) {
                put_user(p, envp); envp++;
                p += strlen_user(p);
        }
        put_user(NULL,envp);
        current->mm->env_end = (u_long) p;
        return (sp);
}

static __inline int
isnotaligned(struct xseg *seg)
{
	dprintk(KERN_DEBUG
		"xout: %04x %04x %04x %02x %08lx %08lx %08lx %08lx\n",
		seg->xs_type, seg->xs_attr, seg->xs_seg, seg->xs_align,
		seg->xs_filpos, seg->xs_psize, seg->xs_vsize, seg->xs_rbase);

#if defined(XOUT_DEBUG)
	if ((seg->xs_filpos - seg->xs_rbase) & ~PAGE_MASK) {
		printk(KERN_DEBUG
			"xout: bad alignment - demand paging disabled\n");
	}
#endif
	return ((seg->xs_filpos & ~PAGE_MASK) | (seg->xs_rbase & ~PAGE_MASK));
}

static __inline__ void
clear_memory(u_long addr, u_long size)
{
	while (size-- != 0)
		put_user(0, (char *)addr++);
}

#if defined(XOUT_SEGMENTS)
#if defined(CONFIG_X86)
/* from linux-2.4.25/include/asm-i386/ldt.h */
struct modify_ldt_ldt_s {
        unsigned int  entry_number;
        unsigned long base_addr;
        unsigned int  limit;
        unsigned int  seg_32bit:1;
        unsigned int  contents:2;
        unsigned int  read_exec_only:1;
        unsigned int  limit_in_pages:1;
        unsigned int  seg_not_present:1;
        unsigned int  useable:1;
};
#endif
#endif

static int
xout_amen(struct file *fp, struct xseg *sp, int pageable, u_long *addrp,
		struct xexec *xexec, struct pt_regs *regs, int impure)
{
#if defined(XOUT_SEGMENTS)
	struct xext		*xext = (struct xext *)(xexec + 1);
	struct desc_struct	def_ldt;
	struct modify_ldt_ldt_s	ldt_info;
	mm_segment_t		old_fs;
	u_long			mirror_addr = 0;
	int			l;
#endif
	u_long			bss_size, bss_base;
	int			err = 0;

#if defined(XOUT_SEGMENTS)
	old_fs = get_fs();

seg_again:
	l = 0;

	/*
	 * Xenix 386 segments simply map the whole address
	 * space either read-exec only or read-write.
	 */
	ldt_info.entry_number = sp->xs_seg >> 3;
	ldt_info.read_exec_only = 0 /* ((s->xs_attr & XS_APURE) ? 1 : 0) */;
	ldt_info.contents = ((sp->xs_type == XS_TTEXT) ? 2 : 0);
	ldt_info.seg_not_present = 0;
	ldt_info.seg_32bit = ((sp->xs_attr & XS_A32BIT) ? 1 : 0);
	if (!ldt_info.seg_32bit) {
		ldt_info.base_addr = *addrp;
		*addrp = PAGE_ALIGN(*addrp + sp->xs_vsize);
		sp->xs_rbase = ldt_info.base_addr;
	} else
		ldt_info.base_addr = 0;
#endif

	bss_size = sp->xs_vsize - sp->xs_psize;
	bss_base = sp->xs_rbase + sp->xs_psize;

	/*
	 * If it is a text segment update the code boundary
	 * markers. If it is a data segment update the data
	 * boundary markers.
	 */
	if (sp->xs_type == XS_TTEXT) {
		if ((sp->xs_rbase + sp->xs_psize) > current->mm->end_code)
			current->mm->end_code = (sp->xs_rbase + sp->xs_psize);
	} else if (sp->xs_type == XS_TDATA) {
#if defined(XOUT_SEGMENTS)
		/*
		 * If it is the first data segment note that
		 * this is the segment we start in. If this
	 	 * isn't a 386 binary add the stack to the
		 * top of this segment.
		 */
		if ((xexec->x_cpu & XC_CPU) != XC_386) {
			if (regs->ebx == regs->ecx) {
				regs->ecx = sp->xs_seg;
				regs->edx = sp->xs_vsize;
				sp->xs_vsize = 0x10000;
				*addrp = PAGE_ALIGN(ldt_info.base_addr + sp->xs_vsize);
			}
		} else {
			if (regs->xds == regs->xcs)
				regs->xds = regs->xes = regs->xss = sp->xs_seg;
		}
#endif
		if ((sp->xs_rbase + sp->xs_psize) > current->mm->end_data)
			current->mm->end_data = (sp->xs_rbase + sp->xs_psize);
	}

	if ((sp->xs_rbase + sp->xs_vsize) > current->mm->brk) {
		current->mm->start_brk =
			current->mm->brk = PAGE_ALIGN(sp->xs_rbase + sp->xs_vsize);
	}

#if defined(XOUT_SEGMENTS)
	if (ldt_info.seg_32bit) {
		ldt_info.limit = (TASK_SIZE-1) >> 12;
		ldt_info.limit_in_pages = 1;
	} else {
		ldt_info.limit = sp->xs_vsize-1;
		ldt_info.limit_in_pages = 0;
	}

	dprintk(KERN_DEBUG "xout: ldt %02x, type=%d, base=0x%08lx, "
			"limit=0x%08x, pages=%d, 32bit=%d\n",
			ldt_info.entry_number, ldt_info.contents,
			ldt_info.base_addr, ldt_info.limit,
			ldt_info.limit_in_pages, ldt_info.seg_32bit);

	/*
	 * Use the modify_ldt syscall since this allocates
	 * the initial space for the LDT table, tweaks the
	 * GDT etc. We need to read the current LDT first
	 * since we need to copy the lcall7 call gate.
	 */
	set_fs(get_ds());
	if (!current->mm->context.size) {
		sys_modify_ldt(2, &def_ldt, sizeof(def_ldt));

		dprintk(KERN_DEBUG
			"x.out: def_ldt.a 0x%08lx, def_ldt.b 0x%08lx\n",
			def_ldt.a, def_ldt.b);

		l = 1;
	}

	err = sys_modify_ldt(1, &ldt_info, sizeof(ldt_info));
#if 0
	if (status >= 0 && !ntext && s->xs_seg == 0x47) {
		/* Uh oh, impure binary... */
		ldt_info.entry_number = 0x3f >> 3;
#if 0
		ldt_info.read_exec_only = 1;
#else
		ldt_info.read_exec_only = 0;
#endif
		ldt_info.contents = 2;
		status = sys_modify_ldt)(1, &ldt_info, sizeof(ldt_info));
	}
#endif
	set_fs(old_fs);
	if (l == 1) {
		struct desc_struct *ldt;

		ldt = (struct desc_struct *)current->mm->context.ldt;
		ldt->a = def_ldt.a;
		ldt->b = def_ldt.b;

		l = 0;
	}
	if (err < 0)
		printk(KERN_INFO "xout: modify_ldt returned %d\n", err);
#endif

	if (err < 0)
		goto out;

	if (!pageable) {
		dprintk(KERN_DEBUG "xout: Null map 0x%08lx, length 0x%08lx\n",
				sp->xs_rbase, sp->xs_vsize);
		down_write(&current->mm->mmap_sem);
		err = do_mmap(NULL, sp->xs_rbase, sp->xs_vsize,
				PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_FIXED|MAP_PRIVATE, 0);
		up_write(&current->mm->mmap_sem);
		goto out;
	}

	dprintk(KERN_DEBUG "xout: mmap to 0x%08lx from 0x%08lx, length 0x%08lx\n",
			sp->xs_rbase, sp->xs_filpos, sp->xs_psize);
	if (sp->xs_attr & XS_APURE) {
		down_write(&current->mm->mmap_sem);
		err = do_mmap(fp, sp->xs_rbase, sp->xs_psize,
				PROT_READ|PROT_EXEC, MAP_FIXED|MAP_SHARED,
				sp->xs_filpos);
		up_write(&current->mm->mmap_sem);
	} else {
		down_write(&current->mm->mmap_sem);
		err = do_mmap(fp, sp->xs_rbase, sp->xs_psize,
				PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_FIXED|MAP_PRIVATE | MAP_DENYWRITE | MAP_EXECUTABLE,
				sp->xs_filpos);
		up_write(&current->mm->mmap_sem);
	}

	if (err > TASK_SIZE || err < 0)
		goto out;

	/*
	 * Map uninitialised data.
	 */
	if (bss_size) {
		if (bss_base & PAGE_MASK) {
			clear_memory(bss_base, PAGE_ALIGN(bss_base)-bss_base);
			bss_size -= (PAGE_ALIGN(bss_base) - bss_base);
			bss_base = PAGE_ALIGN(bss_base);
		}

		dprintk(KERN_DEBUG "xout: Null map 0x%08lx, length 0x%08lx\n",
				bss_base, bss_size);

		down_write(&current->mm->mmap_sem);
		err = do_mmap(NULL, bss_base, bss_size,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_FIXED | MAP_PRIVATE, 0);
		up_write(&current->mm->mmap_sem);
	}

out:
	if (err > TASK_SIZE)
		return -EINVAL;
#if defined(XOUT_SEGMENTS)
	if (err >= 0 && impure && sp->xs_seg >= 0x47) {
		/*
		 * Uh oh, impure binary.
		 * Mirror this data segment to the text segment
		 */
		*addrp = mirror_addr = sp->xs_rbase;
		sp->xs_seg = xext->xe_eseg;
		sp->xs_type = XS_TTEXT;
		goto seg_again;
	}
#endif
	return (err);
}

/*
 * Helper function to process the load operation.
 */
static int
xout_load_object(struct linux_binprm * bpp, struct pt_regs *rp, int executable)
{
	struct xexec			*xexec = (struct xexec *)bpp->buf;
	struct xext			*xext = (struct xext *)(xexec + 1);
	struct xseg			*seglist;
	struct file			*fp = NULL;
	u_long				addr;
	int				nsegs, ntext, ndata;
	int				pageable = 1, err = 0;
	int				i;

	dprintk(KERN_DEBUG "xout: binfmt_xout entry: %s\n",
			bpp->file->f_dentry->d_name.name);

	if (xexec->x_magic != X_MAGIC) {
		dprintk(KERN_DEBUG "xout: bad magic %04x\n", xexec->x_magic);
		return -ENOEXEC;
	}

	switch (xexec->x_cpu & XC_CPU) {
		case XC_386:
			break;
#if defined(CONFIG_BINFMT_XOUT_X286)
		case XC_8086:
		case XC_286:
		case XC_286V:
		case XC_186:
			break;
#endif
		default:
			dprintk(KERN_DEBUG "xout: unsupported CPU type (%02x)\n",
					xexec->x_cpu);
			return -ENOEXEC;
	}

	/*
	 * We can't handle byte or word swapped headers. Well, we
	 * *could* but they should never happen surely?
	 */
	if ((xexec->x_cpu & (XC_BSWAP | XC_WSWAP)) != XC_WSWAP) {
		dprintk(KERN_DEBUG "xout: wrong byte or word sex (%02x)\n",
				xexec->x_cpu);
		return -ENOEXEC;
	}

	/* Check it's an executable. */
	if (!(xexec->x_renv & XE_EXEC)) {
		dprintk(KERN_DEBUG "xout: not executable\n");
		return -ENOEXEC;
	}

	/*
	 * There should be an extended header and there should be
	 * some segments. At this stage we don't handle non-segmented
	 * binaries. I'm not sure you can get them under Xenix anyway.
	 */
	if (xexec->x_ext != sizeof(struct xext)) {
		dprintk(KERN_DEBUG "xout: bad extended header\n");
		return -ENOEXEC;
	}

	if (!(xexec->x_renv & XE_SEG) || !xext->xe_segsize) {
		dprintk(KERN_DEBUG "xout: not segmented\n");
		return -ENOEXEC;
	}

	if (!(seglist = kmalloc(xext->xe_segsize, GFP_KERNEL))) {
		printk(KERN_WARNING "xout: allocating segment list failed\n");
		return -ENOMEM;
	}

	err = kernel_read(bpp->file, xext->xe_segpos,
			(char *)seglist, xext->xe_segsize);
	if (err < 0) {
		dprintk(KERN_DEBUG "xout: problem reading segment table\n");
		goto out;
	}

	if (!bpp->file->f_op->mmap)
		pageable = 0;

	nsegs = xext->xe_segsize / sizeof(struct xseg);

	ntext = ndata = 0;
	for (i = 0; i < nsegs; i++) {
		switch (seglist[i].xs_type) {
			case XS_TTEXT:
				if (isnotaligned(seglist+i))
					pageable = 0;
				ntext++;
				break;
			case XS_TDATA:
				if (isnotaligned(seglist+i))
					pageable = 0;
				ndata++;
				break;
		}
	}

	if (!ndata)
		goto out;

	/*
	 * Generate the proper values for the text fields
	 *
	 * THIS IS THE POINT OF NO RETURN. THE NEW PROCESS WILL TRAP OUT SHOULD
	 * SOMETHING FAIL IN THE LOAD SEQUENCE FROM THIS POINT ONWARD.
	 */

	/*
	 *  Flush the executable from memory. At this point the executable is
	 *  committed to being defined or a segmentation violation will occur.
	 */
	if (executable) {
		dprintk(KERN_DEBUG "xout: flushing executable\n");

		flush_old_exec(bpp);

		current->mm->mmap        = NULL;
		current->mm->_rss        = 0;

		if ((err = setup_arg_pages(bpp, STACK_TOP, EXSTACK_DEFAULT)) < 0) {
			send_sig(SIGSEGV, current, 1);
			return (err);
		}

		bpp->p = (u_long)xout_create_tables((char *)bpp->p, bpp,
				(xexec->x_cpu & XC_CPU) == XC_386 ? 1 : 0);

#if defined(XOUT_SEGMENTS)
		/*
		 * These will be set up later once we've seen the
		 * segments that make the program up.
		 */
		current->mm->start_code  = 0;
		current->mm->start_data  = 0;
		current->mm->end_code    = 0;
		current->mm->end_data    = 0;
		current->mm->start_brk   = 0;
		current->mm->brk         = 0;

		compute_creds(bpp);
		current->flags &= ~PF_FORKNOEXEC;

		/*
		 * The code selector is advertised in the header.
		 */
		if ((xexec->x_cpu & XC_CPU) != XC_386) {
			rp->ebx = rp->ecx = xext->xe_eseg;
			rp->eax = xexec->x_entry;
		} else {
			rp->xcs = rp->xds = rp->xes = rp->xss = xext->xe_eseg;
			rp->eip = xexec->x_entry;
		}
#else /* XOUT_SEGMENTS */
		current->mm->start_code  = 0;
		current->mm->end_code    = xexec->x_text;
		current->mm->end_data    = xexec->x_text + xexec->x_data;
		current->mm->start_brk   =
		current->mm->brk         = xexec->x_text + xexec->x_data + xexec->x_bss;

		compute_creds(bpp);
		current->flags &= ~PF_FORKNOEXEC;

		rp->xcs = __USER_CS;
		rp->xds = rp->xes = rp->xss = __USER_DS;
		rp->eip = xexec->x_entry;
#endif /* XOUT_SEGMENTS */

		dprintk(KERN_DEBUG "xout: entry point = 0x%x:0x%08lx\n",
				xext->xe_eseg, xexec->x_entry);

		rp->esp = current->mm->start_stack = bpp->p;

		set_personality(PER_XENIX);
	}

	/*
	 * Base address for mapping 16bit segments. This should lie above
	 * the emulator overlay.
	 */
	addr = X286_MAP_ADDR;

#if defined(CONFIG_BINFMT_XOUT_X286)
	/*
	 * If this isn't a 386 executable we need to load the overlay
	 * library to emulate a [2]86 environment and save the binary
	 * headers for later reference by the emulator.
	 */
	if ((xexec->x_cpu & XC_CPU) != XC_386) {
		mm_segment_t fs = get_fs();

    		set_fs(get_ds());
    		err = sys_uselib(_PATH_X286EMUL);
    		set_fs(fs);
		if (err < 0) {
			printk(KERN_ERR
			    "xout: loading of %s failed with error %d\n",
			    _PATH_X286EMUL, err);
			goto out;
		}

		down_write(&current->mm->mmap_sem);
		err = do_mmap(NULL,
			addr, sizeof(struct xexec)+sizeof(struct xext),
			PROT_READ|PROT_WRITE|PROT_EXEC,
			MAP_FIXED|MAP_PRIVATE,
  			0);
		up_write(&current->mm->mmap_sem);

		if (err > TASK_SIZE)
			goto Einval;
		if (err >= 0) {
			if (copy_to_user((char *)addr, xexec, sizeof(struct xexec)))
			{	err = -EFAULT;
				goto out;
			}
			if (copy_to_user((char *)addr+sizeof(struct xexec), xext, sizeof(struct xext)))
			{	err = -EFAULT;
				goto out;
			}
			addr = PAGE_ALIGN(addr+sizeof(struct xexec)+sizeof(struct xext));
		}
	}
#endif

	/*
	 * Scan the segments and map them into the process space. If this
	 * executable is pageable (unlikely since Xenix aligns to 1k
	 * boundaries and we want it aligned to 4k boundaries) this is
	 * all we need to do. If it isn't pageable we go round again
	 * afterwards and load the data. We have to do this in two steps
	 * because if segments overlap within a 4K page we'll lose the
	 * first instance when we remap the page. Hope that's clear...
	 */
	for (i = 0; err >= 0 && i < nsegs; i++) {
		struct xseg		*sp = seglist+i;

		if (sp->xs_attr & XS_AMEM) {
			err = xout_amen(fp, sp, pageable, &addr,
				xexec, rp, (!ntext && ndata == 1));
		}

	}

	/*
	 * We better fix start_data because sys_brk looks there to
	 * calculate data size.
	 * Kernel 2.2 did look at end_code so this is reasonable.
	 */
	if (current->mm->start_data == current->mm->start_code)
		current->mm->start_data = current->mm->end_code;

	dprintk(KERN_DEBUG "xout: start code 0x%08lx, end code 0x%08lx,"
	    " start data 0x%08lx, end data 0x%08lx, brk 0x%08lx\n",
	    current->mm->start_code, current->mm->end_code,
	    current->mm->start_data, current->mm->end_data,
	    current->mm->brk);

#if defined(XOUT_DEBUG) && defined(XOUT_SEGMENTS)
	print_desc(1);
	print_desc(0);
#endif

	if (pageable)
		goto trap;
	if (err < 0)
		goto trap;

	for (i = 0; (err >= 0) && (i < nsegs); i++) {
		struct xseg		*sp = seglist + i;
		u_long			psize;

		if (sp->xs_type == XS_TTEXT || sp->xs_type == XS_TDATA) {
			dprintk(KERN_DEBUG "xout: read to 0x%08lx from 0x%08lx,"
					" length 0x%08lx\n", sp->xs_rbase,
					sp->xs_filpos, sp->xs_psize);

			if (sp->xs_psize < 0)
				continue;

			/*
			 * Do we still get the size ? Yes! [joerg]
			 */
			psize = kernel_read(bpp->file, sp->xs_filpos,
					(char *)sp->xs_rbase, sp->xs_psize);

			if (psize != sp->xs_psize) {
				dprintk(KERN_DEBUG "xout: short read\n");
				err = -1;
				break;
			}
		}
	}

	/*
	 * Generate any needed trap for this process. If an error occured then
	 * generate a segmentation violation. If the process is being debugged
	 * then generate the load trap. (Note: If this is a library load then
	 * do not generate the trap here. Pass the error to the caller who
	 * will do it for the process in the outer lay of this procedure call.)
	 */
trap:
	if (executable) {
		if (err < 0) {
			dprintk(KERN_DEBUG "xout: loader forces seg fault "
					"(err = %d)\n", err);
			send_sig(SIGSEGV, current, 0);
		} else if (current->ptrace & PT_PTRACED)
			send_sig(SIGTRAP, current, 0);
		err = 0;
	}

out:
	kfree(seglist);

	dprintk(KERN_DEBUG "xout: binfmt_xout: result = %d\n", err);

	/*
	 * If we are using the [2]86 emulation overlay we enter this
	 * rather than the real program and give it the information
	 * it needs to start the ball rolling.
	 */
	if ((xexec->x_cpu & XC_CPU) != XC_386) {
#if 0
		regs->eax = regs->eip;
		regs->ebx = regs->xcs;
		regs->ecx = regs->xds;
		regs->xcs = __USER_CS;
		regs->xds = regs->xes = regs->xss = __USER_DS;
#endif
		rp->eip = 0x1020;
		dprintk(KERN_DEBUG "xout: x286emul 0x%02lx:0x%04lx,"
				" ds=0x%02lx, stack 0x%02lx:0x%04lx\n",
				rp->ebx, rp->eax, rp->ecx, rp->ecx,
				rp->edx);
#ifdef notdef
		while (!signal_pending(current))
			schedule();
#endif
		return (err < 0 ? err : rp->eax);
	}

#ifdef notdef
	while (!signal_pending(current))
		schedule();
#endif
	/*
	 * Xenix 386 programs expect the initial brk value to be in eax
	 * on start up. Hence if we succeeded we need to pass back
	 * the brk value rather than the status. Ultimately the
	 * ret_from_sys_call assembly will place this in eax before
	 * resuming (starting) the process.
	 */
	return (err < 0 ? err : current->mm->brk);

#if defined(CONFIG_BINFMT_XOUT_X286)
Einval:
	kfree(seglist);
	return -EINVAL;
#endif
}


/*
 *  This procedure is called by the main load sequence. It will load
 *  the executable and prepare it for execution. It provides the additional
 *  parameters used by the recursive xout loader and tells the loader that
 *  this is the main executable. How simple it is . . . .
 */
static int
xout_load_binary(struct linux_binprm *bpp, struct pt_regs *rp)
{
	return (xout_load_object(bpp, rp, 1));
}

/*
 * Load the image for any shared library.  This is called when
 * we need to load a library based upon a file name.
 *
 * XXX: I have never seen a Xenix shared library...  --hch
 */
static int
xout_load_library(struct file *fp)
{
	struct linux_binprm		*bpp;
	struct pt_regs			regs;
	int				err = -ENOMEM;

	if (!(bpp = kmalloc(sizeof(struct linux_binprm), GFP_KERNEL))) {
		printk(KERN_WARNING "xout: kmalloc failed\n");
		goto out;
	}

	memset(bpp, 0, sizeof(struct linux_binprm));
	bpp->file = fp;

	if ((err = kernel_read(fp, 0L, bpp->buf, sizeof(bpp->buf))) < 0)
		printk(KERN_WARNING "xout: unable to read library header\n");
	else
		err = xout_load_object(bpp, &regs, 0);

	kfree(bpp);
out:
	return (err);
}

static int __init
binfmt_xout_init(void)
{
	return register_binfmt(&xout_format);
}

static void __exit
binfmt_xout_exit(void)
{
	unregister_binfmt(&xout_format);
}

module_init(binfmt_xout_init);
module_exit(binfmt_xout_exit);
