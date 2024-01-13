#include "zipfile.h"
#include "zipfile_internal.h"

#include <ctime>
#include <filesystem>
#include <fmt/format.h>
#include <zlib.h>

const static uint32_t sigLocalFileHeader = 0x04034b50;
const static uint32_t sigCentralFileHeader = 0x02014b50;
const static uint32_t sigEndCentralDir = 0x06054b50;
const static uint32_t sigZip64EndCentralDirRecord = 0x06064b50;
const static uint32_t sigZip64EndCentralDirLocator = 0x07064b50;

const static uint8_t lenLocalHeader = 30;      // + filename + extra
const static uint8_t lenCentralDirHeader = 46; // + filename + extra + comment
const static uint8_t lenEndCentralDir = 22;    // + comment
const static uint8_t lenZip64EndCentralDirLocator = 20; //
const static uint8_t lenZip64EndCentralDir = 56;        // + extra

// CreatorVersion
const static uint16_t creatorDOS = 0;
const static uint16_t creatorUnix = 3;
const static uint16_t creatorNTFS = 10;

// ZIP Version
const static uint16_t zipVersion45 = 45; // 4.5 (zip64 support)
const static uint16_t zipVersion63 = 63; // 6.3 (utf-8 support)

// Compression method
const static uint16_t methodStore = 0; // no compression

// Flags
const static uint16_t flagUTF8 = 0x800;

// Extra header IDs
const static uint16_t zip64ExtraID = 0x0001;    // Zip64 extended information
const static uint16_t extTimeExtraID = 0x5455;  // Extended timestamp
const static uint16_t extTimeExtraSize = 5;     // ModTime only
const static uint8_t extTimeFlagModTime = 0x01; // ModTime only

ZipFile::ZipFile() {}

ZipFile::ZipFile(std::filesystem::path filename) { open(filename); }

ZipFile::~ZipFile() { close(); }

void ZipFile::open(std::filesystem::path filename)
{
    close();

    if (std::filesystem::exists(filename)) {
        fs = std::fstream();
        fs.open(filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!fs.is_open()) {
            throw std::runtime_error("failed to open file");
        }
        readEndOfCentralDir();
        readCentralDir();
    } else {
        fs = std::fstream();
        fs.open(filename, std::ios::binary | std::ios::in | std::ios::out |
                              std::ios::trunc);
        if (!fs.is_open()) {
            throw std::runtime_error("failed to create file");
        }
        eocd = new ZipEndOfCentralDir;

        eocd64 = new Zip64EndOfCentralDir;
        eocd64->size_eocd = lenZip64EndCentralDir - 12;
        eocd64->creator_version = zipVersion63 | (creatorDOS << 8);
        eocd64->reader_version = zipVersion45 | (creatorDOS << 8);

        eocd64_locator = new Zip64EndCentralDirLocator;
        eocd64_locator->total_disk_number = 1;

        flush();
    }
}

void ZipFile::close()
{
    if (fs.is_open() && flush_needed) {
        flush();
        fs.close();
    }

    for (auto &entry : dir_entries) {
        delete entry;
    }
    dir_entries.clear();
    dir_entry_map.clear();

    if (eocd) {
        delete eocd;
        eocd = nullptr;
    }
}

