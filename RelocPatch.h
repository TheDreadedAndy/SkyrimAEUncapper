/**
 * @file RelocPatch.h
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

#ifndef __SKYRIM_UNCAPPER_AE_RELOC_PATCH_H__
#define __SKYRIM_UNCAPPER_AE_RELOC_PATCH_H__

void ApplyGamePatches(void);

#endif /* __SKYRIM_UNCAPPER_AE_RELOC_PATCH_H__ */
