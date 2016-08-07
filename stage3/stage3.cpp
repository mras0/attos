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
#include <attos/net/i825x.h>
#include <attos/net/tftp.h>
#include <attos/string.h>
#include <attos/syscall.h>

#define assert REQUIRE // undefined yadayda
#include <attos/tree.h>

namespace attos {

namespace ps2 {
constexpr uint8_t data_port    = 0x60;
constexpr uint8_t status_port  = 0x64; // when reading
constexpr uint8_t command_port = 0x64; // when writing

constexpr uint8_t status_mask_output_full      = 0x01;
constexpr uint8_t status_mask_input_fill       = 0x02;
constexpr uint8_t status_mask_system_status    = 0x04;
constexpr uint8_t status_mask_is_command       = 0x08;
constexpr uint8_t status_mask_keyboard_lock    = 0x10;
constexpr uint8_t status_mask_transmit_timeout = 0x20;
constexpr uint8_t status_mask_receive_timeout  = 0x40;
constexpr uint8_t status_mask_parity_error     = 0x80;

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
    mm->map_memory(virtual_address(identity_map_start), identity_map_length, memory_type_rw | memory_type::ps_2mb, physical_address{0ULL});

    // Map in kernel executable image
    const auto& nth = image_base.nt_headers();
    const auto image_phys = physical_address::from_identity_mapped_ptr(&image_base);

    // Map headers
    mm->map_memory(virtual_address{nth.OptionalHeader.ImageBase}, round_up(nth.OptionalHeader.SizeOfHeaders, memory_manager::page_size), memory_type::read, image_phys);
    for (const auto& s : nth.sections()) {
        //dbgout() << format_str((char*)s.Name).width(8) << " " << as_hex(s.Characteristics) << " " << as_hex(s.VirtualAddress) << " " << as_hex(s.Misc.VirtualSize) << "\n";
        memory_type t{};
        if (s.Characteristics & pe::IMAGE_SCN_MEM_EXECUTE) t = t | memory_type::execute;
        if (s.Characteristics & pe::IMAGE_SCN_MEM_READ)    t = t | memory_type::read;
        if (s.Characteristics & pe::IMAGE_SCN_MEM_WRITE)   t = t | memory_type::write;
        mm->map_memory(virtual_address{nth.OptionalHeader.ImageBase + s.VirtualAddress}, round_up(s.Misc.VirtualSize, memory_manager::page_size), t, image_phys + s.PointerToRawData);
    }

    // Switch to the new PML4
    __writecr3(mm->pml4());
    return mm;
}

} // namespace attos

