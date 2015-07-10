/* DO NOT EDIT -- Generated automatically by vregs */
/// \file
/// \brief Register Information: Generated automatically by vregs
///
/// See SystemC::Vregs::Rules file: chip_magic_spec__rules.pl
///
/* ==================================================================== */
/* */
/* RESTRICTED RIGHTS LEGEND */
/* */
/* Unpublished Work, Copyright 2005-2006 SiCortex Inc. All Rights Reserved. */
/* */
/* This computer program is the property of SiCortex and contains its */
/* confidential trade secrets.  Use, examination, reproduction, transfer */
/* and disclosure to others, in whole or in part, are prohibited except */
/* with the express prior written consent of SiCortex, Inc. */
/* */
/*==================================================================== */

#ifndef _ICE9_MAGIC_SPEC_SW_H_
#define _ICE9_MAGIC_SPEC_SW_H_ 1

#include "ice9_vregs_defs.h"

/* ====================================================================== */
/* Enumerations */

#ifndef VREGS_ASSEMBLER

typedef enum {
	Ice9_MipsMagicInstrs_PUTCHAR             = 0x424,	///< Putchar
	Ice9_MipsMagicInstrs_LLTIMEFORCE         = 0x464,	///< Set LLTIME to RS value
	Ice9_MipsMagicInstrs_ECCINJ              = 0x4a4,	///< Inject a ECC error, presently SIMH only
	Ice9_MipsMagicInstrs_MPIGET              = 0x4e4,	///< Receive data from MPI socket
	Ice9_MipsMagicInstrs_DIRWRLONG           = 0x524,	///< Direct write long
	Ice9_MipsMagicInstrs_SECONDRETURN        = 0x564,	///< Second return value from last magic instr
	Ice9_MipsMagicInstrs_DIRRDLONG           = 0x5a4,	///< Direct read long
	Ice9_MipsMagicInstrs_DIRWRQUAD           = 0x5e4,	///< Direct write quad
	Ice9_MipsMagicInstrs_DIRRDQUAD           = 0x624,	///< Direct read quad
	Ice9_MipsMagicInstrs_ISSWITCH            = 0x664,	///< Return state of simulation model, and if switchable, etc
	Ice9_MipsMagicInstrs_HALTFAIL            = 0x2c00abc1,	///< Mips, Test fails, halt simulation
	Ice9_MipsMagicInstrs_HALTPASS            = 0x2c00abc2,	///< Mips, Test passes, halt simulation
	Ice9_MipsMagicInstrs_COMPAREOFF          = 0x2c00abc3,	///< Mips, AVP use only
	Ice9_MipsMagicInstrs_COMPAREON           = 0x2c00abc4,	///< Mips, AVP use only
	Ice9_MipsMagicInstrs_COMPAREOFFTHRUEX    = 0x2c00abc5,	///< Mips, AVP use only
	Ice9_MipsMagicInstrs_COMPARESKIP         = 0x2c00abd0,	///< Mips, AVP use only
	Ice9_MipsMagicInstrs_COMPARESKIPVA       = 0x2c00abe0,	///< Mips, AVP use only
	Ice9_MipsMagicInstrs_COMPARESKIPVACANCEL = 0x2c00ac00,	///< Mips, AVP use only
	Ice9_MipsMagicInstrs_MIPSLOAD            = 0x2c00ad01,	///< No longer used
	Ice9_MipsMagicInstrs_SIMHTRACEON         = 0x2c00ad02,	///< Simh enable tracefile
	Ice9_MipsMagicInstrs_SIMHTRACEOFF        = 0x2c00ad03,	///< Simh disable tracefile
	Ice9_MipsMagicInstrs_SIMHSWITCHRTL       = 0x2c00ad04,	///< Some simulation models are able to run part of the simulation on a fast model an
	Ice9_MipsMagicInstrs_DUMPCTB             = 0x2c00ad05,	///< Dump OCLA CTBs
	Ice9_MipsMagicInstrs_CSIMON              = 0x2c00ad06,	///< Cache simulation enable and config
	Ice9_MipsMagicInstrs_CSIMOFF             = 0x2c00ad07,	///< Cache simulation disable
	Ice9_MipsMagicInstrs_CSIMPREFON          = 0x2c00ad08,	///< Cache simulation prefetch instruction enable
	Ice9_MipsMagicInstrs_CSIMPREFOFF         = 0x2c00ad09,	///< Cache simulation prefetch instruction disable
	Ice9_MipsMagicInstrs_CSIMREPORT          = 0x2c00ad0a,	///< Generate cache simulation report
	Ice9_MipsMagicInstrs_WAVESON             = 0x2c00ad0b,	///< Allows simulated code to turn waveform capture on
	Ice9_MipsMagicInstrs_WAVESOFF            = 0x2c00ad0c,	///< Allows simulated code to turn waveform capture off
	Ice9_MipsMagicInstrs_FPCMPSAVE           = 0x2c00ad0d,	///< Floating point comparison watcher should save following instructions
	Ice9_MipsMagicInstrs_FPCMPCHECK          = 0x2c00ad0e,	///< Floating point comparison watcher should compare following instructions
	Ice9_MipsMagicInstrs_FPCMPOFF            = 0x2c00ad0f,	///< Floating point comparison watcher should turn off
	Ice9_MipsMagicInstrs_MAX                 = 0xffffffff	///< MAXIMUM (-1 adjusted so will fit in 32-bits)
} Ice9_MipsMagicInstrs;

