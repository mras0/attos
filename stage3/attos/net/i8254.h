#ifndef ATTOS_NET_I8254_H
#define ATTOS_NET_I8254_H

#include <attos/pci.h>
#include <attos/net/net.h>

namespace attos { namespace net {

ethernet_device_ptr i82545_probe(const pci::device_info& dev_info);

} } // namespace attos::net

#endif

