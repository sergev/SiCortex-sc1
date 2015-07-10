/* sc1_stats.h: SC1 statistics

   Copyright (c) 2006, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.
*/

#ifndef _SC1_STATS_H_
#define _SC1_STATS_H_    0

#ifndef USE_STATS
// when statistics turned off, define every macro to be empty

// called once at start of simulation
#define STATS_INIT()

// called at end of simulation
#define STATS_SHUTDOWN()

// called before the read to fetch each instruction
#define STATS_IFETCH_START(ctx) 

// called when the instruction fetch succeeds
#define STATS_IFETCH_DONE(ctx,opcode) 

// called when emulation is done for an instruction
#define STATS_EMULATION_DONE(cpu)

// Called when time passes even though no instruction was executed,
// basically whenever STATS_EMULATION_DONE() was never called.
#define STATS_TICK(cpu, ticks)

// Called whenever there is a TLB miss.
// The virtual address is sent in va, and
// the instr argument is 1 if ITLB miss, or 0 if DTLB miss0
#define STATS_TLBMISS(ctx,va,instr)

// called after a READIO has occured, with the data returned by the read
#define STATS_READIO(ctx,pa,dat,unit)

// called after a WRITEIO has occured, with the data that was written
#define STATS_WRITEIO(ctx,pa,dat,unit)

/* The Raven interface needs to know when things are read and written,
   so it takes advantage of some of the STATS_* macros.  Raven and stats
   should never be turned on at the same time, or things will break. */

#ifndef RAVEN_INTERFACE   
// called after a ReadP* has occured, with the data returned by the read
#define STATS_READPB(ctx,pa,dat,catr)
#define STATS_READPH(ctx,pa,dat,catr)
#define STATS_READPW(ctx,pa,dat,catr)
#define STATS_READPD(ctx,pa,dat,catr)
#define STATS_READPI(ctx,pa,dat,catr)

// called after a WriteP* has occured, with the data that was written
#define STATS_WRITEPB(ctx,pa,dat,catr)
#define STATS_WRITEPH(ctx,pa,dat,catr)
#define STATS_WRITEPW(ctx,pa,dat,catr)
#define STATS_WRITEPD(ctx,pa,dat,catr)
#define STATS_STALL_BEGIN(ctx)
#define STATS_STALL_END(ctx)
#endif   /* end if not defined RAVEN_INTERFACE */

// called after a Sync instruction has occurred
#define STATS_SYNC(ctx)

// for cpu_simh only: called just before sc1_scx.c steps the hardware model
#define STATS_SCX_BEGIN_EPOCH(cpu_cycles)
// for cpu_simh only: called just after sc1_scx.c steps the hardware model
#define STATS_SCX_END_EPOCH(cpu_cycles)

#else   /* #if not defined SIMH_STATS */

#ifdef RAVEN_INTERFACE   
#error Do not turn on RAVEN_INTERFACE and USE_STATS at the same time. Raven defines some of the STATS_ macros in its own special way.
#endif

#define STATS_INIT()			               stats_init()
#define STATS_SHUTDOWN()		               stats_shutdown()
#define STATS_IFETCH_START(ctx)                        stats_ifetch_start(ctx)        
#define STATS_IFETCH_DONE(ctx,opcode)                  stats_ifetch_done(ctx,opcode) 
#define STATS_EMULATION_DONE(ctx)                      stats_emulation_done(ctx)
#define STATS_TICK(cpu, ticks)                         stats_tick(ctx,ticks)
#define STATS_TLBMISS(ctx,va,instr)                    stats_tlbmiss(ctx,va,instr)
#define STATS_READIO(ctx,pa,dat,unit)                  stats_readio(ctx,pa,dat,unit)
#define STATS_WRITEIO(ctx,pa,dat,unit)                 stats_writeio(ctx,pa,dat,unit)
#define STATS_READPB(ctx,pa,dat,catr)                  stats_readpb(ctx,pa,dat,catr)
#define STATS_READPH(ctx,pa,dat,catr)                  stats_readph(ctx,pa,dat,catr)
#define STATS_READPW(ctx,pa,dat,catr)                  stats_readpw(ctx,pa,dat,catr)
#define STATS_READPD(ctx,pa,dat,catr)                  stats_readpd(ctx,pa,dat,catr)
#define STATS_READPI(ctx,pa,dat,catr)                  stats_readpi(ctx,pa,dat,catr)
#define STATS_WRITEPB(ctx,pa,dat,catr)                 stats_writepb(ctx,pa,dat,catr)
#define STATS_WRITEPH(ctx,pa,dat,catr)                 stats_writeph(ctx,pa,dat,catr)
#define STATS_WRITEPW(ctx,pa,dat,catr)                 stats_writepw(ctx,pa,dat,catr)
#define STATS_WRITEPD(ctx,pa,dat,catr)                 stats_writepd(ctx,pa,dat,catr)
#define STATS_SYNC(ctx)                                stats_sync(ctx)
#define STATS_SCX_BEGIN_EPOCH(cpu_cycles)              stats_begin_epoch(cpu_cycles)
#define STATS_SCX_END_EPOCH(cpu_cycles)                stats_end_epoch(cpu_cycles)
#define STATS_STALL_BEGIN(ctx)                         stats_stall_begin(ctx)
#define STATS_STALL_END(ctx)                           stats_stall_end(ctx)

// prototypes for all statistics functions
void stats_init();
void stats_shutdown();
void stats_ifetch_start(CORECTX *ctx);
void stats_ifetch_done(CORECTX *ctx, t_uint64 opcode);
void stats_emulation_done(CORECTX *ctx);
void stats_tick (CORECTX *ctx, uint32 ticks);
void stats_tlbmiss (CORECTX *ctx, t_uint64 va, uint32 instr);
void stats_readio  (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 unit);
void stats_writeio (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 unit);
void stats_readpb  (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_readph  (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_readpw  (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_readpd  (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_readpi  (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_writepb (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_writeph (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_writepw (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_writepd (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
void stats_sync    (CORECTX *ctx);
void stats_begin_epoch (uint32 cpu_cycles);
void stats_end_epoch (uint32 cpu_cycles);
void stats_stall_begin(CORECTX *ctx);
void stats_stall_end(CORECTX *ctx);


#endif

#endif /* header guard */
