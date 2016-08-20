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

    hack_mem_map,
};

template<kernel_object_protocol_number>
struct kernel_object_protocol_traits;

template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::read> { using type = in_stream; };
template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::write> { using type = out_stream; };
class user_process;
template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::process> { using type = user_process; };
class mem_map_helper;
template<> struct kernel_object_protocol_traits<kernel_object_protocol_number::hack_mem_map> { using type = mem_map_helper; };

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

class mem_map_helper : kernel_object_helper<mem_map_helper, kernel_object_protocol_number::hack_mem_map> {
public:
    explicit mem_map_helper(memory_manager& mm, physical_address phys, uint64_t length, memory_type type)
        : mm_(&mm)
        , phys_(phys)
        , length_(length)
        , type_(type)
        , mapped_virt_(mm.map_memory(memory_manager::map_alloc_virt, mapped_length(), type, mapped_base())) {
    }
    mem_map_helper(mem_map_helper&& other) : mm_(nullptr), phys_(), length_(0), mapped_virt_() {
        *this = std::move(other);
    }
    mem_map_helper& operator=(mem_map_helper&& other) {
        std::swap(mm_, other.mm_);
        std::swap(phys_, other.phys_);
        std::swap(length_, other.length_);
        std::swap(type_, other.type_);
        std::swap(mapped_virt_, other.mapped_virt_);
        return *this;
    }
    mem_map_helper(const mem_map_helper&) = delete;
    mem_map_helper& operator=(const mem_map_helper&) = delete;

    ~mem_map_helper() {
        if (length_) {
            mm_->unmap_memory(mapped_virt_, mapped_length());
        }
    }

    uint8_t* ptr() {
        return reinterpret_cast<uint8_t*>(static_cast<uint64_t>(mapped_virt_) + (phys_ & (memory_manager::page_size-1)));
    }

    template<typename T>
    T& as() {
        return *reinterpret_cast<T*>(ptr());
    }

    uint64_t length() const {
        return length_;
    }

    memory_type type() const {
        return type_;
    }

private:
    memory_manager*  mm_;
    physical_address phys_;
    uint64_t         length_;
    memory_type      type_;
    virtual_address  mapped_virt_;

    physical_address mapped_base() const {
        return phys_ & ~(memory_manager::page_size - 1);
    }

