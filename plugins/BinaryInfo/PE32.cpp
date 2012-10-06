#include "PE.h"

#include "Util.h"
#include "Debugger.h"
#include "IDebuggerCore.h"
#include "ByteShiftArray.h"

PE32::PE32(const MemoryRegion &region) : IBinary(region) {
}

PE32::~PE32() {
}

bool PE32::validate_header() {
    read_header();
    return true;
}

bool PE32::native() const {
#ifdef YAD64_X86
    return true;
#else
    return false;
#endif
}

yad64::address_t PE32::entry_point() {
    return 0;
}

size_t PE32::header_size() const {
    return 0;
}

yad64::address_t PE32::debug_pointer() {
    return 0;
}

yad64::address_t PE32::calculate_main() {
    return 0;
}

void PE32::read_header() {
    if(lpDosHeader == NULL) {
        /*header_ = new Elf32_Ehdr;

        if(!yad64::v1::debugger_core->read_bytes(region_.start(), header_, sizeof(Elf32_Ehdr))) {
            std::memset(header_, 0, sizeof(Elf32_Ehdr));
        }*/
    }
}