void ZipFile::readEndOfCentralDir()
{
    // Read end of central directory, assume there is no comment
    fs.seekg(-lenEndCentralDir, std::ios::end);
    uint64_t offset_eocd = fs.tellg();
    if (zipReadU32(fs) != sigEndCentralDir) {
        throw std::runtime_error("failed to find end of central dir record");
    }
    eocd = new ZipEndOfCentralDir;
    zipRead(fs, &eocd->disk_number);
    zipRead(fs, &eocd->dir_disk_number);
    zipRead(fs, &eocd->n_dir_records_this_disk);
    zipRead(fs, &eocd->n_dir_records);
    zipRead(fs, &eocd->dir_size);
    zipRead(fs, &eocd->dir_offset);
    zipRead(fs, &eocd->len_comment);
    if (eocd->len_comment != 0) {
        throw std::runtime_error(
            "error when reading end of central dir record");
    }
    if ((eocd->disk_number != 0) || (eocd->dir_disk_number != 0)) {
        throw std::runtime_error("multi-volume zip file is not supported");
    }
    if (eocd->dir_offset + eocd->dir_size > offset_eocd) {
        throw std::runtime_error("invalid central dir offset");
    }

    // Zip64
    if (readZip64EndOfCentralDir(offset_eocd)) {
        return;
    } else if ((eocd->n_dir_records == 0xffff) ||
               (eocd->dir_size == 0xffffffff) ||
               (eocd->dir_offset == 0xffffffff))
    {
        throw std::runtime_error("failed to read zip64 central dir");
    } else {
        eocd64 = new Zip64EndOfCentralDir;
        eocd64->size_eocd = lenZip64EndCentralDir - 12;
        eocd64->creator_version = zipVersion63 | (creatorDOS << 8);
        eocd64->reader_version = zipVersion45 | (creatorDOS << 8);
        eocd64->disk_number = eocd->disk_number;
        eocd64->dir_disk_number = eocd->dir_disk_number;
        eocd64->n_dir_records = eocd->n_dir_records;
        eocd64->n_dir_records_this_disk = eocd->n_dir_records_this_disk;
        eocd64->dir_size = eocd->dir_size;
        eocd64->dir_offset = eocd->dir_offset;

        eocd64_locator = new Zip64EndCentralDirLocator;
        eocd64_locator->total_disk_number = 1;
        return;
    }
}

bool ZipFile::readZip64EndOfCentralDir(uint64_t offset_eocd)
{
    fs.seekg(offset_eocd - lenZip64EndCentralDirLocator, std::ios::beg);
    if (zipReadU32(fs) != sigZip64EndCentralDirLocator) {
        return false;
    }

    // Read Zip64 end of central dir locator
    eocd64_locator = new Zip64EndCentralDirLocator;
    zipRead(fs, &eocd64_locator->dir_disk_number);
    zipRead(fs, &eocd64_locator->eocd64_offset);
    zipRead(fs, &eocd64_locator->total_disk_number);

    // Read Zip64 end of central dir
    fs.seekg(eocd64_locator->eocd64_offset, std::ios::beg);
    if (zipReadU32(fs) != sigZip64EndCentralDirRecord) {
        return false;
    }
    eocd64 = new Zip64EndOfCentralDir;
    zipRead(fs, &eocd64->size_eocd);
    zipRead(fs, &eocd64->creator_version);
    zipRead(fs, &eocd64->reader_version);
    zipRead(fs, &eocd64->disk_number);
    zipRead(fs, &eocd64->dir_disk_number);
    zipRead(fs, &eocd64->n_dir_records_this_disk);
    zipRead(fs, &eocd64->n_dir_records);
    zipRead(fs, &eocd64->dir_size);
    zipRead(fs, &eocd64->dir_offset);
    uint64_t size_extra = eocd64->size_eocd + 12 - lenZip64EndCentralDir;
    eocd64->extra = zipReadString(fs, size_extra);

    // Check values
    if ((eocd64_locator->dir_disk_number != 0) ||
        (eocd64_locator->total_disk_number != 1))
    {
        throw std::runtime_error("multi-volume zip64 file is not supported");
    }

    return true;
}

