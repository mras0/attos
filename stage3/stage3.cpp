#include <stdint.h>
#include <intrin.h>
#include <type_traits>
#include <atomic>

#include <attos/cpu.h>
#include <attos/mem.h>
#include <attos/mm.h>
#include <attos/pe.h>
#include <attos/isr.h>
#include <attos/pci.h>
#include <attos/ata.h>
#include <attos/vga/text_screen.h>
#include <attos/net/i8254.h>
#include <attos/string.h>

#define assert REQUIRE // undefined yadayda
#include <attos/tree.h>

namespace attos {

namespace ps2 {
constexpr uint8_t data_port    = 0x60;
constexpr uint8_t status_port  = 0x64; // when reading
constexpr uint8_t command_port = 0x64; // when writing

constexpr uint8_t status_mask_output_full = 0x01;
constexpr uint8_t status_mask_input_fill  = 0x02;

uint8_t data() {
    return __inbyte(ps2::data_port);
}

uint8_t status() {
    return __inbyte(ps2::status_port);
}


constexpr uint8_t sc_invalid = 0;
constexpr uint8_t scan_code_set_1_size = 0x58;
constexpr uint8_t scan_code_set_1[scan_code_set_1_size] = {
    /* 0x00 */ sc_invalid,
    /* 0x01 */ '\x1b', // escape
    /* 0x02 */ '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    /* 0x0c */ '-',
    /* 0x0d */ '=',
    /* 0x0e */ '\x08', // backspace
    /* 0x0f */ '\x09', // tab
    /* 0x10 */ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\x0A', // (enter) newline
    /* 0x1D */ sc_invalid, // left control
    /* 0x1E */ 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
    /* 0x2A */ sc_invalid, // left shift
    /* 0x2B */ '\\',
    /* 0x2C */ 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/',
    /* 0x36 */ sc_invalid, // right shift
    /* 0x37 */ sc_invalid, // keypad *
    /* 0x38 */ sc_invalid, // left alt
    /* 0x39 */ ' ', // space
    /* 0x3A */ sc_invalid, // caps lock
    /* 0x3B */ sc_invalid, // f1
    /* 0x3C */ sc_invalid, // f2
    /* 0x3D */ sc_invalid, // ...
    /* 0x3E */ sc_invalid,
    /* 0x3F */ sc_invalid,
    /* 0x40 */ sc_invalid,
    /* 0x41 */ sc_invalid,
    /* 0x42 */ sc_invalid,
    /* 0x43 */ sc_invalid,
    /* 0x44 */ sc_invalid, // f10
    /* 0x45 */ sc_invalid, // numlock
    /* 0x46 */ sc_invalid, // scolllock
    /* 0x47 */ '7', // keypad 7
    /* 0x48 */ '8', // keypad 8
    /* 0x49 */ '9', // keypad 9
    /* 0x4A */ '-', // keypad -
    /* 0x4B */ '4', // keypad 4
    /* 0x4C */ '5', // keypad 5
    /* 0x4D */ '6', // keypad 6
    /* 0x4E */ '+', // keypad +
    /* 0x4F */ '1', // keypad 1
    /* 0x50 */ '2', // keypad 2
    /* 0x51 */ '3', // keypad 3
    /* 0x52 */ '0', // keypad 0
    /* 0x53 */ '.', // keypad .
    /* 0x54 */ sc_invalid,
    /* 0x55 */ sc_invalid,
    /* 0x56 */ sc_invalid,
    /* 0x57 */ sc_invalid, // f11
    /* 0x58 */ 'Z', // f12
};

template<typename T, size_t Size>
class fixed_size_queue {
public:
    fixed_size_queue() {
        static_assert((Size & (Size-1)) == 0, "");
        static_assert(std::is_trivially_copyable_v<T>, "");
    }

    bool empty() const {
        return size() == 0;
    }

    bool full() const {
        return size() == Size;
    }

    size_t size() const {
        return write_pos_ - read_pos_;
    }

    void push(T elem) {
        elems_[(write_pos_++) & (Size - 1)] = elem;
    }

    T pop() {
        return elems_[(read_pos_++) & (Size - 1)];
    }

    class iterator {
    public:
        bool operator==(const iterator& rhs) const { return pos_ == rhs.pos_; }
        bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

        iterator& operator++() {
            ++pos_;
            return *this;
        }

        auto* operator->() {
            return &*this;
        }

        auto& operator*() {
            return parent_->elems_[pos_ & (Size - 1)];
        }

    private:
        iterator(const fixed_size_queue& parent, uint32_t pos) : parent_(&parent), pos_(pos) {}
        friend fixed_size_queue;
        const fixed_size_queue* parent_;
        uint32_t pos_;
    };