typedef enum {
	Ice9_SimhMemModel_NONE          = 0x0,	///< No memory model has been chosen
	Ice9_SimhMemModel_NORMAL        = 0x1,	///< Memory and I/O references stall only the CPU that initiated them
	Ice9_SimhMemModel_SERIAL        = 0x2,	///< Memory and I/O references are serialized across the whole machine
	Ice9_SimhMemModel_BACKDOOR      = 0x3,	///< Memory references take zero time
	Ice9_SimhMemModel_BACKDOOR_NBIO = 0x4,	///< Memory references take zero time
	Ice9_SimhMemModel_MAX           = 0xffffffff	///< MAXIMUM (-1 adjusted so will fit in 32-bits)
} Ice9_SimhMemModel;

#endif

/* ====================================================================== */
/* Register SHIFT/Masks */

#define ICE9_SimhEpochCycles_EpochCycles_SHIFT	0	///< When allowing the SystemC model to step, by how many pclk cycles do we step by a
#define ICE9_SimhEpochCycles_EpochCycles_MASK	VREGS_ULL(0xFFFFFFFFFFFFFFFF)
#define ICE9_SimhMemModel_MemModel_SHIFT	0	///< Memory model to use for cpu_simh simulations
#define ICE9_SimhMemModel_MemModel_MASK	VREGS_ULL(0x00000000FFFFFFFF)
#define ICE9_SimhRatio_SimhRatio_SHIFT	0	///< How much faster is SIMH than a real CPU, if it doesn t have to wait for the hard
#define ICE9_SimhRatio_SimhRatio_MASK	VREGS_ULL(0xFFFFFFFFFFFFFFFF)

/* ====================================================================== */
/* Definitions */

///  General convention:
///      RA_{regname}     Register beginning address
///      RAE_{regname}    Register ending address + 1
///      RAC_{regname}    Number of entries in register
///      RAM_{regname}    Register region address mask
///      RRP_{regname}    Register RANGE spacing in bytes, if arrayed
///      RRS_{regname}    Register RANGE size, if arrayed
///
///      RBASEA_{regs}    Register common-prefix starting address
///      RBASEAE_{regs}   Register common-prefix ending address + 1
///      RBASEAM_{regs}   Register common-prefix bit mask
///
///      E_{enum}_{alias}           Value of enumeration encoding
///
///      CM{w}_{class}_WRITABLE     Mask of all writable bits
///      CB{w}_{class}_{field}_{f}  Class field starting bit
///      CE{w}_{class}_{field}_{f}  Class field ending bit
///      CR{w}_{class}_{field}_{f}  Class field range
///           {w}=32=bit word number,  {f}=field number if discontinuous



