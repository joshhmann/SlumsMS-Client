#pragma once
#ifndef MEMPATCH2_LIBRARY_H
#define MEMPATCH2_LIBRARY_H


#include <array>
#include <cstring>
#include <cstdint>
#include <tuple>


#include <windows.h>
typedef DWORD mprot_t;
#define MEM_WIN


uintptr_t calc_jmp(uintptr_t from, uintptr_t to);

void ms_memcpy(void* dest, const void* src, size_t size, bool unprotect = true);

void ms_memset(void* dest, int value, size_t size, bool unprotect = true);


class MemRegion {
public:
    MemRegion(void* base, size_t size, bool unprotect = false);

    ~MemRegion();

    void Set(uint8_t value, size_t size);

    void WriteBytes(const void* src, size_t size);

    void Nop(size_t size);


    void WriteByte(uint8_t value);

    void WriteInt16(int16_t value);

    void WriteUInt16(uint16_t value);

    void WriteInt32(int32_t value);

    void WriteUInt32(uint32_t value);

    void WriteShort(short value);

    void WriteUShort(unsigned short value);

    void WriteDword(uint32_t value);

    void WriteDouble(double value);

    void WriteString(const char* value);

    void WriteAddr(uintptr_t value);

    void WriteJmp(uintptr_t to, size_t nops = 0);

    void ResetOffset();


private:
    uintptr_t _base;
    size_t _offset{};
    size_t _size;

    mprot_t _old_protect{};
};

class ToggleAble {
public:
    ToggleAble() = default;

    virtual ~ToggleAble() = default;

    void Enable();

    void Disable();

    bool IsEnabled() const;

    bool Toggle();

protected:
    bool _enabled = false;

    virtual void f_enable() = 0;

    virtual void f_disable() = 0;
};


template<const size_t N>
class MemRegionPatch : public ToggleAble {
public:
    explicit MemRegionPatch(void* base) : _base(base) {
        ms_memcpy(_original.data(), _base, N);
    }

    ~MemRegionPatch() override = default;

protected:
    virtual void r_enable(MemRegion& region) = 0;

    void f_enable() override {
        MemRegion region(_base, N, true);
        r_enable(region);
    }

    void f_disable() override {
        MemRegion region(_base, N, true);
        region.WriteBytes(_original.data(), N);
    }

    void* _base;
private:
    std::array<uint8_t, N> _original;
};

template<const size_t N>
class MemBytesPatch : public MemRegionPatch<N> {
public:
    explicit MemBytesPatch(void* base, const std::array<uint8_t, N>& patch) : MemRegionPatch<N>(base), _patch(patch) {
    }

    MemBytesPatch(void* base, uint8_t const (&patch)[N]) : MemBytesPatch(base, std::to_array(patch)) {}

    ~MemBytesPatch() override = default;

protected:
    void r_enable(MemRegion& region) override {
        region.WriteBytes(_patch.data(), N);
    }

private:
    std::array<uint8_t, N> _patch;
};

template<const size_t N>
class MemSetPatch : public MemRegionPatch<N> {
public:
    explicit MemSetPatch(void* base, uint8_t value) : MemRegionPatch<N>(base), _value(value) {
    }

    ~MemSetPatch() override = default;

protected:
    void r_enable(MemRegion& region) override {
        region.Set(_value, N);
    }

private:
    uint8_t _value;
};

template<typename T>
class MemValuePatch : public MemRegionPatch<sizeof(T)> {
public:
    explicit MemValuePatch(T* base, T value) : MemRegionPatch<sizeof(T)>(base), _value(value) {
    }

    ~MemValuePatch() override = default;

protected:
    void r_enable(MemRegion& region) override {
        region.WriteBytes(&_value, sizeof(T));
    }


private:
    T _value;
};


class MemJumpPatch : public MemRegionPatch<16> {
public:
    explicit MemJumpPatch(void* base, void* to, size_t nops);

    ~MemJumpPatch() override = default;

protected:
    void r_enable(MemRegion& region) override;

private:
    uintptr_t _to;
    size_t _nops;
};

/// Group Toogleable with a variadic tuple of patches

template<typename... Patches>
class ToggleGroup : public ToggleAble {
public:
    explicit ToggleGroup(Patches... patches) : _patches(patches...) {
    }

    ~ToggleGroup() override = default;

protected:
    void f_enable() override {
        std::apply([](auto &&... args) { (args.Enable(), ...); }, _patches);
    }

    void f_disable() override {
        std::apply([](auto &&... args) { (args.Disable(), ...); }, _patches);
    }

private:
    std::tuple<Patches...> _patches;
};

#endif //MEMPATCH2_LIBRARY_H