    iterator begin() const { return iterator{*this, read_pos_}; }
    iterator end() const { return iterator{*this, write_pos_}; }

private:
    T elems_[Size];
    uint32_t read_pos_  = 0;
    uint32_t write_pos_ = 0;
};


class controller : public singleton<controller> {
public:
    explicit controller() {
        reg_ = register_irq_handler(irq, [this]() { isr(); });
    }

    ~controller() {
        reg_.reset();
    }

    bool key_available() const {
        interrupt_disabler id{};
        // Peek to see if there any key releases
        return std::any_of(buffer_.begin(), buffer_.end(), [](uint8_t c) { return c > 0x80 && c < 0xE0; });
    }

    uint8_t read_key() {
        // Very crude translation of scan keys in Scan Code Set 1
        for (;;) {
            auto sk = get_scan_key();
            if (sk == 0xE0) {
                auto sk2 = get_scan_key();
                dbgout() << "[ps2] ignoring scan key = 0xE0 0x" << as_hex(sk2) << "\n";
                continue;
            } else if (sk == 0xE1) {
                auto sk2 = get_scan_key();
                auto sk3 = get_scan_key();
                dbgout() << "[ps2] ignoring scan key = 0xE1 0x" << as_hex(sk2) << " 0x" << as_hex(sk3) << "\n";
                continue;
            }

            // Only act on key release as they are not repeated
            const bool released = (sk & 0x80) != 0;
            if (!released) continue;
            sk &= ~0x80;

            if (sk < scan_code_set_1_size) {
                const auto k = scan_code_set_1[sk];
                if (k != sc_invalid) {
                    return k;
                }
            }
            dbgout() << "[ps2] ignoring scan key = 0x" << as_hex(sk) << "\n";
        }
    }

private:
    static constexpr uint8_t irq = 1;

    isr_registration_ptr reg_;
    fixed_size_queue<uint8_t, 32> buffer_;

    bool buffer_empty() {
        interrupt_disabler id{};
        return buffer_.empty();
    }

    uint8_t get_scan_key() {
        while (buffer_empty()) {
            __halt();
        }
        interrupt_disabler id{};
        return buffer_.pop();
    }


    void isr() {
        const auto st = status();
        REQUIRE(st & status_mask_output_full);
        if (st & ~status_mask_output_full) {
            dbgout() << "[ps2] Status = 0x" << as_hex(st) << "\n";
        }

        if (!buffer_.full()) {
            buffer_.push(data());
        } else {
            dbgout() << "[ps2] Keyboard buffer is full!!\n";
        }
    }
};

} // namespace ps2

uint8_t read_key() {
    if (ps2::controller::has_instance()) {
        return ps2::controller::instance().read_key();
    }

    for (;;) {
        while (!(ps2::status() & ps2::status_mask_output_full)) { // wait key
            _mm_pause();
        }
        const uint8_t scan_code = ps2::data();  // read key
        if (scan_code > 0x80 && scan_code < 0xE0) { // crude wait for release of 'normal' key
            return scan_code;
        }
    }
}

enum class smap_type : uint32_t {
    end_of_list  = 0, // last entry in list (placed by stage2)
    available    = 1, // memory, available to OS
    reserved     = 2, // reserved, not available (e.g. system ROM, memory-mapped device)
    acpi_reclaim = 3, // ACPI Reclaim Memory (usable by OS after reading ACPI tables)
    acpi_nvs     = 4, // ACPI NVS Memory (OS is required to save this memory between NVS
};

#pragma pack(push, 1)
struct smap_entry {
    uint64_t  base;
    uint64_t  length;
    smap_type type;
};
#pragma pack(pop)

owned_ptr<memory_manager, destruct_deleter> construct_mm(const smap_entry* smap, const pe::IMAGE_DOS_HEADER& image_base)
{
    // Find suitable place to construct initial memory manager
    physical_address base_addr{};
    uint64_t         base_len  = 0;

    dbgout() << "Base             Length           Type\n";
    dbgout() << "FEDCBA9876543210 FEDCBA9876543210 76543210\n";

    constexpr uint64_t min_len_megabytes = 4;
    constexpr uint64_t min_base = 1ULL << 20;
    constexpr uint64_t min_len  = min_len_megabytes << 20;
    constexpr uint64_t max_base = identity_map_length - min_len_megabytes;

    // TODO: Handle unaligned areas
    for (auto e = smap; e->type != smap_type::end_of_list; ++e) {
        dbgout() << as_hex(e->base) << ' ' << as_hex(e->length) << ' ' << as_hex(static_cast<uint32_t>(e->type));
        if (e->type == smap_type::available && e->base >= min_base && e->base <= max_base && e->length >= min_len) {
            if (!base_len) {
                // Selected this one
                base_addr = physical_address{e->base};
                base_len  = e->length;
                dbgout() << " *\n";
            } else {
                // We would have selected this one
                dbgout() << " +\n";
            }
        } else {
            dbgout() << " -\n";
        }
    }

    REQUIRE(base_len != 0);
    auto mm = mm_init(base_addr, base_len);
    // Handle identity map
    static_assert(identity_map_length == 1<<30, "");
    mm->map_memory(virtual_address(identity_map_start), identity_map_length, memory_type_rwx | memory_type::ps_2mb, physical_address{0ULL});

    // Map in kernel executable image
    const auto& nth = image_base.nt_headers();
    const auto image_phys = physical_address::from_identity_mapped_ptr(&image_base);
    const auto image_size = round_up(static_cast<uint64_t>(nth.OptionalHeader.SizeOfImage), memory_manager::page_size);
    mm->map_memory(virtual_address{nth.OptionalHeader.ImageBase}, image_size, memory_type_rwx, image_phys);

    // Switch to the new PML4
    __writecr3(mm->pml4());
    return mm;
}

} // namespace attos