    uint64_t mapped_length() const {
        const auto end_page = round_up(phys_ + length_, memory_manager::page_size);
        return end_page - static_cast<uint64_t>(mapped_base());
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
        for (auto& o : objects_) {
            if (o) {
                dbgout() << "[user] Warning: Unfreed object\n";
                o.reset();
            }
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

    memory_manager& mm() {
        return *mm_;
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
        interrupt_enabler ie{};
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

physical_address hack_dsdt_phys;
uint32_t hack_dsdt_len;

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
                } else if(string_equal(name, "hack-acpi-dsdt")) {
                    REQUIRE(hack_dsdt_phys && hack_dsdt_len);
                    regs.rax = create_object<mem_map_helper>(user_process::current().mm(), hack_dsdt_phys, hack_dsdt_len, memory_type::read | memory_type::user);
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
        case syscall_number::mem_map_info:
            {
                auto& mem_map = user_process::current().object_get(regs.rdx).get_protocol<kernel_object_protocol_number::hack_mem_map>();
                uint64_t* ptr = reinterpret_cast<uint64_t*>(regs.r8);
                ptr[0] = reinterpret_cast<uint64_t>(mem_map.ptr());
                ptr[1] = mem_map.length();
                ptr[2] = static_cast<uint64_t>(mem_map.type());
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

namespace attos { namespace acpi {

enum class rsdp_revision : uint8_t {
    v1 = 0,
    v2 = 2,
};

#pragma pack(push, 1)
constexpr char rsdp_signature[8] = { 'R', 'S', 'D' , ' ', 'P', 'T', 'R', ' ' };
struct root_system_description_pointer { // Root System Description Pointer Structure
    char          signature[8]; // "RSD PTR "
    uint8_t       checksum;     // Checksum of the first 20 bytes
    char          oem_id[6];    // OEM supplied ID
    rsdp_revision revision;     // 0 = ACPI 1.0, 2 = ACPI 2.0 or later
    uint32_t      rsdt_address; // Physical address of the RSDT
};
static_assert(sizeof(root_system_description_pointer) == 20, "");
static_assert(sizeof(rsdp_signature) == sizeof(root_system_description_pointer::signature), "");

struct root_system_description_pointer_v2 : root_system_description_pointer {
    uint32_t length;            // Length of the table
    uint64_t xsdt_address;      // Physical address of XSDT
    uint8_t  extended_checksum; // Checksum of entire structure
    uint8_t  reserved[3];
};
static_assert(sizeof(root_system_description_pointer_v2) == 36, "");

struct description {
    char     signature[4];      // ASCII signature of table
    uint32_t length;            // Length of table
    uint8_t  revision;          // Table revision number
    uint8_t  checksum;          // Checksum of entire table
    char     oem_id[6];         // OEM supplied ID
    char     oem_table_id[8];   // OEM supplied table ID
    uint32_t oem_revision;      // OEM revision number
    char     vendor_id[4];      // Vendor ID
    uint32_t vendor_revision;   // Vendor revision number
};
static_assert(sizeof(description) == 36, "");

struct multiple_apic_description : description {
    static constexpr char expected_signature[4] = { 'A', 'P', 'I', 'C' };
    uint32_t local_interrupt_controller_address; // Physical address where each CPU can access its local interrupt controller
    uint32_t flags;
    // Interrupt controller structure follows <id, length, data [length-2]>
};
static_assert(sizeof(multiple_apic_description) == 44, "");

struct fixed_acpi_description : description {
    static constexpr char expected_signature[4] = { 'F', 'A', 'C', 'P' };
    uint32_t firmware_ctrl;         // Physical address of the FACS
    uint32_t dsdt;                  // Physical address of the DSDT
    uint8_t  int_model;             // ACPI 1.0 INT_MODEL, reserved in ACPI 2.0+
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;               // System vector the SCI interrupt is wired to in 8259 mode. Otherwise global system interrupt number.
    uint32_t sci_cmd;               // System port address of the SCI command port
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_cnt;
    uint32_t pm1a_evt_blk;          // System port address of the PM1a event register block
    uint32_t pm1b_evt_blk;          // System port address of the PM1b event register block
    uint32_t pm1a_cnt_blk;          // System port address of the PM1a control register block
    uint32_t pm1b_cnt_blk;          // System port address of the PM1b control register block
    uint32_t pm2_cnt_blk;           // System port address of the PM2 control register block
    uint32_t pm_tmr_blk;            // System port address of the power management timer control register block
    uint32_t gpe0_blk;              // System port address of the general purpose event 0 register block
    uint32_t gpe1_blk;              // System port address of the general purpose event 1 register block
    uint8_t  pm1_evt_len;           // Number of bytes decoded by pm1a_evt_blk
    uint8_t  pm1_cnt_len;           // Number of bytes decoded by pm1a_cnt_blk
    uint8_t  pm2_cnt_len;           // Number of bytes decoded by pm2_cnt_blk
    uint8_t  pm_tmr_len;            // Number of bytes decoded by pm_tmr_blk
    uint8_t  gpe0_blk_len;          // Number of bytes decoded by gpe0_blk
    uint8_t  gpe1_blk_len;          // Number of bytes decoded by gpe1_blk
    uint8_t  gpe1_base;             // Start number of GPE1 based events
    uint8_t  cst_cnt;
    uint16_t p_lvl2_lat;            // Worst-case hardware latency to enter/exit C2 state (in microseconds)
    uint16_t p_lvl3_lat;            // Worst-case hardware latency to enter/exit C3 state (in microseconds)
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint16_t iapc_boot_arch;
    uint8_t  reserved;
    uint32_t flags;
};
static_assert(sizeof(fixed_acpi_description) == 116, "");

#pragma pack(pop)

out_stream& operator<<(out_stream& os, const description& desc) {
    os << format_str(desc.signature).width(4) << " " << as_hex(desc.length-sizeof(description)).width(4);
    os << " OEM " << format_str(desc.oem_id).width(6) << " " << format_str(desc.oem_table_id).width(8);
    os << " Vendor " << format_str(desc.vendor_id).width(4);
    return os;
}

uint8_t checksum(const void* ptr, uint32_t bytes) {
    auto d = static_cast<const uint8_t*>(ptr);
    uint8_t sum = 0;
    while (bytes--) {
        sum += *d++;
    }
    return sum;
}

template<typename T>
const T* match_structure(const description& desc) {
    // Desc must be prevalidated (i.e. checksum and length verified)
    return std::equal(desc.signature, desc.signature+sizeof(desc.signature), T::expected_signature)
        && desc.length >= sizeof(T) ? &static_cast<const T&>(desc) : nullptr;
}

const root_system_description_pointer* find_rsdp(physical_address low, physical_address high)
{
    REQUIRE(!(static_cast<uint64_t>(low) & 15) && !(static_cast<uint64_t>(high) & 15) && low < high);
    for (auto addr = low; addr < high; addr += 16) {
        const auto& rsdp = *static_cast<const root_system_description_pointer*>(addr);
        if (!std::equal(rsdp.signature, rsdp.signature+sizeof(rsdp.signature), rsdp_signature)) {
            continue;
        }
        if (checksum(&rsdp, sizeof(rsdp))) {
            continue;
        }
        if (rsdp.revision == rsdp_revision::v1) {
            dbgout() << "Found v1 rsdp @ " << as_hex(&rsdp) << "\n";
            return &rsdp;
        }
        if (rsdp.revision != rsdp_revision::v2) {
            // Unsupported/invalid revision
            continue;
        }
        const auto& v2 = static_cast<const root_system_description_pointer_v2&>(rsdp);
        if (checksum(&v2, sizeof(v2))) {
            continue;
        }
        dbgout() << "Found v2 rsdp @ " << as_hex(&rsdp) << "\n";
        return &v2;
    }
    return nullptr;
}

} } // namespace attos::acpi

kvector<uint8_t> get_description(physical_address phys) {
    using namespace attos::acpi;

    mem_map_helper mapping{kmemory_manager(), phys, 0x1000, memory_type::read}; // Speculatively map more bytes, to reduce the number of remappings
    const auto len = mapping.as<const description>().length;
    REQUIRE(len >= sizeof(description));
    if (len > mapping.length()) {
        mapping = mem_map_helper{kmemory_manager(), phys, len, memory_type::read}; // TODO: This could be done smarter
    }
    const auto& desc = mapping.as<const description>();
    REQUIRE(!checksum(&desc, desc.length));
    return kvector<uint8_t>{mapping.ptr(), mapping.ptr() + desc.length};
}

void handle(const acpi::multiple_apic_description& madt) {
    enum class icst_type : uint8_t { // Interrupt Controller Structure Type
        processor_local_apic      = 0x00,
        io_apic                   = 0x01,
        interrupt_source_override = 0x02,
        local_apic_nmi            = 0x04,
    };

    const uint8_t* const desc_begin = reinterpret_cast<const uint8_t*>(&madt);
    const uint8_t* const beg = desc_begin + sizeof(acpi::multiple_apic_description);
    const uint8_t* const end = desc_begin + madt.length;

    dbgout() << madt << "\n";
    for (auto p = beg; p != end;) {
        REQUIRE(p + 2 <= end);
        const auto type = static_cast<icst_type>(p[0]);
        const auto len  = p[1];
        REQUIRE(p + p[1] <= end);
        switch (type) {
            case icst_type::processor_local_apic:
                {
                    REQUIRE(len == 8);
                    const auto flags = *reinterpret_cast<const uint32_t*>(p + 4);
                    REQUIRE(flags == 0 || flags == 1);
                    dbgout() << " Processor Local APIC processor id " << as_hex(p[2]) << " APIC id " << as_hex(p[3]) << " " << (flags?"Enabled":"Disabled") << "\n";
                    break;
                }
            case icst_type::io_apic:
                REQUIRE(len == 12);
                REQUIRE(p[3] == 0);
                dbgout() << " I/O APIC id " << as_hex(p[2]) << " address " << as_hex(*reinterpret_cast<const uint32_t*>(p+4)) << " interrupt base " << as_hex(*reinterpret_cast<const uint32_t*>(p+8)) << "\n";
                break;
            case icst_type::interrupt_source_override:
                REQUIRE(len == 10);
                REQUIRE(p[2] == 0); // Bus must be ISA
                dbgout() << " Interrupt source override IRQ#" << as_hex(p[3]) << " global interrupt " << as_hex(*reinterpret_cast<const uint32_t*>(p+4)) << " flags " << as_hex(*reinterpret_cast<const uint16_t*>(p+8)) << "\n";
                break;
            case icst_type::local_apic_nmi:
                REQUIRE(len == 6);
                dbgout () << " Local APIC NMI processor id " << as_hex(p[2]) << " flags " << as_hex(*reinterpret_cast<const uint16_t*>(p+3)) << " LINT# " << as_hex(p[5]) << "\n";
                break;
            default:
                dbgout() << "Unknown MADT entry: ";
                for (int i = 0; i < p[1]; ++i) {
                    dbgout() << " " << as_hex(p[i]);
                }
                dbgout() << "\n";
                REQUIRE(false);
        }
        p += len;
    }
}

void handle(const acpi::fixed_acpi_description& facp) {
    dbgout() << facp << "\n";
    dbgout() << " Revision " << facp.revision << "\n";
    REQUIRE((facp.revision == 1 && facp.length == 0x50+sizeof(acpi::description)) || (facp.revision == 2 && facp.length == 0x5d+sizeof(acpi::description)));
    dbgout() << " FACS    " << as_hex(facp.firmware_ctrl) << "\n";
    dbgout() << " DSDT    " << as_hex(facp.dsdt) << "\n";
    dbgout() << " SCI_INT " << as_hex(facp.sci_int) << "\n";
    dbgout() << " SCI_CMD " << as_hex(facp.sci_cmd) << "\n";

    kvector<uint8_t> dsdt_bytes = get_description(physical_address{facp.dsdt});
    const auto& dsdt_desc = *reinterpret_cast<const acpi::description*>(dsdt_bytes.begin());
    dbgout() << " " << dsdt_desc << "\n";
    hack_dsdt_phys = physical_address{facp.dsdt+sizeof(dsdt_desc)};
    hack_dsdt_len  = dsdt_desc.length - sizeof(dsdt_desc);
}

void acpi_test() {
    using namespace attos::acpi;

    const auto& ebda_segment = *fixed_physical_address<uint16_t, 0x40E>;
    const physical_address ebda_address{ebda_segment*16ULL};
    dbgout() << "Extended BIOS Data Area: " << as_hex(ebda_address) << "\n";
    const root_system_description_pointer* rsdp = nullptr;
    // The RSDP is either within the extended BIOS data area
    if ((rsdp = find_rsdp(ebda_address, ebda_address + (1ULL<<10))) == nullptr) {
        dbgout() << "Searching main BIOS area\n";
        // or in the general BIOS data area
        rsdp = find_rsdp(physical_address{0x000e0000}, physical_address{0x000ffff0});
    }
    REQUIRE(rsdp && "ACPI not available");
    auto rsdt_bytes = get_description(physical_address{rsdp->rsdt_address});
    const auto& rsdt_desc = *reinterpret_cast<const description*>(rsdt_bytes.begin());
    dbgout() << rsdt_desc << "\n";
    REQUIRE(rsdt_desc.revision == 1);
    const auto entries = reinterpret_cast<const uint32_t*>(rsdt_bytes.begin() + sizeof(description));
    kvector<kvector<uint8_t>> rsdt_entries;
    for (uint32_t i = 0; i < (rsdt_desc.length-sizeof(description)) / 4; ++i) {
        rsdt_entries.push_back(get_description(physical_address{entries[i]}));
    }

#if 0
    //   x200s VMWARE Bochs
    APIC   x     x      x
    ASF!   x
    BOOT   x     x
    ECDT   x
    FACP   x     x      x
    HPET   x     x
    MCFG   x     x
    SLIC   x
    SRAT         x
    SSDT   x            x
    TCPA   x
    WAET         x
#endif

    dbgout() << "ACPI OEMID    " << format_str(rsdp->oem_id) << "\n";
    dbgout() << "ACPI Revision " << static_cast<uint8_t>(rsdp->revision) << "\n";
    dbgout() << "ACPI RsdtAddress " << as_hex(rsdp->rsdt_address) << "\n";
    if (rsdp->revision == rsdp_revision::v2) {
        const auto& v2 = *static_cast<const root_system_description_pointer_v2*>(rsdp);
        dbgout() << "Length      " << v2.length << "\n";
        dbgout() << "XsdtAddress " << as_hex(v2.xsdt_address) << "\n";
    }

    for (const auto& entry_bytes : rsdt_entries) {
        const auto& desc = *reinterpret_cast<const description*>(entry_bytes.begin());
        if (auto madt = match_structure<multiple_apic_description>(desc)) {
            handle(*madt);
        } else if (auto facp = match_structure<fixed_acpi_description>(desc)) {
            handle(*facp);
        } else {
            dbgout() << " ??" << desc << "\n";
        }
    }
}

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

    acpi_test();

    // Networking
    kowned_ptr<net::ethernet_device> netdev{};

    for (const auto& d : pci->devices()) {
        if (!!(netdev = net::i825x::probe(d))) {
            ko_ethdev::set_dev(netdev.get());
            break;
        }
    }

    if (netdev) {
        auto should_quit = []() { return ps2::key_available() && ps2::read_key() == '\x1b'; };
        auto ipv4dev = net::make_ipv4_device(*netdev);
        do_dhcp(*ipv4dev, should_quit);
        auto data = net::tftp::read(*ipv4dev, should_quit, "test.txt");
        hexdump(dbgout(), data.begin(), data.size());
    }

    // User mode
    usermode_test(*cpu, user_exe);
    ko_ethdev::set_dev(nullptr);
}
