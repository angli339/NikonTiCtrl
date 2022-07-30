#ifndef ZIPFILE_INTERNAL_H
#define ZIPFILE_INTERNAL_H

#include <fstream>
#include <string>

template <typename T>
void zipRead(std::istream &is, T *value)
{
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>);
    is.read(reinterpret_cast<char *>(value), sizeof(*value));
}

inline uint8_t zipReadU8(std::istream &is)
{
    uint8_t value;
    zipRead(is, &value);
    return value;
}

inline uint16_t zipReadU16(std::istream &is)
{
    uint16_t value;
    zipRead(is, &value);
    return value;
}

inline uint32_t zipReadU32(std::istream &is)
{
    uint32_t value;
    zipRead(is, &value);
    return value;
}

inline std::string zipReadString(std::istream &is, size_t size)
{
    if (size == 0) {
        return "";
    }
    char *buf = (char *)malloc(size);
    is.read(buf, size);
    std::string value = std::string(buf, size);
    free(buf);
    return value;
}

template <typename T>
void zipWrite(std::ostream &os, T value)
{
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>);
    os.write(reinterpret_cast<char *>(&value), sizeof(value));
}

inline void zipWrite(std::ostream &os, std::string value)
{
    if (value.empty()) {
        return;
    }
    os.write(reinterpret_cast<char *>(&value[0]), value.size());
}

#endif
