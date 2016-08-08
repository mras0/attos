#include "i825x.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>
#include "isr.h"

namespace attos { namespace net { namespace i825x {

enum class reg {
    CTRL         = 0x00000, /* Device Control - RW */
    STATUS       = 0x00008, /* Device Status - RO */
    EECD         = 0x00010, /* EEPROM/Flash Control - RW */
    EERD         = 0x00014, /* EEPROM Read - RW */
    CTRL_EXT     = 0x00018, /* Extended Device Control - RW */
    FLA          = 0x0001C, /* Flash Access - RW */
    MDIC         = 0x00020, /* MDI Control - RW */
    SCTL         = 0x00024, /* SerDes Control - RW */
    FCAL         = 0x00028, /* Flow Control Address Low - RW */
    FCAH         = 0x0002C, /* Flow Control Address High -RW */
    FEXTNVM4     = 0x00024, /* Future Extended NVM 4 - RW */
    FEXTNVM      = 0x00028, /* Future Extended NVM - RW */
    FCT          = 0x00030, /* Flow Control Type - RW */
    VET          = 0x00038, /* VLAN Ether Type - RW */
    FEXTNVM3     = 0x0003C, /* Future Extended NVM 3 - RW */
    ICR          = 0x000C0, /* Interrupt Cause Read - R/clr */
    ITR          = 0x000C4, /* Interrupt Throttling Rate - RW */
    ICS          = 0x000C8, /* Interrupt Cause Set - WO */
    IMS          = 0x000D0, /* Interrupt Mask Set - RW */
    IMC          = 0x000D8, /* Interrupt Mask Clear - WO */
    IAM          = 0x000E0, /* Interrupt Acknowledge Auto Mask */
    IVAR         = 0x000E4, /* Interrupt Vector Allocation - RW */
    RCTL         = 0x00100, /* Rx Control - RW */
    FCTTV        = 0x00170, /* Flow Control Transmit Timer Value - RW */
    TXCW         = 0x00178, /* Tx Configuration Word - RW */
    RXCW         = 0x00180, /* Rx Configuration Word - RO */
    TCTL         = 0x00400, /* Tx Control - RW */
    TCTL_EXT     = 0x00404, /* Extended Tx Control - RW */
    TIPG         = 0x00410, /* Tx Inter-packet gap -RW */
    AIT          = 0x00458, /* Adaptive Interframe Spacing Throttle -RW */
    LEDCTL       = 0x00E00, /* LED Control - RW */
    EXTCNF_CTRL  = 0x00F00, /* Extended Configuration Control */
    EXTCNF_SIZE  = 0x00F08, /* Extended Configuration Size */
    PHY_CTRL     = 0x00F10, /* PHY Control Register in CSR */
    PBA          = 0x01000, /* Packet Buffer Allocation - RW */
    PBS          = 0x01008, /* Packet Buffer Size */
    EEMNGCTL     = 0x01010, /* MNG EEprom Control */
    EEWR         = 0x0102C, /* EEPROM Write Register - RW */
    FLOP         = 0x0103C, /* FLASH Opcode Register */
    PBA_ECC      = 0x01100, /* PBA ECC Register */
    ERT          = 0x02008, /* Early Rx Threshold - RW */
    FCRTL        = 0x02160, /* Flow Control Receive Threshold Low - RW */
    FCRTH        = 0x02168, /* Flow Control Receive Threshold High - RW */
    PSRCTL       = 0x02170, /* Packet Split Receive Control - RW */
    RDBAL0       = 0x02800, /* Rx Descriptor Base Address Low - RW */
    RDBAH0       = 0x02804, /* Rx Descriptor Base Address High - RW */
    RDLEN0       = 0x02808, /* Rx Descriptor Length - RW */
    RDH0         = 0x02810, /* Rx Descriptor Head - RW */
    RDT0         = 0x02818, /* Rx Descriptor Tail - RW */
    RDTR         = 0x02820, /* Rx Delay Timer - RW */
    RXDCTL0      = 0x02828, /* Rx Descriptor Control - RW */
    RADV         = 0x0282C, /* Rx Interrupt Absolute Delay Timer - RW */
    KABGTXD      = 0x03004, /* AFE Band Gap Transmit Ref Data */
    TDBAL0       = 0x03800, /* Tx Descriptor Base Address Low - RW */
    TDBAH0       = 0x03804, /* Tx Descriptor Base Address High - RW */
    TDLEN0       = 0x03808, /* Tx Descriptor Length - RW */
    TDH0         = 0x03810, /* Tx Descriptor Head - RW */
    TDT0         = 0x03818, /* Tx Descriptor Tail - RW */
    TIDV         = 0x03820, /* Tx Interrupt Delay Value - RW */
    TXDCTL0      = 0x03828, /* Tx Descriptor Control - RW */
    TADV         = 0x0382C, /* Tx Interrupt Absolute Delay Val - RW */
    TARC0        = 0x03840, /* Tx Arbitration Count (0) */
    CRCERRS      = 0x04000, /* CRC Error Count - R/clr */
    ALGNERRC     = 0x04004, /* Alignment Error Count - R/clr */
    SYMERRS      = 0x04008, /* Symbol Error Count - R/clr */
    RXERRC       = 0x0400C, /* Receive Error Count - R/clr */
    MPC          = 0x04010, /* Missed Packet Count - R/clr */
    SCC          = 0x04014, /* Single Collision Count - R/clr */
    ECOL         = 0x04018, /* Excessive Collision Count - R/clr */
    MCC          = 0x0401C, /* Multiple Collision Count - R/clr */
    LATECOL      = 0x04020, /* Late Collision Count - R/clr */
    COLC         = 0x04028, /* Collision Count - R/clr */
    DC           = 0x04030, /* Defer Count - R/clr */
    TNCRS        = 0x04034, /* Tx-No CRS - R/clr */
    SEC          = 0x04038, /* Sequence Error Count - R/clr */
    CEXTERR      = 0x0403C, /* Carrier Extension Error Count - R/clr */
    RLEC         = 0x04040, /* Receive Length Error Count - R/clr */
    XONRXC       = 0x04048, /* XON Rx Count - R/clr */
    XONTXC       = 0x0404C, /* XON Tx Count - R/clr */
    XOFFRXC      = 0x04050, /* XOFF Rx Count - R/clr */
    XOFFTXC      = 0x04054, /* XOFF Tx Count - R/clr */
    FCRUC        = 0x04058, /* Flow Control Rx Unsupported Count- R/clr */
    PRC64        = 0x0405C, /* Packets Rx (64 bytes) - R/clr */
    PRC127       = 0x04060, /* Packets Rx (65-127 bytes) - R/clr */
    PRC255       = 0x04064, /* Packets Rx (128-255 bytes) - R/clr */
    PRC511       = 0x04068, /* Packets Rx (255-511 bytes) - R/clr */
    PRC1023      = 0x0406C, /* Packets Rx (512-1023 bytes) - R/clr */
    PRC1522      = 0x04070, /* Packets Rx (1024-1522 bytes) - R/clr */
    GPRC         = 0x04074, /* Good Packets Rx Count - R/clr */
    BPRC         = 0x04078, /* Broadcast Packets Rx Count - R/clr */
    MPRC         = 0x0407C, /* Multicast Packets Rx Count - R/clr */
    GPTC         = 0x04080, /* Good Packets Tx Count - R/clr */
    GORCL        = 0x04088, /* Good Octets Rx Count Low - R/clr */
    GORCH        = 0x0408C, /* Good Octets Rx Count High - R/clr */
    GOTCL        = 0x04090, /* Good Octets Tx Count Low - R/clr */
    GOTCH        = 0x04094, /* Good Octets Tx Count High - R/clr */
    RNBC         = 0x040A0, /* Rx No Buffers Count - R/clr */
    RUC          = 0x040A4, /* Rx Undersize Count - R/clr */
    RFC          = 0x040A8, /* Rx Fragment Count - R/clr */
    ROC          = 0x040AC, /* Rx Oversize Count - R/clr */
    RJC          = 0x040B0, /* Rx Jabber Count - R/clr */
    MGTPRC       = 0x040B4, /* Management Packets Rx Count - R/clr */
    MGTPDC       = 0x040B8, /* Management Packets Dropped Count - R/clr */
    MGTPTC       = 0x040BC, /* Management Packets Tx Count - R/clr */
    TORL         = 0x040C0, /* Total Octets Rx Low - R/clr */
    TORH         = 0x040C4, /* Total Octets Rx High - R/clr */
    TOTL         = 0x040C8, /* Total Octets Tx Low - R/clr */
    TOTH         = 0x040CC, /* Total Octets Tx High - R/clr */
    TPR          = 0x040D0, /* Total Packets Rx - R/clr */
    TPT          = 0x040D4, /* Total Packets Tx - R/clr */
    PTC64        = 0x040D8, /* Packets Tx (64 bytes) - R/clr */
    PTC127       = 0x040DC, /* Packets Tx (65-127 bytes) - R/clr */
    PTC255       = 0x040E0, /* Packets Tx (128-255 bytes) - R/clr */
    PTC511       = 0x040E4, /* Packets Tx (256-511 bytes) - R/clr */
    PTC1023      = 0x040E8, /* Packets Tx (512-1023 bytes) - R/clr */
    PTC1522      = 0x040EC, /* Packets Tx (1024-1522 Bytes) - R/clr */
    MPTC         = 0x040F0, /* Multicast Packets Tx Count - R/clr */
    BPTC         = 0x040F4, /* Broadcast Packets Tx Count - R/clr */
    TSCTC        = 0x040F8, /* TCP Segmentation Context Tx - R/clr */
    TSCTFC       = 0x040FC, /* TCP Segmentation Context Tx Fail - R/clr */
    IAC          = 0x04100, /* Interrupt Assertion Count */
    ICRXPTC      = 0x04104, /* Irq Cause Rx Packet Timer Expire Count */
    ICRXATC      = 0x04108, /* Irq Cause Rx Abs Timer Expire Count */
    ICTXPTC      = 0x0410C, /* Irq Cause Tx Packet Timer Expire Count */
    ICTXATC      = 0x04110, /* Irq Cause Tx Abs Timer Expire Count */
    ICTXQEC      = 0x04118, /* Irq Cause Tx Queue Empty Count */
    ICTXQMTC     = 0x0411C, /* Irq Cause Tx Queue MinThreshold Count */
    ICRXDMTC     = 0x04120, /* Irq Cause Rx Desc MinThreshold Count */
    ICRXOC       = 0x04124, /* Irq Cause Receiver Overrun Count */
    RXCSUM       = 0x05000, /* Rx Checksum Control - RW */
    RFCTL        = 0x05008, /* Receive Filter Control */
    MTA          = 0x05200, /* Multicast Table Array - RW Array */
    RAL0         = 0x05400, /* Receive Address Low - RW */
    RAH0         = 0x05404, /* Receive Address High - RW */
};

constexpr uint32_t CTRL_FD                 = 0x00000001; /* Full duplex.0=half; 1=full */
constexpr uint32_t CTRL_GIO_MASTER_DISABLE = 0x00000004; /* Blocks new Master requests */
constexpr uint32_t CTRL_LRST               = 0x00000008; /* Link reset. 0=normal,1=reset */
constexpr uint32_t CTRL_ASDE               = 0x00000020; /* Auto-speed detect enable */
constexpr uint32_t CTRL_SLU                = 0x00000040; /* Set link up (Force Link) */
constexpr uint32_t CTRL_ILOS               = 0x00000080; /* Invert Loss-Of Signal */
constexpr uint32_t CTRL_SPD_SEL            = 0x00000300; /* Speed Select Mask */
constexpr uint32_t CTRL_SPD_10             = 0x00000000; /* Force 10Mb */
constexpr uint32_t CTRL_SPD_100            = 0x00000100; /* Force 100Mb */
constexpr uint32_t CTRL_SPD_1000           = 0x00000200; /* Force 1Gb */
constexpr uint32_t CTRL_FRCSPD             = 0x00000800; /* Force Speed */
constexpr uint32_t CTRL_FRCDPX             = 0x00001000; /* Force Duplex */
constexpr uint32_t CTRL_LANPHYPC_OVERRIDE  = 0x00010000; /* SW control of LANPHYPC */
constexpr uint32_t CTRL_LANPHYPC_VALUE     = 0x00020000; /* SW value of LANPHYPC */
constexpr uint32_t CTRL_SWDPIN0            = 0x00040000; /* SWDPIN 0 value */
constexpr uint32_t CTRL_SWDPIN1            = 0x00080000; /* SWDPIN 1 value */
constexpr uint32_t CTRL_SWDPIO0            = 0x00400000; /* SWDPIN 0 Input or output */
constexpr uint32_t CTRL_RST                = 0x04000000; /* Global reset */
constexpr uint32_t CTRL_RFCE               = 0x08000000; /* Receive Flow Control enable */
constexpr uint32_t CTRL_TFCE               = 0x10000000; /* Transmit flow control enable */
constexpr uint32_t CTRL_VME                = 0x40000000; /* IEEE VLAN mode enable */
constexpr uint32_t CTRL_PHY_RST            = 0x80000000; /* PHY Reset */

/* Device Status */
constexpr uint32_t STATUS_FD                = 0x00000001; /* Full duplex.0=half,1=full */
constexpr uint32_t STATUS_LU                = 0x00000002; /* Link up.0=no,1=link */
constexpr uint32_t STATUS_FUNC_MASK         = 0x0000000C; /* PCI Function Mask */
constexpr uint32_t STATUS_FUNC_SHIFT        = 2;
constexpr uint32_t STATUS_FUNC_1            = 0x00000004; /* Function 1 */
constexpr uint32_t STATUS_TXOFF             = 0x00000010; /* transmission paused */
constexpr uint32_t STATUS_SPEED_10          = 0x00000000; /* Speed 10Mb/s */
constexpr uint32_t STATUS_SPEED_100         = 0x00000040; /* Speed 100Mb/s */
constexpr uint32_t STATUS_SPEED_1000        = 0x00000080; /* Speed 1000Mb/s */
constexpr uint32_t STATUS_LAN_INIT_DONE     = 0x00000200; /* Lan Init Completion by NVM */
constexpr uint32_t STATUS_PHYRA             = 0x00000400; /* PHY Reset Asserted */
constexpr uint32_t STATUS_GIO_MASTER_ENABLE = 0x00080000; /* Status of Master requests. */

constexpr uint32_t RAH_AV                  = 0x80000000; /* Receive descriptor valid */

constexpr uint32_t RCTL_EN                 = 0x00000002;   /* enable */
constexpr uint32_t RCTL_SBP                = 0x00000004;   /* store bad packet */
constexpr uint32_t RCTL_UPE                = 0x00000008;   /* unicast promiscuous enable */
constexpr uint32_t RCTL_MPE                = 0x00000010;   /* multicast promiscuous enab */
constexpr uint32_t RCTL_LPE                = 0x00000020;   /* long packet enable */
constexpr uint32_t RCTL_LBM_NO             = 0x00000000;   /* no loopback mode */
constexpr uint32_t RCTL_LBM_MAC            = 0x00000040;   /* MAC loopback mode */
constexpr uint32_t RCTL_LBM_TCVR           = 0x000000C0;   /* tcvr loopback mode */
constexpr uint32_t RCTL_DTYP_PS            = 0x00000400;   /* Packet Split descriptor */
constexpr uint32_t RCTL_RDMTS_HALF         = 0x00000000;   /* Rx desc min threshold size */
constexpr uint32_t RCTL_MO_SHIFT           = 12        ;   /* multicast offset shift */
constexpr uint32_t RCTL_MO_3               = 0x00003000;   /* multicast offset 15:4 */
constexpr uint32_t RCTL_BAM                = 0x00008000;   /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
constexpr uint32_t RCTL_SZ_2048            = 0x00000000;   /* Rx buffer size 2048 */
constexpr uint32_t RCTL_SZ_1024            = 0x00010000;   /* Rx buffer size 1024 */
constexpr uint32_t RCTL_SZ_512             = 0x00020000;   /* Rx buffer size 512 */
constexpr uint32_t RCTL_SZ_256             = 0x00030000;   /* Rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
constexpr uint32_t RCTL_SZ_16384           = 0x00010000;   /* Rx buffer size 16384 */
constexpr uint32_t RCTL_SZ_8192            = 0x00020000;   /* Rx buffer size 8192 */
constexpr uint32_t RCTL_SZ_4096            = 0x00030000;   /* Rx buffer size 4096 */
constexpr uint32_t RCTL_VFE                = 0x00040000;   /* vlan filter enable */
constexpr uint32_t RCTL_CFIEN              = 0x00080000;   /* canonical form enable */
constexpr uint32_t RCTL_CFI                = 0x00100000;   /* canonical form indicator */
constexpr uint32_t RCTL_DPF                = 0x00400000;   /* Discard Pause Frames */
constexpr uint32_t RCTL_PMCF               = 0x00800000;   /* pass MAC control frames */
constexpr uint32_t RCTL_BSEX               = 0x02000000;   /* Buffer size extension */
constexpr uint32_t RCTL_SECRC              = 0x04000000;   /* Strip Ethernet CRC */

/* Receive Descriptor bit definitions */
constexpr uint32_t RXD_STAT_DD             = 0x01;         /* Descriptor Done */
constexpr uint32_t RXD_STAT_EOP            = 0x02;         /* End of Packet */
constexpr uint32_t RXD_STAT_IXSM           = 0x04;         /* Ignore checksum */
constexpr uint32_t RXD_STAT_VP             = 0x08;         /* IEEE VLAN Packet */
constexpr uint32_t RXD_STAT_UDPCS          = 0x10;         /* UDP xsum calculated */
constexpr uint32_t RXD_STAT_TCPCS          = 0x20;         /* TCP xsum calculated */
constexpr uint32_t RXD_ERR_CE              = 0x01;         /* CRC Error */
constexpr uint32_t RXD_ERR_SE              = 0x02;         /* Symbol Error */
constexpr uint32_t RXD_ERR_SEQ             = 0x04;         /* Sequence Error */
constexpr uint32_t RXD_ERR_CXE             = 0x10;         /* Carrier Extension Error */
constexpr uint32_t RXD_ERR_TCPE            = 0x20;         /* TCP/UDP Checksum Error */
constexpr uint32_t RXD_ERR_IPE             = 0x40;         /* IP Checksum Error */
constexpr uint32_t RXD_ERR_RXE             = 0x80;         /* Rx Data Error */

/* Transmit Control */
constexpr uint32_t TCTL_EN                 = 0x00000002;   /* enable Tx */
constexpr uint32_t TCTL_PSP                = 0x00000008;   /* pad short packets */
constexpr uint32_t TCTL_CT                 = 0x00000ff0;   /* collision threshold */
constexpr uint32_t TCTL_COLD               = 0x003ff000;   /* collision distance */
constexpr uint32_t TCTL_RTLC               = 0x01000000;   /* Re-transmit on late collision */
constexpr uint32_t TCTL_MULR               = 0x10000000;   /* Multiple request support */

/* Transmit Descriptor Control */
constexpr uint32_t TXDCTL_PTHRESH              = 0x0000003F; /* TXDCTL Prefetch Threshold */
constexpr uint32_t TXDCTL_HTHRESH              = 0x00003F00; /* TXDCTL Host Threshold */
constexpr uint32_t TXDCTL_WTHRESH              = 0x003F0000; /* TXDCTL Writeback Threshold */
constexpr uint32_t TXDCTL_GRAN                 = 0x01000000; /* TXDCTL Granularity */
constexpr uint32_t TXDCTL_FULL_TX_DESC_WB      = 0x01010000; /* GRAN=1, WTHRESH=1 */
constexpr uint32_t TXDCTL_MAX_TX_DESC_PREFETCH = 0x0100001F; /* GRAN=1, PTHRESH=31 */
/* Enable the counting of desc. still to be processed. */
constexpr uint32_t TXDCTL_COUNT_DESC           = 0x00400000;


constexpr uint32_t TXD_POPTS_IXSM          = 0x00000001;   /* Insert IP checksum */
constexpr uint32_t TXD_POPTS_TXSM          = 0x00000002;   /* Insert TCP/UDP checksum */
constexpr uint32_t TXD_CMD_EOP             = 0x01000000;   /* End of Packet */
constexpr uint32_t TXD_CMD_IFCS            = 0x02000000;   /* Insert FCS (Ethernet CRC) */
constexpr uint32_t TXD_CMD_IC              = 0x04000000;   /* Insert Checksum */
constexpr uint32_t TXD_CMD_RS              = 0x08000000;   /* Report Status */
constexpr uint32_t TXD_CMD_RPS             = 0x10000000;   /* Report Packet Sent */
constexpr uint32_t TXD_CMD_DEXT            = 0x20000000;   /* Descriptor extension (0 = legacy) */
constexpr uint32_t TXD_CMD_VLE             = 0x40000000;   /* Add VLAN tag */
constexpr uint32_t TXD_CMD_IDE             = 0x80000000;   /* Enable Tidv register */
constexpr uint32_t TXD_STAT_DD             = 0x00000001;   /* Descriptor Done */
constexpr uint32_t TXD_STAT_EC             = 0x00000002;   /* Excess Collisions */
constexpr uint32_t TXD_STAT_LC             = 0x00000004;   /* Late Collisions */
constexpr uint32_t TXD_STAT_TU             = 0x00000008;   /* Transmit underrun */
constexpr uint32_t TXD_CMD_TCP             = 0x01000000;   /* TCP packet */
constexpr uint32_t TXD_CMD_IP              = 0x02000000;   /* IP packet */
constexpr uint32_t TXD_CMD_TSE             = 0x04000000;   /* TCP Seg enable */

/* Interrupt Cause Read */
constexpr uint32_t ICR_TXDW                = 0x00000001;/* Transmit desc written back */
constexpr uint32_t ICR_TXQE                = 0x00000002;/* Transmit queue empty */
constexpr uint32_t ICR_LSC                 = 0x00000004;/* Link Status Change */
constexpr uint32_t ICR_RXSEQ               = 0x00000008;/* Rx sequence error */
constexpr uint32_t ICR_RXDMT0              = 0x00000010;/* Rx desc min. threshold (0) */
constexpr uint32_t ICR_RXT0                = 0x00000080;/* Rx timer intr (ring 0) */

/* Receive Descriptor */
struct alignas(16) rx_desc {
	uint64_t buffer_addr; /* Address of the descriptor's data buffer */
	uint16_t length;      /* Length of data DMAed into data buffer */
	uint16_t csum;	      /* Packet checksum */
	uint8_t  status;      /* Descriptor status */
	uint8_t  errors;      /* Descriptor Errors */
	uint16_t special;
};
static_assert(sizeof(rx_desc) == 128/8, "");

/* Receive Descriptor */
struct alignas(16) tx_desc {
    uint64_t buffer_addr;       /* Address of the descriptor's data buffer */
    union {
        uint32_t data;
        struct {
            uint16_t length;    /* Data buffer length */
            uint8_t  cso;	    /* Checksum offset */
            uint8_t  cmd;	    /* Descriptor control */
        } flags;
    } lower;
    union {
        uint32_t data;
        struct {
            uint8_t  status;     /* Descriptor status */
            uint8_t  css;	     /* Checksum start */
            uint16_t special;
        } fields;
    } upper;
};
static_assert(sizeof(tx_desc) == 128/8, "");

void delay_microseconds(uint32_t count)
{
    // Should be around 1us...
    while (count--) {
        __outbyte(0x80, 0);
    }
}

constexpr uint16_t i825x0em_a = 0x100e; // desktop
constexpr uint16_t i825x5em_a = 0x100f; // copper
constexpr uint16_t i82567_lm  = 0x10f5; // 82567LM Gigabit Network Connection

class i825x_ethernet_device : public ethernet_device {
public:
    static constexpr uint32_t io_mem_size = 128<<10; // 128K