using namespace attos;

struct arguments {
    const pe::IMAGE_DOS_HEADER& image_base() const {
        return *static_cast<const pe::IMAGE_DOS_HEADER*>(image_base_);
    }

    const uint8_t* orig_file_data() const {
        return static_cast<const uint8_t*>(orig_file_data_);
    }

    const smap_entry* smap_entries() const {
        return static_cast<const smap_entry*>(smap_entries_);
    }

private:
    physical_address image_base_;
    physical_address orig_file_data_;
    physical_address smap_entries_;
};


class interrupt_timer : public singleton<interrupt_timer> {
public:
    explicit interrupt_timer() {
        reg_ = register_irq_handler(irq, [this]() { isr(); });
    }
    ~interrupt_timer() {
        reg_.reset();
        dbgout() << "[pit] " << pit_ticks_ << " ticks elapsed\n";
    }

private:
    std::atomic<uint64_t> pit_ticks_{0};
    isr_registration_ptr reg_;

    static constexpr uint8_t irq = 0;
    void isr() {
        ++pit_ticks_;
        ++*static_cast<uint8_t*>(physical_address{0xb8000});
    }
};

template<typename T>
struct push_back_stream_adapter : public out_stream {
    explicit push_back_stream_adapter(T& c) : c_(c) {
    }

    virtual void write(const void* data, size_t size) override {
        auto src = reinterpret_cast<const uint8_t*>(data);
        while (size--) {
            c_.push_back(*src++);
        }
    }

private:
    T& c_;
};

__declspec(noinline) void test_func()
{
    __debugbreak();
}

__declspec(noinline) void test_func2()
{
    dbgout() << "Trigger interrupt 0x80\n";
    sw_int<0x80>();
}

void interactive_mode(ps2::controller& ps2c)
{
    dbgout() << "Interactive mode. Use escape to quit.\n";

    kvector<char> cmd;
    push_back_stream_adapter<kvector<char>> cmd_stream(cmd);
    for (;;) {
        if (ps2c.key_available()) {
            auto k = ps2c.read_key();
            if (k == '\x1b') {
                dbgout() << "\nEscape pressed\n";
                return;
            } else if (k == '\n') {
                if (!cmd.empty()) {
                    cmd.push_back('\0');
                    if (string_equal(cmd.begin(), "EXIT")) {
                        dbgout() << "\nExit\n";
                        return;
                    } else if (string_equal(cmd.begin(), "B")) {
                        dbgout() << "\nBreak\n";
                        test_func();
                    } else if (string_equal(cmd.begin(), "T")) {
                        dbgout() << "\nTest\n";
                        test_func2();
                    } else {
                        dbgout() << "\nCOMMAND IGNORED: '" << cmd.begin() << "'\n";
                    }
                    cmd.clear();
                }
            } else {
                cmd_stream << (char)k;
                dbgout() << (char)k;
            }
        }
        _mm_pause();
    }
}

void usermode_test(cpu_manager& cpum)
{
    const auto old_cr3 = __readcr3();

    const physical_address user_area{4<<20};
    auto user_area_ptr = static_cast<uint8_t*>(user_area);
    user_area_ptr[0] = 0xCD; user_area_ptr[1] = 0x80; // int 0x80

    const virtual_address user_area_virt{1<<16};
    const auto user_rsp = static_cast<uint64_t>(user_area + (1<<20));
    auto mm = create_default_memory_manager();
    mm->map_memory(user_area_virt, memory_manager::page_size, memory_type_rwx | memory_type::user, user_area);
    //print_page_tables(mm->pml4());

    __writecr3(mm->pml4()); // set user process PML4
    dbgout() << "Doing magic!\n";
    cpum.switch_to_context(user_cs, user_area_virt, user_ds, user_rsp, __readeflags());
    __writecr3(old_cr3); // restore CR3
    dbgout() << "Bach from magic!\n";
}

namespace attos { namespace net {

class udp_socket {
public:
    using send_function_type = function<void (uint8_t*, uint32_t)>;
    using unregister_function_type = function<void (void)>;

