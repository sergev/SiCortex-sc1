/* sc1_stats.c: SC1 statistics code

   The statistics code consists of macros called in various places in SIMH,
   which can be defined however you want.  By default, the macros are defined
   to do nothing so that there's no performance impact.  If you want to count
   how many of a certain kind of event has occurred, or do something more elaborate,
   you can define the macros however you want.
   
   For example, let's say you want to know what proportion of your instructions are
   floating point.  You could define STATS_IFETCH_DONE() to call a new function
   that you write, and the function could look at the opcode to see which kind of
   instruction is running and increment the appropriate counter.

   The application that motivated Bryce to add the stats code in the first place
   was to profile MIPS code running in cpu_simh to see what was causing the systemc
   model to step.  So the hooks that he added are probably biased toward that
   application, but the hooks (or ones that are added later) could be used to measure 
   all sorts of things.

----------------
   Copyright (c) 2005-2006, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

   Copyright (c) 2005, Robert M Supnik

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

*/

#include "sc1_defs.h"
#include "sc1_stats.h"

#ifdef ENABLE_SC1_STATS

// Use the debug file handle to print everything, the same file as the instruction trace
// Remember that it may be NULL at any time.
extern FILE *sim_deb;

// abbrevation for pulling cpu number out of a CORECTX structure
#define CPU (ctx->cpu_num)

// To make it easy to keep the output uniform, here are macros that control output.
// To enable debug messages or per-instruction messages, there is one place that you
// can switch them on and off.
//
// Note that the macros do not test whether sim_deb is NULL, so that must be done beforehand.
t_bool stats_show_debug   = 0;
t_bool stats_show_info    = 0;
t_bool stats_show_ldst    = 0;
t_bool stats_show_ioref   = 0;
t_bool stats_show_summary = 0;
t_bool stats_show_cost    = 1;