    explicit i825x_ethernet_device(const pci::device_info& dev_info) : mac_addr_{ 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 }, dev_addr_{dev_info.address} {
        REQUIRE(!(dev_info.bars[0].address & pci::bar_is_io_mask)); // Register base address
        REQUIRE(dev_info.bars[0].size == io_mem_size);

        const uint64_t iobase = dev_info.bars[0].address&pci::bar_mem_address_mask;

        reg_base_ = static_cast<volatile uint32_t*>(iomem_map(physical_address{iobase}, io_mem_size));
        dbgout() << "[i825x] Initializing. IOBASE = " << as_hex(iobase).width(8) << " IRQ# " << dev_info.config.header0.intr_line << "\n";
        reg_ = register_irq_handler(dev_info.config.header0.intr_line, [this]() { isr(); });

        reset();
        pci::bus_master(dev_addr_, true);
    }

    virtual ~i825x_ethernet_device() override {
        dbgout() << "[i825x] Shutting down\n";
        // TODO: Stop device
        pci::bus_master(dev_addr_, false);
        iomem_unmap(reg_base_, io_mem_size);
    }

private:
    // The number of descriptors must be a multiple of 8 (size divisible by 128b)
    static constexpr uint32_t num_rx_descriptors = 16; // Must be at least 8
    static constexpr uint32_t num_tx_descriptors = 16; // Must be at least 16

