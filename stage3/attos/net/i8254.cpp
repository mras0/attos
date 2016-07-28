#include "i8254.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>
#include <attos/isr.h>

namespace attos { namespace net {

namespace {

enum class i8254_reg {
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
    RDBAL_BASE   = 0x02800, /* Rx Descriptor Base Address Low - RW */
    RDBAH_BASE   = 0x02804, /* Rx Descriptor Base Address High - RW */
    RDLEN_BASE   = 0x02808, /* Rx Descriptor Length - RW */
    RDH_BASE     = 0x02810, /* Rx Descriptor Head - RW */
    RDT_BASE     = 0x02818, /* Rx Descriptor Tail - RW */
    RDTR         = 0x02820, /* Rx Delay Timer - RW */
    RXDCTL_BASE  = 0x02828, /* Rx Descriptor Control - RW */
    RADV         = 0x0282C, /* Rx Interrupt Absolute Delay Timer - RW */
    KABGTXD      = 0x03004, /* AFE Band Gap Transmit Ref Data */
    TDBAL_BASE   = 0x03800, /* Tx Descriptor Base Address Low - RW */
    TDBAH_BASE   = 0x03804, /* Tx Descriptor Base Address High - RW */
    TDLEN_BASE   = 0x03808, /* Tx Descriptor Length - RW */
    TDH_BASE     = 0x03810, /* Tx Descriptor Head - RW */
    TDT_BASE     = 0x03818, /* Tx Descriptor Tail - RW */
    TIDV         = 0x03820, /* Tx Interrupt Delay Value - RW */
    TXDCTL_BASE  = 0x03828, /* Tx Descriptor Control - RW */
    TADV         = 0x0382C, /* Tx Interrupt Absolute Delay Val - RW */
    TARC_BASE    = 0x03840, /* Tx Arbitration Count (0) */
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
    RAL_BASE     = 0x05400, /* Receive Address Low - RW */
    RAH_BASE     = 0x05404, /* Receive Address High - RW */
};

constexpr uint32_t i8254_CTRL_FD                 = 0x00000001; /* Full duplex.0=half; 1=full */
constexpr uint32_t i8254_CTRL_GIO_MASTER_DISABLE = 0x00000004; /* Blocks new Master requests */
constexpr uint32_t i8254_CTRL_LRST               = 0x00000008; /* Link reset. 0=normal,1=reset */
constexpr uint32_t i8254_CTRL_ASDE               = 0x00000020; /* Auto-speed detect enable */
constexpr uint32_t i8254_CTRL_SLU                = 0x00000040; /* Set link up (Force Link) */
constexpr uint32_t i8254_CTRL_ILOS               = 0x00000080; /* Invert Loss-Of Signal */
constexpr uint32_t i8254_CTRL_SPD_SEL            = 0x00000300; /* Speed Select Mask */
constexpr uint32_t i8254_CTRL_SPD_10             = 0x00000000; /* Force 10Mb */
constexpr uint32_t i8254_CTRL_SPD_100            = 0x00000100; /* Force 100Mb */
constexpr uint32_t i8254_CTRL_SPD_1000           = 0x00000200; /* Force 1Gb */
constexpr uint32_t i8254_CTRL_FRCSPD             = 0x00000800; /* Force Speed */
constexpr uint32_t i8254_CTRL_FRCDPX             = 0x00001000; /* Force Duplex */
constexpr uint32_t i8254_CTRL_LANPHYPC_OVERRIDE  = 0x00010000; /* SW control of LANPHYPC */
constexpr uint32_t i8254_CTRL_LANPHYPC_VALUE     = 0x00020000; /* SW value of LANPHYPC */
constexpr uint32_t i8254_CTRL_SWDPIN0            = 0x00040000; /* SWDPIN 0 value */
constexpr uint32_t i8254_CTRL_SWDPIN1            = 0x00080000; /* SWDPIN 1 value */
constexpr uint32_t i8254_CTRL_SWDPIO0            = 0x00400000; /* SWDPIN 0 Input or output */
constexpr uint32_t i8254_CTRL_RST                = 0x04000000; /* Global reset */
constexpr uint32_t i8254_CTRL_RFCE               = 0x08000000; /* Receive Flow Control enable */
constexpr uint32_t i8254_CTRL_TFCE               = 0x10000000; /* Transmit flow control enable */
constexpr uint32_t i8254_CTRL_VME                = 0x40000000; /* IEEE VLAN mode enable */
constexpr uint32_t i8254_CTRL_PHY_RST            = 0x80000000; /* PHY Reset */

constexpr uint32_t i8254_RAH_AV                  = 0x80000000; /* Receive descriptor valid */

constexpr uint32_t i8254_RCTL_EN                 = 0x00000002;   /* enable */
constexpr uint32_t i8254_RCTL_SBP                = 0x00000004;   /* store bad packet */
constexpr uint32_t i8254_RCTL_UPE                = 0x00000008;   /* unicast promiscuous enable */
constexpr uint32_t i8254_RCTL_MPE                = 0x00000010;   /* multicast promiscuous enab */
constexpr uint32_t i8254_RCTL_LPE                = 0x00000020;   /* long packet enable */
constexpr uint32_t i8254_RCTL_LBM_NO             = 0x00000000;   /* no loopback mode */
constexpr uint32_t i8254_RCTL_LBM_MAC            = 0x00000040;   /* MAC loopback mode */
constexpr uint32_t i8254_RCTL_LBM_TCVR           = 0x000000C0;   /* tcvr loopback mode */
constexpr uint32_t i8254_RCTL_DTYP_PS            = 0x00000400;   /* Packet Split descriptor */
constexpr uint32_t i8254_RCTL_RDMTS_HALF         = 0x00000000;   /* Rx desc min threshold size */
constexpr uint32_t i8254_RCTL_MO_SHIFT           = 12        ;   /* multicast offset shift */
constexpr uint32_t i8254_RCTL_MO_3               = 0x00003000;   /* multicast offset 15:4 */
constexpr uint32_t i8254_RCTL_BAM                = 0x00008000;   /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
constexpr uint32_t i8254_RCTL_SZ_2048            = 0x00000000;   /* Rx buffer size 2048 */
constexpr uint32_t i8254_RCTL_SZ_1024            = 0x00010000;   /* Rx buffer size 1024 */
constexpr uint32_t i8254_RCTL_SZ_512             = 0x00020000;   /* Rx buffer size 512 */
constexpr uint32_t i8254_RCTL_SZ_256             = 0x00030000;   /* Rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
constexpr uint32_t i8254_RCTL_SZ_16384           = 0x00010000;   /* Rx buffer size 16384 */
constexpr uint32_t i8254_RCTL_SZ_8192            = 0x00020000;   /* Rx buffer size 8192 */
constexpr uint32_t i8254_RCTL_SZ_4096            = 0x00030000;   /* Rx buffer size 4096 */
constexpr uint32_t i8254_RCTL_VFE                = 0x00040000;   /* vlan filter enable */
constexpr uint32_t i8254_RCTL_CFIEN              = 0x00080000;   /* canonical form enable */
constexpr uint32_t i8254_RCTL_CFI                = 0x00100000;   /* canonical form indicator */
constexpr uint32_t i8254_RCTL_DPF                = 0x00400000;   /* Discard Pause Frames */
constexpr uint32_t i8254_RCTL_PMCF               = 0x00800000;   /* pass MAC control frames */
constexpr uint32_t i8254_RCTL_BSEX               = 0x02000000;   /* Buffer size extension */
constexpr uint32_t i8254_RCTL_SECRC              = 0x04000000;   /* Strip Ethernet CRC */

/* Transmit Control */
constexpr uint32_t i8254_TCTL_EN                 = 0x00000002;   /* enable Tx */
constexpr uint32_t i8254_TCTL_PSP                = 0x00000008;   /* pad short packets */
constexpr uint32_t i8254_TCTL_CT                 = 0x00000ff0;   /* collision threshold */
constexpr uint32_t i8254_TCTL_COLD               = 0x003ff000;   /* collision distance */
constexpr uint32_t i8254_TCTL_RTLC               = 0x01000000;   /* Re-transmit on late collision */
constexpr uint32_t i8254_TCTL_MULR               = 0x10000000;   /* Multiple request support */

constexpr uint32_t i8254_TXD_POPTS_IXSM          = 0x00000001;   /* Insert IP checksum */
constexpr uint32_t i8254_TXD_POPTS_TXSM          = 0x00000002;   /* Insert TCP/UDP checksum */
constexpr uint32_t i8254_TXD_CMD_EOP             = 0x01000000;   /* End of Packet */
constexpr uint32_t i8254_TXD_CMD_IFCS            = 0x02000000;   /* Insert FCS (Ethernet CRC) */
constexpr uint32_t i8254_TXD_CMD_IC              = 0x04000000;   /* Insert Checksum */
constexpr uint32_t i8254_TXD_CMD_RS              = 0x08000000;   /* Report Status */
constexpr uint32_t i8254_TXD_CMD_RPS             = 0x10000000;   /* Report Packet Sent */
constexpr uint32_t i8254_TXD_CMD_DEXT            = 0x20000000;   /* Descriptor extension (0 = legacy) */
constexpr uint32_t i8254_TXD_CMD_VLE             = 0x40000000;   /* Add VLAN tag */
constexpr uint32_t i8254_TXD_CMD_IDE             = 0x80000000;   /* Enable Tidv register */
constexpr uint32_t i8254_TXD_STAT_DD             = 0x00000001;   /* Descriptor Done */
constexpr uint32_t i8254_TXD_STAT_EC             = 0x00000002;   /* Excess Collisions */
constexpr uint32_t i8254_TXD_STAT_LC             = 0x00000004;   /* Late Collisions */
constexpr uint32_t i8254_TXD_STAT_TU             = 0x00000008;   /* Transmit underrun */
constexpr uint32_t i8254_TXD_CMD_TCP             = 0x01000000;   /* TCP packet */
constexpr uint32_t i8254_TXD_CMD_IP              = 0x02000000;   /* IP packet */
constexpr uint32_t i8254_TXD_CMD_TSE             = 0x04000000;   /* TCP Seg enable */

/* Interrupt Cause Read */
constexpr uint32_t i8254_ICR_TXDW                = 0x00000001;/* Transmit desc written back */
constexpr uint32_t i8254_ICR_LSC                 = 0x00000004;/* Link Status Change */
constexpr uint32_t i8254_ICR_RXSEQ               = 0x00000008;/* Rx sequence error */
constexpr uint32_t i8254_ICR_RXDMT0              = 0x00000010;/* Rx desc min. threshold (0) */
constexpr uint32_t i8254_ICR_RXT0                = 0x00000080;/* Rx timer intr (ring 0) */

/* Receive Descriptor */
struct alignas(16) i8254_rx_desc {
	uint64_t buffer_addr; /* Address of the descriptor's data buffer */
	uint16_t length;      /* Length of data DMAed into data buffer */
	uint16_t csum;	      /* Packet checksum */
	uint8_t  status;      /* Descriptor status */
	uint8_t  errors;      /* Descriptor Errors */
	uint16_t special;
};
static_assert(sizeof(i8254_rx_desc) == 128/8, "");

/* Receive Descriptor */
struct alignas(16) i8254_tx_desc {
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
static_assert(sizeof(i8254_tx_desc) == 128/8, "");

void delay_microseconds(uint32_t count)
{
    // Should be around 1us...
    while (count--) {
        __outbyte(0x80, 0);
    }
}

} // unnamed namespace

static const uint8_t mac_addr[6]      = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static const uint8_t broadcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
class i8254 : public netdev {
public:
    static constexpr uint32_t io_mem_size = 128<<10; // 128K

