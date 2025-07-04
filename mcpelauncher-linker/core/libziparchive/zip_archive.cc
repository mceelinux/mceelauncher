/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Read-only access to Zip archives, with minimal heap allocation.
 */

#define LOG_TAG "ziparchive"

#include "ziparchive/zip_archive.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <memory>
#include <vector>

#if defined(__APPLE__) || defined(__FreeBSD__)
#define lseek64 lseek
#endif

#if defined(__BIONIC__)
#include <android/fdsan.h>
#endif

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>  // TEMP_FAILURE_RETRY may or may not be in unistd
#include <android-base/mapped_file.h>
#include <android-base/memory.h>
#include <android-base/strings.h>
#include <android-base/utf8.h>
#include <log/log.h>
#include "zlib.h"

#include "entry_name_utils-inl.h"
#include "zip_archive_common.h"
#include "zip_archive_private.h"

using android::base::get_unaligned;

// Used to turn on crc checks - verify that the content CRC matches the values
// specified in the local file header and the central directory.
static constexpr bool kCrcChecksEnabled = false;

// The maximum number of bytes to scan backwards for the EOCD start.
static const uint32_t kMaxEOCDSearch = kMaxCommentLen + sizeof(EocdRecord);

/*
 * A Read-only Zip archive.
 *
 * We want "open" and "find entry by name" to be fast operations, and
 * we want to use as little memory as possible.  We memory-map the zip
 * central directory, and load a hash table with pointers to the filenames
 * (which aren't null-terminated).  The other fields are at a fixed offset
 * from the filename, so we don't need to extract those (but we do need
 * to byte-read and endian-swap them every time we want them).
 *
 * It's possible that somebody has handed us a massive (~1GB) zip archive,
 * so we can't expect to mmap the entire file.
 *
 * To speed comparisons when doing a lookup by name, we could make the mapping
 * "private" (copy-on-write) and null-terminate the filenames after verifying
 * the record structure.  However, this requires a private mapping of
 * every page that the Central Directory touches.  Easier to tuck a copy
 * of the string length into the hash table entry.
 */

/*
 * Round up to the next highest power of 2.
 *
 * Found on http://graphics.stanford.edu/~seander/bithacks.html.
 */
static uint32_t RoundUpPower2(uint32_t val) {
  val--;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  val++;

  return val;
}

static uint32_t ComputeHash(std::string_view name) {
  return static_cast<uint32_t>(std::hash<std::string_view>{}(name));
}

/*
 * Convert a ZipEntry to a hash table index, verifying that it's in a
 * valid range.
 */
static int64_t EntryToIndex(const ZipStringOffset* hash_table, const uint32_t hash_table_size,
                            std::string_view name, const uint8_t* start) {
  const uint32_t hash = ComputeHash(name);

  // NOTE: (hash_table_size - 1) is guaranteed to be non-negative.
  uint32_t ent = hash & (hash_table_size - 1);
  while (hash_table[ent].name_offset != 0) {
    if (hash_table[ent].ToStringView(start) == name) {
      return ent;
    }
    ent = (ent + 1) & (hash_table_size - 1);
  }

  ALOGV("Zip: Unable to find entry %.*s", static_cast<int>(name.size()), name.data());
  return kEntryNotFound;
}

/*
 * Add a new entry to the hash table.
 */
static int32_t AddToHash(ZipStringOffset* hash_table, const uint32_t hash_table_size,
                         std::string_view name, const uint8_t* start) {
  const uint64_t hash = ComputeHash(name);
  uint32_t ent = hash & (hash_table_size - 1);

  /*
   * We over-allocated the table, so we're guaranteed to find an empty slot.
   * Further, we guarantee that the hashtable size is not 0.
   */
  while (hash_table[ent].name_offset != 0) {
    if (hash_table[ent].ToStringView(start) == name) {
      // We've found a duplicate entry. We don't accept duplicates.
      ALOGW("Zip: Found duplicate entry %.*s", static_cast<int>(name.size()), name.data());
      return kDuplicateEntry;
    }
    ent = (ent + 1) & (hash_table_size - 1);
  }

  // `name` has already been validated before entry.
  const char* start_char = reinterpret_cast<const char*>(start);
  hash_table[ent].name_offset = static_cast<uint32_t>(name.data() - start_char);
  hash_table[ent].name_length = static_cast<uint16_t>(name.size());
  return 0;
}

#if defined(__BIONIC__)
uint64_t GetOwnerTag(const ZipArchive* archive) {
  return android_fdsan_create_owner_tag(ANDROID_FDSAN_OWNER_TYPE_ZIPARCHIVE,
                                        reinterpret_cast<uint64_t>(archive));
}
#endif

ZipArchive::ZipArchive(MappedZipFile&& map, bool assume_ownership)
    : mapped_zip(map),
      close_file(assume_ownership),
      directory_offset(0),
      central_directory(),
      directory_map(),
      num_entries(0),
      hash_table_size(0),
      hash_table(nullptr) {
#if defined(__BIONIC__)
  if (assume_ownership) {
    CHECK(mapped_zip.HasFd());
    android_fdsan_exchange_owner_tag(mapped_zip.GetFileDescriptor(), 0, GetOwnerTag(this));
  }
#endif
}

ZipArchive::ZipArchive(const void* address, size_t length)
    : mapped_zip(address, length),
      close_file(false),
      directory_offset(0),
      central_directory(),
      directory_map(),
      num_entries(0),
      hash_table_size(0),
      hash_table(nullptr) {}

ZipArchive::~ZipArchive() {
  if (close_file && mapped_zip.GetFileDescriptor() >= 0) {
#if defined(__BIONIC__)
    android_fdsan_close_with_tag(mapped_zip.GetFileDescriptor(), GetOwnerTag(this));
#else
    close(mapped_zip.GetFileDescriptor());
#endif
  }

  free(hash_table);
}