void ZipFile::readCentralDir()
{
    fs.seekg(eocd64->dir_offset, std::ios::beg);

    for (int i = 0; i < eocd64->n_dir_records; i++) {
        if (zipReadU32(fs) != sigCentralFileHeader) {
            throw std::runtime_error(
                fmt::format("invalid central dir header at record {}/{}", i,
                            eocd->n_dir_records));
        }
        ZipDirEntry *entry = new ZipDirEntry;
        zipRead(fs, &entry->creator_version);
        zipRead(fs, &entry->reader_version);
        zipRead(fs, &entry->flags);
        zipRead(fs, &entry->method);
        zipRead(fs, &entry->modified_time);
        zipRead(fs, &entry->modified_date);
        zipRead(fs, &entry->crc32);
        zipRead(fs, &entry->compressed_size);
        zipRead(fs, &entry->uncompressed_size);
        zipRead(fs, &entry->len_filename);
        zipRead(fs, &entry->len_extra_central);
        zipRead(fs, &entry->len_comment);
        zipRead(fs, &entry->start_disk_number);
        zipRead(fs, &entry->internal_attrs);
        zipRead(fs, &entry->external_attrs);
        zipRead(fs, &entry->header_offset);
        entry->filename = zipReadString(fs, entry->len_filename);
        entry->extra_central = zipReadString(fs, entry->len_extra_central);
        entry->comment = zipReadString(fs, entry->len_comment);

        // fill with 32-bit info as default
        entry->header_offset64 = entry->header_offset;

        // Read extra fields
        if (entry->len_extra_central > 0) {
            std::stringstream ss_extra(entry->extra_central);
            while (!ss_extra.eof()) {
                uint16_t header_id = zipReadU16(ss_extra);
                uint16_t data_size = zipReadU16(ss_extra);
                std::string data = zipReadString(ss_extra, data_size);
                std::stringstream ss_extra_data(data);
                if (header_id == zip64ExtraID) {
                    if (entry->uncompressed_size == 0xffffffff) {
                        uint64_t value;
                        zipRead(ss_extra_data, &value);
                        if (value > 0xffffffff) {
                            throw std::runtime_error("uncompressed_size > 4G");
                        }
                        entry->uncompressed_size = value;
                    }
                    if (entry->compressed_size == 0xffffffff) {
                        uint64_t value;
                        zipRead(ss_extra_data, &value);
                        if (value > 0xffffffff) {
                            throw std::runtime_error("compressed_size > 4G");
                        }
                        entry->compressed_size = value;
                    }
                    if (entry->header_offset == 0xffffffff) {
                        uint64_t value;
                        zipRead(ss_extra_data, &value);
                        entry->header_offset64 = value;
                    }
                }
                if (header_id == extTimeExtraID) {
                    uint8_t ext_time_flag = zipReadU8(ss_extra_data);
                    if (ext_time_flag && extTimeFlagModTime) {
                        zipRead(ss_extra_data, &entry->unix_modtime);
                    }
                }
            }
        }

        dir_entries.push_back(entry);
        dir_entry_map[entry->filename] = entry;
    }

    uint64_t dir_end_offset = fs.tellg();
    uint64_t dir_size = dir_end_offset - eocd64->dir_offset;
    if (dir_size != eocd64->dir_size) {
        throw std::runtime_error("central dir size does not match eocd record");
    }

    fs.seekg(eocd64->dir_offset, std::ios::beg);
    char *buf = (char *)malloc(eocd64->dir_size);
    fs.read(buf, eocd64->dir_size);
    dir_stream.write(buf, eocd64->dir_size);
}

std::vector<std::string> ZipFile::Filenames()
{
    std::shared_lock<std::shared_mutex> lk(zip_mutex);

    std::vector<std::string> filenames;
    for (const auto &entry : dir_entries) {
        filenames.push_back(entry->filename);
    }
    return filenames;
}

std::string ZipFile::GetData(std::string name)
{
    //
    // Lock for I/O
    //
    std::unique_lock<std::shared_mutex> lk(zip_mutex);

    ZipDirEntry *central_dir_entry;
    if (auto it = dir_entry_map.find(name); it != dir_entry_map.end()) {
        central_dir_entry = it->second;
    } else {
        throw std::invalid_argument("not found");
    }

    // Read local header
    ZipDirEntry local_entry;
    fs.seekg(central_dir_entry->header_offset64);
    if (zipReadU32(fs) != sigLocalFileHeader) {
        throw std::runtime_error("invalid local header");
    }
    zipRead(fs, &local_entry.reader_version);
    zipRead(fs, &local_entry.flags);
    zipRead(fs, &local_entry.method);
    zipRead(fs, &local_entry.modified_time);
    zipRead(fs, &local_entry.modified_date);
    zipRead(fs, &local_entry.crc32);
    zipRead(fs, &local_entry.compressed_size);
    zipRead(fs, &local_entry.uncompressed_size);
    zipRead(fs, &local_entry.len_filename);
    zipRead(fs, &local_entry.len_extra_local);
    local_entry.filename = zipReadString(fs, local_entry.len_filename);
    local_entry.extra_local = zipReadString(fs, local_entry.len_extra_local);

    if (local_entry.method != methodStore) {
        throw std::runtime_error("compressed data is not supported");
    }
    if ((local_entry.compressed_size == 0xffffffff) ||
        (local_entry.uncompressed_size == 0xffffffff))
    {
        throw std::runtime_error("local entry file size = FFFFFFFF");
    }

    // Get data
    std::string buf;
    buf.resize(central_dir_entry->compressed_size);
    fs.read((char *)&buf[0], central_dir_entry->compressed_size);

    // Check CRC32
    uint32_t crc = crc32(0, (uint8_t *)&buf[0], buf.size());
    if (local_entry.crc32 != crc) {
        throw std::runtime_error("crc32 mismatch");
    }

    return buf;
}

