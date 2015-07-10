/* sc1_sys.c: SC1 simulator interface

   Copyright (c) 2005-2007, SiCortex, Inc.  All rights reserved.

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   12-Oct-07    RMS     Added support for Mips64 R2 instructions
   17-Jan-06    RMS     Added I2C device
   16-Jan-06    RMS     Added TRACE capability
   05-Dec-05    RMS     Removed NVR device
   16-Nov-05	RMS     Fixed decoding of S?XC1 instructions
   31-Oct-05    RMS     Added disk to Windows configuration
   24-Oct-05    RMS     Fixed miscoding of DCLZ, DCLO
   13-Oct-05    RMS     Added pseudo-instructions li, move
   09-Sep-05    RMS     Fixed bug, sprintf of opcode (only) returned 0
   23-Aug-05    ADB     Adding NVRAM support, fixing cpu0/mem dependancy
   23-Aug-05    RMS     Added sprintf-like capability to disassembler
   25-Jul-05    RMS     Fixed problem printing 64b addresses
*/

#include <ctype.h>
#include <sys/stat.h>
#if !defined (_WIN32)
#include <elf.h>
#endif

#include "sc1_defs.h"
#include "sc1_sys.h"
#if defined (_WIN32)
# include "ice9_magic_spec_sw.h"
#elif defined (SIMX_NATIVE) /* Built into simx */
# include "ice9_magic_spec_sw.h"
#else
# include "sicortex/ice9/ice9_magic_spec_sw.h"
#endif

#ifdef USE_TAP
extern DEVICE eth_dev;
#endif
#ifdef SIMH_USE_PCIE
extern DEVICE pcie_dev;
extern DEVICE pmi_dev;
#endif
extern DEVICE mem_dev;
extern DEVICE cpu0_dev;
extern DEVICE uart_dev;
extern DEVICE i2c_dev;
extern DEVICE rom_dev;
extern DEVICE disk_dev;
extern DEVICE cac_dev;
extern DEVICE coh_dev[2];
#if defined(USE_MSP)
extern DEVICE scmsp_dev; /* supplied by sc1_scmsp.c */
#endif
extern DEVICE fsw_dev;
extern DEVICE qsc_dev;
extern DEVICE sdma_dev;
extern DEVICE flr_dev[3];
extern DEVICE flt_dev[3];
#if defined(USE_SCDMA) || defined(USE_NODMA) || defined(USE_HLMDMA)
extern DEVICE scdma_dev;  /* supplied by either sc1_scdma.c, sc1_nodma.c, or sc1_dma.c */
#endif
#ifdef SIMH_CPUSIMH
extern DEVICE scx_dev;
#endif
extern DEVICE gdb_dev;
extern DEVICE scb_dev;
extern DEVICE ddr_dev;

extern REG cpu0_reg[];
extern int32 sim_switches;

extern CORECTX *cpu_ctx[NUM_CORES];

uint32 sprint_sym_m (char *cptr, t_addr addr, uint32 inst);
t_stat parse_sym_m (char *cptr, t_addr addr, t_value *inst);
int32 parse_reg (char *cptr, char **optr);
uint32 parse_imm (char *cptr, char **optr);
char *sprint_addr (char *cptr, t_uint64 ad);

extern UNIT mem_unit;

/* SCP data structures and interface routines

   sim_name             simulator name string
   sim_PC               pointer to saved PC register descriptor
   sim_emax             number of words for examine
   sim_devices          array of pointers to simulated devices
   sim_stop_messages    array of pointers to stop messages
   sim_load             binary loader
*/

char sim_name[] = "SC1";

REG *sim_PC = &cpu0_reg[0];

int32 sim_emax = 8;

DEVICE *sim_devices[DEV_MAX + 1] = {
    &mem_dev,
    &cpu0_dev,
    &cac_dev,
    &coh_dev[0],
    &coh_dev[1],
    &rom_dev,
    &uart_dev,
    &i2c_dev,
//    &nvr_dev,
#ifdef USE_TAP
    &eth_dev,
#endif
    &disk_dev,
#ifdef SIMH_USE_PCIE
    &pcie_dev,
    &pmi_dev,
#endif
#if !defined(_WIN32)
    &fsw_dev,
    &qsc_dev,
    &sdma_dev,
    &flr_dev[0],
    &flr_dev[1],
    &flr_dev[2],
    &flt_dev[0],
    &flt_dev[1],
    &flt_dev[2],
#endif
#if defined(USE_SCDMA) || defined(USE_NODMA) || defined(USE_HLMDMA)
    &scdma_dev,
#endif
#ifdef SIMH_CPUSIMH
    &scx_dev,
#endif
#if defined(USE_MSP)
    &scmsp_dev,
#endif
#if !defined(_WIN32)
    &scb_dev,
    &ddr_dev,
#endif
#if !defined (_WIN32)
    &gdb_dev,
#endif
    NULL
    };

extern char sc1_stop_trap[];

char *sim_stop_messages[] = {
    "Unknown error",
    "HALT instruction",
    "Breakpoint",
    "Memory management error",
    "Wait state",
    "%Error: Test failed",	// %Error is magic so vtest will exit with error
    "Test passed",
    sc1_stop_trap,
    "Simulator hook detected",
    };

/* Local routines to write either memory or ROM */

t_bool sim_load_WritePB (t_uint64 pa, t_uint64 dat, uint32 catr)
{
if (PA_IS_MEM (pa)) return CALL_LOAD_WRITEPB (NULL, pa, dat, catr);
else if (PA_IS_ROM (pa)) return CALL_LOAD_WRITEIO (NULL, pa, dat, L_BYTE);
return FALSE;
}

t_bool sim_load_WritePW (t_uint64 pa, t_uint64 dat, uint32 catr)
{
if (PA_IS_MEM (pa)) return CALL_LOAD_WRITEPW (NULL, pa, dat, catr);
else if (PA_IS_ROM (pa)) return CALL_LOAD_WRITEIO (NULL, pa, dat, L_WORD);
return FALSE;
}

t_stat sim_load_WritePtr (t_uint64 pa, unsigned char* datap, t_uint64 size)
{
    t_uint64 leftpa = pa;
    t_uint64 leftsize = size;
    unsigned char* leftdatap = datap;
    while (leftsize) {
	if (!sim_load_WritePB(leftpa, *leftdatap, 0)) return SCPE_NXM;
	leftpa += 1;
	leftsize -= 1;
	leftdatap += 1;
    }
    return SCPE_OK;
}


/*
 * YAMON style argument / environment loader
 *      str - string containing value to be stored
 *      loc - where to store str
 *      ent - the entry in which to store the pointer to loc
 */

static t_stat yamon_arg_env(char *str, t_uint64 loc, t_uint64 ent)
{
    unsigned int i;

    if( str ) {
        for(i=0; i<=strlen(str); i++) {
            if (!sim_load_WritePB(loc+i, str[i], 0)) {
                return SCPE_NXM;
            }
        }
        if( !sim_load_WritePW(ent, (XKSEG_COMP + loc) & M32, 0) ) {
            return SCPE_NXM;
        }
    } else {
        if( !sim_load_WritePW(ent, 0, 0) ) {
            return SCPE_NXM;
        }
    }
    return SCPE_OK;
}

#if defined (_WIN32)

static t_stat sim_load_elf (FILE *fileref, char *cptr, char *fnam, int flag)
{
return SCPE_NOFNC;
}

