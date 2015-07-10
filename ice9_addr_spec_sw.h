/* DO NOT EDIT -- Generated automatically by vregs */
/// \file
/// \brief Register Information: Generated automatically by vregs
///
/// See SystemC::Vregs::Rules file: chip_addr_spec__rules.pl
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

#ifndef _ICE9_ADDR_SPEC_SW_H_
#define _ICE9_ADDR_SPEC_SW_H_ 1

#include "ice9_vregs_defs.h"

/* ====================================================================== */
/* Enumerations */

#ifndef VREGS_ASSEMBLER

typedef enum {
	Ice9_AddrBusStop_COHO          = 0x0,	///< Coherence controller on odd side
	Ice9_AddrBusStop_DMA           = 0x1,	///< DMA controller
	Ice9_AddrBusStop_PS0           = 0x2,	///< L2 segment for processor 0
	Ice9_AddrBusStop_PS1           = 0x3,	///< L2 segment for processor 1
	Ice9_AddrBusStop_PS2           = 0x4,	///< L2 segment for processor 2
	Ice9_AddrBusStop_PS3           = 0x5,	///< L2 segment for processor 3
	Ice9_AddrBusStop_PS4           = 0x6,	///< L2 segment for processor 4
	Ice9_AddrBusStop_PS5           = 0x7,	///< L2 segment for processor 5
	Ice9_AddrBusStop_PCI           = 0x8,	///< Pci controller
	Ice9_AddrBusStop_COHE          = 0x9,	///< Coherence controller on even side
	Ice9_AddrBusStop_PS6           = 0xa,	///< L2 segment for processor 6
	Ice9_AddrBusStop_PS7           = 0xb,	///< L2 segment for processor 7
	Ice9_AddrBusStop_PS8           = 0xc,	///< L2 segment for processor 8
	Ice9_AddrBusStop_PS9           = 0xd,	///< L2 segment for processor 9
	Ice9_AddrBusStop_MAX           = 0x10	///< MAXIMUM+1
} Ice9_AddrBusStop;

typedef enum {
	Ice9_AddrProduct_ICE9          = 0x13,	///< Ice9a for SCX-1000 series
	Ice9_AddrProduct_ICE9_CPU0     = 0x14,	///< Ice9 EJTAG for CPU0
	Ice9_AddrProduct_ICE9_CPU1     = 0x15,	///< Ice9 EJTAG for CPU1
	Ice9_AddrProduct_ICE9_CPU2     = 0x16,	///< Ice9 EJTAG for CPU2
	Ice9_AddrProduct_ICE9_CPU3     = 0x17,	///< Ice9 EJTAG for CPU3
	Ice9_AddrProduct_ICE9_CPU4     = 0x18,	///< Ice9 EJTAG for CPU4
	Ice9_AddrProduct_ICE9_CPU5     = 0x19,	///< Ice9 EJTAG for CPU5
	Ice9_AddrProduct_ICE9B         = 0x1a,	///< Ice9b for SCX-1000 series
	Ice9_AddrProduct_ICE9B_CPU     = 0x1b,	///< Ice9b EJTAG part number for CPUs
	Ice9_AddrProduct_TWC9A         = 0x1e,	///< Twice9A
	Ice9_AddrProduct_TWC9A_CPU     = 0x1f,	///< Twice9A EJTAG part number for CPUs
	Ice9_AddrProduct_MAX           = 0x100	///< MAXIMUM+1
} Ice9_AddrProduct;

