#include "tftp.h"
#include <attos/string.h>
#include <attos/cpu.h>
#include <attos/containers.h>
#include <attos/out_stream.h>

namespace attos { namespace net { namespace tftp {

uint8_t* put(uint8_t* b, const char* s) {
    const auto l = string_length(s);
    memcpy(b, s, l + 1); // also copy nul-terminator
    return b + l + 1;
}

uint8_t* put_error_reply(uint8_t* b, error_code ec, const char* msg) {
    b = put(b, opcode::error);
    b = put(b, static_cast<uint16_t>(ec));
    return put(b, msg);
}

uint16_t get_u16(const uint8_t*& data, uint32_t& length) {
    REQUIRE(length >= 2);
    const auto x = static_cast<uint16_t>(data[0]*256 + data[1]);
    data   += 2;
    length -= 2;
    return x;
}

opcode get_opcode(const uint8_t*& data, uint32_t& length) {
    return static_cast<opcode>(get_u16(data, length));
}

const char* get_string(const uint8_t*& data, uint32_t& length) {
    auto str = reinterpret_cast<const char*>(data);
    while (length--) {
        if (!*data++) {
            return str;
        }
    }
    REQUIRE(!"Invalid TFTP string");
    return nullptr;
}

} } } // namespace attos::net::tftp

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
};

class ipv4_ethernet_device : public ipv4_device {
public:
    explicit ipv4_ethernet_device(ethernet_device& ethdev) : ethdev_{ethdev} {
    }

    virtual ~ipv4_ethernet_device() override {
    }

    kowned_ptr<udp_socket> udp_open(ipv4_address local_addr, uint16_t local_port, const packet_process_function& recv_func) {
        if (local_port == 0) {
            // Find unused port
            // TODO: Start search from last used port number + 1
            for (local_port = 49152; local_port < 65535; ++local_port) {
                if (!find_open_udp_socket(local_addr, local_port)) {
                    break;
                }
            }
        }
        REQUIRE(local_addr == ipv4_config_.addr || local_addr == inaddr_any);
        REQUIRE(!find_open_udp_socket(local_addr, local_port));
        dbgout() << "[udp] Opening " << local_addr << ':' << local_port << "\n";
        auto s = knew<udp_socket>([this] (uint8_t* data, uint32_t length) { ipv4_out(data, length); }, [this, local_addr, local_port] { udp_close(local_addr, local_port); }, local_addr, local_port);
        udp_sockets_.push_back({s.get(), recv_func});
        return s;
    }

    void process_packets() {
        ethdev_.process_packets([this] (const uint8_t* data, uint32_t length) { eth_in(data, length); }, /*max_packets*/ 8);
    }

    mac_address hw_address() const {
        return ethdev_.hw_address();
    }

    ipv4_net_config ipv4_config() const {
        return ipv4_config_;
    }