    explicit udp_socket(send_function_type send_func, unregister_function_type unregister_func, ipv4_address local_addr, uint16_t local_port)
        : send_func_(send_func)
        , unregister_func_(unregister_func)
        , local_addr_(local_addr)
        , local_port_(local_port) {
        REQUIRE(local_port != 0);
    }

    ~udp_socket() {
        unregister_func_();
    }

    ipv4_address local_addr() const { return local_addr_; }
    uint16_t     local_port() const { return local_port_; }

    void sendto(ipv4_address remote_addr, uint16_t remote_port, const void* data, uint32_t length) {
        REQUIRE(length <= sizeof(send_buffer_) - (sizeof(ethernet_header) + sizeof(ipv4_header) + sizeof(udp_header)));

        uint8_t* b = &send_buffer_[sizeof(ethernet_header)];

        auto& ih = *reinterpret_cast<ipv4_header*>(b);
        b += sizeof(ipv4_header);
        memset(&ih, 0, sizeof(ipv4_header));
        ih.ihl      = sizeof(ipv4_header)/4;
        ih.ver      = 4;
        ih.length   = static_cast<uint16_t>(sizeof(ipv4_header) + sizeof(udp_header) + length);
        ih.ttl      = 64;
        ih.protocol = ip_protocol::udp;
        ih.src      = local_addr_;
        ih.dst      = remote_addr;
        ih.checksum = inet_csum(&ih, sizeof(ih));

        auto& uh = *reinterpret_cast<udp_header*>(b);
        b += sizeof(udp_header);
        uh.src_port = local_port_;
        uh.dst_port = remote_port;
        uh.length   = static_cast<uint16_t>(sizeof(udp_header) + length);
        uh.checksum = 0;

        memcpy(b, data, length);
        b += length;

        send_func_(send_buffer_, static_cast<uint32_t>(b-&send_buffer_[0]));
    }

private:
    send_function_type          send_func_;
    unregister_function_type    unregister_func_;
    ipv4_address                local_addr_;
    uint16_t                    local_port_;
    uint8_t                     send_buffer_[ethernet_max_bytes];
    uint8_t                     recv_buffer_[ethernet_max_bytes];
    bool                        recv_buffer_empty_ = true;
};

class ipv4_ethernet_device {
public:
    explicit ipv4_ethernet_device(ethernet_device& ethdev) : ethdev_{ethdev}, ip_addr_{inaddr_any} {
    }

    kowned_ptr<udp_socket> udp_open(ipv4_address local_addr, uint16_t local_port, const packet_process_function& recv_func) {
        REQUIRE(local_port != 0); // TODO: Support binding to an open port
        REQUIRE(local_addr == ip_addr_ || local_addr == inaddr_any);
        REQUIRE(!find_open_udp_socket(local_addr, local_port));
        dbgout() << "[udp] Opening " << local_addr << ':' << local_port << "\n";
        auto s = knew<udp_socket>([this] (uint8_t* data, uint32_t length) { ipv4_out(data, length); }, [this, local_addr, local_port] { udp_close(local_addr, local_port); }, local_addr, local_port);
        udp_sockets_.push_back({s.get(), recv_func});
        return s;
    }

    void process_packets() {
        ethdev_.process_packets([this] (const uint8_t* data, uint32_t length) { eth_in(data, length); });
    }

    mac_address hw_address() const {
        return ethdev_.hw_address();
    }

    void ip_address(ipv4_address addr) {
        REQUIRE(ip_addr_ == inaddr_any);
        ip_addr_ = addr;
    }

private:
    struct arp_entry {
        ipv4_address pa;   // Protocol address
        mac_address  ha;   // Hardware address
    };
    struct open_udp_socket {
        udp_socket*             socket;
        packet_process_function recv_func;
    };

    ethernet_device&            ethdev_;
    ipv4_address                ip_addr_;
    kvector<arp_entry>          arp_entries_;
    kvector<open_udp_socket>    udp_sockets_;

    void eth_in(const uint8_t* data, uint32_t length) {
        REQUIRE(length >= sizeof(ethernet_header));
        const auto& eh = * reinterpret_cast<const ethernet_header*>(data);
        data   += sizeof(ethernet_header);
        length -= sizeof(ethernet_header);

        switch (eh.type) {
            case net::ethertype::ipv4:
                {
                    REQUIRE(length >= sizeof(ipv4_header));
                    const auto& ih = *reinterpret_cast<const ipv4_header*>(data);
                    REQUIRE(ih.ver == 4);
                    REQUIRE(ih.ihl >= 5);
                    REQUIRE(ih.ihl*4U <= length);
                    REQUIRE(ih.length <= length);
                    REQUIRE(inet_csum(&ih, ih.ihl * 4) == 0);
                    ipv4_in(ih, data + ih.ihl * 4, ih.length - ih.ihl * 4);
                    break;
                }
            case net::ethertype::arp:
                REQUIRE(length >= sizeof(arp_header));
                arp_in(*reinterpret_cast<const arp_header*>(data));
                break;
            case net::ethertype::ipv6:
                REQUIRE(length >= 40);
                REQUIRE((data[0]>>4) == 6); // Version
                dbgout() << "[ipv6] Ignoring IPv6 packet\n";
                break;
            default:
                hexdump(dbgout(), data, length);
                dbgout() << "Dst MAC:  " << eh.dst << "\n";
                dbgout() << "Src MAC:  " << eh.src << "\n";
                dbgout() << "Type:     " << as_hex(static_cast<uint16_t>(eh.type)).width(4) << "\n";
                dbgout() << "Length:   " << length << "\n";
                dbgout() << "Unknown ethernet type\n";
                REQUIRE(false);
        }
    }

