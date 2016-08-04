#ifndef ATTOS_NET_I825X_H
#define ATTOS_NET_I825X_H

#include <attos/pci.h>
#include <attos/net/net.h>

namespace attos { namespace net { namespace i825x {

ethernet_device_ptr probe(const pci::device_info& dev_info);

} } } // namespace attos::net::i825x

#endif

