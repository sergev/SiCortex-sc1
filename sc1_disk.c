/* Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

   Copyright (c) 2003-2005, Robert M Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   31-Oct-05    RMS     Windows fixups

   * Revision $Id: sc1_disk.c 50164 2008-01-28 17:02:43Z denney $
*/

#include "sc1_defs.h"
#include "sc1_disk.h"

extern t_uint64 *M;
extern int32 sim_switches;
extern UNIT mem_unit;

#define NDISK 8

/* Declarations */

t_bool disk_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool disk_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat disk_reset (DEVICE *dptr);
t_stat disk_attach (UNIT *uptr, char *cptr);

/* DISK data structures

   disk_dib	    DISK dib
   disk_dev	    DISK device descriptor
   disk_unit	DISK unit descriptor
   disk_reg	    DISK register list
*/

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
    NULL,                           /* detach routine */
    (void *) &disk_dib,             /* context */
    DEV_DIB,                        /* flags */
#if 0
    NULL,                           /* debug control */
    NULL,                           /* debug flags */
    NULL,                           /* mem size routine */
    NULL                            /* logical name */
#endif
};


/* DISK: stored in a file */

void disk_docmd()
{
    int32 unit = (int32) disk_ctl.command & CMD_UNITMASK;
    UNIT *uptr = &disk_unit[unit];
    DiskRegs *regs = &disk_ctl;
    int32 err;

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
    if ((regs->diskaddress + regs->count) >= uptr->capac)
	{
	    regs->status = STATUS_SEEKERROR;
	    return;
	}
    err = fseek(uptr->fileref, regs->diskaddress, SEEK_SET);
    if (err < 0)
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
	    err = sim_fread(&M[regs->memaddress >> 3], 
			    sizeof (t_uint64), 
			    (uint32) (regs->count >> 3),
			    uptr->fileref);
	    if (err < 0) regs->status = STATUS_READERROR;
	    else regs->status = STATUS_GOOD;
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
	    err = sim_fwrite(&M[regs->memaddress >> 3], 
			     sizeof (t_uint64), 
			     (uint32) (regs->count >> 3),
			     uptr->fileref);
	    if (err < 0) regs->status = STATUS_WRITEERROR;
	    else regs->status = STATUS_GOOD;
	}
    if (regs->command & CMD_READBUFFER)
	{

	    if (regs->count > DISKBUFSIZE) regs->count = DISKBUFSIZE;
	    regs->read_cmds += 1;
	    regs->read_bytes+=regs->count;
	    err = sim_fread(&disk_ctl.buffer[0], sizeof (t_uint64), (uint32) regs->count >> 3,
			    uptr->fileref);
	    if (err < 0) regs->status = STATUS_READERROR;
	    else regs->status = STATUS_GOOD;
	}
    if (regs->command & CMD_WRITEBUFFER)
	{
	    if (regs->count > DISKBUFSIZE) regs->count = DISKBUFSIZE;
	    regs->write_cmds += 1;
	    regs->write_bytes+=regs->count;
	    err = sim_fwrite(&disk_ctl.buffer[0], sizeof (t_uint64), (uint32) regs->count >> 3,
			    uptr->fileref);
	    if (err < 0) regs->status = STATUS_WRITEERROR;
	    else regs->status = STATUS_GOOD;
	}
    return;
}



/* This function reads a device register */

t_bool disk_rd( t_uint64 pa, t_uint64 *val, uint32 lnt)
{
    int32 unit, regnum;
    t_uint64 cv, sc, offset;

    offset = pa - DISKBASE;
    regnum = (int32) (offset >> 3);
    if (offset > DISKSIZE) return (TRUE);

    switch(regnum)
    {
        case 0:
        {
	        unit = (int) disk_ctl.command & CMD_UNITMASK;
	        cv = disk_unit[unit].capac;
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
    t_stat r;
    sim_switches |= SWMASK('E');
    r = attach_unit (uptr, cptr);
    if (r == SCPE_OK)
    {
	    fseek(uptr->fileref, 0, SEEK_END);
	    uptr->capac = ftell(uptr->fileref);
    }

    return r;
}
