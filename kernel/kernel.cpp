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
    print_default_stack_trace(dbgout(), 1);
    dbgout() << file << ':' << line << ": " << detail << ". Hanging\n";
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

enum class kernel_object_protocol_number {
    read,
    write,
    process,
};

template<kernel_object_protocol_number>
struct kernel_object_protocol_traits;

template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::read> { using type = in_stream; };
template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::write> { using type = out_stream; };
class user_process;
template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::process> { using type = user_process; };

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

template<typename Derived, kernel_object_protocol_number... protocols>
class __declspec(novtable) kernel_object_helper : public kernel_object {
private:
    template<kernel_object_protocol_number... ns>
    struct get_protocol_impl;

    template<>
    struct get_protocol_impl<> {
        static void* get(Derived*, kernel_object_protocol_number protocol) {
            dbgout() << "protocol " << int(protocol) << " not supported\n";
            REQUIRE(false);
        }
    };

    template<kernel_object_protocol_number n0, kernel_object_protocol_number... ns>
    struct get_protocol_impl<n0, ns...> {
        static void* get(Derived* self, kernel_object_protocol_number protocol) {
            if (protocol == n0) {
                return static_cast<typename kernel_object_protocol_traits<n0>::type*>(self);
            }
            return get_protocol_impl<ns...>::get(self, protocol);
        }
    };

    virtual void* do_get_protocol(kernel_object_protocol_number protocol) {
        return get_protocol_impl<protocols...>::get(static_cast<Derived*>(this), protocol);
    }
};

class user_process : public kernel_object_helper<user_process, kernel_object_protocol_number::process> {
public:
    explicit user_process() : mm_(create_default_memory_manager()), context_() {
        next_ = first_process_;
        first_process_ = this;
    }

    user_process(user_process&&) = default;
    user_process& operator=(user_process&&) = default;

    ~user_process() {
        REQUIRE(state_ == states::created || state_ == states::exited);
        REQUIRE(current_process_ != this);
        for (const auto& o : objects_) {
            REQUIRE(!o && "Unfreed objects");
        }
        if (this == first_process_) {
            first_process_ = next_;
        } else {
            auto p = first_process_;
            while (p->next_ != this) {
                REQUIRE(p->next_);
                p = p->next_;
            }
            p->next_ = next_;
        }
    }

    bool running() const {
        return state_ == states::running;
    }

    uint64_t exit_code() const {
        REQUIRE(state_ == states::exited);
        return exit_code_;
    }

    registers& context() {
        REQUIRE(state_ != states::exited);
        return context_;
    }