static int32_t MapCentralDirectory0(const char* debug_file_name, ZipArchive* archive,
                                    off64_t file_length, uint32_t read_amount,
                                    uint8_t* scan_buffer) {
  const off64_t search_start = file_length - read_amount;

  if (!archive->mapped_zip.ReadAtOffset(scan_buffer, read_amount, search_start)) {
    ALOGE("Zip: read %" PRId64 " from offset %" PRId64 " failed", static_cast<int64_t>(read_amount),
          static_cast<int64_t>(search_start));
    return kIoError;
  }

  /*
   * Scan backward for the EOCD magic.  In an archive without a trailing
   * comment, we'll find it on the first try.  (We may want to consider
   * doing an initial minimal read; if we don't find it, retry with a
   * second read as above.)
   */
  CHECK_LE(read_amount, std::numeric_limits<int32_t>::max());
  int32_t i = read_amount - sizeof(EocdRecord);
  for (; i >= 0; i--) {
    if (scan_buffer[i] == 0x50) {
      uint32_t* sig_addr = reinterpret_cast<uint32_t*>(&scan_buffer[i]);
      if (get_unaligned<uint32_t>(sig_addr) == EocdRecord::kSignature) {
        ALOGV("+++ Found EOCD at buf+%d", i);
        break;
      }
    }
  }
  if (i < 0) {
    ALOGD("Zip: EOCD not found, %s is not zip", debug_file_name);
    return kInvalidFile;
  }

  const off64_t eocd_offset = search_start + i;
  const EocdRecord* eocd = reinterpret_cast<const EocdRecord*>(scan_buffer + i);
  /*
   * Verify that there's no trailing space at the end of the central directory
   * and its comment.
   */
  const off64_t calculated_length = eocd_offset + sizeof(EocdRecord) + eocd->comment_length;
  if (calculated_length != file_length) {
    ALOGW("Zip: %" PRId64 " extraneous bytes at the end of the central directory",
          static_cast<int64_t>(file_length - calculated_length));
    return kInvalidFile;
  }

  /*
   * Grab the CD offset and size, and the number of entries in the
   * archive and verify that they look reasonable.
   */
  if (static_cast<off64_t>(eocd->cd_start_offset) + eocd->cd_size > eocd_offset) {
    ALOGW("Zip: bad offsets (dir %" PRIu32 ", size %" PRIu32 ", eocd %" PRId64 ")",
          eocd->cd_start_offset, eocd->cd_size, static_cast<int64_t>(eocd_offset));
    return kInvalidOffset;
  }
  if (eocd->num_records == 0) {
#if defined(__ANDROID__)
    ALOGW("Zip: empty archive?");
#endif
    return kEmptyArchive;
  }

  ALOGV("+++ num_entries=%" PRIu32 " dir_size=%" PRIu32 " dir_offset=%" PRIu32, eocd->num_records,
        eocd->cd_size, eocd->cd_start_offset);

  // It all looks good.  Create a mapping for the CD, and set the fields
  // in archive.
  if (!archive->InitializeCentralDirectory(static_cast<off64_t>(eocd->cd_start_offset),
                                           static_cast<size_t>(eocd->cd_size))) {
    return kMmapFailed;
  }

  archive->num_entries = eocd->num_records;
  archive->directory_offset = eocd->cd_start_offset;

  return 0;
}

/*
 * Find the zip Central Directory and memory-map it.
 *
 * On success, returns 0 after populating fields from the EOCD area:
 *   directory_offset
 *   directory_ptr
 *   num_entries
 */
static int32_t MapCentralDirectory(const char* debug_file_name, ZipArchive* archive) {
  // Test file length. We use lseek64 to make sure the file
  // is small enough to be a zip file (Its size must be less than
  // 0xffffffff bytes).
  off64_t file_length = archive->mapped_zip.GetFileLength();
  if (file_length == -1) {
    return kInvalidFile;
  }

  if (file_length > static_cast<off64_t>(0xffffffff)) {
    ALOGV("Zip: zip file too long %" PRId64, static_cast<int64_t>(file_length));
    return kInvalidFile;
  }

  if (file_length < static_cast<off64_t>(sizeof(EocdRecord))) {
    ALOGV("Zip: length %" PRId64 " is too small to be zip", static_cast<int64_t>(file_length));
    return kInvalidFile;
  }

  /*
   * Perform the traditional EOCD snipe hunt.
   *
   * We're searching for the End of Central Directory magic number,
   * which appears at the start of the EOCD block.  It's followed by
   * 18 bytes of EOCD stuff and up to 64KB of archive comment.  We
   * need to read the last part of the file into a buffer, dig through
   * it to find the magic number, parse some values out, and use those
   * to determine the extent of the CD.
   *
   * We start by pulling in the last part of the file.
   */
  uint32_t read_amount = kMaxEOCDSearch;
  if (file_length < read_amount) {
    read_amount = static_cast<uint32_t>(file_length);
  }

  std::vector<uint8_t> scan_buffer(read_amount);
  int32_t result =
      MapCentralDirectory0(debug_file_name, archive, file_length, read_amount, scan_buffer.data());
  return result;
}

/*
 * Parses the Zip archive's Central Directory.  Allocates and populates the
 * hash table.
 *
 * Returns 0 on success.
 */