    //
    // ARP
    //
    void arp_in(const arp_header& ah) {
        REQUIRE(ah.htype == arp_htype::ethernet);
        REQUIRE(ah.ptype == ethertype::ipv4);
        REQUIRE(ah.hlen  == 0x06);
        REQUIRE(ah.plen  == 0x04);
        REQUIRE(ah.oper == arp_operation::request || ah.oper == arp_operation::reply);
        REQUIRE(ah.sha != mac_address::broadcast);
        dbgout() << "[arp] " << (ah.oper == arp_operation::request ? "REQ" : "RSP")
                 << " S " << ah.sha << " " << ah.spa
                 << " T " << ah.tha << " " << ah.tpa << "\n";

        bool merge_flag = false;
        if (auto e = find_arp_entry(ah.spa)) {
            dbgout() << "[arp] Updating HW address for " << ah.spa << " to " << ah.sha << "\n";
            e->ha = ah.sha;
            merge_flag = true;
        }

        if (ip_addr_ == inaddr_any) {
            //dbgout() << "[arp] No IP address assigned yet\n";
            return;
        }

        if (ip_addr_ == ah.tpa) { // Are we the target?
            if (!merge_flag) {
                dbgout() << "[arp] Adding ARP entry for " << ah.spa << " -> " << ah.sha << "\n";
                add_arp_entry(ah.spa, ah.sha);
            }
            if (ah.oper == arp_operation::request) {
                // Swap hardware and protocol fields, putting the local hardware and protocol addresses in the sender fields.
                // Set the ar$op field to ares_op$REPLY
                // Send the packet to the (new) target hardware address on the same hardware on which the request was received.
                dbgout() << "[arp] Sending ARP reply for " << ah.tpa << " to " << ah.spa << "\n";
                send_arp(arp_operation::reply, ip_addr_, ah.sha, ah.spa);
            }
        }
    }

    arp_entry* find_arp_entry(ipv4_address ip) {
        auto it = std::find_if(arp_entries_.begin(), arp_entries_.end(), [ip] (const arp_entry& e) { return e.pa == ip; });
        return it != arp_entries_.end() ? &*it : nullptr;
    }

    void add_arp_entry(ipv4_address pa, const mac_address& ha) {
        REQUIRE(!find_arp_entry(pa));
        arp_entries_.push_back({pa, ha});
    }

    void send_arp(arp_operation oper, ipv4_address spa, mac_address tha, ipv4_address tpa) {
        uint8_t buffer[sizeof(ethernet_header) + sizeof(arp_header)];
        auto& eh = *reinterpret_cast<ethernet_header*>(buffer);
        auto& ah = *reinterpret_cast<arp_header*>(buffer + sizeof(ethernet_header));
        eh.dst   = tha;
        eh.src   = ethdev_.hw_address();
        eh.type  = ethertype::arp;
        ah.htype = arp_htype::ethernet;
        ah.ptype = ethertype::ipv4;
        ah.hlen  = 0x06;
        ah.plen  = 0x04;
        ah.oper  = oper;
        ah.sha   = eh.src;
        ah.spa   = spa;
        ah.tha   = tha;
        ah.tpa   = tpa;
        ethdev_.send_packet(buffer, sizeof(buffer));
    }

    //
    // IPv4
    //
    void ipv4_in(const ipv4_header& ih, const uint8_t* data, uint32_t length) {
        switch (ih.protocol) {
            case ip_protocol::icmp:
                REQUIRE(length >= sizeof(icmp_header));
                REQUIRE(inet_csum(data, static_cast<uint16_t>(length)) == 0);
                icmp_in(ih, *reinterpret_cast<const icmp_header*>(data), data + sizeof(icmp_header), length - sizeof(icmp_header));
                break;
            case ip_protocol::igmp:
                dbgout() << "[ipv4] Ignoring IGMP message src = " << ih.src << " dst = " << ih.dst << "\n";
                break;
            case ip_protocol::tcp:
                dbgout() << "[ipv4] Ignoring TCP message src = " << ih.src << " dst = " << ih.dst << "\n";
                break;
            case ip_protocol::udp:
                REQUIRE(length >= sizeof(udp_header));
                udp_in(ih, *reinterpret_cast<const udp_header*>(data), data + sizeof(udp_header), length - sizeof(udp_header));
                break;
            default:
                hexdump(dbgout(), data, length);
                dbgout() << "[ipv4] Unhandled protocol = " << as_hex(ih.protocol) << " src = " << ih.src << " dst = " << ih.dst << "\n";
                REQUIRE(false);
        }
    }