#else
static t_stat sim_load_elf (FILE *fileref, char *cptr, char *fnam, int flag)
{
    struct stat statbuf;
    unsigned char *mappedfilep;
    unsigned char *loadblockp;
    Elf64_Ehdr* ehdrp;

    // Suck the whole file in (mmap isn't on all systems)
    fstat (fileno(fileref), &statbuf);

    mappedfilep = (unsigned char*)malloc(statbuf.st_size);
    loadblockp  = (unsigned char*)malloc(statbuf.st_size);
    fread (mappedfilep, statbuf.st_size, 1, fileref);

    // First is the header
    ehdrp = (Elf64_Ehdr*) mappedfilep;
    if (strncmp((char*)ehdrp->e_ident, "\177ELF",3)) {
	fprintf(stderr, "%s is not an ELF (exe) file\n", fnam);
	return SCPE_FMT;
    }
    if (ehdrp->e_ident[EI_CLASS]!=ELFCLASS64
	|| ehdrp->e_ident[EI_DATA]!=ELFDATA2LSB) {
	fprintf(stderr, "%s is not 64-bit little endian ELF\n", fnam);
	return SCPE_FMT;
    }

    // Now through each loader section...
#define LOADER_NTOHS(x) (x)
#define LOADER_NTOHL(x) (x)
#define LOADER_NTOHLL(x) (x)
    Elf64_Phdr* phdrp = (Elf64_Phdr*) &mappedfilep[LOADER_NTOHL(ehdrp->e_phoff)];
    for (int i=0; i < LOADER_NTOHS(ehdrp->e_phnum); i++, phdrp++) {
	int off = LOADER_NTOHL(phdrp->p_offset);
	t_uint64 loadaddr = LOADER_NTOHLL(phdrp->p_vaddr);
	if (phdrp->p_memsz) {
	    int loadsize = 0;
	    //int memsz = LOADER_NTOHL(phdrp->p_memsz);
	    int filesz = LOADER_NTOHL(phdrp->p_filesz);
	    if (LOADER_NTOHL(phdrp->p_type) & PT_LOAD) {
		t_uint64 adr;
		if (filesz > statbuf.st_size) {
		    fprintf(stderr, "Elf specified load larger then ELF file!\n");
		    return SCPE_FMT;
		}
		if (loadaddr == ~0ULL) loadaddr=0;
		for (adr = loadaddr; adr < (loadaddr + filesz); adr++, off++) {
		    unsigned char data = *((unsigned char *) (mappedfilep + off));
		    loadblockp[loadsize++] = data;
		}
		if (loadsize) {
		    // Write to destination
		    t_uint64 pa = loadaddr & 0x3fffffff;  // Cheap virtual->physical conversion
		    sim_load_WritePtr(pa, loadblockp, loadsize);
		}
		// Note if filesz<memsz, then there are ZEROs to load.
		// For speed, memory already is zeroed, so we can skip that part of the section.
	    }
	}
    }

    // Entry address
    if (ehdrp->e_entry) {
	int j;
	for (j = 0; j < NUM_CORES; j++) {
	    cpu_ctx[j]->PC = ehdrp->e_entry |  ~((t_uint64) M32);
	}
    }

    free(mappedfilep); mappedfilep=NULL;
    free(loadblockp);  loadblockp=NULL;
    return SCPE_OK;
}

#endif

/* Binary loader

   The binary loader handles absolute system images.  Supported switches:

   -b           simple binary byte stream
   -h           Mips hex test format (# comments, otherwise address data)
   -o           for memory, specify origin
   -s           swap bytes
   -m           Motorola SREC format
   -e		ELF
*/

t_stat sim_load (FILE *fileref, char *cptr, char *fnam, int flag)
{
    t_stat r;
    char gbuf[CBUFSIZE];
    uint32 addr, dat;
    int32 i, j;
    t_uint64 origin=0;

    if (flag) return SCPE_ARG;                          /* dump? */
    if (sim_switches & SWMASK ('O')) {                  /* origin? */
        origin = get_uint (cptr, 16, PA_MAX, &r);
        if (r != SCPE_OK) return SCPE_ARG;
    }

    if ((sim_switches & SWMASK ('H')) ||                /* HEX format? */
        (match_ext (fnam, "HEX") && !(sim_switches & SWMASK ('B')))) {
        while (fgets (gbuf, CBUFSIZE - 1, fileref) != NULL) {
            if (gbuf[0] == '#') continue;
            if (sscanf (gbuf, "%X%X", &addr, &dat) != 2) return SCPE_FMT;
            if (!sim_load_WritePW (((t_uint64) addr) << 2, dat, 0)) return SCPE_NXM;
        }
    } else if( sim_switches & SWMASK ('M') ) {          /* Motorola SREC format */
        char line[1024];
        unsigned int i, count;
        int bigendian = 0;
        long ori;
        long byte;
        while(fgets(line, 1024, fileref)) {
            switch(line[0]) {
                                                        /* We do not currently check the checksum */
                case '!':
                    switch(line[1]) {
                        case 'L': bigendian = 0;
                            break;
                        case 'B': bigendian = 1;
                            break;
                        default: return SCPE_FMT;
                    }
                    break;
                case 'S':
                    switch(line[1]) {
                        case '0': break; /* We don't read this currently */
                        case '1': break; /* We don't read this currently */
                        case '2': break; /* We don't read this currently */
                        case '3': /* 32-bit entry */
                            sscanf(line, "S3%2X", &count);
                            sscanf(&line[4], "%8lX", &ori);
                            ori &= ~XKSEG_COMP;
                            for(i=0; i<(count-5); i++) {
                                sscanf(&line[12+(i*2)], "%2lX", &byte);
                                if( bigendian ) {
                                    if (!sim_load_WritePB ((ori+i)+3-2*((ori+i)%4), byte, 0)) {
                                        return SCPE_NXM;
                                    }
                                } else {
                                    if (!sim_load_WritePB (ori+i, byte, 0)) {
                                        return SCPE_NXM;
                                    }
                                }
                            }
                            break;
                        case '4': break; /* We don't read this currently */
                        case '5': break; /* We don't read this currently */
                        case '6': return SCPE_FMT; break; /* UNUSED */
                        case '7': /* 32-bit Entry point address */
                            // If there are multiple S7 records, only the last one sticks...
                            sscanf(&line[4], "%8lX", &ori);
			    for (j = 0; j < NUM_CORES; j++) {
				cpu_ctx[j]->PC = ori |  ~((t_uint64) M32);
			    }
                            // printf("S7 entry point: ffffffff%.8s\n", &line[4]);
                            break;
                        case '8': break; /* We don't read this currently */
                        case '9': break; /* We don't read this currently */
                        default:
                            return SCPE_FMT;
                            break;
                    }
                    break;
                default:
                    return SCPE_FMT;
            }
        }
    } else if( sim_switches & SWMASK ('E') ) {          /* ELF format */
	return sim_load_elf (fileref, cptr, fnam, flag);
    } else if( sim_switches & SWMASK ('A') ) {          /* Kernel arguments in a string */
        char line[1024];
        t_uint64 argc, argv, argp;

        argc = argv = argp = 0;

        if( origin == 0 ) {
            argv = 0x1000;
        } else {
            argv = (origin & M32);
        }

        printf("Storing args at %#08Lx\r\n", argv);
        argp = argv + (4 * 256);
        while(fgets(line, 1024, fileref) && (argc < 256)) {
            line[strlen(line)-1] = '\0';
            r = yamon_arg_env(line, argp, argv+(4*argc));
            if(r != SCPE_OK) return r;
            argp += strlen(line) + 1;
            argc++;
        }

        // ARGC
        *(t_uint64 *)cpu0_reg[6].loc = argc;
        // ARGV
        *(t_uint64 *)cpu0_reg[7].loc = XKSEG_COMP + argv;
        // ENVP
        *(t_uint64 *)cpu0_reg[8].loc = XKSEG_COMP + argv+(4*argc);

        // Store "memsize" environment name
        sprintf(line, "memsize");
        r = yamon_arg_env(line, argp, argv+(4*argc));
        if(r != SCPE_OK) return r;
        argp += strlen(line) + 1;
        argc++;

        // Store "memsize" environment value
        sprintf(line, "%lld", MEMSIZE);
        r = yamon_arg_env(line, argp, argv+(4*argc));
        if(r != SCPE_OK) return r;
        argp += strlen(line) + 1;
        argc++;

        // Be sure that the argument list is terminated
        r = yamon_arg_env(NULL, argp, argv+(4*argc));
        if(r != SCPE_OK) return r;
        argp += strlen(line) + 1;
        argc++;
        r = yamon_arg_env(NULL, argp, argv+(4*argc));
        if(r != SCPE_OK) return r;
#if DEBUG
	printf("END : %#08Lx\r\n", argp);
	printf("ARGC: %#08Lx\r\n", *(t_uint64 *)cpu0_reg[6].loc);
	printf("ARGV: %#08Lx\r\n", *(t_uint64 *)cpu0_reg[7].loc);
	printf("ENVP: %#08Lx\r\n", *(t_uint64 *)cpu0_reg[8].loc);
#endif
    } else {
        while ((i = getc (fileref)) != EOF) {           /* read byte stream */
            if (sim_switches & SWMASK ('S')) {
                /* The below kludge writes bytes in reverse-endian order */
                if (!sim_load_WritePB ( origin + 3 - 2*(origin%4) , i, 0)) return SCPE_NXM;
            } else {
                if (!sim_load_WritePB (origin, i, 0)) return SCPE_NXM;
            }
            origin = origin + 1;
        }
    }
    return SCPE_OK;
}