    void ipv4_config(ipv4_net_config config) {
        REQUIRE(ipv4_config_.addr == inaddr_any);
        REQUIRE(config.addr != inaddr_any && config.addr != inaddr_broadcast);
        REQUIRE(config.netmask != inaddr_any);
        dbgout() << "[ipv4] Configuration IP " << config.addr << " Net " << config.netmask << " Gateway " << config.gateway << "\n";
        ipv4_config_ = config;
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
    ipv4_net_config             ipv4_config_ = ipv4_net_config_none;
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

        if (ipv4_config_.addr == inaddr_any) {
            // Ignore ARP until we have an IP address assigned
            return;
        }

        bool merge_flag = false;
        if (auto e = find_arp_entry(ah.spa)) {
            if (e->ha != ah.sha) {
                dbgout() << "[arp] Updating ARP entry " << ah.spa << " = " << ah.sha << "\n";
                e->ha = ah.sha;
            }
            merge_flag = true;
        }

        if (ipv4_config_.addr == ah.tpa) { // Are we the target?
            dbgout() << "[arp] " << (ah.oper == arp_operation::request ? "RQ" : "RP")
                << " " << ah.sha << " " << ah.spa << " -> " << ah.tha << " " << ah.tpa << "\n";

            if (!merge_flag) {
                dbgout() << "[arp] Adding ARP entry " << ah.spa << " = " << ah.sha << "\n";
                add_arp_entry(ah.spa, ah.sha);
            }
            if (ah.oper == arp_operation::request) {
                // Swap hardware and protocol fields, putting the local hardware and protocol addresses in the sender fields.
                // Set the ar$op field to ares_op$REPLY
                // Send the packet to the (new) target hardware address on the same hardware on which the request was received.
                dbgout() << "[arp] Sending ARP reply for " << ah.tpa << " to " << ah.spa << "\n";
                send_arp(arp_operation::reply, ah.tpa, ah.sha, ah.spa);
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
            auto tpa = ih.dst;
            if ((tpa & ipv4_config_.netmask) != (ipv4_config_.addr & ipv4_config_.netmask)) {
                REQUIRE(ipv4_config_.gateway != inaddr_any);
                tpa = ipv4_config_.gateway;
            }

            if (auto ae = find_arp_entry(tpa)) {
                eh.dst = ae->ha;
            } else {
                dbgout() << "[arp] Sending ARP request for " << tpa << "\n";
                send_arp(arp_operation::request, ipv4_config_.addr, mac_address::broadcast, tpa);
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
        if (ih.dst != inaddr_any && ih.dst == ipv4_config_.addr) {
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
        REQUIRE(uh.length >= sizeof(udp_header));
        REQUIRE(length >= uh.length - sizeof(udp_header));
        length = uh.length - sizeof(udp_header);
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

    bool finished() const {
        return state_ == state::finished;
    }

    ipv4_net_config config() const {
        REQUIRE(finished());
        return config_;
    }

    void tick() {
        if (timeout_ && !--timeout_) {
            dbgout() << "[dhcp] Timed out.\n";
            send_dhcp_discover();
        }
    }

private:
    ipv4_ethernet_device& dev_;
    kowned_ptr<udp_socket> s_;
    ipv4_net_config config_ = ipv4_net_config_none;
    enum class state { wait_for_offer, wait_for_ack, finished } state_ = state::wait_for_offer;
    static constexpr uint16_t dhcp_src_port = 68;
    static constexpr uint16_t dhcp_dst_port = 67;
    uint32_t timeout_ = 0;

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
        timeout_ = 50;
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
        ipv4_address       netmask   = inaddr_any;
        ipv4_address       router    = inaddr_any;
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
        if (dh.message_type != expected_messge) {
            dbgout() << "dh.message_type = " << as_hex((uint16_t)dh.message_type) << "\n";
        }
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
                res.netmask = *reinterpret_cast<const ipv4_address*>(data);
                break;
            case dhcp_option::router:
                REQUIRE(len >= 4 && len % 4 == 0);
                res.router = *reinterpret_cast<const ipv4_address*>(data);
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
                config_.addr = pr.dh->yiaddr;
                config_.netmask = pr.netmask;
                config_.gateway = pr.router;
                REQUIRE(config_.addr != inaddr_any && config_.addr != inaddr_broadcast);
                if (config_.netmask == inaddr_any) {
                    config_.netmask = ipv4_address{255, 255, 255, 0};
                }
                state_ = state::wait_for_ack;
                send_dhcp_request(pr.dh->yiaddr, pr.server_id);
                break;
            }
        case state::wait_for_ack:
            {
                auto pr = parse_reply(dhcp_message_type::ack, data, length);
                dbgout() << "[dhcp] Got DHCPACK for " << pr.dh->yiaddr << " from " << pr.server_id << "\n";
                REQUIRE(pr.dh->yiaddr == config_.addr);
                state_ = state::finished;
                break;
            }
        }
    }
};

kowned_ptr<ipv4_device> make_ipv4_device(ethernet_device& ethdev) {
    return kowned_ptr<ipv4_device>{knew<ipv4_ethernet_device>(ethdev).release()};
}

bool do_dhcp(ipv4_device& ipv4dev_, should_quit_function_type should_quit)
{
    auto& ipv4dev = static_cast<ipv4_ethernet_device&>(ipv4dev_);
    net::dhcp_handler dhcp_h{ipv4dev};
    while (!should_quit()) {
        if (dhcp_h.finished()) {
            ipv4dev.ipv4_config(dhcp_h.config());
            return true;
        }
        dhcp_h.tick();
        ipv4dev.process_packets();
        yield();
    }
    return false;
}

class __declspec(novtable) tftp_base {
public:
    explicit tftp_base(ipv4_ethernet_device& dev, ipv4_address remote_addr)
        : dev_{dev}
        , remote_addr_(remote_addr)
        , s_{dev_.udp_open(dev.ipv4_config().addr, 0, [this] (const uint8_t* data, uint32_t length) { tftp_in(data, length); })} {
    }

    enum class result { running, timeout, done };

    result tick() {
        if (is_done()) {
            return result::done;
        }
        if (timeout_ && !--timeout_) {
            on_timeout();
            return result::timeout;
        }
        return result::running;
    }

private:
    ipv4_ethernet_device&  dev_;
    const ipv4_address     remote_addr_;
    kowned_ptr<udp_socket> s_;
    uint8_t                buffer_[4 + tftp::block_size]; // DATA 2 byte code, 2 byte block number + data bytes
    uint32_t               timeout_;

    static constexpr uint32_t default_timeout = 50;

protected:
    uint8_t* start_packet(tftp::opcode op) {
        return tftp::put(buffer_, op);
    }

    void send_packet(const uint8_t* b) {
        s_->sendto(remote_addr_, tftp::dst_port, buffer_, static_cast<uint16_t>(b - buffer_));
        timeout_ = default_timeout;
    }

private:
    void tftp_in(const uint8_t* data, uint32_t length) {
        REQUIRE(length <= 4 + tftp::block_size);
        const auto opcode = tftp::get_opcode(data, length);
        if (on_packet(opcode, data, length)) {
            return;
        }
        switch (opcode) {
        case tftp::opcode::error:
            {
                const auto error_code = tftp::get_u16(data, length);
                const auto error_msg  = tftp::get_string(data, length);
                dbgout() << "[tftp] Error " << error_code << ": " << error_msg << "\n";
                REQUIRE(false); // TODO: Handle errors
                break;
            }
        default:
            dbgout() << "Got Unhandled TFTP packet opcode " << static_cast<uint16_t>(opcode) << ":\n";
            hexdump(dbgout(), data, length);
            REQUIRE(false);
        }

    }

    virtual bool is_done() const = 0;
    virtual void on_timeout() = 0;
    virtual bool on_packet(tftp::opcode opcode, const uint8_t* data, uint32_t length) = 0;
};

class tftp_reader : public tftp_base {
public:
    explicit tftp_reader(ipv4_ethernet_device& dev, ipv4_address remote_addr, const char* filename)
        : tftp_base{dev, remote_addr} {
        const auto filename_length = string_length(filename);
        REQUIRE(filename_length < sizeof(filename_));
        memcpy(filename_, filename, filename_length + 1);

        send_rrq();
    }

    kvector<uint8_t>&& data() {
        REQUIRE(is_done());
        return std::move(data_);
    }

private:
    char             filename_[64];
    uint16_t         last_block_;
    kvector<uint8_t> data_;
    bool             done_;

    void send_rrq() {
        dbgout() << "[tftp] Sending RRQ for " << filename_ << "\n";
        done_ = false;
        last_block_ = 0;
        data_.clear();
        auto b = start_packet(tftp::opcode::rrq);
        b = tftp::put(b, filename_);
        b = tftp::put(b, "octet");
        send_packet(b);
    }

    void send_ack(uint16_t block_number) {
        auto b = start_packet(tftp::opcode::ack);
        b = tftp::put(b, block_number);
        send_packet(b);
    }

    virtual bool is_done() const override {
        return done_;
    }

    virtual void on_timeout() override {
        dbgout() << "[tftp] Timed out! Last block " << last_block_ << "\n";
        if (!last_block_) {
            send_rrq();
        } else {
            REQUIRE(false);
        }
    }

    virtual bool on_packet(tftp::opcode opcode, const uint8_t* data, uint32_t length) override {
        if (opcode != tftp::opcode::data) return false;
        const auto block_number = tftp::get_u16(data, length);
        dbgout() << "[tftp] Got Data #" << block_number << ":\n";
        REQUIRE(block_number - 1 == last_block_);
        data_.insert(data_.end(), data, data + length);
        last_block_ = block_number;
        send_ack(block_number);
        if (length != tftp::block_size) {
            dbgout() << "[tftp] Read of " << filename_ << " done!\n";
            done_ = true;
        }
        return true;
    }
};

class tftp_writer : public tftp_base {
public:
    explicit tftp_writer(ipv4_ethernet_device& dev, ipv4_address remote_addr, const char* filename, array_view<uint8_t> data)
        : tftp_base{dev, remote_addr}
        , data_{data} {
        const auto filename_length = string_length(filename);
        REQUIRE(filename_length < sizeof(filename_));
        memcpy(filename_, filename, filename_length + 1);

        REQUIRE(tftp::legal_size(data_.size()));
        send_wrq();
    }

private:
    char                filename_[64];
    array_view<uint8_t> data_;
    uint16_t            last_ack_;
    static constexpr auto  no_acks = static_cast<uint16_t>(-1);

    uint16_t block_count() const {
        return tftp::block_count(data_.size());
    }

    void send_wrq() {
        dbgout() << "[tftp] Sending WRQ for " << filename_ << "\n";
        last_ack_ = no_acks;
        auto b = start_packet(tftp::opcode::wrq);
        b = tftp::put(b, filename_);
        b = tftp::put(b, "octet");
        send_packet(b);
    }

    void send_data(uint16_t block_number) {
        dbgout() << "[tftp] Sending block " << block_number << " of " << filename_ << "\n";
        REQUIRE(block_number >= 1 && block_number <= block_count());
        const auto index = (block_number-1) * tftp::block_size;
        const auto size  = std::min(static_cast<uint64_t>(tftp::block_size), data_.size() - index);
        auto b = start_packet(tftp::opcode::data);
        b = tftp::put(b, block_number);
        memcpy(b, data_.begin() + index, size);
        b += size;
        send_packet(b);
    }

    virtual bool is_done() const override {
        return last_ack_ == block_count();
    }

    virtual void on_timeout() override {
        dbgout() << "[tftp] Timed out! Last ack " << last_ack_ << "\n";
        if (last_ack_ == no_acks) {
            send_wrq();
        } else if (last_ack_ < block_count()) {
            send_data(last_ack_ + 1);
        } else {
            REQUIRE(false);
        }
    }

    virtual bool on_packet(tftp::opcode opcode, const uint8_t* data, uint32_t length) override {
        if (opcode != tftp::opcode::ack) return false;
        const auto block_number = tftp::get_u16(data, length);
        dbgout() << "[tftp] Got ACK #" << block_number << ":\n";
        REQUIRE(block_number <= block_count());
        last_ack_ = block_number;
        if (block_number < block_count()) {
            send_data(last_ack_ + 1);
        } else {
            dbgout() << "[tftp] Write of " << filename_ << " done!\n";
        }
        return true;
    }
};


template<typename T>
struct tftp_op {
    template<typename... Args>
    explicit tftp_op(ipv4_device& ipv4dev_, should_quit_function_type should_quit, Args&&... args) 
        : ipv4dev{static_cast<ipv4_ethernet_device&>(ipv4dev_)}
        , should_quit_(should_quit)
        , op_{ipv4dev, tftp_address(), static_cast<Args&&>(args)...} {
    }

    bool do_op() {
        while (!should_quit_()) {
            ipv4dev.process_packets();
            if (op_.tick() == tftp_base::result::done) {
                return true;
            }
            yield();
        }
        return false;
    }

    T& get_op() {
        return op_;
    }

private:
    ipv4_ethernet_device&     ipv4dev;
    should_quit_function_type should_quit_;
    T                         op_;

    ipv4_address tftp_address() const {
        // HAAAAACK
        return ipv4dev.ipv4_config().addr == ipv4_address{192, 168, 10, 2} ? ipv4_address{192, 168, 10, 1} : ipv4_address{192, 168, 1, 67};
    }
};

kvector<uint8_t> tftp::read(ipv4_device& ipv4dev, should_quit_function_type should_quit, const char* filename)
{
    tftp_op<tftp_reader> reader{ipv4dev, should_quit, filename};
    if (reader.do_op()) {
        return reader.get_op().data();
    }
    return kvector<uint8_t>{};
}

bool tftp::write(ipv4_device& ipv4dev, should_quit_function_type should_quit, const char* filename, array_view<uint8_t> data)
{
    tftp_op<tftp_writer> writer{ipv4dev, should_quit, filename, data};
    return writer.do_op();
}


} } // namespace attos::net