    // assumes room for ethernet header at front with ipv4 header and the rest of the packet immediately following
    void ipv4_out(uint8_t* data, uint32_t length) {
        REQUIRE(length >= sizeof(ethernet_header) + sizeof(ipv4_header) && length <= ethernet_max_bytes);
        auto& eh = *reinterpret_cast<ethernet_header*>(data);
        auto& ih = *reinterpret_cast<ipv4_header*>(data + sizeof(ethernet_header));
        REQUIRE(ih.ver == 4 && ih.ihl == 5); // Sanity check, NOTE: IHL could legally be >= 5, but we know it isn't at the momemnt
        if (ih.dst == inaddr_broadcast) {
            eh.dst  = mac_address::broadcast;
        } else if (ih.dst == inaddr_any) {
            REQUIRE(!"Invalid destination IP");
        } else {
            if (auto ae = find_arp_entry(ih.dst)) {
                eh.dst = ae->ha;
            } else {
                dbgout() << "[arp] Sending ARP request for " << ih.dst << "\n";
                send_arp(arp_operation::request, ip_addr_, mac_address::broadcast, ih.dst);
                return;
            }
        }
        eh.src  = ethdev_.hw_address();
        eh.type = ethertype::ipv4;
        ethdev_.send_packet(data, length);
    }

    //
    // ICMP
    //
    void icmp_in(const ipv4_header& ih, const icmp_header& icmp_h, const uint8_t* data, uint32_t length) {
        uint8_t buffer[ethernet_max_bytes];
        if (ip_addr_ != inaddr_any && ih.dst == ip_addr_) {
            switch (icmp_h.type) {
                case icmp_type::echo_request:
                    {
                        REQUIRE(icmp_h.code == 0);
                        REQUIRE(length < sizeof(buffer) - (sizeof(ethernet_header) + sizeof(ipv4_header) + sizeof(icmp_header)));

                        auto b = &buffer[sizeof(ethernet_header)];

                        auto& oih = *reinterpret_cast<ipv4_header*>(b);
                        b += sizeof(ipv4_header);
                        memset(&oih, 0, sizeof(ipv4_header));
                        oih.ihl      = sizeof(ipv4_header)/4;
                        oih.ver      = 4;
                        oih.length   = static_cast<uint16_t>(sizeof(ipv4_header) + sizeof(icmp_header) + length);
                        oih.ttl      = 64;
                        oih.protocol = ip_protocol::icmp;
                        oih.src      = ih.dst;
                        oih.dst      = ih.src;
                        oih.checksum = inet_csum(&oih, sizeof(oih));

                        auto& oicmp = *reinterpret_cast<icmp_header*>(b);
                        b += sizeof(icmp_header);
                        oicmp.type = icmp_type::echo_reply;
                        oicmp.code = 0;
                        oicmp.checksum = 0;
                        oicmp.rest_of_header = icmp_h.rest_of_header;

                        memcpy(b, data, length);
                        b += length;

                        oicmp.checksum = inet_csum(&oicmp, static_cast<uint16_t>(sizeof(icmp_header) + length));

                        ipv4_out(buffer, static_cast<uint16_t>(b - buffer));
                        return;
                    }
                default:
                    break;
            }
        }
        dbgout() << "[icmp] Ignoring type " << as_hex(static_cast<uint8_t>(icmp_h.type)) << " code " << as_hex(icmp_h.code) << " from " << ih.src << " to " << ih.dst << "\n";
    }

    //
    // UDP
    //

    open_udp_socket* find_open_udp_socket(ipv4_address local_addr, uint16_t local_port) {
        auto it = std::find_if(udp_sockets_.begin(), udp_sockets_.end(),
                [local_addr, local_port] (const open_udp_socket& s) {
                    return (s.socket->local_addr() == inaddr_any || s.socket->local_addr() == local_addr) && s.socket->local_port() == local_port;
                });
        return it != udp_sockets_.end() ? &*it : nullptr;
    }

    void udp_in(const ipv4_header& ih, const udp_header& uh, const uint8_t* data, uint32_t length) {
        if (auto s = find_open_udp_socket(ih.dst, uh.dst_port)) {
            s->recv_func(data, length);
            return;
        }
        dbgout() << "[udp] Ignoring data from " << ih.src << ':' << uh.src_port << " to " << ih.dst << ':' << uh.dst_port << "\n";
        (void) data; (void) length;
    }

