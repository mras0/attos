#ifndef ATTOS_NET_NETDEV_H
#define ATTOS_NET_NETDEV_H

#include <attos/mm.h>

namespace attos { namespace net {

class __declspec(novtable) netdev {
public:
    virtual ~netdev();
};

using netdev_ptr = kowned_ptr<netdev>;

} } // namespace attos::net

#endif