    mac_address             mac_addr_;
    pci::device_address     dev_addr_;
    volatile uint32_t*      reg_base_;
#pragma warning(suppress: 4324) // struct was padded due to alignment specifier
    volatile rx_desc        rx_desc_[num_rx_descriptors];
    uint8_t                 rx_buffer_[num_rx_descriptors][2048];
    uint32_t                rx_head_ = 0;
#pragma warning(suppress: 4324) // struct was padded due to alignment specifier
    volatile tx_desc        tx_desc_[num_tx_descriptors];
    uint32_t                tx_tail_ = 0;
    isr_registration_ptr    reg_;

    uint32_t ioreg(reg r) {
        return reg_base_[static_cast<uint32_t>(r)>>2];
    }

    void ioreg(reg r, uint32_t val) {
        reg_base_[static_cast<uint32_t>(r)>>2] = val;
    }

    void phy_reset() {
        const auto orig = ioreg(reg::CTRL);
        ioreg(reg::CTRL, orig | CTRL_RST | CTRL_PHY_RST);
        delay_microseconds(4); // Should be held high for at least 3 us. NOTE: Should be held high for 10ms(!) for 82546GB
        ioreg(reg::CTRL, orig & ~CTRL_PHY_RST);
    }

    void reset() {
        phy_reset();
        // device reset - the RST bit is self-clearing
        ioreg(reg::CTRL, ioreg(reg::CTRL) | CTRL_RST);
        for (int timeout = 0; ; ++timeout) {
            REQUIRE(timeout < 1000 && "Failed to reset i825x device");
            delay_microseconds(2); // Need to delay at least 1us before reading status after a reset
            if (!(ioreg(reg::CTRL) & CTRL_RST)) {
                break;
            }
        }

        // LRST->0 enables auto-negotiation, clear ILOS and disable VLAN Mode Enable
        ioreg(reg::CTRL, (ioreg(reg::CTRL) & ~(CTRL_LRST | CTRL_ILOS | CTRL_VME)) | CTRL_ASDE | CTRL_SLU);

        // TODO: Reset flow control
        // TODO: Clear statistics counters

        // Program the Receive Address Register
        ioreg(reg::RAL0, (mac_addr_[0]) | (mac_addr_[1] << 8) | (mac_addr_[2] << 16) | (mac_addr_[3] << 24));
        ioreg(reg::RAH0, (mac_addr_[4]) | (mac_addr_[5] << 8) | RAH_AV);

        // Initialize the MTA (Multicast Table Array) to 0
        for (int i = 0; i < 128; i++) {
            ioreg(static_cast<reg>(static_cast<uint32_t>(reg::MTA) + i * 4), 0);
        }

        // Program the Interrupt Mask Set/Read (IMS) register
        ioreg(reg::IMC, ~0U); // Inhibit interrupts
        ioreg(reg::ICR, ~0U); // Clear pending interrupts
        ioreg(reg::IMS, ICR_TXDW | ICR_LSC | ICR_RXDMT0 | ICR_RXT0); // Enable interrupts

        // Program the Receive Descriptor Base Address
        const uint64_t rx_desc_phys = virt_to_phys((const void*)&rx_desc_[0]);
        ioreg(reg::RDBAL0, static_cast<uint32_t>(rx_desc_phys));
        ioreg(reg::RDBAH0, static_cast<uint32_t>(rx_desc_phys>>32));

        // Set the Receive Descriptor Length (RDLEN) register to the size (in bytes) of the descriptor ring.
        ioreg(reg::RDLEN0, sizeof(rx_desc_));

        // Initialize Receive Descriptor Head and Tail registers
        ioreg(reg::RDH0, 0);
        ioreg(reg::RDT0, num_rx_descriptors-1);

        for (uint32_t i = 0; i < num_rx_descriptors; ++i) {
            rx_desc_[i].buffer_addr = virt_to_phys(rx_buffer_[i]);
            rx_desc_[i].status = 0;
        }

        // Enable RX
        ioreg(reg::RCTL, RCTL_EN
                           | RCTL_SBP // recieve everyting
                           | RCTL_UPE // even
                           | RCTL_MPE // bad packets
                           | RCTL_BAM // and multicast..
                           | RCTL_SZ_2048);
        rx_head_ = 0;

        // Enable TX
        const uint64_t tx_desc_phys = virt_to_phys((const void*)&tx_desc_[0]);
        REQUIRE((tx_desc_phys & 15) == 0);
        ioreg(reg::TDBAL0, static_cast<uint32_t>(tx_desc_phys));
        ioreg(reg::TDBAH0, static_cast<uint32_t>(tx_desc_phys>>32));

        ioreg(reg::TDLEN0, sizeof(tx_desc_));
        static_assert(sizeof(tx_desc_) % 128 == 0, "");
        static_assert(sizeof(tx_desc_) >= 128, "");

        // Set the transmit descriptor write-back policy
        ioreg(reg::TXDCTL0, (ioreg(reg::TXDCTL0) & TXDCTL_WTHRESH) | TXDCTL_FULL_TX_DESC_WB/* | TXDCTL_COUNT_DESC*/);
        ioreg(reg::TDH0, 0);
        ioreg(reg::TDT0, 0);
        ioreg(reg::TCTL, ioreg(reg::TCTL) | TCTL_EN | TCTL_PSP);

        tx_tail_ = 0;
    }

