/**
 * @file RelocFn.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Provides an interface for finding assembly hooks within the AE EXE.
 * @bug No known bugs.
 *
 * This file is intended to replace the previous implementation of address
 * finding, which required modifying the RelocAddr template in the SKSE source
 * so that the overload of the assignment operator could be made public. This
 * template instead takes in a signature which it will resolve either when
 * asked to do so or when doing so is necessary for the current opperator.
 *
 * The template has intentionally been written so that it can be constructed in
 * constant time and resolved in non-constant time, to avoid any wierdness with
 * C++ and its lack of "delayed init" idioms.
 *
 * I miss Rust.
 */

#ifndef __SKYRIM_UNCAPPER_AE_RELOC_FN_H__
#define __SKYRIM_UNCAPPER_AE_RELOC_FN_H__

#include <cstddef>
#include <cstdint>

#include "common/IErrors.h"
#include "Relocation.h"
#include "SafeWrite.h"
#include "reg2k/RVA.h"

#include "Signatures.h"

/// @brief The maximum size of an instruction in the x86 ISA.
const size_t kMaxInstrSize = 15;

/// @brief A buffer containing up to the max instruction size in NOPs.
const uint8_t kNopBuf[kMaxInstrSize] = {
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
};

/// @brief Encodes the various types of hooks which can be injected.
class HookType {
  public:
    enum t { Branch5, Branch6, Call5, Call6 };

    static size_t
    Size(
        t type
    ) {
        switch (type) {
            case Branch5:
            case Call5:
                return 5;
            case Branch6:
            case Call6:
                return 6;
        }
    }
}

template<typename T>
class RelocFn {
  private:
    const char *name;
    const char *sig;
    size_t hook_offset;
    size_t hook_instr_size;
    HookType::t hook_type;

    bool hook_done = false;
    uintptr_t real_address = 0;

  public:
    /**
     * @brief Constructs a new relocatable function hook.
     * @param name The name of the relocatable function.
     * @param sig The binary code signature of the function.
     * @param hook_offset The offset from the beginning of the signature to
     *        the start of the hook.
     * @param hook_instr_size The size of the instruction being overwritten
     *        for the hook.
     */
    RelocFn(
        const char *name,
        const char *sig,
        size_t hook_offset,
        size_t hook_instr_size,
        HookType::t hook_type
    ) : name(name),
        sig(sig),
        hook_offset(hook_offset),
        hook_instr_size(hook_instr_size),
        hook_type(hook_type)
    {}

    /**
     * @brief Resolves the underlying address, if it has not already done so.
     */
    void
    Resolve() {
        if (real_address) { return; }
        auto rva = RVAScan<T>(name, sig, hook_offset);
        real_address = rva.GetUIntPtr();
        ASSERT(real_address);
    }

    /**
     * @brief Gets the RelocAddr associated with this RelocFn.
     */
    RelocAddr<T>
    GetRelocAddr() {
        Resolve();
        ASSERT(real_address >= RelocationManager::s_baseAddr);
        return RelocAddr(real_address - RelocationManager::s_baseAddr);
    }

    /**
     * @brief Gets the effective address of the found hook.
     */
    uintptr_t
    GetUIntPtr() {
        Resolve();
        return real_address;
    }

    /**
     * @brief Gets the address that should be returned to from the hook.
     */
    uintptr_t
    GetRetAddr() {
        Resolve();
        return real_address + HookType::Size(hook_type);
    }

    /**
     * @brief Writes the hook to the given address.
     *
     * It is illegal to install a hook more than once.
     */
    void
    Hook(
        uintptr_t target
    ) {
        Resolve();

        size_t hook_size = HookType::Size(hook_type);
        ASSERT(!hook_done);
        ASSERT(hook_size <= hook_instr_size);

        // Install the hook, linking to the given address.
        switch(hook_type) {
            case HookType::Branch5:
                ASSERT(g_branchTrampoline.Write5Branch(real_address, target));
                break;
            case HookType::Branch6:
                ASSERT(g_branchTrampoline.Write6Branch(real_address, target));
                break;
            case HookType::Call5:
                ASSERT(g_branchTrampoline.Write5Call(real_address, target));
                break;
            case HookType::Call6:
                ASSERT(g_branchTrampoline.Write6Call(real_address, target));
                break;
        }

        // Overwrite the rest of the instruction with NOPs. We do this with
        // every hook to ensure the best compatibility with other SKSE
        // plugins.
        if (hook_instr_size > hook_size) {
            SafeWriteBuf(GetRetAddr(), kNopBuf, hook_instr_size - hook_size);
        }

        hook_done = true;
    }
};

#endif /* __SKYRIM_UNCAPPER_AE_RELOC_FN_H__ */
