/**
 * @file RelocPatch.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exposes the function which patches the game binary.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_RELOC_PATCH_H__
#define __SKYRIM_UNCAPPER_AE_RELOC_PATCH_H__

int ApplyGamePatches(void *img_base, unsigned int runtime_version);

#endif /* __SKYRIM_UNCAPPER_AE_RELOC_PATCH_H__ */
