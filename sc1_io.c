/* sc1_io.c: SC1 I/O and miscellaneous devices

   Copyright (c) 2005-2006, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

   Copyright (c) 2005, Robert M. Supnik

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   rom          boot ROM

   09-Jan-06    RMS     Fixed bug in ROM allocation
   04-Jan-06    RMS     Revised for new L2 interrupt mechanism
   14-Oct-05    RMS     Updated interrupts
   18-Aug-05    RMS     Changed counter, compare to 32b
   17-Aug-05    RMS     Removed real-time counter
*/

#include "sc1_defs.h"
#include "sc1_stats.h"

t_uint64 *rom = NULL;                                   /* boot ROM */

extern uint32 global_int;
extern CORECTX *cpu_ctx[NUM_CORES];
extern DEVICE *sim_devices[];

t_bool rom_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool rom_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat rom_ex (t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat rom_dep (t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat rom_reset (DEVICE *dptr);
void counter_reset (CORECTX *ctx, UNIT *uptr);

/* ROM data structures

   rom_dev      ROM device descriptor
   rom_unit     ROM units
   rom_reg      ROM register list
*/

DIB rom_dib = { ROMBASE_29, ROMBASE_29 + ROMSIZE, &rom_rd, &rom_wr, 0 };

UNIT rom_unit = { UDATA (NULL, UNIT_FIX+UNIT_BINK, ROMSIZE) };

REG rom_reg[] = {
    { NULL }  };

DEVICE rom_dev = {
    "ROM", &rom_unit, rom_reg, NULL,
    1, 16, ROMAWIDTH, 4, 16, 64,
    &rom_ex, &rom_dep, &rom_reset,
    NULL, NULL, NULL,
    (void *) &rom_dib, DEV_DIB };

/* ReadIO - read IO space

   Inputs:
        ctx     =       CPU context
        pa      =       physical address
        *dat    =       pointer to data
        lnt     =       length (BHWD)
   Output:
        TRUE if read succeeds, else FALSE
*/

t_bool ReadIO (CORECTX *ctx, t_uint64 pa, t_uint64 *dat, uint32 lnt)
{
DEVICE *dptr;
uint32 i;


for (i = 0; sim_devices[i] != NULL; i++) {
    dptr = sim_devices[i];
    if (dptr->flags & DEV_DIB) {
        DIB *dibp = (DIB *) dptr->ctxt;
        if ((pa >= dibp->low) && (pa < dibp->high)) {
	    uint32 unit = (dptr->flags & DEV_CORE)? ctx->cpu_num: lnt;
            t_bool out = dibp->read (pa, dat, unit);
	    STATS_READIO(ctx, pa, *dat, unit);
	    return out;
	    }
        }
    }
return FALSE;
}

/* WriteIO - write register space

   Inputs:
        ctx     =       CPU context
        pa      =       physical address
        val     =       data to write, right justified in 64b doubleword
        lnt     =       length (BHWD)
   Output:
        TRUE if write succeeds, else FALSE
*/

t_bool WriteIO (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 lnt)
{
DEVICE *dptr;
uint32 i;

for (i = 0; sim_devices[i] != NULL; i++) {
    dptr = sim_devices[i];
    if (dptr->flags & DEV_DIB) {
        DIB *dibp = (DIB *) dptr->ctxt;
        if ((pa >= dibp->low) && (pa < dibp->high)) {
	    uint32 unit = (dptr->flags & DEV_CORE)? ctx->cpu_num: lnt;
	    t_bool out = dibp->write (pa, dat, unit);
	    STATS_WRITEIO(ctx, pa, dat, unit);
	    return out;
	    }
        }
    }
return FALSE;
}

/* eval_intr - evaluate outstanding interrupts

   Inputs:
        ctx     =       core context
   Outputs (implicit):
        EVT_INT is set or cleared
*/

void eval_intr (CORECTX *ctx)
{
uint32 i;

ctx->irq_pins = cac_eval_intr (ctx);
set_cp0_cause ((get_cp0_cause() & ~(CP0_CAUSE_HIP|CP0_CAUSE_TI|CP0_CAUSE_PCI)) |
    (ctx->irq_pins << CP0_CAUSE_V_HIP) | global_int);
for (i = 0; i < NUM_PERF; i++) {
    if ((get_cp0_perf((i * 2) + 1) & W_SIGN) &&
        (get_cp0_perf(i * 2) & CP0_PCTRL_IE))
        set_cp0_cause (get_cp0_cause()|IRQ_PERF|CP0_CAUSE_PCI);
    }
if (ctx->irq_count)
    set_cp0_cause (get_cp0_cause()|IRQ_COUNT|CP0_CAUSE_TI);
if ((get_cp0_cause() & get_cp0_sr() & CP0_SR_IM) &&
    ((get_cp0_sr() & (CP0_SR_IE|CP0_SR_ERL|CP0_SR_EXL)) == CP0_SR_IE))
    ctx->events |= EVT_INT;
else ctx->events &= ~EVT_INT;
return;
}

/* eval_intr_all - evaluate outstanding interrupts for all cores */

void eval_intr_all (void)
{
uint32 i;

for (i = 0; i < NUM_CORES; i++)
    eval_intr (cpu_ctx[i]);
return;
}

/* Boot ROM read */

t_bool rom_rd (t_uint64 pa, t_uint64 *val, uint32 lnt)
{
uint32 sc, rg = ((uint32) ((pa - ROMBASE_29) & ROMAMASK)) >> 3;

switch (lnt) {
    case L_BYTE:
        sc = (((uint32) pa) & 7) * 8;
        *val = (rom[rg] >> sc) & M8;
        break;
    case L_HALF:
        sc = (((uint32) pa) & 6) * 8;
        *val = (rom[rg] >> sc) & M16;
        break;
    case L_WORD:
        if (pa & 4) *val = (rom[rg] >> 32) & M32;
        else *val = rom[rg] & M32;
        break;
    case L_DOUB:
        *val = rom[rg];
        break;
        }
return TRUE;
}

/* Boot ROM write */

t_bool rom_wr (t_uint64 pa, t_uint64 val, uint32 lnt)
{
uint32 sc, rg = ((uint32) ((pa - ROMBASE_29) & ROMAMASK)) >> 3;

switch (lnt) {
    case L_BYTE:
        sc = (((uint32) pa) & 7) * 8;
        rom[rg] = (rom[rg] & ~(((t_uint64) M8) << sc)) | (((t_uint64) (val & M8)) << sc);
        break;
    case L_HALF:
        sc = (((uint32) pa) & 6) * 8;
        rom[rg] = (rom[rg] & ~(((t_uint64) M16) << sc)) | (((t_uint64) (val & M16)) << sc);
        break;
    case L_WORD:
        if (pa & 4) rom[rg] = ((t_uint64) (rom[rg] & M32)) | (((t_uint64) (val & M32)) << 32);
        else rom[rg] = (rom[rg] & ~((t_uint64) M32)) | ((t_uint64) val & M32);
        break;
    case L_DOUB:
        rom[rg] = val;
        break;
        }

return TRUE;
}

/* ROM examine */

t_stat rom_ex (t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
uint32 addr = (uint32) exta;

if (vptr == NULL) return SCPE_ARG;
if (addr >= ROMSIZE) return SCPE_NXM;
*vptr = rom[addr >> 3];
return SCPE_OK;
}

/* ROM deposit */

t_stat rom_dep (t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
uint32 addr = (uint32) exta;

if (addr >= ROMSIZE) return SCPE_NXM;
rom[addr >> 3] = val;
return SCPE_OK;
}

/* ROM reset */

t_stat rom_reset (DEVICE *dptr)
{
if (rom == NULL) rom = (t_uint64 *) calloc (ROMSIZE >> 3, sizeof (t_uint64));
if (rom == NULL) return SCPE_MEM;
return SCPE_OK;
}

/* Interval timer (counter) */

void counter_wr_count (CORECTX *ctx, t_uint64 val)
{
set_cp0_count ((uint32) val & M32);
return;
}

void counter_wr_compr (CORECTX *ctx, t_uint64 val)
{
set_cp0_compr ((uint32) val & M32);
ctx->irq_count = 0;
return;
}

/* Coprocessor 2 stubs */

t_stat op_cop2 (CORECTX *ctx, uint32 ir)
{
return SCPE_OK;
}

t_uint64 cp2_getspr (CORECTX *ctx, uint32 rn)
{
return 0;
}

t_uint64 cp2_getdpr (CORECTX *ctx, uint32 rn)
{
return 0;
}

void cp2_putspr (CORECTX *ctx, uint32 rn, t_uint64 val)
{
return;
}

void cp2_putdpr (CORECTX *ctx, uint32 rn, t_uint64 val)
{
return;
}