static const uint32 fld_shift[16] = {
 0, I_V_RS, I_V_RT, I_V_RD, I_V_SA, I_V_FNC, 0, 0,
 I_V_SA, 0, 0, 0, 0, 0, 0, 0
 };

struct opcode {
    char*               name;
    uint32              val;
    uint32              mask;
    uint32              flg;
    };

/* Instruction table */

struct opcode opc[] = {
    { "NOP",    0x00000000, M_NOP, F_NOP },             /* special */
    { "SSNOP",  0x00000040, M_NOP, F_NOP },
    { "EHB",    0x000000C0, M_NOP, F_NOP },             // R2
    { "SLL",    0x00000000, M_SH3, F_SH3 },
    { "MOVF",   0x00000001, M_MCG, F_MCG },
    { "MOVT",   0x00010001, M_MCG, F_MCG },
    { "SRL",    0x00000002, M_SH3, F_SH3 },
    { "ROTR",   0x00200002, M_SH3, F_SH3 },             // R2
    { "SRA",    0x00000003, M_SH3, F_SH3 },
    { "SLLV",   0x00000004, M_SV3, F_SV3 },
    { "SRLV",   0x00000006, M_SV3, F_SV3 },
    { "ROTRV",  0x00000046, M_SV3, F_SV3 },             // R2
    { "SRAV",   0x00000007, M_SV3, F_SV3 },
    { "JR",     0x00000008, M_OR1, F_OR1 },
    { "JALR",   0x00000009, M_CJ2, F_CJ2 },
    { "MOVZ",   0x0000000A, M_OP3, F_OP3 },
    { "MOVN",   0x0000000B, M_OP3, F_OP3 },
    { "SYSCALL",0x0000000C, M_COD, F_COD },
    { "BREAK",  0x0000000D, M_COD, F_COD },
    { "SYNC",   0x0000000F, M_COD, F_COD },
    { "MFHI",   0x00000010, M_OW1, F_OW1 },
    { "MTHI",   0x00000011, M_OR1, F_OR1 },
    { "MFLO",   0x00000012, M_OW1, F_OW1 },
    { "MTLO",   0x00000013, M_OR1, F_OR1 },
    { "DSLLV",  0x00000014, M_SV3, F_SV3 },
    { "DSRLV",  0x00000016, M_SV3, F_SV3 },
    { "DROTRV", 0x00000056, M_SV3, F_SV3 },             // R2
    { "DSRAV",  0x00000017, M_SV3, F_SV3 },
    { "MULT",   0x00000018, M_OR2, F_OR2 },
    { "MULTU",  0x00000019, M_OR2, F_OR2 },
    { "DIV",    0x0000001A, M_OR2, F_OR2 },
    { "DIVU",   0x0000001B, M_OR2, F_OR2 },
    { "DMULT",  0x0000001C, M_OR2, F_OR2 },
    { "DMULTU", 0x0000001D, M_OR2, F_OR2 },
    { "DDIV",   0x0000001E, M_OR2, F_OR2 },
    { "DDIVU",  0x0000001F, M_OR2, F_OR2 },
    { "ADD",    0x00000020, M_OP3, F_OP3 },
    { "ADDU",   0x00000021, M_OP3, F_OP3 },
    { "SUB",    0x00000022, M_OP3, F_OP3 },
    { "SUBU",   0x00000023, M_OP3, F_OP3 },
    { "AND",    0x00000024, M_OP3, F_OP3 },
    { "MOVE",   0x00000025, M_OP2, F_OP2 },             /* must preceed or */
    { "OR",     0x00000025, M_OP3, F_OP3 },
    { "XOR",    0x00000026, M_OP3, F_OP3 },
    { "NOR",    0x00000027, M_OP3, F_OP3 },
    { "SLT",    0x0000002A, M_OP3, F_OP3 },
    { "SLTU",   0x0000002B, M_OP3, F_OP3 },
    { "DADD",   0x0000002C, M_OP3, F_OP3 },
    { "DADDU",  0x0000002D, M_OP3, F_OP3 },
    { "DSUB",   0x0000002E, M_OP3, F_OP3 },
    { "DSUBU",  0x0000002F, M_OP3, F_OP3 },
    { "TGE",    0x00000030, M_OR2, F_OR2 },
    { "TGEU",   0x00000031, M_OR2, F_OR2 },
    { "TLT",    0x00000032, M_OR2, F_OR2 },
    { "TLTU",   0x00000033, M_OR2, F_OR2 },
    { "TEQ",    0x00000034, M_OR2, F_OR2 },
    { "TNE",    0x00000036, M_OR2, F_OR2 },
    { "DSLL",   0x00000038, M_SH3, F_SH3 },
    { "DSRL",   0x0000003A, M_SH3, F_SH3 },
    { "DROTR",  0x0020003A, M_SH3, F_SH3 },             // R2
    { "DSRA",   0x0000003B, M_SH3, F_SH3 },
    { "DSLL32", 0x0000003C, M_SH3, F_SH3 },
    { "DSRL32", 0x0000003E, M_SH3, F_SH3 },
    { "DROTR32",0x0020003E, M_SH3, F_SH3 },             // R2
    { "DSRA32", 0x0000003F, M_SH3, F_SH3 },

    { "BLTZ",   0x04000000, M_BR1, F_BR1 },             /* regimm */
    { "BGEZ",   0x04010000, M_BR1, F_BR1 },
    { "BLTZL",  0x04020000, M_BR1, F_BR1 },
    { "BGEZL",  0x04030000, M_BR1, F_BR1 },
    { "TGEI",   0x04080000, M_RGI, F_RGI },
    { "TGEIU",  0x04090000, M_RGI, F_RGI },
    { "TLTI",   0x040A0000, M_RGI, F_RGI },
    { "TLTIU",  0x040B0000, M_RGI, F_RGI },
    { "TEQI",   0x040C0000, M_RGI, F_RGI },
    { "TNEI",   0x040E0000, M_RGI, F_RGI },
    { "BLTZAL", 0x04000000, M_BR1, F_BR1 },
    { "BAL",    0x04110000, M_BR0, F_BR0 },
    { "BGEZAL", 0x04110000, M_BR1, F_BR1 },
    { "BLTZALL",0x04120000, M_BR1, F_BR1 },
    { "BGEZALL",0x04130000, M_BR1, F_BR1 },
    { "SYNCI",  0x041F0000, M_SYI, F_SYI },
    
    { "J",      0x08000000, M_JMP, F_JMP },
    { "JAL",    0x0C000000, M_JMP, F_JMP },
    { "B",      0x10000000, M_BRU, F_BRU },
    { "BEQ",    0x10000000, M_BR2, F_BR2 },
    { "BNE",    0x14000000, M_BR2, F_BR2 },
    { "BLEZ",   0x18000000, M_BR1, F_BR1 },
    { "BGTZ",   0x1C000000, M_BR1, F_BR1 },
    { "ADDI",   0x20000000, M_IMM, F_IMM },
    { "LI",     0x24000000, M_LI,  F_LI },              /* must preceed ADDIU */
    { "ADDIU",  0x24000000, M_IMM, F_IMM },
    { "SLTI",   0x28000000, M_IMM, F_IMM },
    { "TEST_FAIL", ICE9_E_MipsMagicInstrs_HALTFAIL,	M_NOP, F_NOP },
    { "TEST_PASS", ICE9_E_MipsMagicInstrs_HALTPASS,	M_NOP, F_NOP },
    { "TRACE_ON",  ICE9_E_MipsMagicInstrs_SIMHTRACEON,	M_NOP, F_NOP },
    { "TRACE_OFF", ICE9_E_MipsMagicInstrs_SIMHTRACEOFF,	M_NOP, F_NOP },
    { "SLTIU",  0x2C000000, M_IMM, F_IMM },
    { "ANDI",   0x30000000, M_IMM, F_IMM },
    { "ORI",    0x34000000, M_IMM, F_IMM },
    { "XORI",   0x38000000, M_IMM, F_IMM },
    { "LUI",    0x3C000000, M_LUI, F_LUI },

