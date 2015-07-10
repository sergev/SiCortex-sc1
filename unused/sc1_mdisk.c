
/*
 * sc1_mdisk.c:
 *
 * Works like sc1_disk.c, but memory-maps the disk image into the
 * simulator.  With -m flag, maps COW semantics, such that the image
 * file is never changed (we call this the "moot" filesystem).
 */

#include "sc1_defs.h"
#include "sc1_disk.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

extern t_uint64 *M;
extern int32 sim_switches;
extern UNIT mem_unit;

#define NDISK 8

/* Declarations */

t_bool disk_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool disk_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat disk_reset (DEVICE *dptr);
t_stat disk_attach (UNIT *uptr, char *cptr);
t_stat disk_detach (UNIT *uptr);

/* DISK data structures

   disk_dib	    DISK dib
   disk_dev	    DISK device descriptor
   disk_unit	DISK unit descriptor
   disk_reg	    DISK register list
*/

struct disk_priv
{
    int fd;
    size_t size;
    void *membuf;
};

static struct disk_priv priv[NDISK];

DIB disk_dib = { DISKBASE, DISKBASE+DISKSIZE, &disk_rd, &disk_wr, 0 };

UNIT disk_unit[NDISK] =
{
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) },
    { UDATA (NULL,  UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE, 0) }
};

DiskRegs disk_ctl;

REG disk_reg[] =
{
    { HRDATA(size, disk_ctl.size, 64) },
    { HRDATA(diskaddress, disk_ctl.diskaddress, 64) },
    { HRDATA(memaddress, disk_ctl.memaddress, 64) },
    { HRDATA(count, disk_ctl.count, 64) },
    { HRDATA(command, disk_ctl.command, 64) },
    { HRDATA(status, disk_ctl.status, 64) },
    { HRDATA(read_cmds, disk_ctl.read_cmds, 64) },
    { HRDATA(read_bytes, disk_ctl.read_bytes, 64) },
    { HRDATA(write_cmds, disk_ctl.write_cmds, 64) },
    { HRDATA(write_bytes, disk_ctl.write_bytes, 64) },
    { NULL }
};

DEVICE disk_dev =
{
    "DISK",                         /* name */
    disk_unit,                      /* units */
    disk_reg,                       /* registers */
    NULL,                           /* modifiers */
    NDISK,                          /* #units */
    16,                             /* address radix */
    64,                             /* address width */
    8,                              /* addr increment */
    16,                             /* data radix */
    64,                             /* data width */
    NULL,	                        /* examine routine */
    NULL,       	                /* deposit routine */
    &disk_reset,                    /* reset routine */
    NULL,                           /* boot routine */
    &disk_attach,                   /* attach routine */
    &disk_detach,                   /* detach routine */
    (void *) &disk_dib,             /* context */
    DEV_DIB,                        /* flags */
#if 0
    NULL,                           /* debug control */
    NULL,                           /* debug flags */
    NULL,                           /* mem size routine */
    NULL                            /* logical name */
#endif
};

/* DISK: stored in a file, modified in memory */

void disk_docmd()
{
    int32 unit = (int32) disk_ctl.command & CMD_UNITMASK;
    UNIT *uptr = &disk_unit[unit];
    DiskRegs *regs = &disk_ctl;

    if (!(uptr->flags & UNIT_ATT))
    {
	    regs->status = STATUS_NODEV;
	    return;
    }
    if ((regs->command & (CMD_CMDMASK)) == 0)
    {
	    regs->status = STATUS_GOOD;
	    return;
    }
    if (((regs->memaddress & 7) != 0)
	|| ((regs->count & 7) != 0))
    {
	    regs->status = STATUS_SEEKERROR;
	    return;
    }
    if ((regs->diskaddress > priv[unit].size) ||
	(regs->diskaddress + regs->count > priv[unit].size))
    {
	regs->status = STATUS_SEEKERROR;
	return;
    }
    if (regs->command & CMD_READ)
    {
	if (!PA_IS_MEM(regs->memaddress) || 
	    !PA_IS_MEM(regs->memaddress + regs->count - 1))
        {
	    regs->status=STATUS_SEEKERROR;
	    return;
	}
	regs->read_cmds += 1;
	regs->read_bytes+=regs->count;
	memcpy((char *) (M + regs->diskaddress),
	       (char *)priv[unit].membuf + regs->memaddress,
	       regs->count);
	regs->status = STATUS_GOOD;
    }
    if (regs->command & CMD_WRITE)
    {
	if (!PA_IS_MEM(regs->memaddress) || 
	    !PA_IS_MEM(regs->memaddress + regs->count - 1))
        {
	    regs->status=STATUS_SEEKERROR;
	    return;
	}
	regs->write_cmds += 1;
	regs->write_bytes+=regs->count;
	memcpy((char *)priv[unit].membuf + regs->diskaddress,
	       (char *) M + regs->memaddress,
	       regs->count);
	regs->status = STATUS_GOOD;
    }
    if (regs->command & CMD_READBUFFER)
    {
	if (regs->count > DISKBUFSIZE) regs->count = DISKBUFSIZE;
	regs->read_cmds += 1;
	regs->read_bytes+=regs->count;
	memcpy(disk_ctl.buffer, 
		(char *)priv[unit].membuf + regs->diskaddress,
		regs->count);
	regs->status = STATUS_GOOD;
    }
    if (regs->command & CMD_WRITEBUFFER)
    {
	if (regs->count > DISKBUFSIZE) regs->count = DISKBUFSIZE;
	regs->write_cmds += 1;
	regs->write_bytes+=regs->count;
	memcpy((char *)priv[unit].membuf + regs->diskaddress,
		disk_ctl.buffer,
		regs->count);
	regs->status = STATUS_GOOD;
    }
    return;
}