typedef enum {
	Ice9_AddrSubId_COHO          = 0x0,	///< Odd Coherence Controller
	Ice9_AddrSubId_DMA           = 0x1,	///< DMA Engine
	Ice9_AddrSubId_CPU0          = 0x2,	///< Processor 0
	Ice9_AddrSubId_CPU1          = 0x3,	///< Processor 1
	Ice9_AddrSubId_CPU2          = 0x4,	///< Processor 2
	Ice9_AddrSubId_CPU3          = 0x5,	///< Processor 3
	Ice9_AddrSubId_CPU4          = 0x6,	///< Processor 4
	Ice9_AddrSubId_CPU5          = 0x7,	///< Processor 5
	Ice9_AddrSubId_SCBM          = 0x8,	///< Serial Control Bus Master
	Ice9_AddrSubId_COHE          = 0x9,	///< Even Coherence Controller
	Ice9_AddrSubId_OCTBCOHE      = 0xa,	///< OCLA Collector block for COHE
	Ice9_AddrSubId_OCTBPS0       = 0xb,	///< OCLA Collector block for PS0
	Ice9_AddrSubId_OTRBCPS0      = 0xc,	///< OCLA Trigger block for PS0
	Ice9_AddrSubId_FLR0          = 0xd,	///< Fabric Link 0 Receive
	Ice9_AddrSubId_OTRBCPMI      = 0xf,	///< OCLA Trigger block for PMI/CSW Bus Stop
	Ice9_AddrSubId_WTIO          = 0x10,	///< Magic address range used internally by CSW WTIO transactions
	Ice9_AddrSubId_CAC0          = 0x12,	///< L2 Cache 0
	Ice9_AddrSubId_CAC1          = 0x13,	///< L2 Cache 1
	Ice9_AddrSubId_CAC2          = 0x14,	///< L2 Cache 2
	Ice9_AddrSubId_CAC3          = 0x15,	///< L2 Cache 3
	Ice9_AddrSubId_CAC4          = 0x16,	///< L2 Cache 4
	Ice9_AddrSubId_CAC5          = 0x17,	///< L2 Cache 5
	Ice9_AddrSubId_PCIE          = 0x18,	///< PCI-Express PMI internal registers
	Ice9_AddrSubId_OCTBCOHO      = 0x1a,	///< OCLA Collector block for COHO
	Ice9_AddrSubId_OCTBPS1       = 0x1b,	///< OCLA Collector block for PS1
	Ice9_AddrSubId_OTRBCPS1      = 0x1c,	///< OCLA Trigger block for PS1
	Ice9_AddrSubId_FLR1          = 0x1d,	///< Fabric Link 1 Receive
	Ice9_AddrSubId_CACLOC        = 0x1e,	///< L2 Local Cache
	Ice9_AddrSubId_OTRBVFSWO     = 0x1f,	///< OCLA Trigger block for FSW Vector Output
	Ice9_AddrSubId_SIM           = 0x20,	///< Magic address range for simulator control only
	Ice9_AddrSubId_CPU6          = 0x22,	///< Processor 6
	Ice9_AddrSubId_CPU7          = 0x23,	///< Processor 7
	Ice9_AddrSubId_CPU8          = 0x24,	///< Processor 8
	Ice9_AddrSubId_CPU9          = 0x25,	///< Processor 9
	Ice9_AddrSubId_I2C           = 0x28,	///< I2C Bus Controller
	Ice9_AddrSubId_OTRBCCOHE     = 0x2a,	///< OCLA Trigger block for COHE
	Ice9_AddrSubId_OCTBPS2       = 0x2b,	///< OCLA Collector block for PS2
	Ice9_AddrSubId_OTRBCPS2      = 0x2c,	///< OCLA Trigger block for PS2
	Ice9_AddrSubId_FLR2          = 0x2d,	///< Fabric Link 2 Receive
	Ice9_AddrSubId_INTR          = 0x2e,	///< Interrupt cycle
	Ice9_AddrSubId_OTRBVFSWI     = 0x2f,	///< OCLA Trigger block for FSW Vector Input
	Ice9_AddrSubId_CAC6          = 0x32,	///< L2 Cache 6
	Ice9_AddrSubId_CAC7          = 0x33,	///< L2 Cache 7
	Ice9_AddrSubId_CAC8          = 0x34,	///< L2 Cache 8
	Ice9_AddrSubId_CAC9          = 0x35,	///< L2 Cache 9
	Ice9_AddrSubId_UART          = 0x38,	///< UART
	Ice9_AddrSubId_OTRBCCOHO     = 0x3a,	///< OCLA Trigger block for COHO
	Ice9_AddrSubId_OCTBPS3       = 0x3b,	///< OCLA Collector block for PS3
	Ice9_AddrSubId_OTRBCPS3      = 0x3c,	///< OCLA Trigger block for PS3
	Ice9_AddrSubId_FLT0          = 0x3d,	///< Fabric Link 0 Transmit
	Ice9_AddrSubId_SPCL          = 0x3e,	///< Special cycle
	Ice9_AddrSubId_OTRBCFSW      = 0x3f,	///< OCLA Trigger block for FSW Codeword
	Ice9_AddrSubId_OCTBPS6       = 0x41,	///< OCLA Collector block for PS6
	Ice9_AddrSubId_DDR0          = 0x48,	///< SDRAM 0
	Ice9_AddrSubId_OTRBCPS6      = 0x49,	///< OCLA Trigger block for PS6
	Ice9_AddrSubId_OCTBFSWI      = 0x4a,	///< OCLA Collector block for FSW Inputs
	Ice9_AddrSubId_OCTBPS4       = 0x4b,	///< OCLA Collector block for PS4
	Ice9_AddrSubId_OTRBCPS4      = 0x4c,	///< OCLA Trigger block for PS4
	Ice9_AddrSubId_FLT1          = 0x4d,	///< Fabric Link 1 Transmit
	Ice9_AddrSubId_OTRBCPMII     = 0x4f,	///< OCLA Trigger block for PMI/BBS Internals
	Ice9_AddrSubId_OCTBPS7       = 0x51,	///< OCLA Collector block for PS7
	Ice9_AddrSubId_DDR1          = 0x58,	///< SDRAM 1
	Ice9_AddrSubId_OTRBCPS7      = 0x59,	///< OCLA Trigger block for PS7
	Ice9_AddrSubId_OCTBFSWO      = 0x5a,	///< OCLA Collector block for FSW Outputs
	Ice9_AddrSubId_OCTBPS5       = 0x5b,	///< OCLA Collector block for PS5
	Ice9_AddrSubId_OTRBCPS5      = 0x5c,	///< OCLA Trigger block for PS5
	Ice9_AddrSubId_FLT2          = 0x5d,	///< Fabric Link 2 Transmit
	Ice9_AddrSubId_OCTBPS8       = 0x61,	///< OCLA Collector block for PS8
	Ice9_AddrSubId_OCLA          = 0x68,	///< On-chip logic analyzer, common control block
	Ice9_AddrSubId_OTRBCPS8      = 0x69,	///< OCLA Trigger block for PS8
	Ice9_AddrSubId_OCTBDMA       = 0x6b,	///< OCLA Collector block for DMA
	Ice9_AddrSubId_OTRBCDMA      = 0x6c,	///< OCLA Trigger block for DMA Codeword
	Ice9_AddrSubId_QSC           = 0x6d,	///< Fabric Link Quad Controller
	Ice9_AddrSubId_OCTBPS9       = 0x71,	///< OCLA Collector block for PS9
	Ice9_AddrSubId_OTRBCPS9      = 0x79,	///< OCLA Trigger block for PS9
	Ice9_AddrSubId_OCTBPMI       = 0x7b,	///< OCLA Collector block for PMI/BBS
	Ice9_AddrSubId_OTRBVDMA      = 0x7c,	///< OCLA Trigger block for DMA Vector
	Ice9_AddrSubId_FSW           = 0x7d,	///< Fabric Switch
	Ice9_AddrSubId_MAX           = 0x80	///< MAXIMUM+1
} Ice9_AddrSubId;