static int32_t ParseZipArchive(ZipArchive* archive) {
  const uint8_t* const cd_ptr = archive->central_directory.GetBasePtr();
  const size_t cd_length = archive->central_directory.GetMapLength();
  const uint16_t num_entries = archive->num_entries;

  /*
   * Create hash table.  We have a minimum 75% load factor, possibly as
   * low as 50% after we round off to a power of 2.  There must be at
   * least one unused entry to avoid an infinite loop during creation.
   */
  archive->hash_table_size = RoundUpPower2(1 + (num_entries * 4) / 3);
  archive->hash_table =
      reinterpret_cast<ZipStringOffset*>(calloc(archive->hash_table_size, sizeof(ZipStringOffset)));
  if (archive->hash_table == nullptr) {
    ALOGW("Zip: unable to allocate the %u-entry hash_table, entry size: %zu",
          archive->hash_table_size, sizeof(ZipStringOffset));
    return kAllocationFailed;
  }

  /*
   * Walk through the central directory, adding entries to the hash
   * table and verifying values.
   */
  const uint8_t* const cd_end = cd_ptr + cd_length;
  const uint8_t* ptr = cd_ptr;
  for (uint16_t i = 0; i < num_entries; i++) {
    if (ptr > cd_end - sizeof(CentralDirectoryRecord)) {
      ALOGW("Zip: ran off the end (item #%" PRIu16 ", %zu bytes of central directory)", i,
            cd_length);
#if defined(__ANDROID__)
      android_errorWriteLog(0x534e4554, "36392138");
#endif
      return kInvalidFile;
    }

    const CentralDirectoryRecord* cdr = reinterpret_cast<const CentralDirectoryRecord*>(ptr);
    if (cdr->record_signature != CentralDirectoryRecord::kSignature) {
      ALOGW("Zip: missed a central dir sig (at %" PRIu16 ")", i);
      return kInvalidFile;
    }

    const off64_t local_header_offset = cdr->local_file_header_offset;
    if (local_header_offset >= archive->directory_offset) {
      ALOGW("Zip: bad LFH offset %" PRId64 " at entry %" PRIu16,
            static_cast<int64_t>(local_header_offset), i);
      return kInvalidFile;
    }

    const uint16_t file_name_length = cdr->file_name_length;
    const uint16_t extra_length = cdr->extra_field_length;
    const uint16_t comment_length = cdr->comment_length;
    const uint8_t* file_name = ptr + sizeof(CentralDirectoryRecord);

    if (file_name + file_name_length > cd_end) {
      ALOGW("Zip: file name for entry %" PRIu16
            " exceeds the central directory range, file_name_length: %" PRIu16 ", cd_length: %zu",
            i, file_name_length, cd_length);
      return kInvalidEntryName;
    }
    // Check that file name is valid UTF-8 and doesn't contain NUL (U+0000) characters.
    if (!IsValidEntryName(file_name, file_name_length)) {
      ALOGW("Zip: invalid file name at entry %" PRIu16, i);
      return kInvalidEntryName;
    }

    // Add the CDE filename to the hash table.
    std::string_view entry_name{reinterpret_cast<const char*>(file_name), file_name_length};
    const int add_result = AddToHash(archive->hash_table, archive->hash_table_size, entry_name,
                                     archive->central_directory.GetBasePtr());
    if (add_result != 0) {
      ALOGW("Zip: Error adding entry to hash table %d", add_result);
      return add_result;
    }

    ptr += sizeof(CentralDirectoryRecord) + file_name_length + extra_length + comment_length;
    if ((ptr - cd_ptr) > static_cast<int64_t>(cd_length)) {
      ALOGW("Zip: bad CD advance (%tu vs %zu) at entry %" PRIu16, ptr - cd_ptr, cd_length, i);
      return kInvalidFile;
    }
  }

  uint32_t lfh_start_bytes;
  if (!archive->mapped_zip.ReadAtOffset(reinterpret_cast<uint8_t*>(&lfh_start_bytes),
                                        sizeof(uint32_t), 0)) {
    ALOGW("Zip: Unable to read header for entry at offset == 0.");
    return kInvalidFile;
  }

  if (lfh_start_bytes != LocalFileHeader::kSignature) {
    ALOGW("Zip: Entry at offset zero has invalid LFH signature %" PRIx32, lfh_start_bytes);
#if defined(__ANDROID__)
    android_errorWriteLog(0x534e4554, "64211847");
#endif
    return kInvalidFile;
  }

  ALOGV("+++ zip good scan %" PRIu16 " entries", num_entries);

  return 0;
}

static int32_t OpenArchiveInternal(ZipArchive* archive, const char* debug_file_name) {
  int32_t result = MapCentralDirectory(debug_file_name, archive);
  return result != 0 ? result : ParseZipArchive(archive);
}

int32_t OpenArchiveFd(int fd, const char* debug_file_name, ZipArchiveHandle* handle,
                      bool assume_ownership) {
  ZipArchive* archive = new ZipArchive(MappedZipFile(fd), assume_ownership);
  *handle = archive;
  return OpenArchiveInternal(archive, debug_file_name);
}

int32_t OpenArchiveFdRange(int fd, const char* debug_file_name, ZipArchiveHandle* handle,
                           off64_t length, off64_t offset, bool assume_ownership) {
  ZipArchive* archive = new ZipArchive(MappedZipFile(fd, length, offset), assume_ownership);
  *handle = archive;

  if (length < 0) {
    ALOGW("Invalid zip length %" PRId64, length);
    return kIoError;
  }

  if (offset < 0) {
    ALOGW("Invalid zip offset %" PRId64, offset);
    return kIoError;
  }

  return OpenArchiveInternal(archive, debug_file_name);
}

int32_t OpenArchive(const char* fileName, ZipArchiveHandle* handle) {
  const int fd = ::android::base::utf8::open(fileName, O_RDONLY | O_BINARY | O_CLOEXEC, 0);
  ZipArchive* archive = new ZipArchive(MappedZipFile(fd), true);
  *handle = archive;

  if (fd < 0) {
    ALOGW("Unable to open '%s': %s", fileName, strerror(errno));
    return kIoError;
  }

  return OpenArchiveInternal(archive, fileName);
}

int32_t OpenArchiveFromMemory(const void* address, size_t length, const char* debug_file_name,
                              ZipArchiveHandle* handle) {
  ZipArchive* archive = new ZipArchive(address, length);
  *handle = archive;
  return OpenArchiveInternal(archive, debug_file_name);
}

ZipArchiveInfo GetArchiveInfo(ZipArchiveHandle archive) {
  ZipArchiveInfo result;
  result.archive_size = archive->mapped_zip.GetFileLength();
  result.entry_count = archive->num_entries;
  return result;
}

/*
 * Close a ZipArchive, closing the file and freeing the contents.
 */
void CloseArchive(ZipArchiveHandle archive) {
  ALOGV("Closing archive %p", archive);
  delete archive;
}