    explicit i8254(const pci::device_info& dev_info) : dev_addr_(dev_info.address) {
        REQUIRE(!(dev_info.bars[0].address & pci::bar_is_io_mask)); // Register base address
        REQUIRE(dev_info.bars[0].size == i8254::io_mem_size);

        const uint64_t iobase = dev_info.bars[0].address&pci::bar_mem_address_mask;

        dbgout() << "[i8254] Initializing. IOBASE = " << as_hex(iobase).width(8) << " IRQ# " << dev_info.config.header0.intr_line << "\n";
        reg_base_ = static_cast<volatile uint32_t*>(iomem_map(physical_address{iobase}, io_mem_size));
        reg_ = register_irq_handler(dev_info.config.header0.intr_line, [this]() { isr(); });
        reset();
        pci::bus_master(dev_addr_, true);
    }

    virtual ~i8254() override {
        dbgout() << "[i8254] Shutting down\n";
        // TODO: Stop device
        pci::bus_master(dev_addr_, false);
        iomem_unmap(reg_base_, io_mem_size);
    }

private:
    // The number of descriptors must be a multiple of 8 (size divisible by 128b)
    static constexpr uint32_t num_rx_buffers = 8;
    static constexpr uint32_t num_tx_buffers = 8;

    pci::device_address     dev_addr_;
    volatile uint32_t*      reg_base_;
#pragma warning(suppress: 4324) // struct was padded due to alignment specifier
    volatile i8254_rx_desc  rx_desc_[num_rx_buffers];
    uint8_t                 rx_buffer_[num_rx_buffers][2048];
#pragma warning(suppress: 4324) // struct was padded due to alignment specifier
    volatile i8254_tx_desc  tx_desc_[num_tx_buffers];
    uint8_t                 tx_buffer_[num_tx_buffers][2048];
    isr_registration_ptr    reg_;