typedef enum {
	Ice9_AddrTapMfgr_SICORTEX      = 0x2c2,	///< EJTAG Manufacturer ID for SiCortex
	Ice9_AddrTapMfgr_MAX           = 0x800	///< MAXIMUM+1
} Ice9_AddrTapMfgr;

#endif

/* ====================================================================== */
/* Register SHIFT/Masks */

#define ICE9_Io_Data_SHIFT		0	///< Internal Non-SCB Registers
#define ICE9_Io_Data_MASK		VREGS_ULL(0x00000000FFFFFFFF)
#define ICE9_IoScb_Data_SHIFT		0	///< Internal SCB Registers
#define ICE9_IoScb_Data_MASK		VREGS_ULL(0x00000000FFFFFFFF)
#define ICE9_Mem_Data_SHIFT		0	///< Main Memory
#define ICE9_Mem_Data_MASK		VREGS_ULL(0x00000000FFFFFFFF)
#define ICE9_PciConfig_Data_SHIFT	0	///< PCI-Express Config Space
#define ICE9_PciConfig_Data_MASK	VREGS_ULL(0x00000000FFFFFFFF)
#define ICE9_PciIo_Data_SHIFT		0	///< PCI-Express IO Space
#define ICE9_PciIo_Data_MASK		VREGS_ULL(0x00000000FFFFFFFF)
#define ICE9_PciMem_Data_SHIFT		0	///< PCI-Express Memory
#define ICE9_PciMem_Data_MASK		VREGS_ULL(0x00000000FFFFFFFF)

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

