#include <attos/out_stream.h>
#include <attos/containers.h>
#include "aml.h"

using namespace attos;

#include <fstream>
bool read_file(const char* filename, kvector<uint8_t>& data)
{
    std::ifstream in(filename, std::ifstream::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    data.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(&data[0]), data.size());
    return true;
}

int main(int argc, char* argv[])
{
    kvector<uint8_t> data;
    //const char* filename = "vmware_dsdt.aml";
    const char* filename = argc >= 2 ? argv[1] : "bochs_dsdt.aml";
    if (!read_file(filename, data)) {
        dbgout() << "Could not read " << filename << "\n";
        return 1;
    }
    acpi::process(make_array_view(data.begin(), data.size()));
}