    void udp_close(ipv4_address local_addr, uint16_t port) {
        dbgout() << "[udp] Closing " << local_addr << ':' << port << "\n";
        auto s = find_open_udp_socket(local_addr, port);
        REQUIRE(s != nullptr);
        udp_sockets_.erase(s);
    }
};

class dhcp_handler {
public:
    explicit dhcp_handler(ipv4_ethernet_device& dev)
        : dev_{dev}
        , s_{dev_.udp_open(inaddr_any, dhcp_src_port, [this] (const uint8_t* data, uint32_t length) { dhcp_in(data, length); })} {
        send_dhcp_discover();
    }

    ipv4_address address() const {
        REQUIRE((state_ == state::finished) == (ip_ != inaddr_any));
        return ip_;
    }

private:
    ipv4_ethernet_device& dev_;
    kowned_ptr<udp_socket> s_;
    ipv4_address           ip_ = inaddr_any;
    enum class state { wait_for_offer, wait_for_ack, finished } state_ = state::wait_for_offer;
    static constexpr uint16_t dhcp_src_port = 68;
    static constexpr uint16_t dhcp_dst_port = 67;

#pragma pack(push, 1)
    struct dhcp_header : bootp_header {
        be_uint32_t       cookie;
        dhcp_option       message_type_opt;
        uint8_t           message_type_len;
        dhcp_message_type message_type;
    };
#pragma pack(pop)

    static constexpr uint32_t transaction_id_ = 0x2A2A2A2A; // TODO: Randomize
    uint8_t buffer_[512];

    uint8_t* start_request(dhcp_message_type message_type, uint16_t flags = 0) {
        auto& dh = *reinterpret_cast<dhcp_header*>(&buffer_[0]);

        memset(&dh, 0, sizeof(dh));
        dh.op       = bootp_operation::request;
        dh.htype    = static_cast<uint8_t>(arp_htype::ethernet);
        dh.hlen     = 6;
        dh.xid      = transaction_id_;
        dh.flags    = flags;
        dh.chaddr   = dev_.hw_address();

        dh.cookie           = dhcp_magic_cookie;
        dh.message_type_opt = dhcp_option::message_type;
        dh.message_type_len = 1;
        dh.message_type     = message_type;

        uint8_t* b = &buffer_[sizeof(dhcp_header)];
        *b++ = 0xff; // end_octet

        return &buffer_[sizeof(dhcp_header)];
    }

    void finish_request(uint8_t* b) {
        REQUIRE(b >= &buffer_[sizeof(dhcp_header)] && b < &buffer_[sizeof(buffer_)-1]);
        *b++ = static_cast<uint8_t>(dhcp_option::end);
        s_->sendto(inaddr_broadcast, dhcp_dst_port, buffer_, static_cast<uint16_t>(b - buffer_));
    }

    static uint8_t* put_option(uint8_t* b, dhcp_option opt, ipv4_address addr) {
        *b++ = static_cast<uint8_t>(opt);
        *b++ = static_cast<uint8_t>(sizeof(addr));
        *reinterpret_cast<ipv4_address*>(b) = addr;
        b += 4;
        return b;
    }

    struct dhcp_parse_result {
        const dhcp_header* dh = nullptr;
        ipv4_address       server_id = inaddr_any;
    };

    dhcp_parse_result parse_reply(dhcp_message_type expected_messge, const uint8_t* data, uint32_t length) const {
        REQUIRE(length >= sizeof(dhcp_header) + 1);
        auto& dh = *reinterpret_cast<const dhcp_header*>(data);

        dhcp_parse_result res;
        res.dh = &dh;

        REQUIRE(dh.op               == bootp_operation::reply);
        REQUIRE(dh.htype            == static_cast<uint8_t>(arp_htype::ethernet));
        REQUIRE(dh.hlen             == 6);
        REQUIRE(dh.xid              == transaction_id_);
        REQUIRE(dh.chaddr           == dev_.hw_address());
        REQUIRE(dh.cookie           == dhcp_magic_cookie);
        REQUIRE(dh.message_type_opt == dhcp_option::message_type);
        REQUIRE(dh.message_type_len == 1);
        REQUIRE(dh.message_type     == expected_messge);

        REQUIRE(dh.giaddr           == inaddr_any); // We want to be on the same subnet as the DHCP server for now

        data += sizeof(dhcp_header);
        length -= sizeof(dhcp_header);

        while (length > 1) {
            const auto type = static_cast<dhcp_option>(data[0]);
            if (type == dhcp_option::padding) continue;
            if (type == dhcp_option::end) break;

            const uint8_t len = data[1];
            REQUIRE(length >= len+2U);

            // Point at option data
            data   += 2;
            length -= 2;

            switch (type) {
            default:
                dbgout() << "[dhcp] Ignoring option " << static_cast<uint8_t>(type) << " of length " << len << "\n";
            case dhcp_option::subnet_mask:
                REQUIRE(len == 4);
                break;
            case dhcp_option::router:
                REQUIRE(len >= 4 && len % 4 == 0);
                break;
            case dhcp_option::domain_name_server:
                REQUIRE(len >= 4 && len % 4 == 0);
                break;
            case dhcp_option::domain_name:
                REQUIRE(len > 0);
                break;
            case dhcp_option::broadcast_address:
                REQUIRE(len == 4);
                break;
            case dhcp_option::netbios_name_server:
                REQUIRE(len >= 4 && len % 4 == 0);
                break;
            case dhcp_option::lease_time:
                REQUIRE(len == 4);
                break;
            case dhcp_option::server_identifier:
                REQUIRE(len == 4);
                res.server_id = *reinterpret_cast<const ipv4_address*>(data);
                REQUIRE(res.server_id != inaddr_any && res.server_id != inaddr_broadcast);
                break;
            case dhcp_option::renewal_time:
                REQUIRE(len == 4);
                break;
            case dhcp_option::rebinding_time:
                REQUIRE(len == 4);
                break;
            }

            // Advance past option data
            length -= len;
            data += len;
        }
        REQUIRE(length >= 1 && *data == 0xff);

        if (res.server_id == inaddr_any) {
            res.server_id = dh.siaddr;
        }

        return res;
    }