#define	ICE9_ADDR_BOOT			       VREGS_ULL(0x01fc00000)	///< Processor Boot Address
#define	ICE9_ADDR_BOOT1_PA		       VREGS_ULL(0x720000000)	///< Scratch space for boot1 phase
#define	ICE9_ADDR_BOOT1_VA		VREGS_ULL(0xa000000720000000)	///< Scratch space for boot1 phase
#define	ICE9_ADDR_BOOT2_PA		       VREGS_ULL(0x720010000)	///< Scratch space for boot2 phase
#define	ICE9_ADDR_BOOT2_VA		VREGS_ULL(0xa000000720010000)	///< Scratch space for boot2 phase
#define	ICE9_ADDR_EJTAG_BOOT_VA		VREGS_ULL(0xffffffffff200200)	///< EJtag Boot address
#define	ICE9_ADDR_EJTAG_FASTDATA_VA	VREGS_ULL(0xffffffffff200000)	///< EJtag Fastdata register
#define	ICE9_ADDR_IOBIT			      0x00000027	///< Memory/IO Bit
#define	ICE9_ADDR_PABITS		      0x00000028	///< Physical Address Bits


/* Automatic Defines */
#define	ICE9_RA_Mem			       VREGS_ULL(0x000000000)
#define	ICE9_RAE_Mem			       VREGS_ULL(0x800000000)
#define	ICE9_RAC_Mem			       VREGS_ULL(0x200000000)
#define	ICE9_RRS_Mem			       VREGS_ULL(0x800000000)
#define	ICE9_RAM_Mem			       VREGS_ULL(0x7FFFFFFFF)
#define	ICE9_RA_PciMem			       VREGS_ULL(0x800000000)
#define	ICE9_RAE_PciMem			       VREGS_ULL(0xC00000000)
#define	ICE9_RAC_PciMem			       VREGS_ULL(0x100000000)
#define	ICE9_RRS_PciMem			       VREGS_ULL(0x400000000)
#define	ICE9_RAM_PciMem			       VREGS_ULL(0x3FFFFFFFF)
#define	ICE9_RA_PciIo			       VREGS_ULL(0xC00000000)
#define	ICE9_RAE_PciIo			       VREGS_ULL(0xCF0000000)
#define	ICE9_RAC_PciIo			      0x3C000000
#define	ICE9_RRS_PciIo			       VREGS_ULL(0x0F0000000)
#define	ICE9_RAM_PciIo			     Not_Aligned	///< Address Mask: This register is not naturally aligned, so a mask will not work.
#define	ICE9_RA_PciConfig		       VREGS_ULL(0xCF0000000)
#define	ICE9_RAE_PciConfig		       VREGS_ULL(0xD00000000)
#define	ICE9_RAC_PciConfig		      0x04000000
#define	ICE9_RRS_PciConfig		       VREGS_ULL(0x010000000)
#define	ICE9_RAM_PciConfig		       VREGS_ULL(0x00FFFFFFF)
#define	ICE9_RA_IoScb			       VREGS_ULL(0xE00000000)
#define	ICE9_RAE_IoScb			       VREGS_ULL(0xE80000000)
#define	ICE9_RAC_IoScb			      0x20000000
#define	ICE9_RRS_IoScb			       VREGS_ULL(0x080000000)
#define	ICE9_RAM_IoScb			       VREGS_ULL(0x07FFFFFFF)
#define	ICE9_RA_Io			       VREGS_ULL(0xE80000000)
#define	ICE9_RAE_Io			       VREGS_ULL(0xF00000000)
#define	ICE9_RAC_Io			      0x20000000
#define	ICE9_RRS_Io			       VREGS_ULL(0x080000000)
#define	ICE9_RAM_Io			       VREGS_ULL(0x07FFFFFFF)
#define	ICE9_RBASEA_Pci			       VREGS_ULL(0x800000000)
#define	ICE9_RBASEAE_Pci		       VREGS_ULL(0xD00000000)
#define	ICE9_RBASEAM_Pci		       VREGS_ULL(0x7FFFFFFFF)
#define	ICE9_CB_Io_Data				       0	///< Field Start Bit:  31:0   RW   X: Internal Non-SCB Registers
#define	ICE9_CE_Io_Data				      31
#define	ICE9_CB_IoScb_Data			       0	///< Field Start Bit:  31:0   RW   X: Internal SCB Registers
#define	ICE9_CE_IoScb_Data			      31
#define	ICE9_CB_Mem_Data			       0	///< Field Start Bit:  31:0   RW   X: Main Memory
#define	ICE9_CE_Mem_Data			      31
#define	ICE9_CB_PciConfig_Data			       0	///< Field Start Bit:  31:0   RW   X: PCI-Express Config Space
#define	ICE9_CE_PciConfig_Data			      31
#define	ICE9_CB_PciIo_Data			       0	///< Field Start Bit:  31:0   RW   X: PCI-Express IO Space
#define	ICE9_CE_PciIo_Data			      31
#define	ICE9_CB_PciMem_Data			       0	///< Field Start Bit:  31:0   RW   X: PCI-Express Memory
#define	ICE9_CE_PciMem_Data			      31
#define	ICE9_E_AddrBusStop_COHO			     0x0	///< Enum Value: Coherence controller on odd side
#define	ICE9_E_AddrBusStop_DMA			     0x1	///< Enum Value: DMA controller
#define	ICE9_E_AddrBusStop_PS0			     0x2	///< Enum Value: L2 segment for processor 0
#define	ICE9_E_AddrBusStop_PS1			     0x3	///< Enum Value: L2 segment for processor 1
#define	ICE9_E_AddrBusStop_PS2			     0x4	///< Enum Value: L2 segment for processor 2
#define	ICE9_E_AddrBusStop_PS3			     0x5	///< Enum Value: L2 segment for processor 3
#define	ICE9_E_AddrBusStop_PS4			     0x6	///< Enum Value: L2 segment for processor 4
#define	ICE9_E_AddrBusStop_PS5			     0x7	///< Enum Value: L2 segment for processor 5
#define	ICE9_E_AddrBusStop_PCI			     0x8	///< Enum Value: Pci controller
#define	ICE9_E_AddrBusStop_COHE			     0x9	///< Enum Value: Coherence controller on even side
#define	ICE9_E_AddrBusStop_PS6			     0xa	///< Enum Value: L2 segment for processor 6
#define	ICE9_E_AddrBusStop_PS7			     0xb	///< Enum Value: L2 segment for processor 7
#define	ICE9_E_AddrBusStop_PS8			     0xc	///< Enum Value: L2 segment for processor 8
#define	ICE9_E_AddrBusStop_PS9			     0xd	///< Enum Value: L2 segment for processor 9
#define	ICE9_E_AddrProduct_ICE9			    0x13	///< Enum Value: Ice9a for SCX-1000 series
#define	ICE9_E_AddrProduct_ICE9_CPU0		    0x14	///< Enum Value: Ice9 EJTAG for CPU0
#define	ICE9_E_AddrProduct_ICE9_CPU1		    0x15	///< Enum Value: Ice9 EJTAG for CPU1
#define	ICE9_E_AddrProduct_ICE9_CPU2		    0x16	///< Enum Value: Ice9 EJTAG for CPU2
#define	ICE9_E_AddrProduct_ICE9_CPU3		    0x17	///< Enum Value: Ice9 EJTAG for CPU3
#define	ICE9_E_AddrProduct_ICE9_CPU4		    0x18	///< Enum Value: Ice9 EJTAG for CPU4
#define	ICE9_E_AddrProduct_ICE9_CPU5		    0x19	///< Enum Value: Ice9 EJTAG for CPU5
#define	ICE9_E_AddrProduct_ICE9B		    0x1a	///< Enum Value: Ice9b for SCX-1000 series
#define	ICE9_E_AddrProduct_ICE9B_CPU		    0x1b	///< Enum Value: Ice9b EJTAG part number for CPUs
#define	ICE9_E_AddrProduct_TWC9A		    0x1e	///< Enum Value: Twice9A
#define	ICE9_E_AddrProduct_TWC9A_CPU		    0x1f	///< Enum Value: Twice9A EJTAG part number for CPUs
#define	ICE9_E_AddrSubId_COHO			    0x00	///< Enum Value: Odd Coherence Controller
#define	ICE9_E_AddrSubId_DMA			    0x01	///< Enum Value: DMA Engine
#define	ICE9_E_AddrSubId_CPU0			    0x02	///< Enum Value: Processor 0
#define	ICE9_E_AddrSubId_CPU1			    0x03	///< Enum Value: Processor 1
#define	ICE9_E_AddrSubId_CPU2			    0x04	///< Enum Value: Processor 2
#define	ICE9_E_AddrSubId_CPU3			    0x05	///< Enum Value: Processor 3
#define	ICE9_E_AddrSubId_CPU4			    0x06	///< Enum Value: Processor 4
#define	ICE9_E_AddrSubId_CPU5			    0x07	///< Enum Value: Processor 5
#define	ICE9_E_AddrSubId_SCBM			    0x08	///< Enum Value: Serial Control Bus Master
#define	ICE9_E_AddrSubId_COHE			    0x09	///< Enum Value: Even Coherence Controller
#define	ICE9_E_AddrSubId_OCTBCOHE		    0x0a	///< Enum Value: OCLA Collector block for COHE
#define	ICE9_E_AddrSubId_OCTBPS0		    0x0b	///< Enum Value: OCLA Collector block for PS0
#define	ICE9_E_AddrSubId_OTRBCPS0		    0x0c	///< Enum Value: OCLA Trigger block for PS0
#define	ICE9_E_AddrSubId_FLR0			    0x0d	///< Enum Value: Fabric Link 0 Receive
#define	ICE9_E_AddrSubId_OTRBCPMI		    0x0f	///< Enum Value: OCLA Trigger block for PMI/CSW Bus Stop
#define	ICE9_E_AddrSubId_WTIO			    0x10	///< Enum Value: Magic address range used internally by CSW WTIO transactions
#define	ICE9_E_AddrSubId_CAC0			    0x12	///< Enum Value: L2 Cache 0
#define	ICE9_E_AddrSubId_CAC1			    0x13	///< Enum Value: L2 Cache 1
#define	ICE9_E_AddrSubId_CAC2			    0x14	///< Enum Value: L2 Cache 2
#define	ICE9_E_AddrSubId_CAC3			    0x15	///< Enum Value: L2 Cache 3
#define	ICE9_E_AddrSubId_CAC4			    0x16	///< Enum Value: L2 Cache 4
#define	ICE9_E_AddrSubId_CAC5			    0x17	///< Enum Value: L2 Cache 5
#define	ICE9_E_AddrSubId_PCIE			    0x18	///< Enum Value: PCI-Express PMI internal registers
#define	ICE9_E_AddrSubId_OCTBCOHO		    0x1a	///< Enum Value: OCLA Collector block for COHO
#define	ICE9_E_AddrSubId_OCTBPS1		    0x1b	///< Enum Value: OCLA Collector block for PS1
#define	ICE9_E_AddrSubId_OTRBCPS1		    0x1c	///< Enum Value: OCLA Trigger block for PS1
#define	ICE9_E_AddrSubId_FLR1			    0x1d	///< Enum Value: Fabric Link 1 Receive
#define	ICE9_E_AddrSubId_CACLOC			    0x1e	///< Enum Value: L2 Local Cache
#define	ICE9_E_AddrSubId_OTRBVFSWO		    0x1f	///< Enum Value: OCLA Trigger block for FSW Vector Output
#define	ICE9_E_AddrSubId_SIM			    0x20	///< Enum Value: Magic address range for simulator control only
#define	ICE9_E_AddrSubId_CPU6			    0x22	///< Enum Value: Processor 6
#define	ICE9_E_AddrSubId_CPU7			    0x23	///< Enum Value: Processor 7
#define	ICE9_E_AddrSubId_CPU8			    0x24	///< Enum Value: Processor 8
#define	ICE9_E_AddrSubId_CPU9			    0x25	///< Enum Value: Processor 9
#define	ICE9_E_AddrSubId_I2C			    0x28	///< Enum Value: I2C Bus Controller
#define	ICE9_E_AddrSubId_OTRBCCOHE		    0x2a	///< Enum Value: OCLA Trigger block for COHE
#define	ICE9_E_AddrSubId_OCTBPS2		    0x2b	///< Enum Value: OCLA Collector block for PS2
#define	ICE9_E_AddrSubId_OTRBCPS2		    0x2c	///< Enum Value: OCLA Trigger block for PS2
#define	ICE9_E_AddrSubId_FLR2			    0x2d	///< Enum Value: Fabric Link 2 Receive
#define	ICE9_E_AddrSubId_INTR			    0x2e	///< Enum Value: Interrupt cycle
#define	ICE9_E_AddrSubId_OTRBVFSWI		    0x2f	///< Enum Value: OCLA Trigger block for FSW Vector Input
#define	ICE9_E_AddrSubId_CAC6			    0x32	///< Enum Value: L2 Cache 6
#define	ICE9_E_AddrSubId_CAC7			    0x33	///< Enum Value: L2 Cache 7
#define	ICE9_E_AddrSubId_CAC8			    0x34	///< Enum Value: L2 Cache 8
#define	ICE9_E_AddrSubId_CAC9			    0x35	///< Enum Value: L2 Cache 9
#define	ICE9_E_AddrSubId_UART			    0x38	///< Enum Value: UART
#define	ICE9_E_AddrSubId_OTRBCCOHO		    0x3a	///< Enum Value: OCLA Trigger block for COHO
#define	ICE9_E_AddrSubId_OCTBPS3		    0x3b	///< Enum Value: OCLA Collector block for PS3
#define	ICE9_E_AddrSubId_OTRBCPS3		    0x3c	///< Enum Value: OCLA Trigger block for PS3
#define	ICE9_E_AddrSubId_FLT0			    0x3d	///< Enum Value: Fabric Link 0 Transmit
#define	ICE9_E_AddrSubId_SPCL			    0x3e	///< Enum Value: Special cycle
#define	ICE9_E_AddrSubId_OTRBCFSW		    0x3f	///< Enum Value: OCLA Trigger block for FSW Codeword
#define	ICE9_E_AddrSubId_OCTBPS6		    0x41	///< Enum Value: OCLA Collector block for PS6
#define	ICE9_E_AddrSubId_DDR0			    0x48	///< Enum Value: SDRAM 0
#define	ICE9_E_AddrSubId_OTRBCPS6		    0x49	///< Enum Value: OCLA Trigger block for PS6
#define	ICE9_E_AddrSubId_OCTBFSWI		    0x4a	///< Enum Value: OCLA Collector block for FSW Inputs
#define	ICE9_E_AddrSubId_OCTBPS4		    0x4b	///< Enum Value: OCLA Collector block for PS4
#define	ICE9_E_AddrSubId_OTRBCPS4		    0x4c	///< Enum Value: OCLA Trigger block for PS4
#define	ICE9_E_AddrSubId_FLT1			    0x4d	///< Enum Value: Fabric Link 1 Transmit
#define	ICE9_E_AddrSubId_OTRBCPMII		    0x4f	///< Enum Value: OCLA Trigger block for PMI/BBS Internals
#define	ICE9_E_AddrSubId_OCTBPS7		    0x51	///< Enum Value: OCLA Collector block for PS7
#define	ICE9_E_AddrSubId_DDR1			    0x58	///< Enum Value: SDRAM 1
#define	ICE9_E_AddrSubId_OTRBCPS7		    0x59	///< Enum Value: OCLA Trigger block for PS7
#define	ICE9_E_AddrSubId_OCTBFSWO		    0x5a	///< Enum Value: OCLA Collector block for FSW Outputs
#define	ICE9_E_AddrSubId_OCTBPS5		    0x5b	///< Enum Value: OCLA Collector block for PS5
#define	ICE9_E_AddrSubId_OTRBCPS5		    0x5c	///< Enum Value: OCLA Trigger block for PS5
#define	ICE9_E_AddrSubId_FLT2			    0x5d	///< Enum Value: Fabric Link 2 Transmit
#define	ICE9_E_AddrSubId_OCTBPS8		    0x61	///< Enum Value: OCLA Collector block for PS8
#define	ICE9_E_AddrSubId_OCLA			    0x68	///< Enum Value: On-chip logic analyzer, common control block
#define	ICE9_E_AddrSubId_OTRBCPS8		    0x69	///< Enum Value: OCLA Trigger block for PS8
#define	ICE9_E_AddrSubId_OCTBDMA		    0x6b	///< Enum Value: OCLA Collector block for DMA
#define	ICE9_E_AddrSubId_OTRBCDMA		    0x6c	///< Enum Value: OCLA Trigger block for DMA Codeword
#define	ICE9_E_AddrSubId_QSC			    0x6d	///< Enum Value: Fabric Link Quad Controller
#define	ICE9_E_AddrSubId_OCTBPS9		    0x71	///< Enum Value: OCLA Collector block for PS9
#define	ICE9_E_AddrSubId_OTRBCPS9		    0x79	///< Enum Value: OCLA Trigger block for PS9
#define	ICE9_E_AddrSubId_OCTBPMI		    0x7b	///< Enum Value: OCLA Collector block for PMI/BBS
#define	ICE9_E_AddrSubId_OTRBVDMA		    0x7c	///< Enum Value: OCLA Trigger block for DMA Vector
#define	ICE9_E_AddrSubId_FSW			    0x7d	///< Enum Value: Fabric Switch
#define	ICE9_E_AddrTapMfgr_SICORTEX		   0x2c2	///< Enum Value: EJTAG Manufacturer ID for SiCortex

#endif /*_ICE9_ADDR_SPEC_SW_H_*/

/* DO NOT EDIT -- Generated automatically by vregs */