/* This function reads a device register */

t_bool disk_rd( t_uint64 pa, t_uint64 *val, uint32 lnt)
{
    int32 unit_no, regnum;
    t_uint64 cv, sc, offset;

    offset = pa - DISKBASE;
    regnum = (int32) (offset >> 3);
    if (offset > DISKSIZE) return (TRUE);

    switch(regnum)
    {
        case 0:
        {
	        unit_no = (int) disk_ctl.command & CMD_UNITMASK;
		if ((unit_no < 0) || (unit_no > NDISK) ||
		    !(disk_unit[unit_no].flags & UNIT_ATT)) {
		    cv = 0;
		    break;
		}
	        cv = priv[unit_no].size;
	        break;
        }
        default:
        {
	        cv = ((t_uint64 *) &disk_ctl)[regnum];
	        break;
        }
    }

    switch (lnt)
    {
        case L_BYTE:
            sc = (((uint32) pa) & 7) * 8;
            *val = (cv >> sc) & M8;
            break;
        case L_HALF:
            sc = (((uint32) pa) & 6) * 8;
            *val = (cv >> sc) & M16;
            break;
        case L_WORD:
            if (pa & 4) *val = (cv >> 32) & M32;
            else *val = cv & M32;
            break;
        case L_DOUB:
            *val = cv;
            break;
    }

    return TRUE;
}

t_bool disk_wr (t_uint64 pa, t_uint64 val, uint32 lnt)
{
    int32 regnum;
    t_uint64 cv, sc, offset;
    int32 err;

    offset = pa - DISKBASE;
    regnum = (int32) (offset >> 3);
    if (offset > DISKSIZE) return (TRUE);

    /* read */
    err = disk_rd(pa, &cv, L_DOUB);
    if (!err) return(FALSE);
    /* update */
    switch (lnt)
    {
        case L_BYTE:
            sc = (((uint32) pa) & 7) * 8;
            cv = (cv & ~(((t_uint64) M8) << sc)) | (((t_uint64) (val & M8)) << sc);
            break;
        case L_HALF:
            sc = (((uint32) pa) & 6) * 8;
            cv = (cv & ~(((t_uint64) M16) << sc)) | (((t_uint64) (val & M16)) << sc);
            break;
        case L_WORD:
            if (pa & 4) cv = ((t_uint64) (cv & M32)) | (((t_uint64) (val & M32)) << 32);
            else cv = (cv & ~((t_uint64) M32)) | ((t_uint64) val & M32);
            break;
        case L_DOUB:
            cv = val;
            break;
    }

    switch(regnum)
    {
        case 4:
        {
	        disk_ctl.command = cv;
	        disk_docmd();
	        break;
            }
        default:
        {
	        ((t_uint64 *) &disk_ctl)[regnum] = cv;
	        break;
        }
    }

    return TRUE;
}

/* DISK reset */

t_stat disk_reset (DEVICE *dptr)
{
return SCPE_OK;
}

/* DISK attach */

t_stat disk_attach (UNIT *uptr, char *cptr)
{
    int unit_no, fd;
    struct stat st;
    void *buf;
    int mmap_prot, mmap_flags, open_flags;

    mmap_prot = PROT_READ | PROT_WRITE;
    mmap_flags = MAP_SHARED;
    open_flags = O_RDWR;

    if (uptr->flags & UNIT_DIS) return SCPE_UDIS;           /* disabled? */
    if (!(uptr->flags & UNIT_ATTABLE)) return SCPE_NOATT;   /* not attachable? */

    /*
     * 'M' flag: The simulator may change the disk contents, but we'll
     * never change the original disk image file.  Map the file with
     * COW semantics.  Moot filesystem.
     *
     * 'R' flag: The disk is a read-only device.
     */

    if (sim_switches & SWMASK ('R')) {                      /* read only? */
	if ((uptr->flags & UNIT_ROABLE) == 0)               /* allowed? */
	    return SCPE_NORO; 			            /* no, error */
	uptr->flags = uptr->flags | UNIT_RO;                /* set rd only */
	mmap_prot = PROT_READ;
	open_flags = O_RDONLY;
    }
    else if (sim_switches & SWMASK ('M')) {		    /* moot? */
	/* copy-on-write mapping */
	mmap_flags = MAP_PRIVATE;
	open_flags = O_RDONLY;
    }

    unit_no = uptr - disk_unit;
    if((unit_no < 0) || (unit_no > NDISK)) {
	return SCPE_NOATT;
    }

    fd = open(cptr, open_flags);
    if(fd < 0)
    {
	return SCPE_OPENERR;
    }

    if(fstat(fd, &st) != 0)
    {
	close(fd);
	return SCPE_OPENERR;
    }

    buf = mmap(NULL, st.st_size, mmap_prot, mmap_flags, fd, 0);
    if(buf == NULL)
    {
	close(fd);
	return SCPE_OPENERR;
    }

    priv[unit_no].fd = fd;
    priv[unit_no].size = st.st_size;
    priv[unit_no].membuf = buf;

    uptr->flags |= UNIT_ATT;
    return SCPE_OK;
}

t_stat disk_detach (UNIT *uptr)
{
    int unit_no;

    if (!(uptr->flags & UNIT_ATT)) return SCPE_OK;          /* attached? */

    unit_no = uptr - disk_unit;
    if((unit_no < 0) || (unit_no > NDISK)) {
	return SCPE_NOATT;
    }

    if (close(priv[unit_no].fd) != 0) return SCPE_IOERR;
    uptr->flags &= ~UNIT_ATT;
    return SCPE_OK;
}