static int32_t ValidateDataDescriptor(MappedZipFile& mapped_zip, ZipEntry* entry) {
  uint8_t ddBuf[sizeof(DataDescriptor) + sizeof(DataDescriptor::kOptSignature)];
  off64_t offset = entry->offset;
  if (entry->method != kCompressStored) {
    offset += entry->compressed_length;
  } else {
    offset += entry->uncompressed_length;
  }

  if (!mapped_zip.ReadAtOffset(ddBuf, sizeof(ddBuf), offset)) {
    return kIoError;
  }

  const uint32_t ddSignature = *(reinterpret_cast<const uint32_t*>(ddBuf));
  const uint16_t ddOffset = (ddSignature == DataDescriptor::kOptSignature) ? 4 : 0;
  const DataDescriptor* descriptor = reinterpret_cast<const DataDescriptor*>(ddBuf + ddOffset);

  // Validate that the values in the data descriptor match those in the central
  // directory.
  if (entry->compressed_length != descriptor->compressed_size ||
      entry->uncompressed_length != descriptor->uncompressed_size ||
      entry->crc32 != descriptor->crc32) {
    ALOGW("Zip: size/crc32 mismatch. expected {%" PRIu32 ", %" PRIu32 ", %" PRIx32
          "}, was {%" PRIu32 ", %" PRIu32 ", %" PRIx32 "}",
          entry->compressed_length, entry->uncompressed_length, entry->crc32,
          descriptor->compressed_size, descriptor->uncompressed_size, descriptor->crc32);
    return kInconsistentInformation;
  }

  return 0;
}

static int32_t FindEntry(const ZipArchive* archive, const int32_t ent, ZipEntry* data) {
  const uint16_t nameLen = archive->hash_table[ent].name_length;

  // Recover the start of the central directory entry from the filename
  // pointer.  The filename is the first entry past the fixed-size data,
  // so we can just subtract back from that.
  const uint8_t* base_ptr = archive->central_directory.GetBasePtr();
  const uint8_t* ptr = base_ptr + archive->hash_table[ent].name_offset;
  ptr -= sizeof(CentralDirectoryRecord);

  // This is the base of our mmapped region, we have to sanity check that
  // the name that's in the hash table is a pointer to a location within
  // this mapped region.
  if (ptr < base_ptr || ptr > base_ptr + archive->central_directory.GetMapLength()) {
    ALOGW("Zip: Invalid entry pointer");
    return kInvalidOffset;
  }

  const CentralDirectoryRecord* cdr = reinterpret_cast<const CentralDirectoryRecord*>(ptr);

  // The offset of the start of the central directory in the zipfile.
  // We keep this lying around so that we can sanity check all our lengths
  // and our per-file structures.
  const off64_t cd_offset = archive->directory_offset;

  // Fill out the compression method, modification time, crc32
  // and other interesting attributes from the central directory. These
  // will later be compared against values from the local file header.
  data->method = cdr->compression_method;
  data->mod_time = cdr->last_mod_date << 16 | cdr->last_mod_time;
  data->crc32 = cdr->crc32;
  data->compressed_length = cdr->compressed_size;
  data->uncompressed_length = cdr->uncompressed_size;

  // Figure out the local header offset from the central directory. The
  // actual file data will begin after the local header and the name /
  // extra comments.
  const off64_t local_header_offset = cdr->local_file_header_offset;
  if (local_header_offset + static_cast<off64_t>(sizeof(LocalFileHeader)) >= cd_offset) {
    ALOGW("Zip: bad local hdr offset in zip");
    return kInvalidOffset;
  }

  uint8_t lfh_buf[sizeof(LocalFileHeader)];
  if (!archive->mapped_zip.ReadAtOffset(lfh_buf, sizeof(lfh_buf), local_header_offset)) {
    ALOGW("Zip: failed reading lfh name from offset %" PRId64,
          static_cast<int64_t>(local_header_offset));
    return kIoError;
  }

  const LocalFileHeader* lfh = reinterpret_cast<const LocalFileHeader*>(lfh_buf);

  if (lfh->lfh_signature != LocalFileHeader::kSignature) {
    ALOGW("Zip: didn't find signature at start of lfh, offset=%" PRId64,
          static_cast<int64_t>(local_header_offset));
    return kInvalidOffset;
  }

  // Paranoia: Match the values specified in the local file header
  // to those specified in the central directory.

  // Warn if central directory and local file header don't agree on the use
  // of a trailing Data Descriptor. The reference implementation is inconsistent
  // and appears to use the LFH value during extraction (unzip) but the CD value
  // while displayng information about archives (zipinfo). The spec remains
  // silent on this inconsistency as well.
  //
  // For now, always use the version from the LFH but make sure that the values
  // specified in the central directory match those in the data descriptor.
  //
  // NOTE: It's also worth noting that unzip *does* warn about inconsistencies in
  // bit 11 (EFS: The language encoding flag, marking that filename and comment are
  // encoded using UTF-8). This implementation does not check for the presence of
  // that flag and always enforces that entry names are valid UTF-8.
  if ((lfh->gpb_flags & kGPBDDFlagMask) != (cdr->gpb_flags & kGPBDDFlagMask)) {
    ALOGW("Zip: gpb flag mismatch at bit 3. expected {%04" PRIx16 "}, was {%04" PRIx16 "}",
          cdr->gpb_flags, lfh->gpb_flags);
  }

  // If there is no trailing data descriptor, verify that the central directory and local file
  // header agree on the crc, compressed, and uncompressed sizes of the entry.
  if ((lfh->gpb_flags & kGPBDDFlagMask) == 0) {
    data->has_data_descriptor = 0;
    if (data->compressed_length != lfh->compressed_size ||
        data->uncompressed_length != lfh->uncompressed_size || data->crc32 != lfh->crc32) {
      ALOGW("Zip: size/crc32 mismatch. expected {%" PRIu32 ", %" PRIu32 ", %" PRIx32
            "}, was {%" PRIu32 ", %" PRIu32 ", %" PRIx32 "}",
            data->compressed_length, data->uncompressed_length, data->crc32, lfh->compressed_size,
            lfh->uncompressed_size, lfh->crc32);
      return kInconsistentInformation;
    }
  } else {
    data->has_data_descriptor = 1;
  }

  // 4.4.2.1: the upper byte of `version_made_by` gives the source OS. Unix is 3.
  data->version_made_by = cdr->version_made_by;
  data->external_file_attributes = cdr->external_file_attributes;
  if ((data->version_made_by >> 8) == 3) {
    data->unix_mode = (cdr->external_file_attributes >> 16) & 0xffff;
  } else {
    data->unix_mode = 0777;
  }

  // 4.4.4: general purpose bit flags.
  data->gpbf = lfh->gpb_flags;

  // 4.4.14: the lowest bit of the internal file attributes field indicates text.
  // Currently only needed to implement zipinfo.
  data->is_text = (cdr->internal_file_attributes & 1);

  // Check that the local file header name matches the declared
  // name in the central directory.
  if (lfh->file_name_length != nameLen) {
    ALOGW("Zip: lfh name length did not match central directory");
    return kInconsistentInformation;
  }
  const off64_t name_offset = local_header_offset + sizeof(LocalFileHeader);
  if (name_offset + lfh->file_name_length > cd_offset) {
    ALOGW("Zip: lfh name has invalid declared length");
    return kInvalidOffset;
  }
  std::vector<uint8_t> name_buf(nameLen);
  if (!archive->mapped_zip.ReadAtOffset(name_buf.data(), nameLen, name_offset)) {
    ALOGW("Zip: failed reading lfh name from offset %" PRId64, static_cast<int64_t>(name_offset));
    return kIoError;
  }
  const std::string_view entry_name =
      archive->hash_table[ent].ToStringView(archive->central_directory.GetBasePtr());
  if (memcmp(entry_name.data(), name_buf.data(), nameLen) != 0) {
    ALOGW("Zip: lfh name did not match central directory");
    return kInconsistentInformation;
  }

  const off64_t data_offset = local_header_offset + sizeof(LocalFileHeader) +
                              lfh->file_name_length + lfh->extra_field_length;
  if (data_offset > cd_offset) {
    ALOGW("Zip: bad data offset %" PRId64 " in zip", static_cast<int64_t>(data_offset));
    return kInvalidOffset;
  }

  if (static_cast<off64_t>(data_offset + data->compressed_length) > cd_offset) {
    ALOGW("Zip: bad compressed length in zip (%" PRId64 " + %" PRIu32 " > %" PRId64 ")",
          static_cast<int64_t>(data_offset), data->compressed_length,
          static_cast<int64_t>(cd_offset));
    return kInvalidOffset;
  }

  if (data->method == kCompressStored &&
      static_cast<off64_t>(data_offset + data->uncompressed_length) > cd_offset) {
    ALOGW("Zip: bad uncompressed length in zip (%" PRId64 " + %" PRIu32 " > %" PRId64 ")",
          static_cast<int64_t>(data_offset), data->uncompressed_length,
          static_cast<int64_t>(cd_offset));
    return kInvalidOffset;
  }

  data->offset = data_offset;
  return 0;
}

