/**
 * @file Ini.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exposes functions and classes for working with INI files.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_INI_H__
#define __SKYRIM_UNCAPPER_AE_INI_H__

#include <string>

#include "simpleini/SimpleIni.h"

/**
 * @brief Gets the character prefix of a type.
 */
///@{
template <typename T> inline char GetPrefix();
template <> inline char GetPrefix<float>() { return 'f'; }
template <> inline char GetPrefix<unsigned int>() { return 'i'; }
template <> inline char GetPrefix<bool>() { return 'b'; }
///@}

/**
 * @brief Reads a value from an INI file.
 */
///@{
template <typename T> inline T ReadIniValue(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    T default_val
);

template <> inline float
ReadIniValue<float>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    float default_val
) {
    return ini.GetDoubleValue(section, key, default_val);
}

template <> inline unsigned int
ReadIniValue<unsigned int>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    unsigned int default_val
) {
    return ini.GetLongValue(section, key, default_val);
}

template <> inline bool
ReadIniValue<bool>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    bool default_val
) {
    return ini.GetBoolValue(section, key, default_val);
}

template <> inline std::string
ReadIniValue<std::string>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    std::string default_val
) {
    return std::string(ini.GetValue(section, key, default_val.c_str()));
}
///@}

/**
 * @brief Writes a value to an INI file.
 */
///@{
template <typename T> inline void SaveIniValue(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    T val,
    const char *comment
);

template <> inline void
SaveIniValue<float>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    float val,
    const char *comment
) {
    ASSERT(ini.SetDoubleValue(section, key, val, comment));
}

template <> inline void
SaveIniValue<unsigned int>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    unsigned int val,
    const char *comment
) {
    ASSERT(ini.SetLongValue(section, key, val, comment));
}

template <> inline void
SaveIniValue<bool>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    bool val,
    const char *comment
) {
    ASSERT(ini.SetBoolValue(section, key, val, comment));
}

template <> inline void
SaveIniValue<std::string>(
    CSimpleIniA &ini,
    const char *section,
    const char *key,
    std::string val,
    const char *comment
) {
    ASSERT(ini.SetValue(section, key, val.c_str(), comment));
}
///@}

template<typename T>
class SectionField {
  private:
    T val;
    const char *name;
    T defaultVal;

  public:
    SectionField(
        const char *name,
        T default_val
    ) : val(default_val),
        name(name),
        defaultVal(default_val)
    {}

    /**
     * @brief Gets the value of the field.
     */
    T
    Get() {
        return val;
    }

    /**
     * @brief Sets the value of the field.
     */
    void
    Set(
        T v
    ) {
        val = v;
    }

    /**
     * @brief Reads in the value from the given INI file.
     * @param ini The INI to read the value from.
     * @param section The section to read the value from.
     */
    void
    ReadConfig(
        CSimpleIniA &ini,
        const char *section
    ) {
        val = ReadIniValue(ini, section, name, defaultVal);
    }

    /**
     * @brief Saves the value to the given INI.
     * @param ini The INI file to save the value to.
     * @param section The section to read the value from.
     */
    void
    SaveConfig(
        CSimpleIniA &ini,
        const char *section,
        const char *comment
    ) {
        SaveIniValue(ini, section, name, val, comment);
    }
};

#endif /* __SKYRIM_UNCAPPER_AE_INI_H__ */