    void image_base(virtual_address image_base) {
        REQUIRE(state_ == states::created);
        REQUIRE(!image_base_);
        image_base_ = image_base;
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

    void start() {
        REQUIRE(state_ == states::created);
        state_ = states::running;
    }

    void switch_from() {
        REQUIRE(state_ == states::running);
        REQUIRE(current_process_ == this);
        current_process_ = nullptr;
        hack_set_user_image(nullptr);
    }

    void switch_to(registers& regs) {
        REQUIRE(state_ == states::running);
        regs = context();
        set_as_current();
    }

    void switch_to(cpu_manager& cpum) {
        set_as_current();
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
        REQUIRE(!"Out of handles!");
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

    static kvector<user_process*> all_processes() {
        kvector<user_process*> res;
        for (auto p = first_process_; p; p = p->next_) {
            res.push_back(p);
        }
        return res;
    }

    static user_process& current() {
        REQUIRE(current_process_ != nullptr);
        return *current_process_;
    }

private:
    static constexpr int max_objects = 16;
    enum class states { created, running, exited } state_ = states::created;
    kowned_ptr<memory_manager>         mm_;
    kvector<physical_allocation>       allocations_;
    registers                          context_;
    uint64_t                           exit_code_ = 0;
    kowned_ptr<kernel_object>          objects_[max_objects];
    virtual_address                    image_base_;
    user_process*                      next_;

    static user_process*               current_process_;
    static user_process*               first_process_;

    void set_as_current() {
        REQUIRE(state_ == states::running);
        mm_->switch_to();
        if (image_base_) {
            hack_set_user_image((pe::IMAGE_DOS_HEADER*)(uint64_t)image_base_);
        }
        current_process_ = this;
    }
};
user_process* user_process::current_process_ = nullptr;
user_process* user_process::first_process_   = nullptr;

class ko_ethdev : public kernel_object_helper<ko_ethdev, kernel_object_protocol_number::read, kernel_object_protocol_number::write>, public in_stream, public out_stream {
public:
    explicit ko_ethdev() {REQUIRE(dev_);}
    virtual ~ko_ethdev() override {}

    auto& dev() { return *dev_; }

    virtual void write(const void* data, size_t n) override {
        interrupt_enabler ie{};
        dev_->send_packet(data, static_cast<uint32_t>(n));
    }

    virtual uint32_t read(void* out, uint32_t max) override {
        uint32_t count = 0;
        dev_->process_packets([&] (const uint8_t* data, uint32_t len) {
                REQUIRE(count == 0);
                REQUIRE(len <= max);
                memcpy(out, data, len);
                count = len;
            }, 1);
        return count;
    }

    static void set_dev(net::ethernet_device* dev) {
        dev_ = dev;
    }

private:
    static net::ethernet_device* dev_;
};
net::ethernet_device* ko_ethdev::dev_;

class ko_keyboard : public kernel_object_helper<ko_keyboard, kernel_object_protocol_number::read>, public in_stream {
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

void alloc_and_map_user_exe(user_process& proc, const pe::IMAGE_DOS_HEADER& image)
{
    REQUIRE(is_64bit_exe(image));
    const auto& nth = image.nt_headers();

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

    proc.image_base(image_base);

    auto& context = proc.context();
    context.cs  = user_cs;
    context.rip = image_base + image.nt_headers().OptionalHeader.AddressOfEntryPoint;
    context.ss  = user_ds;
    context.rsp = static_cast<uint64_t>(image_base) - 0x28;
    context.eflags = rflag_mask_res1;
}

template<typename T, typename... Args>
uint64_t create_object(Args&&... args) {
    return user_process::current().object_open(kowned_ptr<kernel_object>{knew<T>(static_cast<Args&&>(args)...).release()});
}

void syscall_handler(registers& regs)
{
    // TODO: Check user addresses...
    const auto n = static_cast<syscall_number>(regs.rax);
    regs.rax = 0; // Default return value
    switch (n) {
        case syscall_number::exit:
            {
                dbgout() << "[user] Exiting with error code " << as_hex(regs.rdx) << "\n";
                auto& last = user_process::current();
                last.switch_from();
                last.exit(regs.rdx);

                user_process* next = nullptr;
                for (auto p : user_process::all_processes()) {
                    if (p->running()) {
                        next = p;
                        break;
                    }
                }
                if (!next) {
                    dbgout() << "[user] All processes exited!\n";
                    kmemory_manager().switch_to(); // Switch back to pure kernel memory manager before restoring the original context
                    restore_original_context();
                    REQUIRE(false);
                }
                next->switch_to(regs);
                break;
            }
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
                    regs.rax = create_object<ko_ethdev>();
                } else if (string_equal(name, "keyboard")) {
                    regs.rax = create_object<ko_keyboard>();
                } else if(string_equal(name, "process")) {
                    regs.rax = create_object<user_process>();
                } else {
                    REQUIRE(!"Unknown device");
                }
                dbgout() << "[user] handle = " << as_hex(regs.rax) << "\n";
            }
            break;
        case syscall_number::destroy:
            user_process::current().object_close(regs.rdx);
            break;
        case syscall_number::write:
            {
                auto& os = user_process::current().object_get(regs.rdx).get_protocol<kernel_object_protocol_number::write>();
                os.write(reinterpret_cast<const void*>(regs.r8), static_cast<uint32_t>(regs.r9));
                break;
            }
        case syscall_number::read:
            {
                auto& is = user_process::current().object_get(regs.rdx).get_protocol<kernel_object_protocol_number::read>();
                regs.rax = is.read(reinterpret_cast<void*>(regs.r8), static_cast<uint32_t>(regs.r9));
                break;
            }
        case syscall_number::ethdev_hw_address:
            {
                auto& dev = static_cast<ko_ethdev&>(user_process::current().object_get(regs.rdx)).dev();
                const auto hw_address = dev.hw_address();
                memcpy(reinterpret_cast<void*>(regs.r8), &hw_address, sizeof(hw_address));
                break;
            }
        case syscall_number::start_exe:
            {
                dbgout() << "[user] Request to start executable @ " << as_hex(regs.r8) << " process handle " << as_hex(regs.rdx).width(2) << "\n";
                auto& proc = user_process::current().object_get(regs.rdx).get_protocol<kernel_object_protocol_number::process>();
                alloc_and_map_user_exe(proc, *reinterpret_cast<pe::IMAGE_DOS_HEADER*>(regs.r8));
                // Save original context
                user_process::current().context() = regs;
                user_process::current().switch_from();
                // Set new context
                proc.start();
                proc.switch_to(regs);
            }
            break;
        case syscall_number::process_exit_code:
            {
                auto& proc = user_process::current().object_get(regs.rdx).get_protocol<kernel_object_protocol_number::process>();
                regs.rax = proc.exit_code();
                break;
            }
        default:
            dbgout() << "Got syscall 0x" << as_hex(regs.rax).width(0) << " from " << as_hex(regs.rcx) << " flags = " << as_hex(regs.r11) << "!\n";
            REQUIRE(!"Unimplemented syscall");
    }
    _disable(); // Disable interrupts again
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

    user_process proc;
    alloc_and_map_user_exe(proc, image);
    proc.start();
    dbgout() << "Doing magic!\n";
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
            ko_ethdev::set_dev(netdev.get());
            break;
        }
    }


    if (netdev) {
        auto data = net::tftp::nettest(*netdev, []() {
                return ps2::key_available() && ps2::read_key() == '\x1b';
            }, "test.txt");
        hexdump(dbgout(), data.begin(), data.size());
    }

    // User mode
    usermode_test(*cpu, user_exe);
    ko_ethdev::set_dev(nullptr);
}