struct IterationHandle {
  ZipArchive* archive;

  std::string prefix;
  std::string suffix;

  uint32_t position = 0;

  IterationHandle(ZipArchive* archive, std::string_view in_prefix, std::string_view in_suffix)
      : archive(archive), prefix(in_prefix), suffix(in_suffix) {}
};

int32_t StartIteration(ZipArchiveHandle archive, void** cookie_ptr,
                       const std::string_view optional_prefix,
                       const std::string_view optional_suffix) {
  if (archive == NULL || archive->hash_table == NULL) {
    ALOGW("Zip: Invalid ZipArchiveHandle");
    return kInvalidHandle;
  }

  if (optional_prefix.size() > static_cast<size_t>(UINT16_MAX) ||
      optional_suffix.size() > static_cast<size_t>(UINT16_MAX)) {
    ALOGW("Zip: prefix/suffix too long");
    return kInvalidEntryName;
  }

  *cookie_ptr = new IterationHandle(archive, optional_prefix, optional_suffix);
  return 0;
}

void EndIteration(void* cookie) {
  delete reinterpret_cast<IterationHandle*>(cookie);
}

int32_t FindEntry(const ZipArchiveHandle archive, const std::string_view entryName,
                  ZipEntry* data) {
  if (entryName.empty() || entryName.size() > static_cast<size_t>(UINT16_MAX)) {
    ALOGW("Zip: Invalid filename of length %zu", entryName.size());
    return kInvalidEntryName;
  }

  const int64_t ent = EntryToIndex(archive->hash_table, archive->hash_table_size, entryName,
                                   archive->central_directory.GetBasePtr());
  if (ent < 0) {
    ALOGV("Zip: Could not find entry %.*s", static_cast<int>(entryName.size()), entryName.data());
    return static_cast<int32_t>(ent);  // kEntryNotFound is safe to truncate.
  }
  // We know there are at most hash_table_size entries, safe to truncate.
  return FindEntry(archive, static_cast<uint32_t>(ent), data);
}

int32_t Next(void* cookie, ZipEntry* data, std::string* name) {
  std::string_view sv;
  int32_t result = Next(cookie, data, &sv);
  if (result == 0 && name) {
    *name = std::string(sv);
  }
  return result;
}

int32_t Next(void* cookie, ZipEntry* data, std::string_view* name) {
  IterationHandle* handle = reinterpret_cast<IterationHandle*>(cookie);
  if (handle == NULL) {
    ALOGW("Zip: Null ZipArchiveHandle");
    return kInvalidHandle;
  }

  ZipArchive* archive = handle->archive;
  if (archive == NULL || archive->hash_table == NULL) {
    ALOGW("Zip: Invalid ZipArchiveHandle");
    return kInvalidHandle;
  }

  const uint32_t currentOffset = handle->position;
  const uint32_t hash_table_length = archive->hash_table_size;
  const ZipStringOffset* hash_table = archive->hash_table;
  for (uint32_t i = currentOffset; i < hash_table_length; ++i) {
    const std::string_view entry_name =
        hash_table[i].ToStringView(archive->central_directory.GetBasePtr());
    if (hash_table[i].name_offset != 0 && (android::base::StartsWith(entry_name, handle->prefix) &&
                                           android::base::EndsWith(entry_name, handle->suffix))) {
      handle->position = (i + 1);
      const int error = FindEntry(archive, i, data);
      if (!error && name) {
        *name = entry_name;
      }
      return error;
    }
  }

  handle->position = 0;
  return kIterationEnd;
}

// A Writer that writes data to a fixed size memory region.
// The size of the memory region must be equal to the total size of
// the data appended to it.
class MemoryWriter : public zip_archive::Writer {
 public:
  MemoryWriter(uint8_t* buf, size_t size) : Writer(), buf_(buf), size_(size), bytes_written_(0) {}

  virtual bool Append(uint8_t* buf, size_t buf_size) override {
    if (bytes_written_ + buf_size > size_) {
      ALOGW("Zip: Unexpected size %zu (declared) vs %zu (actual)", size_,
            bytes_written_ + buf_size);
      return false;
    }

    memcpy(buf_ + bytes_written_, buf, buf_size);
    bytes_written_ += buf_size;
    return true;
  }

