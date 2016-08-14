#ifndef ATTOS_PS2_H
#define ATTOS_PS2_H

#include <stdint.h>
#include <attos/mem.h>

namespace attos { namespace ps2 {

class __declspec(novtable) controller {
public:
    virtual ~controller() = 0 {}
};

owned_ptr<controller, destruct_deleter> init();

bool    key_available();
uint8_t read_key();

} } // namespace attos::ps2

#endif