using namespace attos;

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
                    } else if (string_equal(cmd.begin(), "X")) {
                        dbgout() << "\nTesting NX\n";
                        using fp = void(*)(void);
                        auto f = (fp)(void*)"\xc3";
                        f();
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

extern "C" void syscall_handler(void);

net::ethernet_device* hack_netdev;

extern "C" void syscall_service_routine(registers& regs)
{
    dbgout() << "Got syscall 0x" << as_hex(regs.rax).width(0) << " from " << as_hex(regs.rcx) << " flags = " << as_hex(regs.r11) << "!\n";

    switch (static_cast<syscall_number>(regs.rax)) {
        case syscall_number::exit:
            dbgout() << "[user] Exiting with error code " << as_hex(regs.rdx) << "\n";
            restore_original_context();
            REQUIRE(false);
        case syscall_number::debug_print:
            dbgout() << "[user] '";
            dbgout().write(reinterpret_cast<const char*>(regs.rdx), regs.r8);
            dbgout() << "'\n";
            break;

        case syscall_number::ethdev_hw_address:
            REQUIRE("!ethdev_hw_address");
        // mac_address hw_address();
            break;
// void send_packet(const void* data, uint32_t length);
// void process_packets(const packet_process_function& ppf);
        default:
            REQUIRE(!"Unimplemented syscall");
    }
}

void alloc_and_copy_section(memory_manager& mm, virtual_address virt, uint32_t virt_size, memory_type t, const void* source, uint32_t src_size)
{
    constexpr auto page_mask = memory_manager::page_size-1;
    REQUIRE((virt & page_mask) == 0);

    const auto phys_size = round_up(virt_size, memory_manager::page_size);
    auto phys = alloc_physical(phys_size);

    if (src_size) {
        const auto to_copy = std::min(virt_size, src_size);
        memcpy(static_cast<void*>(phys), source, to_copy);
    }
    mm.map_memory(virt, phys_size, t, phys);
}

void alloc_and_map_user_exe(memory_manager& mm, const pe::IMAGE_DOS_HEADER& image)
{
    REQUIRE(is_64bit_exe(image));
    const auto& nth = image.nt_headers();

    const auto image_base = virtual_address{nth.OptionalHeader.ImageBase};

    // Map headers
    alloc_and_copy_section(mm, image_base, nth.OptionalHeader.SizeOfHeaders, memory_type::read | memory_type::user, &image, nth.OptionalHeader.SizeOfHeaders);

    // Map sections
    for (const auto& s : nth.sections()) {
        memory_type t = memory_type::user;
        if (s.Characteristics & pe::IMAGE_SCN_MEM_EXECUTE) t = t | memory_type::execute;
        if (s.Characteristics & pe::IMAGE_SCN_MEM_READ)    t = t | memory_type::read;
        if (s.Characteristics & pe::IMAGE_SCN_MEM_WRITE)   t = t | memory_type::write;
        alloc_and_copy_section(mm, image_base + s.VirtualAddress, s.Misc.VirtualSize, t, reinterpret_cast<const uint8_t*>(&image) + s.PointerToRawData, s.SizeOfRawData);
    }

    // Map stack
    const uint64_t stack_size = 0x1000;
    REQUIRE(nth.OptionalHeader.SizeOfStackCommit == stack_size);
    alloc_and_copy_section(mm, image_base - stack_size, stack_size, memory_type_rw | memory_type::user, nullptr, 0);

    hack_set_user_image(*(pe::IMAGE_DOS_HEADER*)(uint64_t)image_base);
}

void usermode_test(cpu_manager& cpum, const pe::IMAGE_DOS_HEADER& image)
{
    // Stress switch_to_context in kernel mode only
    for (int i = 0; i < 10; ++i) {
        cpum.switch_to_context(kernel_cs, (uint64_t)&restore_original_context, kernel_ds, (uint64_t)physical_address{6<<20}, __readeflags());
    }

    auto mm = create_default_memory_manager();

    const auto old_efer = __readmsr(msr_efer);
    const auto old_cr3 = __readcr3();

    // Enable SYSCALL
    static_assert(kernel_cs + 8 == kernel_ds, "");
    static_assert(user_ds + 8 == user_cs, "");
    __writemsr(msr_star, (static_cast<uint64_t>(kernel_cs) << 32) | ((static_cast<uint64_t>(user_ds - 8)) << 48));
    __writemsr(msr_lstar, reinterpret_cast<uint64_t>(&syscall_handler));
    __writemsr(msr_fmask, rflag_mask_tf | rflag_mask_if | rflag_mask_df | rflag_mask_iopl | rflag_mask_ac);
    __writemsr(msr_efer, old_efer | efer_mask_sce);

    alloc_and_map_user_exe(*mm, image);
    const uint64_t image_base = image.nt_headers().OptionalHeader.ImageBase;
    const uint64_t user_rsp = image_base - 0x28;
    const uint64_t user_rip = image_base + image.nt_headers().OptionalHeader.AddressOfEntryPoint;

    __writecr3(mm->pml4()); // set user process PML4
    dbgout() << "Doing magic!\n";
    cpum.switch_to_context(user_cs, user_rip, user_ds, user_rsp, __readeflags());
    __writecr3(old_cr3); // restore CR3
    __writemsr(msr_efer, old_efer); // Disable SYSCALL
    dbgout() << "Bach from magic!\n";
}

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

void stage3_entry(const arguments& args)
{
    // First make sure we can output debug information
    vga::text_screen ts;
    set_dbgout(ts);

    // Initialize GDT
    auto cpu = cpu_init();

    REQUIRE(is_64bit_exe(args.image_base()));

    // Construct initial memory manager
    auto mm = construct_mm(args.smap_entries(), args.image_base());

    // Prepare debugging data
    const auto file_size = file_size_from_header(args.image_base());
    auto debug_info_text = (char*)args.orig_file_data() + file_size;
    const auto& user_exe = *reinterpret_cast<const pe::IMAGE_DOS_HEADER*>(debug_info_text + string_length(debug_info_text) + 1);

    // Initialize interrupt handlers

    auto ih = isr_init(debug_info_text);

    interrupt_timer timer{}; // IRQ0 PIT

    // PS2 controller
    ps2::controller ps2c{};

    // PCI
    auto pci = pci::init();

    // ATA
    //ata::test();

    // Networking
    net::ethernet_device_ptr netdev{};

    for (const auto& d : pci->devices()) {
        if (!!(netdev = net::i825x::probe(d))) {
            hack_netdev = netdev.get();
            break;
        }
    }

    // User mode
    usermode_test(*cpu, user_exe);

    hack_netdev = nullptr;

    if (netdev) {
        nettest(*netdev, []() {
                auto& ps2c = ps2::controller::instance();
                return ps2c.key_available() && ps2c.read_key() == '\x1b';
            });
    }

    interactive_mode(ps2c);
}