 private:
  uint8_t* const buf_;
  const size_t size_;
  size_t bytes_written_;
};

// A Writer that appends data to a file |fd| at its current position.
// The file will be truncated to the end of the written data.
class FileWriter : public zip_archive::Writer {
 public:
  // Creates a FileWriter for |fd| and prepare to write |entry| to it,
  // guaranteeing that the file descriptor is valid and that there's enough
  // space on the volume to write out the entry completely and that the file
  // is truncated to the correct length (no truncation if |fd| references a
  // block device).
  //
  // Returns a valid FileWriter on success, |nullptr| if an error occurred.
  static FileWriter Create(int fd, const ZipEntry* entry) {
    const uint32_t declared_length = entry->uncompressed_length;
    const off64_t current_offset = lseek64(fd, 0, SEEK_CUR);
    if (current_offset == -1) {
      ALOGW("Zip: unable to seek to current location on fd %d: %s", fd, strerror(errno));
      return FileWriter{};
    }

#if defined(__linux__)
    if (declared_length > 0) {
      // Make sure we have enough space on the volume to extract the compressed
      // entry. Note that the call to ftruncate below will change the file size but
      // will not allocate space on disk and this call to fallocate will not
      // change the file size.
      // Note: fallocate is only supported by the following filesystems -
      // btrfs, ext4, ocfs2, and xfs. Therefore fallocate might fail with
      // EOPNOTSUPP error when issued in other filesystems.
      // Hence, check for the return error code before concluding that the
      // disk does not have enough space.
      long result = TEMP_FAILURE_RETRY(fallocate(fd, 0, current_offset, declared_length));
      if (result == -1 && errno == ENOSPC) {
        ALOGW("Zip: unable to allocate %" PRId64 " bytes at offset %" PRId64 ": %s",
              static_cast<int64_t>(declared_length), static_cast<int64_t>(current_offset),
              strerror(errno));
        return FileWriter{};
      }
    }
#endif  // __linux__

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
      ALOGW("Zip: unable to fstat file: %s", strerror(errno));
      return FileWriter{};
    }

    // Block device doesn't support ftruncate(2).
    if (!S_ISBLK(sb.st_mode)) {
      long result = TEMP_FAILURE_RETRY(ftruncate(fd, declared_length + current_offset));
      if (result == -1) {
        ALOGW("Zip: unable to truncate file to %" PRId64 ": %s",
              static_cast<int64_t>(declared_length + current_offset), strerror(errno));
        return FileWriter{};
      }
    }

    return FileWriter(fd, declared_length);
  }

  FileWriter(FileWriter&& other) noexcept
      : fd_(other.fd_),
        declared_length_(other.declared_length_),
        total_bytes_written_(other.total_bytes_written_) {
    other.fd_ = -1;
  }

  bool IsValid() const { return fd_ != -1; }

  virtual bool Append(uint8_t* buf, size_t buf_size) override {
    if (total_bytes_written_ + buf_size > declared_length_) {
      ALOGW("Zip: Unexpected size %zu (declared) vs %zu (actual)", declared_length_,
            total_bytes_written_ + buf_size);
      return false;
    }

    const bool result = android::base::WriteFully(fd_, buf, buf_size);
    if (result) {
      total_bytes_written_ += buf_size;
    } else {
      ALOGW("Zip: unable to write %zu bytes to file; %s", buf_size, strerror(errno));
    }

    return result;
  }

 private:
  explicit FileWriter(const int fd = -1, const size_t declared_length = 0)
      : Writer(), fd_(fd), declared_length_(declared_length), total_bytes_written_(0) {}

  int fd_;
  const size_t declared_length_;
  size_t total_bytes_written_;
};

class EntryReader : public zip_archive::Reader {
 public:
  EntryReader(const MappedZipFile& zip_file, const ZipEntry* entry)
      : Reader(), zip_file_(zip_file), entry_(entry) {}

  virtual bool ReadAtOffset(uint8_t* buf, size_t len, uint32_t offset) const {
    return zip_file_.ReadAtOffset(buf, len, entry_->offset + offset);
  }

  virtual ~EntryReader() {}

 private:
  const MappedZipFile& zip_file_;
  const ZipEntry* entry_;
};

// This method is using libz macros with old-style-casts
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
static inline int zlib_inflateInit2(z_stream* stream, int window_bits) {
  return inflateInit2(stream, window_bits);
}
#pragma GCC diagnostic pop

