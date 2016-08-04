#include "ata.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>

namespace attos { namespace ata {

// Port Offset	Function	                        Description
// 0	        Data Port	                        Read/Write PIO data bytes on this port.
// 1	        Features / Error Information	    Usually used for ATAPI devices.
// 2	        Sector Count	                    Number of sectors to read/write (0 is a special value).
// 3	        Sector Number / LBAlo	            This is CHS / LBA28 / LBA48 specific.
// 4	        Cylinder Low / LBAmid	            Partial Disk Sector address.
// 5	        Cylinder High / LBAhi	            Partial Disk Sector address.
// 6	        Drive / Head Port	                Used to select a drive and/or head. May supports extra address/flag bits.
// 7	        Command port / Regular Status port	Used to send commands or read the current status.
enum class port_offset {
    data          = 0,

    features      = 1,
    error_info    = 1,

    sector_count  = 2,

    sector_number = 3,
    lba_low       = 3,

    cylinder_low  = 4,
    lba_mid       = 4,

    cylinder_high = 5,
    lba_high      = 5,

    drive         = 6,
    head          = 6,

    command       = 7,
    status        = 7,
};

constexpr uint8_t status_mask_err = 1<<0; //ERR	Indicates an error occurred. Send a new command to clear it (or nuke it with a Software Reset).
constexpr uint8_t status_mask_idx = 1<<1; //IDX Index?
constexpr uint8_t status_mask_cor = 1<<2; //    Corrected data
constexpr uint8_t status_mask_drq = 1<<3; //DRQ	Set when the drive has PIO data to transfer, or is ready to accept PIO data.
constexpr uint8_t status_mask_srv = 1<<4; //SRV	Overlapped Mode Service Request.
constexpr uint8_t status_mask_df  = 1<<5; //DF	Drive Fault Error (does not set ERR).
constexpr uint8_t status_mask_rdy = 1<<6; //RDY	Bit is clear when drive is spun down, or after an error. Set otherwise.
constexpr uint8_t status_mask_bsy = 1<<7; //BSY	Indicates the drive is preparing to send/receive data (wait for it to clear). In case of 'hang' (it never clears), do a software reset.

constexpr uint8_t drive_slave_bit = 4;

constexpr uint8_t command_read_pio          = 0x20;
constexpr uint8_t command_read_pio_ext      = 0x24;
constexpr uint8_t command_read_dma          = 0xC8;
constexpr uint8_t command_read_dma_ext      = 0x25;
constexpr uint8_t command_write_pio         = 0x30;
constexpr uint8_t command_write_pio_ext     = 0x34;
constexpr uint8_t command_write_dma         = 0xCA;
constexpr uint8_t command_write_dma_ext     = 0x35;
constexpr uint8_t command_cache_flush       = 0xE7;
constexpr uint8_t command_cache_flush_ext   = 0xEA;
constexpr uint8_t command_packet            = 0xA0;
constexpr uint8_t command_identify_packet   = 0xA1;
constexpr uint8_t command_identify          = 0xEC;

constexpr uint8_t control_register_mask_nien = 1<<1; // nIEN    Set this to stop the current device from sending interrupts.
constexpr uint8_t control_register_mask_srst = 1<<2; // SRST    Set this to do a "Software Reset" on all ATA drives on a bus, if one is misbehaving.
constexpr uint8_t control_register_mask_hob  = 1<<7; // HOB Set this to read back the High Order Byte of the last LBA48 value sent to an IO port.

struct device_info {
    uint16_t first_command_block_register;
    uint16_t control_block_register;
    uint8_t  irq;
    bool     slave;
};

constexpr device_info primary_master   { 0x1F0, 0x3F6, 14, false };
constexpr device_info primary_slave    { 0x1F0, 0x3F6, 14, true };
constexpr device_info secondary_master { 0x170, 0x376, 15, false };
constexpr device_info secondary_slave  { 0x170, 0x376, 15, true };

template<size_t Size>
void ata_string(char (&dst)[Size], const uint8_t* src) {
    for (int i = 0; i < Size/2; ++i) {
        dst[i*2+1] = src[i*2+0];
        dst[i*2+0] = src[i*2+1];
    }
    dst[Size-1]=0;
    for (int i = Size-2; i>=0 && dst[i] == ' '; i--) {
        dst[i] = 0;
    }
}

class device {
public:
    constexpr static uint32_t sector_size_bytes = 512;

    explicit device(const device_info& dev_info) : dev_info_(dev_info) {
        wait_status();
        out(port_offset::drive, 0xA0 | (dev_info.slave << drive_slave_bit));
        wait_status();
        out(port_offset::command, command_identify);
        const auto id_status = wait_status();
        REQUIRE(id_status != 0); // 0-> No drive
        REQUIRE(id_status & status_mask_drq);

        uint8_t id_buffer[512];
        __indwordstring(port_number(port_offset::data), reinterpret_cast<unsigned long*>(id_buffer), sizeof(id_buffer)/sizeof(uint32_t));

        char model[40+1];
        char ser[20+1];
        ata_string(model, id_buffer+0x36);
        ata_string(ser,   id_buffer+0x14);
        lba_count_ = *reinterpret_cast<uint32_t*>(id_buffer+0x78);
        dbgout() << "[ata] " << model << " " << ser << " sectors " << (lba_count_>>1) << " KB\n";
    }

    void read_sector(uint32_t lba, void* buffer) {
        REQUIRE(lba < lba_count_);
        REQUIRE((lba>>28) == 0);
        wait_status();
        out(port_offset::drive,        0xE0 | (dev_info_.slave << drive_slave_bit) | ((lba >> 24) & 0xf));
        out(port_offset::sector_count, 0x01);
        out(port_offset::lba_low,      lba & 0xff);
        out(port_offset::lba_mid,      (lba >> 8) & 0xff);
        out(port_offset::lba_high,     (lba >> 16) & 0xff);
        out(port_offset::command,      command_read_pio);

        REQUIRE(wait_status() & status_mask_drq);
        __inwordstring(port_number(port_offset::data), reinterpret_cast<uint16_t*>(buffer), sector_size_bytes/2);
    }

private:
    const device_info& dev_info_;
    uint32_t lba_count_;

    uint16_t port_number(port_offset port) {
        return dev_info_.first_command_block_register + static_cast<uint16_t>(port);
    }

    uint8_t inbyte(port_offset port) {
        return __inbyte(port_number(port));
    }

    void out(port_offset port, uint8_t value) {
        __outbyte(port_number(port), value);
    }

    void delay() {
        for (int i = 0; i < 4; ++i) {
            __inbyte(dev_info_.control_block_register);
        }
    }

    uint8_t wait_status() {
         for (int cnt = 0; cnt < 1000; ++cnt) {
             delay();
             const auto status = inbyte(port_offset::status);
             if (!(status & status_mask_bsy)) {
                 if (status & (status_mask_err | status_mask_df)) {
                    dbgout() << "ATA device in failure mode: " << as_hex(status) << "\n";
                    REQUIRE(false);
                 }
                 return status;
            }
         }
         fatal_error(__FILE__, __LINE__, "timed out in ata::device::wait_ready");
    }
};

void test() {
    device dev{primary_master};
    uint8_t boot_sector[512];
    dev.read_sector(0, boot_sector);
    //hexdump(dbgout(), boot_sector, sizeof(boot_sector));
    REQUIRE(boot_sector[510] == 0x55);
    REQUIRE(boot_sector[511] == 0xAA);
}

} } // namespace attos::ata