    { "MFC0",   0x40000000, M_MXC, F_MXC },             /* cop0 */
    { "DMFC0",  0x40200000, M_MXC, F_MXC },
    { "MTC0",   0x40800000, M_MXC, F_MXC },
    { "DMTC0",  0x40A00000, M_MXC, F_MXC },
    { "DI",     0x41606000, M_MFM, F_MFM },             // R2
    { "EI",     0x41606020, M_MFM, F_MFM },             // R2
    { "TLBR",   0x42000001, M_NOP, F_NOP },
    { "TLBWI",  0x42000002, M_NOP, F_NOP },
    { "TLBWR",  0x42000006, M_NOP, F_NOP },
    { "TLBP",   0x42000008, M_NOP, F_NOP },
    { "ERET",   0x42000018, M_NOP, F_NOP },
    { "DERET",  0x4200001F, M_NOP, F_NOP },
    { "WAIT",   0x42000020, M_NOP, F_NOP },

    { "MFC1",     0x44000000, M_MVF, F_MVF },           /* cop1 (fp) */
    { "DMFC1",    0x44200000, M_MVF, F_MVF },
    { "CFC1",     0x44400000, M_MVF, F_MVF },
    { "MFHC1",    0x44600000, M_MVF, F_MVF },           // R2
    { "MTC1",     0x44800000, M_MVF, F_MVF },
    { "DMTC1",    0x44A00000, M_MVF, F_MVF },
    { "CTC1",     0x44C00000, M_MVF, F_MVF },
    { "MTHC1",    0x44E00000, M_MVF, F_MVF },           // R2
    { "BC1F",     0x45000000, M_BCO, F_BCO },
    { "BC1T",     0x45010000, M_BCO, F_BCO },
    { "BC1FL",    0x45020000, M_BCO, F_BCO },
    { "BC1TL",    0x45030000, M_BCO, F_BCO },
    { "ADD.S",    0x46000000, M_FP3, F_FP3 },
    { "SUB.S",    0x46000001, M_FP3, F_FP3 },
    { "MUL.S",    0x46000002, M_FP3, F_FP3 },
    { "DIV.S",    0x46000003, M_FP3, F_FP3 },
    { "SQRT.S",   0x46000004, M_FP2, F_FP2 },
    { "ABS.S",    0x46000005, M_FP2, F_FP2 },
    { "MOV.S",    0x46000006, M_FP2, F_FP2 },
    { "NEG.S",    0x46000007, M_FP2, F_FP2 },
    { "ROUND.L.S",0x46000008, M_FP2, F_FP2 },
    { "TRUNC.L.S",0x46000009, M_FP2, F_FP2 },
    { "CEIL.L.S", 0x4600000A, M_FP2, F_FP2 },
    { "FLOOR.L.S",0x4600000B, M_FP2, F_FP2 },
    { "ROUND.W.S",0x4600000C, M_FP2, F_FP2 },
    { "TRUNC.W.S",0x4600000D, M_FP2, F_FP2 },
    { "CEIL.W.S", 0x4600000E, M_FP2, F_FP2 },
    { "FLOOR.W.S",0x4600000F, M_FP2, F_FP2 },
    { "MOVF.S",   0x46000011, M_MCF, F_MCF },
    { "MOVT.S",   0x46010011, M_MCF, F_MCF },
    { "MOVZ.S",   0x46000012, M_MFI, F_MFI },
    { "MOVN.S",   0x46000013, M_MFI, F_MFI },
    { "RECIP.S"  ,0x46000015, M_FP2, F_FP2 },
    { "RSQRT.S"  ,0x46000016, M_FP2, F_FP2 },
    { "CVT.D.S",  0x46000021, M_FP2, F_FP2 },
    { "CVT.W.S",  0x46000024, M_FP2, F_FP2 },
    { "CVT.L.S",  0x46000025, M_FP2, F_FP2 },
    { "CVT.PS.S", 0x46000026, M_FP3, F_FP3 },
    { "C.F.S",    0x46000030, M_FCP, F_FCP },
    { "C.UN.S",   0x46000031, M_FCP, F_FCP },
    { "C.EQ.S",   0x46000032, M_FCP, F_FCP },
    { "C.UEQ.S",  0x46000033, M_FCP, F_FCP },
    { "C.OLT.S",  0x46000034, M_FCP, F_FCP },
    { "C.ULT.S",  0x46000035, M_FCP, F_FCP },
    { "C.OLE.S",  0x46000036, M_FCP, F_FCP },
    { "C.ULE.S",  0x46000037, M_FCP, F_FCP },
    { "C.SF.S",   0x46000038, M_FCP, F_FCP },
    { "C.NGLE.S", 0x46000039, M_FCP, F_FCP },
    { "C.SEQ.S",  0x4600003A, M_FCP, F_FCP },
    { "C.NGL.S",  0x4600003B, M_FCP, F_FCP },
    { "C.LT.S",   0x4600003C, M_FCP, F_FCP },
    { "C.NGE.S",  0x4620003D, M_FCP, F_FCP },
    { "C.LE.S",   0x4620003E, M_FCP, F_FCP },
    { "C.BGT.S",  0x4620003F, M_FCP, F_FCP },
    { "ADD.D",    0x46200000, M_FP3, F_FP3 },
    { "SUB.D",    0x46200001, M_FP3, F_FP3 },
    { "MUL.D",    0x46200002, M_FP3, F_FP3 },
    { "DIV.D",    0x46200003, M_FP3, F_FP3 },
    { "SQRT.D",   0x46200004, M_FP2, F_FP2 },
    { "ABS.D",    0x46200005, M_FP2, F_FP2 },
    { "MOV.D",    0x46200006, M_FP2, F_FP2 },
    { "NEG.D",    0x46200007, M_FP2, F_FP2 },
    { "ROUND.L.D",0x46200008, M_FP2, F_FP2 },
    { "TRUNC.L.D",0x46200009, M_FP2, F_FP2 },
    { "CEIL.L.D", 0x4620000A, M_FP2, F_FP2 },
    { "FLOOR.L.D",0x4620000B, M_FP2, F_FP2 },
    { "ROUND.W.D",0x4620000C, M_FP2, F_FP2 },
    { "TRUNC.W.D",0x4620000D, M_FP2, F_FP2 },
    { "CEIL.W.D", 0x4620000E, M_FP2, F_FP2 },
    { "FLOOR.W.D",0x4620000F, M_FP2, F_FP2 },
    { "MOVF.D",   0x46200011, M_MCF, F_MCF },
    { "MOVT.D",   0x46210011, M_MCF, F_MCF },
    { "MOVZ.D",   0x46200012, M_MFI, F_MFI },
    { "MOVN.D",   0x46200013, M_MFI, F_MFI },
    { "RECIP.D"  ,0x46200015, M_FP2, F_FP2 },
    { "RSQRT.D"  ,0x46200016, M_FP2, F_FP2 },
    { "CVT.S.D",  0x46200020, M_FP2, F_FP2 },
    { "CVT.W.D",  0x46200024, M_FP2, F_FP2 },
    { "CVT.L.D",  0x46200025, M_FP2, F_FP2 },
    { "C.F.D",    0x46200030, M_FCP, F_FCP },
    { "C.UN.D",   0x46200031, M_FCP, F_FCP },
    { "C.EQ.D",   0x46200032, M_FCP, F_FCP },
    { "C.UEQ.D",  0x46200033, M_FCP, F_FCP },
    { "C.OLT.D",  0x46200034, M_FCP, F_FCP },
    { "C.ULT.D",  0x46200035, M_FCP, F_FCP },
    { "C.OLE.D",  0x46200036, M_FCP, F_FCP },
    { "C.ULE.D",  0x46200037, M_FCP, F_FCP },
    { "C.DF.D",   0x46200038, M_FCP, F_FCP },
    { "C.NGLE.D", 0x46200039, M_FCP, F_FCP },
    { "C.DEQ.D",  0x4620003A, M_FCP, F_FCP },
    { "C.NGL.D",  0x4620003B, M_FCP, F_FCP },
    { "C.LT.D",   0x4620003C, M_FCP, F_FCP },
    { "C.NGE.D",  0x4620003D, M_FCP, F_FCP },
    { "C.LE.D",   0x4620003E, M_FCP, F_FCP },
    { "C.BGT.D",  0x4620003F, M_FCP, F_FCP },
    { "CVT.S.W",  0x46800020, M_FP2, F_FP2 },
    { "CVT.D.W",  0x46800021, M_FP2, F_FP2 },
    { "CVT.S.L",  0x46A00020, M_FP2, F_FP2 },
    { "CVT.D.L",  0x46A00021, M_FP2, F_FP2 },
    { "ADD.PS",   0x46C00000, M_FP3, F_FP3 },
    { "SUB.PS",   0x46C00001, M_FP3, F_FP3 },
    { "MUL.PS",   0x46C00002, M_FP3, F_FP3 },
    { "ABS.PS",   0x46C00005, M_FP2, F_FP2 },
    { "MOV.PS",   0x46C00006, M_FP2, F_FP2 },
    { "NEG.PS",   0x46C00007, M_FP2, F_FP2 },
    { "MOVF.PS",  0x46C00011, M_MCF, F_MCF },
    { "MOVT.PS",  0x46C10011, M_MCF, F_MCF },
    { "MOVZ.PS",  0x46C00012, M_MFI, F_MFI },
    { "MOVN.PS",  0x46C00013, M_MFI, F_MFI },
    { "CVT.S.PU", 0x46C00020, M_FP2, F_FP2 },
    { "CVT.S.PL", 0x46C00028, M_FP2, F_FP2 },
    { "PLL.PS",   0x46C0002C, M_FP3, F_FP3 },
    { "PLU.PS",   0x46C0002D, M_FP3, F_FP3 },
    { "PUL.PS",   0x46C0002E, M_FP3, F_FP3 },
    { "PUU.PS",   0x46C0002F, M_FP3, F_FP3 },
    { "C.F.PS",   0x46C00030, M_FCP, F_FCP },
    { "C.UN.PS",  0x46C00031, M_FCP, F_FCP },
    { "C.EQ.PS",  0x46C00032, M_FCP, F_FCP },
    { "C.UEQ.PS", 0x46C00033, M_FCP, F_FCP },
    { "C.OLT.PS", 0x46C00034, M_FCP, F_FCP },
    { "C.ULT.PS", 0x46C00035, M_FCP, F_FCP },
    { "C.OLE.PS", 0x46C00036, M_FCP, F_FCP },
    { "C.ULE.PS", 0x46C00037, M_FCP, F_FCP },
    { "C.DF.PS",  0x46C00038, M_FCP, F_FCP },
    { "C.NGLE.PS",0x46C00039, M_FCP, F_FCP },
    { "C.DEQ.PS", 0x46C0003A, M_FCP, F_FCP },
    { "C.NGL.PS", 0x46C0003B, M_FCP, F_FCP },
    { "C.LT.PS",  0x46C0003C, M_FCP, F_FCP },
    { "C.NGE.PS", 0x46C0003D, M_FCP, F_FCP },
    { "C.LE.PS",  0x46C0003E, M_FCP, F_FCP },
    { "C.BGT.PS", 0x46C0003F, M_FCP, F_FCP },


