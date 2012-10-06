#include "PE.h"

#include "Util.h"
#include "Debugger.h"
#include "IDebuggerCore.h"
#include "ByteShiftArray.h"

PE64::PE64(const MemoryRegion &region) : IBinary(region),lpDosHeader(NULL),lpNtHeaders(NULL) {
}

PE64::~PE64() {
    delete lpDosHeader;
    delete lpNtHeaders;
}

bool PE64::validate_header() {
    read_header();
    if(lpDosHeader == NULL) return false;
    if(lpDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if(lpNtHeaders->Signature != IMAGE_NT_SIGNATURE) return false;
    return true;
}

bool PE64::native() const {
#ifdef YAD64_X86_64
    return true;
#else
    return false;
#endif
}

yad64::address_t PE64::entry_point() {
    read_header();
    if (lpNtHeaders == NULL) return 0;
    return region_.start()+lpNtHeaders->OptionalHeader.AddressOfEntryPoint;
}

size_t PE64::header_size() const {
    return sizeof(IMAGE_DOS_HEADER)+sizeof(IMAGE_NT_HEADERS64);
}

yad64::address_t PE64::debug_pointer() {
    return 0;
}

yad64::address_t PE64::calculate_main() {
    return 0;
}

void PE64::read_header() {
    if(lpDosHeader == NULL) {
        lpDosHeader = new IMAGE_DOS_HEADER;

        if(!yad64::v1::debugger_core->read_bytes(region_.base(), lpDosHeader, sizeof(IMAGE_DOS_HEADER))) {
            std::memset(lpDosHeader, 0, sizeof(IMAGE_DOS_HEADER));
        }

        if(lpDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;
        lpNtHeaders = new IMAGE_NT_HEADERS64;

        if(!yad64::v1::debugger_core->read_bytes(region_.base()+lpDosHeader->e_lfanew, lpNtHeaders, sizeof(IMAGE_NT_HEADERS64))) {
            std::memset(lpNtHeaders, 0, sizeof(IMAGE_NT_HEADERS64));
        }
    }
}
