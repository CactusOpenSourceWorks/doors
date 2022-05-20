#include <iostream>
#include <cstdio>
#include <string>
#include <bitset>
#include <cstring>
#include <unordered_map>
#include <any>
#include <windows.h>

#include <compiler.hpp>
using namespace compiler;

#include <system/error.hpp>

#define IMAGE_GIF_DETAIL
#define IMAGE_GIF_DETAIL_DEBUG
#include <gif.hpp>

#define IMAGE_JPG_DETAIL
#define IMAGE_JPG_DETAIL_DEBUG
#include <jpg.hpp>

#define IMAGE_BMP_DETAIL
// #define IMAGE_BMP_DETAIL_DEBUG
#include <bmp.hpp>

#define IMAGE_PNG_DETAIL
// #define IMAGE_PNG_DETAIL_DEBUG
#include <png.hpp>

#define IMAGE_TGA_DETAIL
#define IMAGE_TGA_DETAIL_DEBUG
#include <tga.hpp>

#define IMAGE_PSD_DETAIL
// #define IMAGE_PSD_DETAIL_DEBUG
#include <psd.hpp>

#define LINE "----------"

typedef uint64_t HighestInteger;

template <size_t size>
std::string bits(HighestInteger n)
{
    return std::bitset<size>(n).to_string();
}

std::string bits_C(HighestInteger n, size_t size)
{
  std::string s;
  for (size_t i = 0; i < size; ++i)
    s += std::to_string((n >> i) & 1u);

  return s;
}

::HANDLE open_file(const char *name)
{
  return ::CreateFile(
    name,
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
    nullptr
  );
}

enum FileOperationMode {
  Read = GENERIC_READ,
  Write = GENERIC_WRITE,
  ReadAndWrite = GENERIC_READ | GENERIC_WRITE
};

namespace globals {
uint64_t get_file_size(const ::HANDLE file, uint64_t divider = 1)
{
  if (file != nullptr) {
    ::DWORD high;
    ::DWORD low = ::GetFileSize(file, &high);
    return ((uint64_t) high << 32 | (uint64_t) low) / divider;
  }

  return (uint64_t) 0u;
}

::HANDLE create_file_generic(const char *name, ::DWORD mode)
{
  return ::CreateFile(
      name,
      mode,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
      nullptr
    );
}
}

class File {
  bool directory;
  uint64_t size;
  uint32_t attribute;
  ::SYSTEMTIME creation_time;
  ::SYSTEMTIME access_time;
  ::SYSTEMTIME write_time;

  ::HANDLE file = nullptr;
  std::string name;

public:
  File(const std::string &name, const FileOperationMode &mode)
  {
    open(name, mode);
  }

  ~File()
  {
    close();
  }

  bool is_valid() const { return file != nullptr; }

  uint64_t get_size() const { return this->size; }

  void close()
  {
    if (file) {
      ::CloseHandle(file);
      this->size = 0u;
      this->attribute = 0u;
    }
  }

  void open(const std::string &name, const FileOperationMode &mode)
  {
    if (file != nullptr) {
      close();
    }

    this->name = std::string(name);
    file = globals::create_file_generic(name.c_str(), mode);

    if (file) {
      size = globals::get_file_size(file);
    }
  }
};

std::string SYSTEMTIME_to_string(const ::SYSTEMTIME *system_time)
{
  if (system_time) {
    return
        std::to_string(system_time->wDay) + "-" +
        std::to_string(system_time->wMonth) + "-" +
        std::to_string(system_time->wYear) + " " +
        std::to_string(system_time->wHour) + ":" +
        std::to_string(system_time->wMinute) + ":" +
        std::to_string(system_time->wSecond)
      ;
  }

  return "";
}

std::string open_file_time(const char *name)
{
  if (name) {
    ::HANDLE file = open_file(name);
    ::FILETIME access;
    if (::GetFileTime(file, nullptr, &access, nullptr) == TRUE) {
      std::string s;
      ::SYSTEMTIME system_time;
      if (::FileTimeToSystemTime(&access, &system_time) == TRUE) {
        s += SYSTEMTIME_to_string(&system_time);
      }

      return s;
    }
  }

  return "";
}

uint64_t size(const char *name)
{
  ::HANDLE file = open_file(name);

  if (file) {
    ::DWORD size;
    DWORD f = ::GetFileSize(file, &size);
    ::CloseHandle(file);
    return (uint64_t) size << 32 | (uint64_t) f;
  }

  return 0u;
}

static void print_basic_information(/* const */ std::unordered_map<std::string, std::any> &image_block)
{
  printf("Magic: %s\n", std::any_cast<std::string>(image_block["magic.s"]).c_str());
  printf("Version: %s\n", std::any_cast<std::string>(image_block["version.s"]).c_str());
  printf("Version (sanitized): %i\n", std::any_cast<uint16_t>(image_block["version_sanitized.u16"]));
  printf("Width/Height: %ix%i\n", std::any_cast<uint32_t>(image_block["width.u32"]), std::any_cast<uint32_t>(image_block["height.u32"]));
  printf("Projected Pixel Aspect Ratio: %.3f\n", std::any_cast<float>(image_block["projected_aspect_ratio.f"]));
}