namespace zip_archive {

// Moved out of line to avoid -Wweak-vtables.
Reader::~Reader() {}
Writer::~Writer() {}

int32_t Inflate(const Reader& reader, const uint32_t compressed_length,
                const uint32_t uncompressed_length, Writer* writer, uint64_t* crc_out) {
  const size_t kBufSize = 32768;
  std::vector<uint8_t> read_buf(kBufSize);
  std::vector<uint8_t> write_buf(kBufSize);
  z_stream zstream;
  int zerr;

  /*
   * Initialize the zlib stream struct.
   */
  memset(&zstream, 0, sizeof(zstream));
  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;
  zstream.next_in = NULL;
  zstream.avail_in = 0;
  zstream.next_out = &write_buf[0];
  zstream.avail_out = kBufSize;
  zstream.data_type = Z_UNKNOWN;

  /*
   * Use the undocumented "negative window bits" feature to tell zlib
   * that there's no zlib header waiting for it.
   */
  zerr = zlib_inflateInit2(&zstream, -MAX_WBITS);
  if (zerr != Z_OK) {
    if (zerr == Z_VERSION_ERROR) {
      ALOGE("Installed zlib is not compatible with linked version (%s)", ZLIB_VERSION);
    } else {
      ALOGW("Call to inflateInit2 failed (zerr=%d)", zerr);
    }

    return kZlibError;
  }

  auto zstream_deleter = [](z_stream* stream) {
    inflateEnd(stream); /* free up any allocated structures */
  };

  std::unique_ptr<z_stream, decltype(zstream_deleter)> zstream_guard(&zstream, zstream_deleter);

  const bool compute_crc = (crc_out != nullptr);
  uLong crc = 0;
  uint32_t remaining_bytes = compressed_length;
  do {
    /* read as much as we can */
    if (zstream.avail_in == 0) {
      const uint32_t read_size = (remaining_bytes > kBufSize) ? kBufSize : remaining_bytes;
      const uint32_t offset = (compressed_length - remaining_bytes);
      // Make sure to read at offset to ensure concurrent access to the fd.
      if (!reader.ReadAtOffset(read_buf.data(), read_size, offset)) {
        ALOGW("Zip: inflate read failed, getSize = %u: %s", read_size, strerror(errno));
        return kIoError;
      }

      remaining_bytes -= read_size;

      zstream.next_in = &read_buf[0];
      zstream.avail_in = read_size;
    }

    /* uncompress the data */
    zerr = inflate(&zstream, Z_NO_FLUSH);
    if (zerr != Z_OK && zerr != Z_STREAM_END) {
      ALOGW("Zip: inflate zerr=%d (nIn=%p aIn=%u nOut=%p aOut=%u)", zerr, zstream.next_in,
            zstream.avail_in, zstream.next_out, zstream.avail_out);
      return kZlibError;
    }

    /* write when we're full or when we're done */
    if (zstream.avail_out == 0 || (zerr == Z_STREAM_END && zstream.avail_out != kBufSize)) {
      const size_t write_size = zstream.next_out - &write_buf[0];
      if (!writer->Append(&write_buf[0], write_size)) {
        return kIoError;
      } else if (compute_crc) {
        DCHECK_LE(write_size, kBufSize);
        crc = crc32(crc, &write_buf[0], static_cast<uint32_t>(write_size));
      }

      zstream.next_out = &write_buf[0];
      zstream.avail_out = kBufSize;
    }
  } while (zerr == Z_OK);

  CHECK_EQ(zerr, Z_STREAM_END); /* other errors should've been caught */

  // NOTE: zstream.adler is always set to 0, because we're using the -MAX_WBITS
  // "feature" of zlib to tell it there won't be a zlib file header. zlib
  // doesn't bother calculating the checksum in that scenario. We just do
  // it ourselves above because there are no additional gains to be made by
  // having zlib calculate it for us, since they do it by calling crc32 in
  // the same manner that we have above.
  if (compute_crc) {
    *crc_out = crc;
  }

  if (zstream.total_out != uncompressed_length || remaining_bytes != 0) {
    ALOGW("Zip: size mismatch on inflated file (%lu vs %" PRIu32 ")", zstream.total_out,
          uncompressed_length);
    return kInconsistentInformation;
  }

  return 0;
}
}  // namespace zip_archive

static int32_t InflateEntryToWriter(MappedZipFile& mapped_zip, const ZipEntry* entry,
                                    zip_archive::Writer* writer, uint64_t* crc_out) {
  const EntryReader reader(mapped_zip, entry);

  return zip_archive::Inflate(reader, entry->compressed_length, entry->uncompressed_length, writer,
                              crc_out);
}

static int32_t CopyEntryToWriter(MappedZipFile& mapped_zip, const ZipEntry* entry,
                                 zip_archive::Writer* writer, uint64_t* crc_out) {
  static const uint32_t kBufSize = 32768;
  std::vector<uint8_t> buf(kBufSize);

  const uint32_t length = entry->uncompressed_length;
  uint32_t count = 0;
  uLong crc = 0;
  while (count < length) {
    uint32_t remaining = length - count;
    off64_t offset = entry->offset + count;

    // Safe conversion because kBufSize is narrow enough for a 32 bit signed value.
    const uint32_t block_size = (remaining > kBufSize) ? kBufSize : remaining;

    // Make sure to read at offset to ensure concurrent access to the fd.
    if (!mapped_zip.ReadAtOffset(buf.data(), block_size, offset)) {
      ALOGW("CopyFileToFile: copy read failed, block_size = %u, offset = %" PRId64 ": %s",
            block_size, static_cast<int64_t>(offset), strerror(errno));
      return kIoError;
    }

    if (!writer->Append(&buf[0], block_size)) {
      return kIoError;
    }
    if (crc_out) {
      crc = crc32(crc, &buf[0], block_size);
    }
    count += block_size;
  }

  if (crc_out) {
    *crc_out = crc;
  }

  return 0;
}

int32_t ExtractToWriter(ZipArchiveHandle archive, ZipEntry* entry, zip_archive::Writer* writer) {
  const uint16_t method = entry->method;

  // this should default to kUnknownCompressionMethod.
  int32_t return_value = -1;
  uint64_t crc = 0;
  if (method == kCompressStored) {
    return_value =
        CopyEntryToWriter(archive->mapped_zip, entry, writer, kCrcChecksEnabled ? &crc : nullptr);
  } else if (method == kCompressDeflated) {
    return_value = InflateEntryToWriter(archive->mapped_zip, entry, writer,
                                        kCrcChecksEnabled ? &crc : nullptr);
  }

  if (!return_value && entry->has_data_descriptor) {
    return_value = ValidateDataDescriptor(archive->mapped_zip, entry);
    if (return_value) {
      return return_value;
    }
  }

  // Validate that the CRC matches the calculated value.
  if (kCrcChecksEnabled && (entry->crc32 != static_cast<uint32_t>(crc))) {
    ALOGW("Zip: crc mismatch: expected %" PRIu32 ", was %" PRIu64, entry->crc32, crc);
    return kInconsistentInformation;
  }

  return return_value;
}

int32_t ExtractToMemory(ZipArchiveHandle archive, ZipEntry* entry, uint8_t* begin, uint32_t size) {
  MemoryWriter writer(begin, size);
  return ExtractToWriter(archive, entry, &writer);
}

int32_t ExtractEntryToFile(ZipArchiveHandle archive, ZipEntry* entry, int fd) {
  auto writer = FileWriter::Create(fd, entry);
  if (!writer.IsValid()) {
    return kIoError;
  }

  return ExtractToWriter(archive, entry, &writer);
}

const char* ErrorCodeString(int32_t error_code) {
  // Make sure that the number of entries in kErrorMessages and ErrorCodes
  // match.
  static_assert((-kLastErrorCode + 1) == arraysize(kErrorMessages),
                "(-kLastErrorCode + 1) != arraysize(kErrorMessages)");

  const uint32_t idx = -error_code;
  if (idx < arraysize(kErrorMessages)) {
    return kErrorMessages[idx];
  }

  return "Unknown return code";
}