    { "LWXC1",    0x4C000000, M_LFX, F_LFX },           /* cop1x */
    { "LDXC1",    0x4C000001, M_LFX, F_LFX },
    { "LUXC1",    0x4C000005, M_LFX, F_LFX },
    { "SWXC1",    0x4C000008, M_SFX, F_SFX },
    { "SDXC1",    0x4C000009, M_SFX, F_SFX },
    { "SUXC1",    0x4C00000D, M_SFX, F_SFX },
    { "PREF",     0x4C00000F, M_LFX, F_LFX },
    { "ALNV.PS",  0x4C00001E, M_ALNV,F_ALNV},
    { "MADD.S",   0x4C000020, M_MAC, F_MAC },
    { "MADD.D",   0x4C000021, M_MAC, F_MAC },
    { "MADD.PS",  0x4C000026, M_MAC, F_MAC },
    { "MSUB.S",   0x4C000028, M_MAC, F_MAC },
    { "MSUB.D",   0x4C000029, M_MAC, F_MAC },
    { "MSUB.PS",  0x4C00002E, M_MAC, F_MAC },
    { "NMADD.S",  0x4C000030, M_MAC, F_MAC },
    { "NMADD.D",  0x4C000031, M_MAC, F_MAC },
    { "NMADD.PS", 0x4C000036, M_MAC, F_MAC },
    { "NMSUB.S",  0x4C000038, M_MAC, F_MAC },
    { "NMSUB.D",  0x4C000039, M_MAC, F_MAC },
    { "NMSUB.PS", 0x4C00003E, M_MAC, F_MAC },

    { "BEQL",   0x50000000, M_BR2, F_BR2 },
    { "BNEL",   0x54000000, M_BR2, F_BR2 },
    { "BLEZL",  0x58000000, M_BR1, F_BR1 },
    { "BGTZL",  0x5C000000, M_BR1, F_BR1 },
    { "DADDI",  0x60000000, M_IMM, F_IMM },
    { "DADDIU", 0x64000000, M_IMM, F_IMM },
    { "LDL",    0x68000000, M_MRF, F_MRF },
    { "LDR",    0x6C000000, M_MRF, F_MRF },

    { "MADD",   0x70000000, M_OR2, F_OR2 },             /* special2 */
    { "MADDU",  0x70000001, M_OR2, F_OR2 },
    { "MUL",    0x70000002, M_OP3, F_OP3 },
    { "MSUB",   0x70000004, M_OR2, F_OR2 },
    { "MSUBU",  0x70000005, M_OR2, F_OR2 },
    { "CLZ",    0x70000020, M_SP2, F_SP2 },
    { "CLO",    0x70000021, M_SP2, F_SP2 },
    { "DCLZ",   0x70000024, M_SP2, F_SP2 },
    { "DCLO",   0x70000025, M_SP2, F_SP2 },
    { "POP",    0x7000002C, M_POP, F_POP },
    { "DPOP",   0x7000002D, M_POP, F_POP },
    { "SDBBP",  0x7000003F, M_COD, F_COD },

    { "EXT",    0x7C000000, M_FLD, F_FLD },             // special3 - R2
    { "DEXTU",  0x7C000001, M_FLD, F_FLD },
    { "DEXTM",  0x7C000002, M_FLD, F_FLD },
    { "DEXT",   0x7C000003, M_FLD, F_FLD },
    { "INS",    0x7C000004, M_FLD, F_FLD },
    { "DINSU",  0x7C000005, M_FLD, F_FLD },
    { "DINSM",  0x7C000006, M_FLD, F_FLD },
    { "DINS",   0x7C000007, M_FLD, F_FLD },
    { "SEB",    0x7C000420, M_OP2, F_OP2 },
    { "SEW",    0x7C000620, M_OP2, F_OP2 },
    { "RDHWR",  0x7C00003B, M_OP2, F_OP2 },
    { "WSBH",   0x7C0000A0, M_OP2, F_OP2 },
    { "DSBH",   0x7C0000A4, M_OP2, F_OP2 },
    { "DSHD",   0x7C0001A4, M_OP2, F_OP2 },