    void isr() {
        const auto icr = ioreg(reg::ICR);
        ioreg(reg::ICR, icr); // clear pending interrupts
        constexpr uint32_t ICR_INT_ASSERTED = 1U << 31; // Reported by bochs
        if (icr & ~(ICR_RXT0 | ICR_TXDW | ICR_INT_ASSERTED)) {
            // Only report interesting IRQs
            dbgout() << "[i825x] IRQ. ICR = " << as_hex(icr) << "\n";
        }
    }

    virtual mac_address do_hw_address() const {
        return mac_addr_;
    }

    virtual void do_send_packet(const void* data, uint32_t length) override {
        REQUIRE(length <= 1500);

        if (!(ioreg(reg::STATUS) & STATUS_LU)) {
            dbgout() << "[i825x] Link not up. Dropping packet\n";
            return;
        }

        // prepare descriptor
        auto& td = tx_desc_[tx_tail_];
        tx_tail_ = (tx_tail_ + 1) % num_tx_descriptors;
        REQUIRE(td.upper.fields.status == 0);
        td.buffer_addr = virt_to_phys(data);
        td.lower.data = length | TXD_CMD_RS | TXD_CMD_EOP | TXD_CMD_IFCS;
        td.upper.data = 0;
        _mm_mfence();
        ioreg(reg::TDT0, tx_tail_);

        //dbgout() << "Waiting for packet to be sent.\n";
        for (uint32_t timeout = 100; !td.upper.fields.status; ) {
            __halt();
            if (!timeout--) {
#if 0
                // Dump stats
                constexpr uint32_t nstats = 0x100 / 4;
                static uint32_t stats[nstats];
                for (uint32_t i = 0; i < nstats; ++i) {
                    stats[i] += ioreg(static_cast<reg>(0x4000 + i * 4));
                    dbgout() << as_hex(stats[i]);
                    dbgout() << ((i % 8 == 7) ? '\n' : ' ');
                }
#endif
                dbgout() << "Transfer NOT done. Timed out! STATUS = " << as_hex(ioreg(reg::STATUS)) << " TDH = " << ioreg(reg::TDH0) << " TDT " <<  ioreg(reg::TDT0) << "\n";
#if 0
                for (uint32_t i = 0; i < num_tx_descriptors; ++i) {
                    dbgout() << as_hex(tx_desc_[i].buffer_addr) << " ";
                    dbgout() << as_hex(tx_desc_[i].lower.data) << " ";
                    dbgout() << as_hex(tx_desc_[i].upper.data) << " ";
                    dbgout() << ((i % 3 == 2) ? '\n' : ' ');
                }
                dbgout() << "\n";
#endif
                return;
            }
        }
        //dbgout() << "[i825x] TX Status = " << as_hex(td.upper.fields.status) << "\n";
        REQUIRE(td.upper.fields.status == TXD_STAT_DD);
        td.upper.data = 0; // Mark ready for re-use
        REQUIRE(ioreg(reg::TDH0) == ioreg(reg::TDT0));

    }