    uint32_t reg(i8254_reg r) {
        return reg_base_[static_cast<uint32_t>(r)>>2];
    }

    void reg(i8254_reg r, uint32_t val) {
        reg_base_[static_cast<uint32_t>(r)>>2] = val;
    }

    void phy_reset() {
        const auto orig = reg(i8254_reg::CTRL);
        reg(i8254_reg::CTRL, orig | i8254_CTRL_PHY_RST);
        delay_microseconds(4); // Should be held high for at least 3 us. NOTE: Should be held high for 10ms(!) for 82546GB
        reg(i8254_reg::CTRL, orig & ~i8254_CTRL_PHY_RST);
    }

    void reset() {
        phy_reset();
        // device reset - the RST bit is self-clearing
        reg(i8254_reg::CTRL, reg(i8254_reg::CTRL) | i8254_CTRL_RST);
        for (int timeout = 0; ; ++timeout) {
            REQUIRE(timeout < 1000 && "Failed to reset i8254 device");
            delay_microseconds(2); // Need to delay at least 1us before reading status after a reset
            if (!(reg(i8254_reg::CTRL) & i8254_CTRL_RST)) {
                break;
            }
        }

        // LRST->0 enables auto-negotiation, clear ILOS and disable VLAN Mode Enable
        reg(i8254_reg::CTRL, (reg(i8254_reg::CTRL) & ~(i8254_CTRL_LRST | i8254_CTRL_ILOS | i8254_CTRL_VME)) | i8254_CTRL_ASDE | i8254_CTRL_SLU);

        // TODO: Reset flow control
        // TODO: Clear statistics counters

        // Program the Receive Address Register
        reg(i8254_reg::RAL_BASE, (mac_addr[0]) | (mac_addr[1] << 8) | (mac_addr[2] << 16) | (mac_addr[3] << 24));
        reg(i8254_reg::RAH_BASE, (mac_addr[4]) | (mac_addr[5] << 8) | i8254_RAH_AV);

        // Initialize the MTA (Multicast Table Array) to 0
        for (int i = 0; i < 128; i++) {
            reg(static_cast<i8254_reg>(static_cast<uint32_t>(i8254_reg::MTA) + i * 4), 0);
        }

        // Program the Interrupt Mask Set/Read (IMS) register
        reg(i8254_reg::IMC, ~0U); // Inhibit interrupts
        reg(i8254_reg::ICR, ~0U); // Clear pending interrupts
        reg(i8254_reg::IMS, i8254_ICR_TXDW
                          | i8254_ICR_LSC
                          | i8254_ICR_RXT0); // Enable interrupts

        // Program the Receive Descriptor Base Address
        const uint64_t rx_desc_phys = virt_to_phys((const void*)&rx_desc_[0]);
        reg(i8254_reg::RDBAL_BASE, static_cast<uint32_t>(rx_desc_phys));
        reg(i8254_reg::RDBAH_BASE, static_cast<uint32_t>(rx_desc_phys>>32));

        // Set the Receive Descriptor Length (RDLEN) register to the size (in bytes) of the descriptor ring.
        reg(i8254_reg::RDLEN_BASE, sizeof(rx_desc_));

        // Initialize Receive Descriptor Head and Tail registers
        reg(i8254_reg::RDH_BASE, 0);
        reg(i8254_reg::RDT_BASE, num_rx_buffers);

        for (uint32_t i = 0; i < num_rx_buffers; ++i) {
            rx_desc_[i].buffer_addr = virt_to_phys(rx_buffer_[i]);
            rx_desc_[i].status = 0;
        }

        // Enable RX
        reg(i8254_reg::RCTL, i8254_RCTL_EN
                           | i8254_RCTL_SBP // recieve everyting
                           | i8254_RCTL_UPE // even
                           | i8254_RCTL_MPE // bad packets
                           | i8254_RCTL_BAM // and multicast..
                           | i8254_RCTL_SZ_2048);

        // Enable TX
        const uint64_t tx_desc_phys = virt_to_phys((const void*)&tx_desc_[0]);
        reg(i8254_reg::TDBAL_BASE, static_cast<uint32_t>(tx_desc_phys));
        reg(i8254_reg::TDBAH_BASE, static_cast<uint32_t>(tx_desc_phys>>32));

        reg(i8254_reg::TDLEN_BASE, sizeof(tx_desc_));

        reg(i8254_reg::TDH_BASE, 0);
        reg(i8254_reg::TDT_BASE, 0);
        reg(i8254_reg::TCTL, i8254_TCTL_EN
                           | i8254_TCTL_PSP
                           | i8254_TCTL_CT
                           | i8254_TCTL_COLD);

        // prepare descriptor
        auto& td = tx_desc_[0];
        memset((void*)&td, 0, sizeof(td));
        td.buffer_addr = virt_to_phys(tx_buffer_[0]);

        // Ethernet header
        uint8_t* b = tx_buffer_[0];
        memcpy(b, broadcast_mac, 6); b += 6;
        memcpy(b, mac_addr, 6); b += 6;
        *b++ = 0x08; // 0x0806 ARP
        *b++ = 0x06;
        // ARP
        *b++ = 0x00; // Hardware type (0x0001 = Ethernet)
        *b++ = 0x01;
        *b++ = 0x08; // Protocol type (0x0800 = Ipv4)
        *b++ = 0x00;
        *b++ = 0x06; // Hardware address length
        *b++ = 0x04; // Protocol address length
        *b++ = 0x00; // Opcode (1=Request)
        *b++ = 0x01;
        memcpy(b, mac_addr, 6); b += 6;
        *b++ = 0xff;
        *b++ = 0xff;
        *b++ = 0xff;
        *b++ = 0xff;
        memcpy(b, broadcast_mac, 6); b += 6;
        *b++ = 0xff;
        *b++ = 0xff;
        *b++ = 0xff;
        *b++ = 0xff;
        uint16_t tx_len = 64;

        td.lower.data = tx_len | i8254_TXD_CMD_RS | i8254_TXD_CMD_RPS | i8254_TXD_CMD_EOP | i8254_TXD_CMD_IFCS;

        reg(i8254_reg::TDT_BASE, 1);

        dbgout() << "Waiting for packet to be sent.\n";
        while (!td.upper.fields.status) {
           _mm_pause();
        }
        dbgout() << "Status = " << as_hex(td.upper.fields.status) << "\n";
        REQUIRE(reg(i8254_reg::TDH_BASE) == reg(i8254_reg::TDT_BASE));

        dbgout() << "Waiting for packet!\n";
        for (bool got_packet = false; !got_packet;) {
            for (uint32_t i = 0; i < num_rx_buffers; ++i) {
                if (rx_desc_[i].status != 0) {
                    dbgout() << "status = " << as_hex(rx_desc_[i].status) << " length = " << as_hex(rx_desc_[i].length) << "\n";
                    hexdump(dbgout(), rx_buffer_[i], rx_desc_[i].length);
                    got_packet = true;
                }
            }
        }
    }

    void isr() {
        const auto icr = reg(i8254_reg::ICR);
        reg(i8254_reg::ICR, icr); // clear pending interrupts
        dbgout() << "[i8254] IRQ. ICR = " << as_hex(icr) << "\n";
    }
};

netdev_ptr i82545_probe(const pci::device_info& dev_info)
{
    constexpr uint16_t i82540em_a = 0x100e; // desktop
    constexpr uint16_t i82545em_a = 0x100f; // copper

    if (dev_info.config.vendor_id == pci::vendor::intel && (dev_info.config.device_id == i82540em_a || dev_info.config.device_id == i82545em_a)) {
        return netdev_ptr{knew<i8254>(dev_info).release()};
    }

    return netdev_ptr{};
}

} } // namespace attos::net