static void gif(const char *name)
{
  puts(LINE " GIF " LINE);
  /* const */ auto v = doors::image::gif::parse(name);

  printf("File: %s\n", name);
  print_basic_information(v);

  printf("Frame(s): %i\n", std::any_cast<uint16_t>(v["frames.u16"]));
}

static void jpg(const char *name)
{
  puts(LINE " JPG " LINE);
  /* const */ auto v = doors::image::jpg::parse(name);

  printf("File: %s\n", name);
  print_basic_information(v);

  printf("Bits per pixel: %i\n", std::any_cast<uint8_t>(v["bits_per_pixel.u8"]));
  printf("Color space: %i\n", std::any_cast<uint8_t>(v["color_space.u8"]));
  printf("Color space (sanitized): %s\n", std::any_cast<std::string>(v["color_space_sanitized.s"]).c_str());
}

static void bmp(const char *name)
{
  puts(LINE " BMP " LINE);
  /* const */ auto v = doors::image::bmp::parse(name);

  printf("File: %s\n", name);
  print_basic_information(v);

  printf("Encoded size: %i KiB\n", std::any_cast<uint32_t>(v["encoded_size.u32"]) / 1024u);
  printf("Bits per pixel: %i\n", std::any_cast<uint8_t>(v["bits_per_pixel.u8"]));
  printf("Planes: %i\n", std::any_cast<uint16_t>(v["planes.u16"]));
}

static void png(const char *name)
{
  puts(LINE " PNG " LINE);

  /* const */ auto v = doors::image::png::parse(name);

  printf("File: %s\n", name);
  print_basic_information(v);

  const auto chunks = std::any_cast<std::unordered_map<std::string, uint16_t>>(v["chunks.unordered_map<s, u16>"]);
  std::string chunk_data;
  for (const auto &pair : chunks) {
    chunk_data += pair.first + " (" + std::to_string(pair.second) + "); ";
  }
  chunk_data.erase(chunk_data.length() - 2, chunk_data.length() - 1);
  printf("Chunk(s): %s\n", chunk_data.c_str());

  uint8_t compression_type = std::any_cast<uint8_t>(v["compression_type.u8"]);
  printf("Compression type (0 = DEFLATE): %i\n", compression_type);

  if (compression_type == 0) {
    printf("DEFLATE compression level: %i\n", std::any_cast<uint8_t>(v["deflate_compression_level.u8"]));
  }

  printf("IHDR CRC: %x\n", std::any_cast<uint32_t>(v["ihdr_crc.u32"]));
  printf("Interlacing: %i\n", std::any_cast<bool>(v["interlaced.b"]));
}

static void psd(const char *name)
{
  puts(LINE " PSD " LINE);
  /* const */ auto v = doors::image::psd::parse(name);

  printf("File: %s\n", name);
  print_basic_information(v);

  printf("Bits per pixel: %i\n", std::any_cast<uint8_t>(v["bits_per_pixel.u8"]));
  printf("Color space: %s (%i)\n", std::any_cast<std::string>(v["color_space_sanitized.s"]).c_str(), std::any_cast<uint8_t>(v["color_space.u8"]));
  printf("Layers: %i\n", std::any_cast<uint16_t>(v["layers.u16"]));
}

static void tga(const char *name)
{
  puts(LINE " TGA " LINE);
  /* const */ auto v = doors::image::tga::parse(name);

  printf("File: %s\n", name);
  print_basic_information(v);

  uint8_t compression_type = std::any_cast<uint8_t>(v["compression_type.u8"]);
  printf("Compression type (0 = Uncompressed, 1 = RLE): %i\n", compression_type);

  if (compression_type == 1) { // RLE
    /* ... */
  }

  printf("Bits per pixel: %i\n", std::any_cast<uint8_t>(v["bits_per_pixel.u8"]));
  printf("Coloring type: %s\n", std::any_cast<std::string>(v["color_type_sanitized.s"]).c_str());

  printf("Author: %s\n", std::any_cast<std::string>(v["author.s"]).c_str());
  printf("Comment: %s\n", std::any_cast<std::string>(v["comment.s"]).c_str());
  printf("Software ID: %s\n", std::any_cast<std::string>(v["software.s"]).c_str());
  printf("Job ID: %s\n", std::any_cast<std::string>(v["job.s"]).c_str());
  printf("Job time: %s\n", std::any_cast<std::string>(v["job_time.s"]).c_str());
  printf("Gamma: %.2f\n", std::any_cast<float>(v["gamma.f"]));
  printf("Encoded pixel aspect ratio: %.2f\n", std::any_cast<float>(v["pixel_aspect_ratio.f"]));
  printf("Encoded time: %s\n", std::any_cast<std::string>(v["time.s"]).c_str());
}

int main(int argc, char *argv[])
{
  traverse("./test/gif/", gif);
  // traverse("./test/jpg/", jpg);
  // traverse("./test/bmp/", bmp);
  // traverse("./test/png/", png);
  // traverse("./test/tga/", tga);
  // traverse("./test/psd/", psd);
  std::printf("%s\n", errors[0].message);
}