    virtual void do_process_packets(const packet_process_function& ppf, int max_packets) override {
        REQUIRE(max_packets >= 1);
        for (int i = 0; i < max_packets; ++i) {
            auto& rd = rx_desc_[rx_head_];
            if (!(rd.status & RXD_STAT_DD)) {
                break;
            }
            REQUIRE(rd.status & RXD_STAT_EOP);
            dbgout() << "[i825x] RX Status = " << as_hex(rd.status) << " length = " << as_hex(rd.length) << " idx = " << rx_head_ << " error = " << as_hex(rd.errors) << "\n";
            if (!rd.errors) {
                ppf(rx_buffer_[rx_head_], rd.length);
            }
            rd.status = 0;              // Mark available for SW
            ioreg(reg::RDT0, rx_head_); // Mark available for HW
            rx_head_ = (rx_head_ + 1) % num_rx_descriptors;
        }
    }
};

kowned_ptr<ethernet_device> probe(const pci::device_info& dev_info)
{
    if (dev_info.config.vendor_id == pci::vendor::intel &&
            (dev_info.config.device_id == i825x0em_a || dev_info.config.device_id == i825x5em_a || dev_info.config.device_id == i82567_lm)) {
        return kowned_ptr<ethernet_device>{knew<i825x_ethernet_device>(dev_info).release()};
    }

    return kowned_ptr<ethernet_device>{};
}

} } } // namespace attos::net::i825x
