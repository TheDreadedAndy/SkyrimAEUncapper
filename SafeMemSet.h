/**
 * @file SafeMemSet.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exposes a memset wrapper around SafeWriteBuf.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_SAFE_MEM_SET_H__
#define __SKYRIM_UNCAPPER_AE_SAFE_MEM_SET_H__

#include <cstdint>

void SafeMemSet(uintptr_t a, uint8_t c, size_t n);

#endif /* __SKYRIM_UNCAPPER_AE_SAFE_MEM_SET_H__ */
