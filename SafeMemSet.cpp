/**
 * @file SafeMemSet.cpp
 * @author Andrew Spaulding (Kasplat)
 * @brief Implementation of the SafeMemSet function.
 * @bug No known bugs.
 */

#include "SafeMemSet.h"

#include <cstdint>

#include "SafeWrite.h"
#include "common/IErrors.h"

/// @brief Gets the min of two numbers.
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief Sets the given memory region to the given byte value.
 */
void
SafeMemSet(
    uintptr_t a,
    uint8_t c,
    size_t n
) {
    if (!n) { return; }

    // We do the SafeWriteBuf operations in bulk.
    const size_t buf_size = 256;
    uint8_t buf[buf_size];

    memset(buf, c, MIN(buf_size, n));
    while (n) {
        size_t chunk = MIN(buf_size, n);
        ASSERT(SafeWriteBuf(a, buf, chunk));
        a += chunk;
        n -= chunk;
    }
}