/* Automatic Defines */
#define	ICE9_RA_SimhRatio		       VREGS_ULL(0xEC0000000)
#define	ICE9_RAE_SimhRatio		       VREGS_ULL(0xEC0000008)
#define	ICE9_RAC_SimhRatio		      0x00000001
#define	ICE9_RRP_SimhRatio		       VREGS_ULL(0x000000000)
#define	ICE9_RRS_SimhRatio		  Non_Contiguous	///< Range byte size: This register region contains gaps.
#define	ICE9_RAM_SimhRatio		       VREGS_ULL(0x000000007)
#define	ICE9_RA_SimhEpochCycles		       VREGS_ULL(0xEC0000008)
#define	ICE9_RAE_SimhEpochCycles	       VREGS_ULL(0xEC0000010)
#define	ICE9_RAC_SimhEpochCycles	      0x00000001
#define	ICE9_RRP_SimhEpochCycles	       VREGS_ULL(0x000000000)
#define	ICE9_RRS_SimhEpochCycles	  Non_Contiguous	///< Range byte size: This register region contains gaps.
#define	ICE9_RAM_SimhEpochCycles	       VREGS_ULL(0x000000007)
#define	ICE9_RA_SimhMemModel		       VREGS_ULL(0xEC0000010)
#define	ICE9_RAE_SimhMemModel		       VREGS_ULL(0xEC0000014)
#define	ICE9_RAC_SimhMemModel		      0x00000001
#define	ICE9_RRP_SimhMemModel		       VREGS_ULL(0x000000000)
#define	ICE9_RBASEA_Simh		       VREGS_ULL(0xEC0000000)
#define	ICE9_RBASEAE_Simh		       VREGS_ULL(0xEC0000014)
#define	ICE9_RBASEAM_Simh		       VREGS_ULL(0x00000001F)
#define	ICE9_CB_SimhEpochCycles_EpochCycles		       0	///< Field Start Bit:  63:0   RW   0: When allowing the SystemC model to step, by how many pclk cycles do we step by a
#define	ICE9_CE_SimhEpochCycles_EpochCycles		      63
#define	ICE9_CB_SimhMemModel_MemModel		       0	///< Field Start Bit:  31:0   RW NORMAL: Memory model to use for cpu_simh simulations
#define	ICE9_CE_SimhMemModel_MemModel		      31
#define	ICE9_CB_SimhRatio_SimhRatio		       0	///< Field Start Bit:  63:0   RW   0: How much faster is SIMH than a real CPU, if it doesn t have to wait for the hard
#define	ICE9_CE_SimhRatio_SimhRatio		      63
#define	ICE9_E_MipsMagicInstrs_PUTCHAR	      0x00000424	///< Enum Value: Putchar
#define	ICE9_E_MipsMagicInstrs_LLTIMEFORCE	      0x00000464	///< Enum Value: Set LLTIME to RS value
#define	ICE9_E_MipsMagicInstrs_ECCINJ	      0x000004a4	///< Enum Value: Inject a ECC error, presently SIMH only
#define	ICE9_E_MipsMagicInstrs_MPIGET	      0x000004e4	///< Enum Value: Receive data from MPI socket
#define	ICE9_E_MipsMagicInstrs_DIRWRLONG	      0x00000524	///< Enum Value: Direct write long
#define	ICE9_E_MipsMagicInstrs_SECONDRETURN	      0x00000564	///< Enum Value: Second return value from last magic instr
#define	ICE9_E_MipsMagicInstrs_DIRRDLONG	      0x000005a4	///< Enum Value: Direct read long
#define	ICE9_E_MipsMagicInstrs_DIRWRQUAD	      0x000005e4	///< Enum Value: Direct write quad
#define	ICE9_E_MipsMagicInstrs_DIRRDQUAD	      0x00000624	///< Enum Value: Direct read quad
#define	ICE9_E_MipsMagicInstrs_ISSWITCH	      0x00000664	///< Enum Value: Return state of simulation model, and if switchable, etc
#define	ICE9_E_MipsMagicInstrs_HALTFAIL	      0x2c00abc1	///< Enum Value: Mips, Test fails, halt simulation
#define	ICE9_E_MipsMagicInstrs_HALTPASS	      0x2c00abc2	///< Enum Value: Mips, Test passes, halt simulation
#define	ICE9_E_MipsMagicInstrs_COMPAREOFF	      0x2c00abc3	///< Enum Value: Mips, AVP use only
#define	ICE9_E_MipsMagicInstrs_COMPAREON	      0x2c00abc4	///< Enum Value: Mips, AVP use only
#define	ICE9_E_MipsMagicInstrs_COMPAREOFFTHRUEX	      0x2c00abc5	///< Enum Value: Mips, AVP use only
#define	ICE9_E_MipsMagicInstrs_COMPARESKIP	      0x2c00abd0	///< Enum Value: Mips, AVP use only
#define	ICE9_E_MipsMagicInstrs_COMPARESKIPVA	      0x2c00abe0	///< Enum Value: Mips, AVP use only
#define	ICE9_E_MipsMagicInstrs_COMPARESKIPVACANCEL	      0x2c00ac00	///< Enum Value: Mips, AVP use only
#define	ICE9_E_MipsMagicInstrs_MIPSLOAD	      0x2c00ad01	///< Enum Value: No longer used
#define	ICE9_E_MipsMagicInstrs_SIMHTRACEON	      0x2c00ad02	///< Enum Value: Simh enable tracefile
#define	ICE9_E_MipsMagicInstrs_SIMHTRACEOFF	      0x2c00ad03	///< Enum Value: Simh disable tracefile
#define	ICE9_E_MipsMagicInstrs_SIMHSWITCHRTL	      0x2c00ad04	///< Enum Value: Some simulation models are able to run part of the simulation on a fast model an
#define	ICE9_E_MipsMagicInstrs_DUMPCTB	      0x2c00ad05	///< Enum Value: Dump OCLA CTBs
#define	ICE9_E_MipsMagicInstrs_CSIMON	      0x2c00ad06	///< Enum Value: Cache simulation enable and config
#define	ICE9_E_MipsMagicInstrs_CSIMOFF	      0x2c00ad07	///< Enum Value: Cache simulation disable
#define	ICE9_E_MipsMagicInstrs_CSIMPREFON	      0x2c00ad08	///< Enum Value: Cache simulation prefetch instruction enable
#define	ICE9_E_MipsMagicInstrs_CSIMPREFOFF	      0x2c00ad09	///< Enum Value: Cache simulation prefetch instruction disable
#define	ICE9_E_MipsMagicInstrs_CSIMREPORT	      0x2c00ad0a	///< Enum Value: Generate cache simulation report
#define	ICE9_E_MipsMagicInstrs_WAVESON	      0x2c00ad0b	///< Enum Value: Allows simulated code to turn waveform capture on
#define	ICE9_E_MipsMagicInstrs_WAVESOFF	      0x2c00ad0c	///< Enum Value: Allows simulated code to turn waveform capture off
#define	ICE9_E_MipsMagicInstrs_FPCMPSAVE	      0x2c00ad0d	///< Enum Value: Floating point comparison watcher should save following instructions
#define	ICE9_E_MipsMagicInstrs_FPCMPCHECK	      0x2c00ad0e	///< Enum Value: Floating point comparison watcher should compare following instructions
#define	ICE9_E_MipsMagicInstrs_FPCMPOFF	      0x2c00ad0f	///< Enum Value: Floating point comparison watcher should turn off
#define	ICE9_E_SimhMemModel_NONE	      0x00000000	///< Enum Value: No memory model has been chosen
#define	ICE9_E_SimhMemModel_NORMAL	      0x00000001	///< Enum Value: Memory and I/O references stall only the CPU that initiated them
#define	ICE9_E_SimhMemModel_SERIAL	      0x00000002	///< Enum Value: Memory and I/O references are serialized across the whole machine
#define	ICE9_E_SimhMemModel_BACKDOOR	      0x00000003	///< Enum Value: Memory references take zero time
#define	ICE9_E_SimhMemModel_BACKDOOR_NBIO	      0x00000004	///< Enum Value: Memory references take zero time

#endif /*_ICE9_MAGIC_SPEC_SW_H_*/

/* DO NOT EDIT -- Generated automatically by vregs */
