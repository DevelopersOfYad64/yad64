#ifndef BINARYINFO_PE_H
#define BINARYINFO_PE_H

#include "IBinary.h"
#include <Windows.h>

class PE32 : public IBinary {
public:
    PE32(const MemoryRegion &region);
    virtual ~PE32();

public:
    virtual bool validate_header();
    virtual edb::address_t entry_point();
    virtual edb::address_t calculate_main();
    virtual bool native() const;
    virtual edb::address_t debug_pointer();
    virtual size_t header_size() const;

private:
    void read_header();

private:
    PIMAGE_DOS_HEADER lpDosHeader;
    PIMAGE_NT_HEADERS32 lpNtHeaders;
};

class PE64 : public IBinary {
public:
    PE64(const MemoryRegion &region);
    virtual ~PE64();

public:
    virtual bool validate_header();
    virtual edb::address_t entry_point();
    virtual edb::address_t calculate_main();
    virtual bool native() const;
    virtual edb::address_t debug_pointer();
    virtual size_t header_size() const;

private:
    void read_header();

private:
    PIMAGE_DOS_HEADER lpDosHeader;
    PIMAGE_NT_HEADERS64 lpNtHeaders;
};

#endif // PE_H
