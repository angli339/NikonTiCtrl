#ifndef ZIPFILE_H
#define ZIPFILE_H

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <shared_mutex>

struct ZipDirEntry;
struct ZipEndOfCentralDir;
struct Zip64EndCentralDirLocator;
struct Zip64EndOfCentralDir;

class ZipFile {
public:
    ZipFile();
    ZipFile(std::filesystem::path filename);
    ~ZipFile();

    void open(std::filesystem::path filename);
    void close();
    bool is_open();
    void flush();

    std::vector<std::string> Filenames();

    std::string GetData(std::string name);
    void AddFile(std::string name, std::string buf);

private:
    std::shared_mutex zip_mutex;

    std::fstream fs;
    std::stringstream dir_stream;
    bool flush_needed = false;
    bool zip_file_open = false;

    ZipEndOfCentralDir *eocd = nullptr;
    Zip64EndOfCentralDir *eocd64 = nullptr;
    Zip64EndCentralDirLocator *eocd64_locator = nullptr;
    std::vector<ZipDirEntry *> dir_entries;
    std::map<std::string, ZipDirEntry *> dir_entry_map;
    void readEndOfCentralDir();
    void readCentralDir();
    bool readZip64EndOfCentralDir(size_t offset_eocd);
};

struct ZipEndOfCentralDir {
    uint16_t disk_number = 0;
    uint16_t dir_disk_number = 0;
    uint16_t n_dir_records_this_disk = 0;
    uint16_t n_dir_records = 0;
    uint32_t dir_size = 0;
    uint32_t dir_offset = 0;
    uint16_t len_comment = 0;
};

struct Zip64EndOfCentralDir {
    uint64_t size_eocd =
        0; // does not include 12 bytes of signature + size_eocd
    uint16_t creator_version = 0;
    uint16_t reader_version = 0;
    uint32_t disk_number = 0;
    uint32_t dir_disk_number = 0;
    uint64_t n_dir_records_this_disk = 0;
    uint64_t n_dir_records = 0;
    uint64_t dir_size = 0;
    uint64_t dir_offset = 0;
    std::string extra;
};

struct Zip64EndCentralDirLocator {
    uint32_t dir_disk_number = 0;
    uint64_t eocd64_offset = 0;
    uint32_t total_disk_number = 0;
};

struct ZipDirEntry {
    uint16_t creator_version = 0;
    uint16_t reader_version = 0;
    uint16_t flags = 0;
    uint16_t method = 0;
    uint16_t modified_time = 0;
    uint16_t modified_date = 0;
    uint32_t crc32 = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    uint16_t len_filename = 0;
    uint16_t len_extra = 0;
    uint16_t len_comment = 0;
    uint16_t start_disk_number = 0;
    uint16_t internal_attrs = 0;
    uint32_t external_attrs = 0;
    uint32_t header_offset = 0;
    std::string filename;
    std::string extra;
    std::string comment;

    // Zip64
    uint64_t compressed_size64 = 0;
    uint64_t uncompressed_size64 = 0;
    uint64_t header_offset64 = 0;

    // Extended Timestamp
    uint32_t unix_modtime = 0;
};

#endif
