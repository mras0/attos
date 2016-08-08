#ifndef ATTOS_NET_I825X_H
#define ATTOS_NET_I825X_H

#include <attos/net/net.h>
#include <attos/pci.h>
#include <attos/mm.h>

namespace attos { namespace net { namespace i825x {

kowned_ptr<ethernet_device> probe(const pci::device_info& dev_info);

} } } // namespace attos::net::i825x

#endif