int GetFileDescriptor(const ZipArchiveHandle archive) {
  return archive->mapped_zip.GetFileDescriptor();
}

off64_t GetFileDescriptorOffset(const ZipArchiveHandle archive) {
  return archive->mapped_zip.GetFileOffset();
}

#if !defined(_WIN32)
class ProcessWriter : public zip_archive::Writer {
 public:
  ProcessWriter(ProcessZipEntryFunction func, void* cookie)
      : Writer(), proc_function_(func), cookie_(cookie) {}

  virtual bool Append(uint8_t* buf, size_t buf_size) override {
    return proc_function_(buf, buf_size, cookie_);
  }

 private:
  ProcessZipEntryFunction proc_function_;
  void* cookie_;
};

int32_t ProcessZipEntryContents(ZipArchiveHandle archive, ZipEntry* entry,
                                ProcessZipEntryFunction func, void* cookie) {
  ProcessWriter writer(func, cookie);
  return ExtractToWriter(archive, entry, &writer);
}

#endif  //! defined(_WIN32)

int MappedZipFile::GetFileDescriptor() const {
  if (!has_fd_) {
    ALOGW("Zip: MappedZipFile doesn't have a file descriptor.");
    return -1;
  }
  return fd_;
}

const void* MappedZipFile::GetBasePtr() const {
  if (has_fd_) {
    ALOGW("Zip: MappedZipFile doesn't have a base pointer.");
    return nullptr;
  }
  return base_ptr_;
}

off64_t MappedZipFile::GetFileOffset() const {
  return fd_offset_;
}

off64_t MappedZipFile::GetFileLength() const {
  if (has_fd_) {
    if (data_length_ != -1) {
      return data_length_;
    }
    data_length_ = lseek64(fd_, 0, SEEK_END);
    if (data_length_ == -1) {
      ALOGE("Zip: lseek on fd %d failed: %s", fd_, strerror(errno));
    }
    return data_length_;
  } else {
    if (base_ptr_ == nullptr) {
      ALOGE("Zip: invalid file map");
      return -1;
    }
    return data_length_;
  }
}

// Attempts to read |len| bytes into |buf| at offset |off|.
bool MappedZipFile::ReadAtOffset(uint8_t* buf, size_t len, off64_t off) const {
  if (has_fd_) {
    if (off < 0) {
      ALOGE("Zip: invalid offset %" PRId64, off);
      return false;
    }

    off64_t read_offset;
    if (__builtin_add_overflow(fd_offset_, off, &read_offset)) {
      ALOGE("Zip: invalid read offset %" PRId64 " overflows, fd offset %" PRId64, off, fd_offset_);
      return false;
    }

    if (data_length_ != -1) {
      off64_t read_end;
      if (len > std::numeric_limits<off64_t>::max() ||
          __builtin_add_overflow(off, static_cast<off64_t>(len), &read_end)) {
        ALOGE("Zip: invalid read length %" PRId64 " overflows, offset %" PRId64,
              static_cast<off64_t>(len), off);
        return false;
      }

      if (read_end > data_length_) {
        ALOGE("Zip: invalid read length %" PRId64 " exceeds data length %" PRId64 ", offset %"
              PRId64, static_cast<off64_t>(len), data_length_, off);
        return false;
      }
    }

    if (!android::base::ReadFullyAtOffset(fd_, buf, len, read_offset)) {
      ALOGE("Zip: failed to read at offset %" PRId64, off);
      return false;
    }
  } else {
    if (off < 0 || off > data_length_) {
      ALOGE("Zip: invalid offset: %" PRId64 ", data length: %" PRId64, off, data_length_);
      return false;
    }
    memcpy(buf, static_cast<const uint8_t*>(base_ptr_) + off, len);
  }
  return true;
}

void CentralDirectory::Initialize(const void* map_base_ptr, off64_t cd_start_offset,
                                  size_t cd_size) {
  base_ptr_ = static_cast<const uint8_t*>(map_base_ptr) + cd_start_offset;
  length_ = cd_size;
}

bool ZipArchive::InitializeCentralDirectory(off64_t cd_start_offset, size_t cd_size) {
  if (mapped_zip.HasFd()) {
    directory_map = android::base::MappedFile::FromFd(mapped_zip.GetFileDescriptor(),
                                                      mapped_zip.GetFileOffset() + cd_start_offset,
                                                      cd_size, PROT_READ);
    if (!directory_map) {
      ALOGE("Zip: failed to map central directory (offset %" PRId64 ", size %zu): %s",
            cd_start_offset, cd_size, strerror(errno));
      return false;
    }

    CHECK_EQ(directory_map->size(), cd_size);
    central_directory.Initialize(directory_map->data(), 0 /*offset*/, cd_size);
  } else {
    if (mapped_zip.GetBasePtr() == nullptr) {
      ALOGE("Zip: Failed to map central directory, bad mapped_zip base pointer");
      return false;
    }
    if (static_cast<off64_t>(cd_start_offset) + static_cast<off64_t>(cd_size) >
        mapped_zip.GetFileLength()) {
      ALOGE(
          "Zip: Failed to map central directory, offset exceeds mapped memory region ("
          "start_offset %" PRId64 ", cd_size %zu, mapped_region_size %" PRId64 ")",
          static_cast<int64_t>(cd_start_offset), cd_size, mapped_zip.GetFileLength());
      return false;
    }

    central_directory.Initialize(mapped_zip.GetBasePtr(), cd_start_offset, cd_size);
  }
  return true;
}

// This function returns the embedded timestamp as is; and doesn't perform validations.
tm ZipEntry::GetModificationTime() const {
  tm t = {};

  t.tm_hour = (mod_time >> 11) & 0x1f;
  t.tm_min = (mod_time >> 5) & 0x3f;
  t.tm_sec = (mod_time & 0x1f) << 1;

  t.tm_year = ((mod_time >> 25) & 0x7f) + 80;
  t.tm_mon = ((mod_time >> 21) & 0xf) - 1;
  t.tm_mday = (mod_time >> 16) & 0x1f;

  return t;
}