    void send_dhcp_discover() {
        dbgout() << "[dhcp] Sending DHCPDISCOVER\n";
        auto b = start_request(dhcp_message_type::discover, bootp_broadcast_flag);
        finish_request(b);
        state_ = state::wait_for_offer;
    }

    void send_dhcp_request(ipv4_address address, ipv4_address server_id) {
        dbgout() << "[dhcp] Sending DHCPREQUEST for " << address << "\n";
        auto b = start_request(dhcp_message_type::request, bootp_broadcast_flag);
        b = put_option(b, dhcp_option::requested_ip, address);
        b = put_option(b, dhcp_option::server_identifier, server_id);
        finish_request(b);
        state_ = state::wait_for_ack;
    }

    void dhcp_in(const uint8_t* data, uint32_t length) {
        switch (state_) {
        case state::wait_for_offer:
            {
                auto pr = parse_reply(dhcp_message_type::offer, data, length);
                dbgout() << "[dhcp] Got DHCPOFFER for " << pr.dh->yiaddr << " from " << pr.server_id << "\n";
                ip_ = pr.dh->yiaddr;
                state_ = state::wait_for_ack;
                send_dhcp_request(pr.dh->yiaddr, pr.server_id);
                break;
            }
        case state::wait_for_ack:
            {
                auto pr = parse_reply(dhcp_message_type::ack, data, length);
                dbgout() << "[dhcp] Got DHCPACK for " << pr.dh->yiaddr << " from " << pr.server_id << "\n";
                REQUIRE(pr.dh->yiaddr == ip_);
                state_ = state::finished;
                break;
            }
        }
    }
};

} } // namespace attos::net

bool escape_pressed()
{
    auto& ps2c = ps2::controller::instance();
    return ps2c.key_available() && ps2c.read_key() == '\x1b';
}

bool do_dhcp(net::ipv4_ethernet_device& ipv4dev)
{
    net::dhcp_handler dhcp_h{ipv4dev};
    while (!escape_pressed()) {
        if (dhcp_h.address() != net::inaddr_any) {
            dbgout() << "Got DHCP IP: " << dhcp_h.address() << "\n";
            ipv4dev.ip_address(dhcp_h.address());
            return true;
        }

        ipv4dev.process_packets();
        __halt();
    }
    return false;
}

void nettest(net::ethernet_device& dev)
{
    using namespace attos::net;

    ipv4_ethernet_device ipv4dev{dev};
    if (!do_dhcp(ipv4dev)) {
        return;
    }

    while (!escape_pressed()) {
        ipv4dev.process_packets();
        __halt();
    }
}

void stage3_entry(const arguments& args)
{
    // First make sure we can output debug information
    vga::text_screen ts;
    set_dbgout(ts);

    // Initialize GDT
    auto cpu = cpu_init();

    // Construct initial memory manager
    auto mm = construct_mm(args.smap_entries(), args.image_base());

    // Prepare debugging data
    const auto file_size = file_size_from_header(args.image_base());
    auto debug_info_text = (char*)args.orig_file_data() + file_size;
    // Initialize interrupt handlers
    auto ih = isr_init(debug_info_text);

    interrupt_timer timer{}; // IRQ0 PIT

    // PS2 controller
    ps2::controller ps2c{};

    // PCI
    auto pci = pci::init();

    // ATA
    //ata::test();

    // User mode
    usermode_test(*cpu);

    // Networking
    net::ethernet_device_ptr netdev{};

    for (const auto& d : pci->devices()) {
        if (!!(netdev = net::i82545_probe(d))) {
           break;
        }
    }

    if (netdev) {
        nettest(*netdev);
    }

    interactive_mode(ps2c);
}
