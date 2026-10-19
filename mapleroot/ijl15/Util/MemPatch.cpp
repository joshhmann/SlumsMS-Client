#include "MemPatch.h"


void ms_memcpy(void* dest, const void* src, size_t size, bool unprotect) {
    MemRegion region(dest, size, unprotect);
    region.WriteBytes(src, size);
}

void ms_memset(void* dest, int value, size_t size, bool unprotect) {
    MemRegion region(dest, size, unprotect);
    region.Set(value, size);
}

uintptr_t calc_jmp(uintptr_t from, uintptr_t to) {
    return to - from - 5;
}

MemRegion::MemRegion(void* base, size_t size, bool unprotect) {
    _base = (uintptr_t)base;
    _size = size;
    if (unprotect) {
#ifdef MEM_WIN
        VirtualProtect((void*) _base, _size, PAGE_EXECUTE_READWRITE, &_old_protect);
#else
        //TODO: mprotect(_base, _size, PROT_READ | PROT_WRITE | PROT_EXEC);
        //TODO
        _old_protect = PROT_READ | PROT_WRITE | PROT_EXEC;
#endif
    }
}

MemRegion::~MemRegion() {
    if (_old_protect) {
#ifdef MEM_WIN
        VirtualProtect((void*)_base, _size, _old_protect, &_old_protect);
#else
        //TODO mprotect(_base, _size, _old_protect);
#endif
    }
}

void MemRegion::Set(uint8_t value, size_t size) {
    std::memset(reinterpret_cast<void*>(_base + _offset), value, size);
    _offset += size;
}

void MemRegion::WriteBytes(const void* src, size_t size) {
    std::memcpy(reinterpret_cast<void*>(_base + _offset), src, size);
    _offset += size;
}

void MemRegion::Nop(size_t size) {
    Set(0x90, size);
}

void MemRegion::WriteByte(uint8_t value) {
    WriteBytes(&value, sizeof(uint8_t));
}

void MemRegion::WriteInt16(int16_t value) {
    WriteBytes(&value, sizeof(int16_t));
}

void MemRegion::WriteUInt16(uint16_t value) {
    WriteBytes(&value, sizeof(uint16_t));
}

void MemRegion::WriteInt32(int32_t value) {
    WriteBytes(&value, sizeof(int32_t));
}

void MemRegion::WriteUInt32(uint32_t value) {
    WriteBytes(&value, sizeof(uint32_t));
}

void MemRegion::WriteShort(short value) {
    WriteBytes(&value, sizeof(short));
}

void MemRegion::WriteUShort(unsigned short value) {
    WriteBytes(&value, sizeof(unsigned short));
}

void MemRegion::WriteDword(uint32_t value) {
    WriteBytes(&value, sizeof(uint32_t));
}

void MemRegion::WriteDouble(double value) {
    WriteBytes(&value, sizeof(double));
}

void MemRegion::WriteString(const char* value) {
    // In theory empty
    if (!value) return;
    WriteBytes(value, std::strlen(value));
}

void MemRegion::WriteAddr(uintptr_t value) {
    WriteBytes(&value, sizeof(uintptr_t));
}

void MemRegion::WriteJmp(uintptr_t to, size_t nops) {
    auto offset = (uintptr_t)_base + _offset;
    WriteByte(0xE9);
    WriteAddr(offset);
    if (nops) {
        Nop(nops);
    }
}

void MemRegion::ResetOffset() {
    _offset = 0;
}

void ToggleAble::Enable() {
    f_enable();
    _enabled = true;
}

void ToggleAble::Disable() {
    f_disable();
    _enabled = false;
}

bool ToggleAble::IsEnabled() const {
    return _enabled;
}

bool ToggleAble::Toggle() {
    _enabled ? Disable() : Enable();
    return _enabled;
}

MemJumpPatch::MemJumpPatch(void* base, void* to, size_t nops) : MemRegionPatch(base), _to((uintptr_t)to), _nops(nops) {
}

void MemJumpPatch::r_enable(MemRegion& region) {
    region.WriteJmp(_to, _nops);
}