void ZipFile::AddFile(std::string name, std::string buf)
{
    if (buf.size() > 0xffffffff) {
        throw std::invalid_argument("buf too large");
    }

    uint32_t crc = crc32(0, (uint8_t *)&buf[0], buf.size());

    std::time_t unixtime = std::time(nullptr);
    std::tm *tm;
    tm = localtime(&unixtime);
    uint16_t msdos_date = tm->tm_mday + ((tm->tm_mon + 1) << 5) +
                          ((tm->tm_year + 1900 - 1980) << 9);
    uint16_t msdos_time =
        tm->tm_sec / 2 + (tm->tm_min << 5) + (tm->tm_hour << 11);

    ZipDirEntry *entry = new ZipDirEntry;
    entry->creator_version = zipVersion63 | (creatorDOS << 8);
    entry->reader_version = zipVersion45 | (creatorDOS << 8);
    entry->flags = flagUTF8;
    entry->method = methodStore;
    entry->modified_date = msdos_date;
    entry->modified_time = msdos_time;
    entry->crc32 = crc;
    entry->compressed_size = buf.size();
    entry->uncompressed_size = buf.size();
    entry->len_filename = name.size();
    entry->filename = name;

    entry->header_offset64 = eocd64->dir_offset;

    entry->header_offset = (entry->header_offset64 < 0xffffffff)
                               ? entry->header_offset64
                               : 0xffffffff;

    entry->unix_modtime = unixtime;

    // Encode extra fields
    std::stringstream ss_extra_local;
    std::stringstream ss_extra_central;

    // Zip64
    if (entry->header_offset == 0xffffffff) {
        zipWrite(ss_extra_central, zip64ExtraID);
        zipWrite(ss_extra_central, (uint16_t)8);
        zipWrite(ss_extra_central, entry->header_offset64);
    }

    // Extended Timestamp
    if (entry->unix_modtime) {
        zipWrite(ss_extra_local, extTimeExtraID);
        zipWrite(ss_extra_local, extTimeExtraSize);
        zipWrite(ss_extra_local, extTimeFlagModTime);
        zipWrite(ss_extra_local, entry->unix_modtime);

        zipWrite(ss_extra_central, extTimeExtraID);
        zipWrite(ss_extra_central, extTimeExtraSize);
        zipWrite(ss_extra_central, extTimeFlagModTime);
        zipWrite(ss_extra_central, entry->unix_modtime);
    }

    entry->extra_local = ss_extra_local.str();
    entry->extra_central = ss_extra_central.str();

    entry->len_extra_local = entry->extra_local.size();
    entry->len_extra_central = entry->extra_central.size();


    //
    // Lock for adding entry and I/O
    //
    std::unique_lock<std::shared_mutex> lk(zip_mutex);

    // Add entry
    dir_entries.push_back(entry);
    dir_entry_map[name] = entry;

    // Write local header to file
    fs.seekp(eocd64->dir_offset, std::ios::beg);
    flush_needed = true;

    zipWrite(fs, sigLocalFileHeader);
    zipWrite(fs, entry->reader_version);
    zipWrite(fs, entry->flags);
    zipWrite(fs, entry->method);
    zipWrite(fs, entry->modified_time);
    zipWrite(fs, entry->modified_date);
    zipWrite(fs, entry->crc32);
    zipWrite(fs, entry->compressed_size);
    zipWrite(fs, entry->uncompressed_size);
    zipWrite(fs, entry->len_filename);
    zipWrite(fs, entry->len_extra_local);
    zipWrite(fs, entry->filename);
    zipWrite(fs, entry->extra_local);

    // Write data to file
    zipWrite(fs, buf);

    // Update EOCD
    eocd64->n_dir_records_this_disk++;
    eocd64->n_dir_records++;
    eocd64->dir_size += lenCentralDirHeader + entry->filename.size() +
                        entry->extra_central.size() + entry->comment.size();
    eocd64->dir_offset = fs.tellp();

    // Update central dir stream
    zipWrite(dir_stream, sigCentralFileHeader);
    zipWrite(dir_stream, entry->creator_version);
    zipWrite(dir_stream, entry->reader_version);
    zipWrite(dir_stream, entry->flags);
    zipWrite(dir_stream, entry->method);
    zipWrite(dir_stream, entry->modified_time);
    zipWrite(dir_stream, entry->modified_date);
    zipWrite(dir_stream, entry->crc32);
    zipWrite(dir_stream, entry->compressed_size);
    zipWrite(dir_stream, entry->uncompressed_size);
    zipWrite(dir_stream, entry->len_filename);
    zipWrite(dir_stream, entry->len_extra_central);
    zipWrite(dir_stream, entry->len_comment);
    zipWrite(dir_stream, entry->start_disk_number);
    zipWrite(dir_stream, entry->internal_attrs);
    zipWrite(dir_stream, entry->external_attrs);
    zipWrite(dir_stream, entry->header_offset);
    zipWrite(dir_stream, entry->filename);
    zipWrite(dir_stream, entry->extra_central);
    zipWrite(dir_stream, entry->comment);
}