    { "LB",     0x80000000, M_MRF, F_MRF },
    { "LH",     0x84000000, M_MRF, F_MRF },
    { "LWL",    0x88000000, M_MRF, F_MRF },
    { "LW",     0x8C000000, M_MRF, F_MRF },
    { "LBU",    0x90000000, M_MRF, F_MRF },
    { "LHU",    0x94000000, M_MRF, F_MRF },
    { "LWR",    0x98000000, M_MRF, F_MRF },
    { "LWU",    0x9C000000, M_MRF, F_MRF },
    { "SB",     0xA0000000, M_MRF, F_MRF },
    { "SH",     0xA4000000, M_MRF, F_MRF },
    { "SWL",    0xA8000000, M_MRF, F_MRF },
    { "SW",     0xAC000000, M_MRF, F_MRF },
    { "SDL",    0xB0000000, M_MRF, F_MRF },
    { "SDR",    0xB4000000, M_MRF, F_MRF },
    { "SWR",    0xB8000000, M_MRF, F_MRF },
    { "CACHE",  0xBC000000, M_MRF, F_MRF },
    { "LL",     0xC0000000, M_MRF, F_MRF },
    { "LWC1",   0xC4000000, M_MRF, F_MRF },
    { "LWC2",   0xC8000000, M_MRF, F_MRF },
    { "PREF",   0xCC000000, M_MRF, F_MRF },
    { "LLD",    0xD0000000, M_MRF, F_MRF },
    { "LDC1",   0xD4000000, M_MRF, F_MRF },
    { "LDC2",   0xD8000000, M_MRF, F_MRF },
    { "LD",     0xDC000000, M_MRF, F_MRF },
    { "SC",     0xE0000000, M_MRF, F_MRF },
    { "SWC1",   0xE4000000, M_MRF, F_MRF },
    { "SWC2",   0xE8000000, M_MRF, F_MRF },
    { "SCD",    0xF0000000, M_MRF, F_MRF },
    { "SDC1",   0xF4000000, M_MRF, F_MRF },
    { "SDC2",   0xF8000000, M_MRF, F_MRF },
    { "SD",     0xFC000000, M_MRF, F_MRF },
    { NULL, 0, M_NOP, F_NOP }
};

/* Symbolic decode

   Inputs:
        *of     =       output stream
        addr    =       current PC
        *val    =       values to decode
        *uptr   =       pointer to unit
        sw      =       switches
   Outputs:
        return  =       if >= 0, error code
                        if < 0, number of extra bytes retired
*/

t_stat fprint_sym (FILE *of, t_addr addr, t_value *val,
    UNIT *uptr, int32 sw)
{
int32 c, sc, rdx, def_rdx;
char in_st[CBUFSIZE];
DEVICE *dptr;

if (uptr) {
    dptr = find_dev_from_unit (uptr);                   /* find dev */
    if (dptr == NULL) return SCPE_IERR;
    if ((dptr->flags & DEV_64B) == 0) return SCPE_ARG;  /* CPU only */
    def_rdx = dptr->dradix;
    }
else def_rdx = 16;
if (sw & SWMASK ('T')) rdx = 10;                        /* get radix */
else if (sw & SWMASK ('O')) rdx = 8;
else if (sw & SWMASK ('S')) rdx = 16;
else rdx = def_rdx;

if (sw & SWMASK ('A')) {                                /* ASCII? */
    sc = (uint32) (addr & 0x7) * 8;                     /* shift count */
    c = (uint32) (val[0] >> sc) & 0x7F;
    fprintf (of, (c < 0x20)? "<%02X>": "%c", c);
    return 0;
    }
if (sw & SWMASK ('B')) {                                /* byte? */
    sc = (uint32) (addr & 0x7) * 8;                     /* shift count */
    c = (uint32) (val[0] >> sc) & M8;
    fprintf (of, "%02X", c);
    return 0;
    }
if (sw & SWMASK ('H')) {                                /* halfword? */
    sc = (uint32) (addr & 0x6) * 8;                     /* shift count */
    c = (uint32) (val[0] >> sc) & M16;
    fprintf (of, "%04X", c);
    return -1;
    }
if (sw & SWMASK ('W')) {                                /* word? */
    if (addr & 4) c = (uint32) (val[0] >> 32) & M32;
    else c = (uint32) val[0] & M32;
    fprintf (of, "%08X", c);
    return -3;
    }
if (sw & SWMASK ('C')) {                                /* char format? */
    for (sc = 0; sc < 64; sc = sc + 8) {                /* print string */
        c = (uint32) (val[0] >> sc) & 0x7F;
        fprintf (of, (c < 0x20)? "<%02X>": "%c", c);
        }
    return -7;
    }
if (sw & SWMASK ('M')) {                                /* inst format? */
    if (addr & 4) c = (uint32) (val[0] >> 32) & M32;
    else c = (uint32) val[0] & M32;
    if (sprint_sym_m (in_st, addr, c))                  /* decode inst */
        fputs (in_st, of);
    else fprintf (of, "%08X", c);
    return -3;
    }

fprint_val (of, val[0], rdx, 64, PV_RZRO);
return -7;
}

/* Symbolic decode for -m

   Inputs:
        cptr    =       output string
        addr    =       current PC
        inst    =       instruction to decode
   Outputs:
        return  =       if >= 0, error code
                        if < 0, number of extra bytes retired (-3)
*/

uint32 sprint_sym_m (char *cptr, t_addr addr, uint32 inst)
{
uint32 i, k, fl, fld[16], any;
char *optr, *oldptr;

oldptr = cptr;                                          /* save start */
for (i = 0; opc[i].name != NULL; i++) {                 /* loop thru ops */
    if (((inst ^ opc[i].val) & opc[i].mask) == 0) {
        fl = opc[i].flg;                                /* flags */
        fld[F_RS] = I_GETRS (inst);                     /* all fields */
        fld[F_RT] = I_GETRT (inst);
        fld[F_RD] = I_GETRD (inst);
        fld[F_SA] = I_GETSA (inst);
        fld[F_FNC] = I_GETFNC (inst);
        fld[F_JT] = I_GETJT (inst);
        fld[F_DISP] = I_GETDISP (inst);
        fld[F_CODE] = (inst >> I_V_SA) & 0xFFFFF;
        fld[F_Z] = 0;
        any = 0;
        for (optr = opc[i].name; *optr; optr++) {       /* copy op, lc */
            if (isupper (*optr)) *cptr++ = tolower (*optr);
            else *cptr++ = *optr;
            }
        *cptr = 0;
        for (k = 0; k < 4; k++) {                       /* up to 4 fields */
            uint32 fmt = SYM_GETFMT (fl, k);            /* get format */
            uint32 val = fld[SYM_GETFLD (fl, k)];       /* get field */
            t_uint64 v64 = val;
            switch (fmt) {                              /* case fmt */

                case FMT_RC:                            /* cond ret reg */
                    if (val == 31) break;               /* dont print 31 */
                case FMT_R:                             /* int reg */
                case FMT_FR:                            /* flt reg */
                    any = sprintf (cptr, (any? ",$%d": " $%d"), val);
                    break;

                case FMT_SA:                            /* shift amt */
                    any = sprintf (cptr, (any? ",%d": " %d"), val);
                    break;

                case FMT_CC:                            /* cond code */
                    val = (val >> 2) & 0x7;             /* don't print 0 */
                    if (val) any = sprintf (cptr, (any? ",%d": " %d"), val);
                    break;

                case FMT_SEL:                           /* select */
                    val = val & 0x7;                    /* don't print 0 */
                    if (val) any = sprintf (cptr, (any? ",%d": " %d"), val);
                    break;

                case FMT_CODE:                          /* code */
                    if (val) any = sprintf (cptr, (any? ",%X": " %X"), val);
                    break;

                case FMT_IMM:                           /* immediate */
                    any = sprintf (cptr, (any? ",%X": " %X"), val);
                    break;

                case FMT_JA:                            /* jump */
                    addr = ((addr + 4) & J_REGION) | (v64 << 2);
                    any = sprintf (cptr, (any? ",": " "));
                    cptr = cptr + strlen (cptr);
                    sprint_addr (cptr, addr);
                    break;

                case FMT_BA:                            /* branch */
                    addr = addr + (SEXT_DISP (v64) << 2) + 4;
                    any = sprintf (cptr, (any? ",": " "));
                    cptr = cptr + strlen (cptr);
                    sprint_addr (cptr, addr);
                    break;

                case FMT_MA:                            /* mem ref */
                    if (val & SIGN_DISP)
                        any = sprintf (cptr, (any? ",-%X": " -%X"), 0x10000 - val);
                    else any = sprintf (cptr, (any? ",%X": " %X"), val);
                    if (fld[F_RS]) {
                        cptr = cptr + strlen (cptr);
                        sprintf (cptr, "($%d)", fld[F_RS]);
                        }
                    break;

                case FMT_XA:                            /* XA */
                    any = sprintf (cptr, (any? ",$%d": " $%d"), val);
                    if (fld[F_RT]) {
                        cptr = cptr + strlen (cptr);
                        sprintf (cptr, "($%d)", fld[F_RT]);
                        }
                    break;
                }                                       /* end case */
            cptr = cptr + strlen (cptr);
            }                                           /* end for k */
        return strlen (oldptr);
        }                                               /* end if */
    }                                                   /* end for i */
return 0;
}