// DEBUG is for verbose messages, one for every event, which is interesting
// when you're trying to understand what's going on.
#define DEBUG(fmt,...)  {\
    if (stats_show_debug) \
      fprintf(sim_deb, "[%lld] " fmt "\n", TIMESTAMP(), ##__VA_ARGS__); }

// INFO is for messages that are approx one line per instruction
#define INFO(fmt,...)  {\
    if (stats_show_info) \
      fprintf(sim_deb, "[%lld] " fmt "\n", TIMESTAMP(), ##__VA_ARGS__); }

// LDST is for info about loads and stores
#define LDST(fmt,...)  {\
    if (stats_show_ldst) \
      fprintf(sim_deb, "[%lld] " fmt "\n", TIMESTAMP(), ##__VA_ARGS__); }

// IOREF is for info about I/O reads and writes
#define IOREF(fmt,...)  {\
    if (stats_show_ioref) \
      fprintf(sim_deb, "[%lld] " fmt "\n", TIMESTAMP(), ##__VA_ARGS__); }

// SUMMARY is for summary messages that are printed only once in "summary_how_often" instructions.
#define SUMMARY(fmt,...)  {\
    if (stats_show_summary) \
      fprintf(sim_deb, "[%lld] " fmt "\n", TIMESTAMP(), ##__VA_ARGS__); }

// CPUCOST is for messages that say which PCs are responsible for how much time going by
#define CPUCOST(cpu,pc,cost_type,cost_amount)  {\
    if (stats_show_cost && cpu_simh_cost) \
      fprintf(cpu_simh_cost, "[%lld] #%d: %08llX %s %d\n", TIMESTAMP(), cpu, pc, cost_type, cost_amount); }

// COST is for messages that say that time went by, but not because of a particular CPU or PC
#define COST(cost_type,cost_amount)  {\
    if (stats_show_cost && cpu_simh_cost) \
      fprintf(cpu_simh_cost, "[%lld] %s %d\n", TIMESTAMP(), cost_type, cost_amount); }


// structure for per-cpu state and counters
typedef struct {
    char stage;
    t_uint64 pc;
    t_bool stall;
    t_uint64 instr_begin_time;
    t_uint64 stall_begin_time;
    uint32 istall_cost;
    uint32 dstall_cost;
    char mem_op_type;  
       // '0'=none, 'r'=memory read, 'w'=memory write, 's'=sync, 'R'=I/O read, 'W'=I/O write
    // totals throughout the run
    t_uint64 total_instructions;
    t_uint64 total_istall;
    t_uint64 total_dstall;
    t_uint64 total_rdio;
    t_uint64 total_wtio;
} sc1_stats_t;

sc1_stats_t stats[6] = {{0}};

// Sum up the number of time units spent stepping the hardware model.
uint32 total_simhratio = 0;

// Every so often, print a summary of statistics. Whenever total_count is a multiple
// of summary_how_often, print the summary again.
t_uint64 summary_how_often = (1ULL<<20);
t_uint64 summary_timer = 0;

uint32 stats_initialized_already = 0;
FILE *cpu_simh_cost = NULL;
#define CPU_SIMH_PROF_FILE "cpu_simh_cost.log"

void stats_print_summary ();

void stats_init() {
    if (sim_deb) DEBUG ("stats_init");
    for (int i=0; i<NUM_CORES; i++) {
	stats[i].stage = '?';
	stats[i].pc = ~0;
	stats[i].instr_begin_time = 0;
	stats[i].stall = 0;
	stats[i].stall_begin_time = 0;
	stats[i].istall_cost = 0;
	stats[i].dstall_cost = 0;
	stats[i].mem_op_type = '0';
	stats[i].total_istall = 0;
	stats[i].total_dstall = 0;
	stats[i].total_rdio = 0;
	stats[i].total_wtio = 0;
    }
    total_simhratio = 0;
    if (stats_show_cost) {
	cpu_simh_cost = fopen (CPU_SIMH_PROF_FILE, "w");
	if (!cpu_simh_cost) {
	    printf ("%%Error: could not open %s for writing\n", CPU_SIMH_PROF_FILE);
	}
    }
    stats_initialized_already = 1;
}

void stats_shutdown() {
    if (sim_deb) DEBUG ("shutdown");
    if (cpu_simh_cost) fclose (cpu_simh_cost);
}

void stats_ifetch_start(CORECTX *ctx) {
    if (!ctx || !sim_deb) return;
    if (!stats_initialized_already) stats_init();
    if (ctx->PC == stats[CPU].pc) {
        // in cpu_simh, this can be called twice: once when memory reference begins and once
	// when it completes.
	DEBUG ("#%d: %08llX ifetch_start called again for same pc", CPU, ctx->PC);
	// stall completed and we're trying the instruction again. assign time cost.
	if (stats[CPU].stage == 'i') {
	    stats[CPU].istall_cost = TIMESTAMP() - stats[CPU].stall_begin_time;
	    DEBUG ("#%d: %08llX instruction stall done in %d", CPU, stats[CPU].pc, stats[CPU].istall_cost);
	} else if (stats[CPU].stage == 'd') {
	    stats[CPU].dstall_cost = TIMESTAMP() - stats[CPU].stall_begin_time;
	    DEBUG ("#%d: %08llX data stall done in %d", CPU, stats[CPU].pc, stats[CPU].dstall_cost);
	} else {
	    DEBUG ("#%d: %08llX stall done but we don't know what kind", CPU, stats[CPU].pc);
	}
	return;
    } else {
	DEBUG ("#%d: %08llX ifetch_start", CPU, ctx->PC);
        stats[CPU].stage = 'i';
	stats[CPU].pc = ctx->PC;
	stats[CPU].instr_begin_time = TIMESTAMP();
	stats[CPU].stall = 0;
	stats[CPU].stall_begin_time = 0;
	stats[CPU].istall_cost = 0;
	stats[CPU].dstall_cost = 0;
	stats[CPU].mem_op_type = '0';
    }
}

void stats_ifetch_done(CORECTX *ctx, t_uint64 opcode) {
    if (!ctx || !sim_deb) return;
    DEBUG ("#%d: %08llX ifetch op=%08llX", CPU, stats[CPU].pc, opcode);
    stats[CPU].stage = 'd';
}

void stats_emulation_done(CORECTX *ctx) {
    if (!ctx || !sim_deb) return;
    if (stats[CPU].stall) {
	DEBUG ("#%d: %08llX done but stalled on memory reference. instruction will try again later.", CPU, stats[CPU].pc);
    } else {
	// Special case for nonblockingL1=0: without nonblockingL1=1, there
	// are no stalls for memory, but some instructions still take time.  If
	// time has passed during this instruction, still assign it to
	// something.
	uint32 delta = TIMESTAMP() - stats[CPU].instr_begin_time;
	if (delta
	    && stats[CPU].istall_cost==0 
	    && stats[CPU].dstall_cost==0) 
	{
	    // if no memory reference, assign it all to istall_cost
	    if (stats[CPU].mem_op_type == '0')
	        stats[CPU].istall_cost = delta;
	    else {
		// otherwise assume all dstall and the code below will do something reasonable.
	        stats[CPU].dstall_cost = delta;
	    }
	}
	if (stats[CPU].istall_cost) {
	    CPUCOST (CPU, stats[CPU].pc, "istall", stats[CPU].istall_cost);
	    stats[CPU].total_istall += stats[CPU].istall_cost;
	}
	if (stats[CPU].dstall_cost) {
	    switch (stats[CPU].mem_op_type) {
		case 'r':
		    CPUCOST (CPU, stats[CPU].pc, "rdmem", stats[CPU].dstall_cost);
		    break;
		case 'w':
		    CPUCOST (CPU, stats[CPU].pc, "wtmem", stats[CPU].dstall_cost);
		    break;
		case 's':
		    CPUCOST (CPU, stats[CPU].pc, "sync", stats[CPU].dstall_cost);
		    break;
		case 'R':
		    CPUCOST (CPU, stats[CPU].pc, "rdio", stats[CPU].dstall_cost);
		    break;
		case 'W':
		    CPUCOST (CPU, stats[CPU].pc, "wtio", stats[CPU].dstall_cost);
		    break;
	        default:
		    INFO("#%d: %08llX dstall is %d, but no mem_op_type. what's up?\n", CPU, stats[CPU].pc, stats[CPU].dstall_cost);
	    }
	}
	DEBUG ("#%d: %08llX instruction completed. istall=%d dstall=%d type %c",
	    CPU, stats[CPU].pc, stats[CPU].istall_cost, stats[CPU].dstall_cost, stats[CPU].mem_op_type);
	stats[CPU].total_dstall += stats[CPU].dstall_cost;
	stats[CPU].total_instructions++;
	stats[CPU].stage = '?';
	summary_timer++;
	if (summary_timer>=summary_how_often) {
	    summary_timer = 0;
	    stats_print_summary();
	}
    }
}

void stats_tick (CORECTX *ctx, uint32 ticks) {
}

void stats_tlbmiss (CORECTX *ctx, t_uint64 va, uint32 instr) {
}

void stats_readio(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 unit) {
    if (!ctx || !sim_deb) return;
    IOREF ("#%d: %08llX rdio pa=%08llX dat=%08llX unit=%d", CPU, stats[CPU].pc, pa, dat, unit);
    stats[CPU].mem_op_type = 'R';
}

void stats_writeio(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 unit) {
    if (!ctx || !sim_deb) return;
    IOREF ("#%d: %08llX wtio pa=%08llX dat=%08llX unit=%d", CPU, stats[CPU].pc, pa, dat, unit);
    stats[CPU].mem_op_type = 'W';
}

void stats_readpb(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX readpb pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'r';
}

void stats_readph(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX readph pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'r';
}

void stats_readpw(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX readpw pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'r';
}

void stats_readpd(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX readpd pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'r';
}

void stats_readpi(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    // since the instruction trace already covers this pretty thoroughly, only
    // print this message with DEBUG.  And don't set mem_op_type.
    DEBUG ("#%d: %08llX readpi pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
}

void stats_writepb(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX writepb pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'w';
}

void stats_writeph(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX writeph pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'w';
}

void stats_writepw(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX writepw pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'w';
}

void stats_writepd(CORECTX *ctx, t_uint64 pa, t_uint64 dat,uint32 catr) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX writepd pa=%08llX dat=%08llX catr=%d", CPU, stats[CPU].pc, pa, dat, catr);
    stats[CPU].mem_op_type = 'w';
}

void stats_sync(CORECTX *ctx) {
    if (!ctx || !sim_deb) return;
    LDST ("#%d: %08llX sync", CPU, stats[CPU].pc);
    stats[CPU].mem_op_type = 's';
}

void stats_stall_begin(CORECTX *ctx) {
    if (!ctx || !sim_deb) return;
    stats[CPU].stall = 1;
    stats[CPU].stall_begin_time = TIMESTAMP();
    if (stats[CPU].stage == 'i') {
	DEBUG ("#%d: %08llX instruction stall", CPU, stats[CPU].pc);
    } else if (stats[CPU].stage == 'd') {
	DEBUG ("#%d: %08llX data stall", CPU, stats[CPU].pc);
    } else {
	DEBUG ("#%d: %08llX stall but we don't know what kind", CPU, stats[CPU].pc);
    }
}

void stats_stall_end(CORECTX *ctx) {
    if (!ctx || !sim_deb) return;
    DEBUG ("#%d: %08llX stall_end", CPU, stats[CPU].pc);
    stats[CPU].stall = 0;
    // I used to consider this to be the end of the stall.  But in fact the
    // instruction does not retry instantly.  I saw one extra pclk cycle before
    // the instruction emulation ran again.
}

void stats_begin_epoch(uint32 cpu_cycles) {
    if (!sim_deb) return;
    DEBUG ("begin stepping systemc model %d cpu cycles", cpu_cycles);
#define NS_PER_PCLK 2
    COST ("epoch", cpu_cycles*NS_PER_PCLK);
}

void stats_end_epoch(uint32 cpu_cycles) {
    if (!sim_deb) return;
    DEBUG ("end stepping systemc model %d cpu cycles", cpu_cycles);
    total_simhratio += cpu_cycles*NS_PER_PCLK;
}

void stats_print_summary() {
    if (!sim_deb) return;
    for (int cpu=0; cpu<NUM_CORES; cpu++) {
	SUMMARY("#%d: total instruction_count=%lld istall=%lld dstall=%lld",
	    cpu,
	    stats[cpu].total_instructions,
	    stats[cpu].total_istall,
	    stats[cpu].total_dstall);
    }
}

#undef DEBUG
#undef ONE_PER_INST
#undef SUMMARY

#endif /* #ifdef ENABLE_SC1_STATS */