void ZipFile::flush()
{
    std::unique_lock<std::shared_mutex> lk(zip_mutex);

    fs.seekp(eocd64->dir_offset, std::ios::beg);

    // Write central dir to file
    zipWrite(fs, dir_stream.str());

    // Write Zip64 end of central dir
    eocd64_locator->eocd64_offset = fs.tellp();
    zipWrite(fs, sigZip64EndCentralDirRecord);
    zipWrite(fs, eocd64->size_eocd);
    zipWrite(fs, eocd64->creator_version);
    zipWrite(fs, eocd64->reader_version);
    zipWrite(fs, eocd64->disk_number);
    zipWrite(fs, eocd64->dir_disk_number);
    zipWrite(fs, eocd64->n_dir_records_this_disk);
    zipWrite(fs, eocd64->n_dir_records);
    zipWrite(fs, eocd64->dir_size);
    zipWrite(fs, eocd64->dir_offset);
    zipWrite(fs, eocd64->extra);

    // Write Zip64 end of central dir locator
    zipWrite(fs, sigZip64EndCentralDirLocator);
    zipWrite(fs, eocd64_locator->dir_disk_number);
    zipWrite(fs, eocd64_locator->eocd64_offset);
    zipWrite(fs, eocd64_locator->total_disk_number);

    // Update end of central dir struct
    eocd->n_dir_records =
        (eocd64->n_dir_records < 0xffff) ? eocd64->n_dir_records : 0xffff;
    eocd->n_dir_records_this_disk = (eocd64->n_dir_records_this_disk < 0xffff)
                                        ? eocd64->n_dir_records_this_disk
                                        : 0xffff;
    eocd->dir_size =
        (eocd64->dir_size < 0xffffffff) ? eocd64->dir_size : 0xffffffff;
    eocd->dir_offset =
        (eocd64->dir_offset < 0xffffffff) ? eocd64->dir_offset : 0xffffffff;

    // Write End of central dir to file
    zipWrite(fs, sigEndCentralDir);
    zipWrite(fs, eocd->disk_number);
    zipWrite(fs, eocd->dir_disk_number);
    zipWrite(fs, eocd->n_dir_records_this_disk);
    zipWrite(fs, eocd->n_dir_records);
    zipWrite(fs, eocd->dir_size);
    zipWrite(fs, eocd->dir_offset);
    zipWrite(fs, eocd->len_comment);

    fs.sync();

    flush_needed = false;
}