/* Symbolic input

   Inputs:
        *cptr   =       pointer to input string
        addr    =       current PC
        *uptr   =       pointer to unit
        *val    =       pointer to output values
        sw      =       switches
   Outputs:
        status  =       > 0   error code
                        <= 0  -number of extra words
*/

t_stat parse_sym (char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw)
{
t_value num;
uint32 i, sc, rdx, def_rdx;
t_stat r;
DEVICE *dptr;

if (uptr) {
    dptr = find_dev_from_unit (uptr);                   /* find dev */
    if (dptr == NULL) return SCPE_IERR;
    if ((dptr->flags & DEV_64B) == 0) return SCPE_ARG;  /* CPU only */
    def_rdx = dptr->dradix;
    }
else def_rdx = 16;
if (sw & SWMASK ('T')) rdx = 10;                        /* get radix */
else if (sw & SWMASK ('O')) rdx = 8;
else if (sw & SWMASK ('S')) rdx = 16;
else rdx = def_rdx;

if ((sw & SWMASK ('A')) || ((*cptr == '\'') && cptr++)) { /* ASCII char? */
    if (cptr[0] == 0) return SCPE_ARG;                  /* must have 1 char */
    sc = (uint32) (addr & 0x7) * 8;                     /* shift count */
    val[0] = (val[0] & ~(((t_uint64) M8) << sc)) |
        (((t_uint64) cptr[0]) << sc);
    return 0;
    }
if (sw & SWMASK ('B')) {                                /* byte? */
    num = get_uint (cptr, rdx, M8, &r);                 /* get byte */
    if (r != SCPE_OK) return SCPE_ARG;
    sc = (uint32) (addr & 0x7) * 8;                     /* shift count */
    val[0] = (val[0] & ~(((t_uint64) M8) << sc)) |
        (num << sc);
    return 0;
    }
if (sw & SWMASK ('H')) {                                /* halfword? */
    num = get_uint (cptr, rdx, M16, &r);                /* get halfword */
    if (r != SCPE_OK) return SCPE_ARG;
    sc = (uint32) (addr & 0x6) * 8;                     /* shift count */
    val[0] = (val[0] & ~(((t_uint64) M16) << sc)) |
        (num << sc);
    return -1;
    }
if (sw & SWMASK ('W')) {                                /* word? */
    num = get_uint (cptr, rdx, M32, &r);                /* get word */
    if (r != SCPE_OK) return SCPE_ARG;
    sc = (uint32) (addr & 0x4) * 8;                     /* shift count */
    val[0] = (val[0] & ~(((t_uint64) M32) << sc)) |
        (num << sc);
    return -3;
    }
if ((sw & SWMASK ('C')) || ((*cptr == '"') && cptr++)) { /* ASCII chars? */
    if (cptr[0] == 0) return SCPE_ARG;                  /* must have 1 char */
    for (i = 0; i < 8; i++) {
        if (cptr[i] == 0) break;
        sc = i * 8;
        val[0] = (val[0] & ~(((t_uint64) M8) << sc)) |
            (((t_uint64) cptr[i]) << sc);
        }
    return -7;
    }

if ((addr & 3) == 0) {                                  /* aligned only */
    r = parse_sym_m (cptr, addr, &num);                 /* try to parse inst */
    if (r <= 0) {                                       /* ok? */
        sc = (uint32) (addr & 0x4) * 8;                 /* shift count */
        val[0] = (val[0] & ~(((t_uint64) M32) << sc)) | (num << sc);
        return -3;
        }
    }

val[0] = get_uint (cptr, rdx, M64, &r);                 /* get number */
if (r != SCPE_OK) return r;
return -7;
}

/* Symbolic input

   Inputs:
        *cptr   =       pointer to input string
        addr    =       current PC
        *val    =       pointer to output values
   Outputs:
        status  =       > 0   error code
                        <= 0  -number of extra words
*/

t_stat parse_sym_m (char *cptr, t_addr addr, t_value *inst)
{
uint32 i, k, fl, num_fmt, num_glyph;
int32 fldv=0, reg;
t_uint64 jba, df, db;
t_stat r;
char *tptr, gbuf[CBUFSIZE];

cptr = get_glyph (cptr, gbuf, 0);                       /* get opcode */
for (i = 0; opc[i].name != NULL; i++) {                 /* loop thru opcodes */
    if (strcmp (opc[i].name, gbuf) == 0) {              /* string match? */
        *inst = opc[i].val;                             /* save base op */
        fl = opc[i].flg;                                /* get parse data */

        for (k = num_fmt = 0; k < 4; k++) {             /* count #formats */
            if ((fl >> (k * 8)) & 0xFF) num_fmt++;
            }
        for (tptr = cptr, num_glyph = 0;                /* count #fields */
            *tptr != 0;
            tptr = get_glyph (tptr, gbuf, ',')) num_glyph++;

        for (k = 0; k < 4; k++) {                       /* up to 4 fields */
            uint32 fn = SYM_GETFLD (fl, k);             /* get field # */
            uint32 fmt = SYM_GETFMT (fl, k);            /* get format */
            switch (fmt) {

            case FMT_RC:                                /* cond ret reg */
                if (num_glyph < num_fmt) {              /* not enough fields? */
                    fldv = 31;                          /* default value */
                    break;
                    }                                   /* otherwise fall thru */
            case FMT_R:                                 /* int reg */
            case FMT_FR:                                /* flt reg */
                cptr = get_glyph (cptr, gbuf, ',');     /* get glyph */
                if ((fldv = parse_reg (gbuf, NULL)) < 0) return SCPE_ARG;
                break;

            case FMT_SA:                                /* 5b literal */
                cptr = get_glyph (cptr, gbuf, ',');     /* get glyph */
                fldv = (int32) get_uint (gbuf, 10, 31, &r);
                if (r != SCPE_OK) return r;
                break;

            case FMT_CC:                                /* cc */
                if (num_glyph < num_fmt) fldv = 0;      /* not enough fields? */
                else {
                    cptr = get_glyph (cptr, gbuf, ',');
                    fldv = (int32) (get_uint (gbuf, 10, 7, &r) << 2);
                    if (r != SCPE_OK) return r;
                    }
                break;

            case FMT_SEL:                               /* select */
                if (num_glyph < num_fmt) fldv = 0;      /* not enough fields? */
                else {
                    cptr = get_glyph (cptr, gbuf, ',');
                    fldv = (int32) get_uint (gbuf, 10, 7, &r);
                    if (r != SCPE_OK) return r;
                    }
                break;

            case FMT_IMM:                               /* 16b literal */
                cptr = get_glyph (cptr, gbuf, 0);
                fldv = parse_imm (gbuf, &tptr);
                if (tptr == gbuf) return SCPE_ARG;
                break;

            case FMT_CODE:                              /* 20b code */
                if (num_glyph < num_fmt) fldv = 0;      /* not enough fields? */
                else {
                    cptr = get_glyph (cptr, gbuf, 0);
                    fldv = (int32) get_uint (gbuf, 16, 0xFFFFF, &r);
                    if (r != SCPE_OK) return r;
                    fldv = fldv << I_V_SA;
                    }
                break;

            case FMT_JA:                                /* jump addr */
                cptr = get_glyph (cptr, gbuf, 0);       /* get glyph */
                jba = get_uint (gbuf, 16, M64, &r);
                if ((r != SCPE_OK) || (jba & 3) ||
                    (((addr + 4) ^ jba) & J_REGION))
                    return SCPE_ARG;
                fldv = ((uint32) (jba & ~J_REGION)) >> 2;
                break;

            case FMT_BA:                                /* branch addr */
                cptr = get_glyph (cptr, gbuf, 0);
                jba = get_uint (gbuf, 16, M64, &r);
                if ((r != SCPE_OK) || (jba & 3)) return SCPE_ARG;
                df = ((jba - (addr + 4)) >> 2) & I_M_DISP;
                db = ((addr + 4 - jba) >> 2) & I_M_DISP;
                if (jba == (addr + 4 + (SEXT_DISP (df) << 2)))
                    fldv = (uint32) df;
                else if (jba == (addr + 4 + (SEXT_DISP (db) << 2)))
                    fldv = (uint32) db;
                else return SCPE_ARG;
                break;

            case FMT_MA:                                /* mem ref? */
                cptr = get_glyph (cptr, gbuf, 0);
                fldv = parse_imm (gbuf, &tptr);
                if (tptr == gbuf) return SCPE_ARG;
                if (*tptr == '(') {
                    tptr = get_glyph (tptr + 1, gbuf, 0);
                    if ((reg = parse_reg (gbuf, &tptr)) < 0) return SCPE_ARG;
                    if (*tptr++ != ')') return SCPE_ARG;
                    fldv |= (reg << I_V_RS);
                    }
                if (*tptr != 0) return SCPE_ARG;
                break;

            case FMT_XA:                                /* reg indexed? */
                cptr = get_glyph (cptr, gbuf, 0);
                if ((fldv = parse_reg (cptr, &tptr)) < 0) return SCPE_ARG;
                if (*tptr == '(') {
                    if ((reg = parse_reg (tptr, &tptr)) < 0) return SCPE_ARG;
                    if (*tptr != ')') return SCPE_ARG;
                    fldv |= (reg << 5);
                    }
                else if (*tptr) return SCPE_ARG;
                break;

            case FMT_NOPARSE:                           /* do nothing */
                if (fn == F_Z) fldv = 0;
                break;          
                }                                       /* end case */

            *inst |= (fldv << fld_shift[fn]);           /* move into place */
            }                                           /* end for k */
        if (*cptr != 0) return SCPE_ARG;                /* any leftovers? */
        return -3;
        }                                               /* end if */
    }                                                   /* end for */
return SCPE_ARG;
}

