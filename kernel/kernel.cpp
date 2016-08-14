#include <stdint.h>
#include <intrin.h>
#include <type_traits>
#include <atomic>

#include <attos/mem.h>
#include <attos/pe.h>
#include "mm.h"
#include "cpu_manager.h"
#include "isr.h"
#include "pci.h"
#include "ata.h"
#include "text_screen.h"
#include "i825x.h"
#include "ps2.h"
#include <attos/net/tftp.h>
#include <attos/string.h>
#include <attos/syscall.h>
#include <attos/in_stream.h>

#define assert REQUIRE // undefined yadayda
#include <attos/tree.h>

namespace attos {

void yield() {
    __halt();
}

void fatal_error(const char* file, int line, const char* detail) {
    _disable();
    dbgout() << file << ':' << line << ": " << detail << ".\nHanging\n";
    bochs_magic();
    for (;;) {
        __halt();
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
        const memory_type t = pe::section_memory_type(s.Characteristics);
        mm->map_memory(virtual_address{nth.OptionalHeader.ImageBase + s.VirtualAddress}, round_up(s.Misc.VirtualSize, memory_manager::page_size), t, image_phys + s.PointerToRawData);
    }

    // Switch to the new PML4
    mm->switch_to();
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

void interactive_mode()
{
    dbgout() << "Interactive mode. Use escape to quit.\n";

    kvector<char> cmd;
    push_back_stream_adapter<kvector<char>> cmd_stream(cmd);
    for (;;) {
        if (ps2::key_available()) {
            auto k = ps2::read_key();
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

enum class kernel_object_protocol_number {
    read,
    write,
};

template<kernel_object_protocol_number>
struct kernel_object_protocol_traits;

template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::read> { using type = in_stream; };
template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::write> { using type = out_stream; };

class __declspec(novtable) kernel_object {
public:
    virtual ~kernel_object() = 0 {}

    template<kernel_object_protocol_number protocol>
    auto& get_protocol() {
        auto ptr = reinterpret_cast<typename kernel_object_protocol_traits<protocol>::type*>(do_get_protocol(protocol));
        REQUIRE(ptr);
        return *ptr;
    }

private:
    virtual void* do_get_protocol(kernel_object_protocol_number protocol) = 0;
};

class user_process : public kernel_object {
public:
    explicit user_process() : mm_(create_default_memory_manager()), context_() {
    }

    user_process(user_process&&) = default;
    user_process& operator=(user_process&&) = default;

    ~user_process() {
        REQUIRE(state_ == states::exited);
        for (const auto& o : objects_) {
            REQUIRE(!o);
        }
    }

    uint64_t exit_code() const {
        REQUIRE(state_ == states::exited);
        return exit_code_;
    }

    registers& context() {
        REQUIRE(state_ == states::created);
        return context_;
    }

    void alloc_and_copy_section(virtual_address virt, uint32_t virt_size, memory_type t, const void* source, uint32_t src_size) {
        REQUIRE(state_ == states::created);
        constexpr auto page_mask = memory_manager::page_size-1;
        REQUIRE((virt & page_mask) == 0);

        const auto phys_size = round_up(virt_size, memory_manager::page_size);
        allocations_.push_back(alloc_physical(phys_size));
        auto& phys = allocations_.back();

        if (src_size) {
            const auto to_copy = std::min(virt_size, src_size);
            memcpy(static_cast<void*>(phys.address()), source, to_copy);
        }
        mm_->map_memory(virt, phys_size, t, phys.address());
    }

    void switch_to(cpu_manager& cpum) {
        REQUIRE(state_ == states::created);
        state_ = states::running;
        mm_->switch_to();
        cpum.switch_to_context(context_);
    }

    void exit(uint64_t exit_code) {
        REQUIRE(state_ == states::running);
        for (const auto& o : objects_) {
            REQUIRE(!o);
        }
        exit_code_ = exit_code;
        state_ = states::exited;
    }

    // Handle = Index + 1

    uint64_t object_open(kowned_ptr<kernel_object>&& obj) {
        for (int i = 0; i < max_objects; ++i) {
            if (!objects_[i]) {
                objects_[i] = std::move(obj);
                return i + 1;
            }
        }
        REQUIRE(false);
        return 0;
    }

    bool handle_valid(uint64_t handle) {
        return handle >= 1 && handle <= max_objects && objects_[handle - 1];
    }

    void object_close(uint64_t handle) {
        REQUIRE(handle_valid(handle));
        objects_[handle - 1].reset();
    }

    kernel_object& object_get(uint64_t handle) {
        REQUIRE(handle_valid(handle));
        return *objects_[handle - 1];
    }

private:
    static constexpr int max_objects = 2;
    enum class states { created, running, exited } state_ = states::created;
    kowned_ptr<memory_manager>         mm_;
    kvector<physical_allocation>       allocations_;
    registers                          context_;
    uint64_t                           exit_code_ = 0;
    kowned_ptr<kernel_object>          objects_[max_objects];

    virtual void* do_get_protocol(kernel_object_protocol_number protocol) {
        dbgout() << "protocol " << int(protocol) << " not supported\n";
        REQUIRE(false);
        return nullptr;
    }
};

template<kernel_object_protocol_number... protocols>
class __declspec(novtable) kernel_object_helper : public kernel_object, public kernel_object_protocol_traits<protocols>::type... {
private:
    template<kernel_object_protocol_number... ns>
    struct get_protocol_impl;

    template<>
    struct get_protocol_impl<> {
        static void* get(kernel_object_helper*, kernel_object_protocol_number protocol) {
            dbgout() << "protocol " << int(protocol) << " not supported\n";
            REQUIRE(false);
        }
    };

    template<kernel_object_protocol_number n0, kernel_object_protocol_number... ns>
    struct get_protocol_impl<n0, ns...> {
        static void* get(kernel_object_helper* self, kernel_object_protocol_number protocol) {
            if (protocol == n0) {
                return static_cast<typename kernel_object_protocol_traits<n0>::type*>(self);
            }
            return get_protocol_impl<ns...>::get(self, protocol);
        }
    };

    virtual void* do_get_protocol(kernel_object_protocol_number protocol) {
        return get_protocol_impl<protocols...>::get(this, protocol);
    }
};

class ko_ethdev : public kernel_object_helper<kernel_object_protocol_number::read, kernel_object_protocol_number::write> { //: public kernel_object, public in_stream, public out_stream {
public:
    explicit ko_ethdev(net::ethernet_device* dev) : dev_(*dev) {
        dbgout() << "ko_ethdev::ko_ethdev MAC " << dev_.hw_address() << "\n";
    }

    virtual ~ko_ethdev() override {
        dbgout() << "ko_ethdev::~ko_ethdev\n";
    }

    auto& dev() { return dev_; }

    virtual void write(const void* data, size_t n) override {
        interrupt_enabler ie{};
        dev_.send_packet(data, static_cast<uint32_t>(n));
    }

    virtual uint32_t read(void* out, uint32_t max) override {
        uint32_t count = 0;
        dev_.process_packets([&] (const uint8_t* data, uint32_t len) {
                REQUIRE(count == 0);
                REQUIRE(len <= max);
                memcpy(out, data, len);
                count = len;
            }, 1);
        return count;
    }

private:
    net::ethernet_device& dev_;
};

class ko_keyboard : public kernel_object_helper<kernel_object_protocol_number::read> {
public:
    explicit ko_keyboard() {
        dbgout() << "ko_keyboard::ko_keyboard\n";
    }

    virtual ~ko_keyboard() override {
        dbgout() << "ko_keyboard::~ko_keyboard\n";
    }

    virtual uint32_t read(void* out, uint32_t max) override {
        uint32_t n = 0;
        while (n < max && ps2::key_available()) {
            reinterpret_cast<uint8_t*>(out)[n++] = ps2::read_key();
        }
        return n;
    }
private:
};


net::ethernet_device* hack_netdev;
user_process* hack_current_process_;

template<typename T, typename... Args>
uint64_t create_object(Args&&... args) {
    return hack_current_process_->object_open(kowned_ptr<kernel_object>{knew<T>(static_cast<Args&&>(args)...).release()});
}

void syscall_handler(registers& regs)
{
    // TODO: Check user addresses...
    const auto n = static_cast<syscall_number>(regs.rax);
    regs.rax = 0; // Default return value
    switch (n) {
        case syscall_number::exit:
            dbgout() << "[user] Exiting with error code " << as_hex(regs.rdx) << "\n";
            hack_current_process_->exit(regs.rdx);
            kmemory_manager().switch_to(); // Switch back to pure kernel memory manager before restoring the original context
            restore_original_context();
            REQUIRE(false);
        case syscall_number::debug_print:
            dbgout().write(reinterpret_cast<const char*>(regs.rdx), regs.r8);
            break;
        case syscall_number::yield:
            _enable();
            yield();
            break;
        case syscall_number::create:
            {
                const char* name = reinterpret_cast<const char*>(regs.rdx);
                dbgout() << "[user] create '" << name << "'\n";
                if (string_equal(name, "ethdev")) {
                    //regs.rax = hack_current_process_->object_open(kowned_ptr<kernel_object>{knew<ko_ethdev>(hack_netdev).release()});
                    regs.rax = create_object<ko_ethdev>(hack_netdev);
                } else if (string_equal(name, "keyboard")) {
                    regs.rax = create_object<ko_keyboard>();
                } else {
                    REQUIRE(!"Unknown device");
                }
            }
            break;
        case syscall_number::destroy:
            hack_current_process_->object_close(regs.rdx);
            break;
        case syscall_number::write:
            {
                auto& os = hack_current_process_->object_get(regs.rdx).get_protocol<kernel_object_protocol_number::write>();
                os.write(reinterpret_cast<const void*>(regs.r8), static_cast<uint32_t>(regs.r9));
                break;
            }
        case syscall_number::read:
            {
                auto& is = hack_current_process_->object_get(regs.rdx).get_protocol<kernel_object_protocol_number::read>();
                regs.rax = is.read(reinterpret_cast<void*>(regs.r8), static_cast<uint32_t>(regs.r9));
                break;
            }
        case syscall_number::ethdev_hw_address:
            {
                auto& dev = static_cast<ko_ethdev&>(hack_current_process_->object_get(regs.rdx)).dev();
                const auto hw_address = dev.hw_address();
                memcpy(reinterpret_cast<void*>(regs.r8), &hw_address, sizeof(hw_address));
                break;
            }
        default:
            dbgout() << "Got syscall 0x" << as_hex(regs.rax).width(0) << " from " << as_hex(regs.rcx) << " flags = " << as_hex(regs.r11) << "!\n";
            REQUIRE(!"Unimplemented syscall");
    }
    _disable(); // Disable interrupts again
}

user_process alloc_and_map_user_exe(const pe::IMAGE_DOS_HEADER& image)
{
    REQUIRE(is_64bit_exe(image));
    const auto& nth = image.nt_headers();

    user_process proc;

    const auto image_base = virtual_address{nth.OptionalHeader.ImageBase};

    // Map headers
    proc.alloc_and_copy_section(image_base, nth.OptionalHeader.SizeOfHeaders, memory_type::read | memory_type::user, &image, nth.OptionalHeader.SizeOfHeaders);

    // Map sections
    for (const auto& s : nth.sections()) {
        const memory_type t = pe::section_memory_type(s.Characteristics) | memory_type::user;
        proc.alloc_and_copy_section(image_base + s.VirtualAddress, s.Misc.VirtualSize, t, reinterpret_cast<const uint8_t*>(&image) + s.PointerToRawData, s.SizeOfRawData);
    }

    // Map stack
    const uint64_t stack_size = 0x1000 * 8;
    REQUIRE(nth.OptionalHeader.SizeOfStackCommit <= stack_size);
    proc.alloc_and_copy_section(image_base - stack_size, stack_size, memory_type_rw | memory_type::user, nullptr, 0);

    hack_set_user_image(*(pe::IMAGE_DOS_HEADER*)(uint64_t)image_base);

    auto& context = proc.context();
    context.cs  = user_cs;
    context.rip = image_base + image.nt_headers().OptionalHeader.AddressOfEntryPoint;
    context.ss  = user_ds;
    context.rsp = static_cast<uint64_t>(image_base) - 0x28;
    context.eflags = rflag_mask_res1;
    return proc;
}

void usermode_test(cpu_manager& cpum, const pe::IMAGE_DOS_HEADER& image)
{
    // Stress switch_to_context in kernel mode only
    for (int i = 0; i < 10; ++i) {
        registers context{};
        context.cs  = kernel_cs;
        context.rip = (uint64_t)&restore_original_context;
        context.ss  = kernel_ds;
        context.rsp = (uint64_t)physical_address{6<<20};
        context.eflags = static_cast<uint32_t>(__readeflags());
        cpum.switch_to_context(context);
    }

    syscall_enabler syscall_enabler_{&syscall_handler};

    user_process proc = alloc_and_map_user_exe(image);
    dbgout() << "Doing magic!\n";
    hack_current_process_ = &proc;
    proc.switch_to(cpum);
    dbgout() << "Bach from magic!\n";
    if (proc.exit_code()) {
        dbgout() << "Process exit code " << as_hex(proc.exit_code()) << " - press any key.\n";
        ps2::read_key();
    }
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

class kernel_debug_out : public out_stream {
public:
    explicit kernel_debug_out() : bochs_debug_out_(bochs_debug_out_available()) {
        set_dbgout(*this);
    }

private:
    static constexpr uint8_t bochs_debug_out_port = 0xe9;
    const bool bochs_debug_out_;
    vga::text_screen ts_;

    static bool bochs_debug_out_available() {
        // Detect bochs debug output facility
        return __inbyte(bochs_debug_out_port) == bochs_debug_out_port;
    }

    virtual void write(const void* data, size_t length) override {
        ts_.write(data, length);
        if (bochs_debug_out_) {
            auto d = reinterpret_cast<const uint8_t*>(data);
            while (length--) {
                __outbyte(bochs_debug_out_port, *d++);
            }
        }
    }
};

void stage3_entry(const arguments& args)
{
    // First make sure we can output debug information
    kernel_debug_out kdbgout{};

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
    auto ps2c = ps2::init();

    // PCI
    auto pci = pci::init();

    // ATA
    //ata::test();

    // Networking
    kowned_ptr<net::ethernet_device> netdev{};

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
        auto data = net::tftp::nettest(*netdev, []() {
                return ps2::key_available() && ps2::read_key() == '\x1b';
            }, "test.txt");
        hexdump(dbgout(), data.begin(), data.size());
    }

    interactive_mode();
}
