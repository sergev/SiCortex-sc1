/* LAN_LAN cut-through ethernet */

#define PMI_BASE       (ICE9_RA_Io)
#define PMI_END        (ICE9_RAE_Io-1)
#define PMI_SIZE       (PMI_END-PMI_BASE)

#define PCIE_BASE       (ICE9_RBASEA_Pci)
#define PCIE_END        (ICE9_RBASEAE_Pci-1)
#define PCIE_SIZE       (PCIE_END-PCIE_BASE)

#define PCIE_MEM_BASE   (ICE9_RA_PciMem)
#define PCIE_MEM_END    (ICE9_RAE_PciMem-1)
#define PCIE_MEM_SIZE   (PCIE_MEM_END-PCIE_MEM_BASE)

#define PCIE_IO_BASE   (ICE9_RA_PciIo)
#define PCIE_IO_END    (ICE9_RAE_PciIo-1)
#define PCIE_IO_SIZE   (PCIE_IO_END-PCIE_IO_BASE)

#define PCIE_CFG_BASE   (ICE9_RA_PciConfig)
#define PCIE_CFG_END    (ICE9_RAE_PciConfig-1)
#define PCIE_CFG_SIZE   (PCIE_CFG_END-PCIE_CFG_BASE)

#define ADDR_IS_PCIE_(x)  ((((uint32) (x)) >= PCIE_BASE) && \
                         (((uint32) (x)) < (PCIE_BASE + PCIE_SIZE)))
#define PA_IS_PCIE_(x)     (((x) >= PCIE_BASE) && ((x) < (PCIE_BASE + PCIE_SIZE)))