/* Parse a register */

int32 parse_reg (char *cptr, char **optr)
{
uint32 reg;
char *tptr;

if ((*cptr == 'R') || (*cptr == 'r') || (*cptr == '$')) cptr++;
reg = (uint32) strtotv (cptr, &tptr, 10);
if ((cptr == tptr) || (reg > 31)) return -1;
if (optr) *optr = tptr;
else if (*tptr != 0) return -1;
return reg;
}

/* Parse an immediate */

uint32 parse_imm (char *cptr, char **optr)
{
uint32 sign;
t_value imm;
static const t_value maxv[3] = {
    0xFFFF, 0x7FFF, 0x8000
    };

if (*cptr == '+') {
    sign = 1;
    cptr++;
    }
else if (*cptr == '-') {
    sign = 2;
    cptr++;
    }
else sign = 0;
imm = strtotv (cptr, optr, 16);
if (cptr == *optr) return 0;
if (imm > maxv[sign]) *optr = cptr;
else if (sign == 2) imm = ~imm + 1;
return ((uint32) imm) & M16;
}

/* String format a 64b address, suppress leading zeroes */

char *sprint_addr (char *cptr, t_uint64 ad)
{
uint32 i, nz;
char c;

for (i = 0, nz = 0; i < 16; i++) {
    c = (char) ((ad >> ((15 - i) * 4)) & 0xF);
    if (c || (i == 15)) nz = 1;
    if (nz) *cptr++ = (c < 10)? (c + '0'): (c - 10 + 'A');
    }
*cptr = 0;
return cptr;
}

/* Routine to fetch int/flt registers for tracing,
   based on opcode */

uint32 trace_fetch_reg (CORECTX *ctx, uint32 inst, t_uint64 *reg)
{
uint32 i, j, k, fl, fld[16];

for (k = 0; k < 4; k++)
    reg[k] = 0;
for (i = 0; opc[i].name != NULL; i++) {                 /* loop thru ops */
    if (((inst ^ opc[i].val) & opc[i].mask) == 0) {
        fl = opc[i].flg;                                /* flags */
        fld[F_RS] = I_GETRS (inst);                     /* all reg fields */
        fld[F_RT] = I_GETRT (inst);
        fld[F_RD] = I_GETRD (inst);
        fld[F_SA] = I_GETSA (inst);
        for (j = k = 0; k < 4; k++) {                   /* up to 4 reg */
            uint32 fmt = SYM_GETFMT (fl, k);            /* get format */
            uint32 fsl = SYM_GETFLD (fl, k);            /* get field select */
            if ((fsl >= F_RS) && (fsl <= F_SA)) {
                if ((fmt == FMT_R) || (fmt == FMT_RC))  /* R? */
                    reg[j++] = gpr(fld[fsl]);        /* get int */
                else if (fmt == FMT_FR)                 /* F? */
                    reg[j++] = fpr(fld[fsl]);        /* get flt */
                }                                       /* end if fsl */
            }                                           /* end for k */
        return fl;                                      /* return flags */
        }                                               /* end if */
    }                                                   /* end for i */
return 0;
}

/* Routine to print one line for tracing */

void trace_fprint_one_inst (FILE *st, uint32 inst, t_uint64 pc,
    uint32 fl, t_uint64 *reg, uint32 coreno)
{
uint32 fld[16], cnt, j, k, t;
t_uint64 ea, sim_val;

#ifdef SIMH_CPUSIMH
extern t_uint64 ScxGetCurrentSimTime();
fprintf (st, "[%lld] ", ScxGetCurrentSimTime());
#endif
if (coreno) fprintf (st, "#%d: ", coreno - 1);
pc = pc & ~3;
fprint_val (st, pc, 16, 64, PV_RZRO);                   /* print pc */
fputc (' ', st);
fld[F_RS] = I_GETRS (inst);                             /* all reg sels */
fld[F_RT] = I_GETRT (inst);
fld[F_RD] = I_GETRD (inst);
fld[F_SA] = I_GETSA (inst);
for (cnt = j = k = 0; k < 4; k++) {                     /* up to 4 fields */
    uint32 fmt = SYM_GETFMT (fl, k);                    /* get format */
    uint32 fsl = SYM_GETFLD (fl, k);                    /* get reg pos */
    switch (fmt) {                                      /* case fmt */

        case FMT_RC:                                    /* cond ret reg */
        case FMT_R:                                     /* int reg */
        case FMT_FR:                                    /* flt reg */
            if ((fsl >= F_RS) && (fsl <= F_SA)) {       /* valid sel? */
                uint32 rn = fld[fsl];
                if (fmt == FMT_FR)
                    fprintf (st, (rn < 10)? " F%d=":"F%d=", rn);
                else fprintf (st, (rn < 10)? " R%d=":"R%d=", rn);
                fprint_val (st, reg[j++], 16, 64, PV_RZRO);
                fputc (' ', st);
                cnt++;
                }
            break;

        case FMT_IMM:                                   /* immediate */
            t = inst & M16;
            if (t & H_SIGN)
                fprintf (st, "               -%4X", 0x10000 - t);
            else fprintf (st, "                %4X", t);
            fputc (' ', st);
            cnt++;
            break;

        case FMT_JA:                                    /* jump */
            ea = (pc & J_REGION) | (I_GETJT (inst) << 2);
            fputs (" ea=", st);
            fprint_val (st, ea, 16, 64, PV_RZRO);
            fputc (' ', st);
            cnt++;
            break;

        case FMT_BA:                                    /* branch */
            ea = pc + 4 + (SEXT_DISP (inst) << 2);
            fputs (" ea=", st);
            fprint_val (st, ea, 16, 64, PV_RZRO);
            fputc (' ', st);
            cnt++;
            break;

        case FMT_MA:                                    /* mem ref */
            ea = reg[1] + SEXT_DISP (I_GETDISP (inst));
            fputs (" ea=", st);
            fprint_val (st, ea, 16, 64, PV_RZRO);
            fputc (' ', st);
            cnt++;
            break;
        }                                               /* end case */
    }                                                   /* end for */
for ( ; cnt < 4; cnt++)
    fputs ("                     ", st);
if (pc & 4)
    sim_val = ((t_uint64) inst) << 32;
else sim_val = inst;
if ((fprint_sym (st, pc, &sim_val, NULL, SWMASK ('M'))) > 0)
    fprintf (st, "(undefined) %08X", inst);
fputc ('\r', st);                                   /* end line */
fputc ('\n', st);                                   /* end line */
return;
}
