#if !defined(__IMAGE_PNG_DETAIL__)
#define __IMAGE_PNG_DETAIL__

// Pending issue(s):
//   CMF byte read isn't working at the moment.
//   Convert the few remaining std::f*(FILE *) calls to utilize scoped_file instead.
//   Validation flags
//   Testing

// Test suite: http://entropymine.com/jason/pngsuite/pngsuite/html/pngsuite.html
// CgBI-based PNGs (Apple's proprietary PNG implementation, with a CgBI chunk preceding IHDR) is NOT supported.
// As of PNG 1.2 (1.5.0), EXIF tags could also be embedded as an auxiliary chunk (eXIf).
//
// Chunk structure: Length (uint32_t), type (uint32_t), data (length bytes), CRC (uint32_t)
//
// #1
// Table of possible CMF/FLG pair(s):
// (courtesy of https://groups.google.com/forum/#!msg/comp.compression/_y2Wwn_Vq_E/EymIVcQ52cEJ)
// Common:
// 78 01, 78 5e, 78 9c, 78 da
//
// Rare:
// 08 1d, 08 5b, 08 99, 08 d7, 18 19, 18 57, 18 95, 18 d3,
// 28 15, 28 53, 28 91, 28 cf, 38 11, 38 4f, 38 8d, 38 cb,
// 48 0d, 48 4b, 48 89, 48 c7, 58 09, 58 47, 58 85, 58 c3,
// 68 05, 68 43, 68 81, 68 de
//
// Very rare:
// 08 3c, 08 7a, 08 b8, 08 f6, 18 38, 18 76, 18 b4, 18 f2,
// 28 34, 28 72, 28 b0, 28 ee, 38 30, 38 6e, 38 ac, 38 ea,
// 48 2c, 48 6a, 48 a8, 48 e6, 58 28, 58 66, 58 a4, 58 e2,
// 68 24, 68 62, 68 bf, 68 fd, 78 3f, 78 7d, 78 bb, 78 f9
//
// #2
// To quote http://www.libpng.org/pub/png/libpng-manual.txt:
// "Libpng-1.6.0 through 1.6.2 used the CMF bytes at the beginning of the IDAT
// stream to set the size of the sliding window for reading instead of using the
// default 32-kbyte sliding window size.  It was discovered that there are
// hundreds of PNG files in the wild that have incorrect CMF bytes that caused
// zlib to issue the "invalid distance too far back" error and reject the file."
// Which, roughly means, CMF CM isn't always has to be 8, and, for general header parsing,
// this bit of information could be safely ignored.

#include <unordered_map>
#include <any>
#include <string>

#include <compiler.hpp>
using namespace compiler;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#ifdef IMAGE_PNG_DETAIL
#define IMAGE_PNG_DETAIL_MAXIMUM_IDAT_COUNT 32
#endif

namespace doors {
  namespace image {
    namespace png {
      namespace detail {
        // Since we're casting a byte array into an IHDR header, let's not do struct padding
        __PACKED_STRUCT_START PNG_IHDR_header_t {
          uint32_t length;
          uint32_t chunk_type;

          uint32_t width;
          uint32_t height;
          uint8_t bpp;
          uint8_t color_type;
          uint8_t compression_type;
          uint8_t filter_type;
          uint8_t interlacing_type;

          uint32_t crc;
        };
        __PACKED_STRUCT_END

        struct PNG_chunk_count_header_t {
          uint16_t idat;
          uint16_t plte;
          uint16_t iend;
          uint16_t iccp;
          uint16_t phys;
          uint16_t srgb;
          uint16_t time;
          uint16_t exif;
          uint16_t gama;
          uint16_t ztxt;
          uint16_t hist;
        };

        struct PNG_header_t {
          char png[8];

          PNG_IHDR_header_t ihdr;
          PNG_chunk_count_header_t chunks;

          uint8_t compression_level;
        };

        error_t read(PNG_header_t *header, const char *name);
      } // namespace detail

      using namespace detail;

      std::unordered_map<std::string, std::any> parse(const char *name);
    } // namespace png
  } // namespace image
} // namespace doors

#endif

#ifdef IMAGE_PNG_DETAIL
#undef IMAGE_PNG_DETAIL

namespace doors {
  namespace image {
    namespace png {
      namespace detail {
        static constexpr const uint8_t magic[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

        // PNG is MSB, swizzling the bytes before reading.
        error_t read(PNG_header_t *header, const char *name)
        {
          const char *signature = __SIGNATURE;
          scoped_file file(name);
#ifdef IMAGE_PNG_DETAIL_DEBUG
          spdlog::set_pattern("[%^%l%$] %v");
          spdlog::set_level(spdlog::level::debug);
#endif

          if (file.valid() && header) {
            if (std::fread(&header->png[0], size::u8, 8, file.p) == 8) {
              if (std::memcmp(header->png, magic, sizeof(magic)) != 0) {
                return error_t::InvalidPNG;
              }
            }

            // The very first chunk MUST be the IHDR one
            uint32_t ihdr_size;
            if (std::fread(&ihdr_size, size::u32, 1, file.p) == 1) {
              std::fseek(file.p, -size::u32, SEEK_CUR);
              if (std::fread(&header->ihdr, sizeof(decltype(header->ihdr)), 1, file.p) == 1) {
#ifdef IMAGE_PNG_DETAIL_DEBUG
                spdlog::debug(
                  "[{}] IHDR length: {}",
                  signature,
                  __SWIZZLE32(header->ihdr.length)
                );
#endif
                if (__SWIZZLE32(header->ihdr.length) != __SWIZZLE32(ihdr_size)) {
#ifdef IMAGE_PNG_DETAIL_DEBUG
                  spdlog::critical(
                    "[{}] IHDR length mismatch! ([{}], [{}])",
                    signature,
                    __SWIZZLE32(header->ihdr.length),
                    __SWIZZLE32(ihdr_size)
                  );
#endif
                  return error_t::InvalidPNG;
                }

                header->ihdr.length = __SWIZZLE32(header->ihdr.length);
                header->ihdr.chunk_type = __SWIZZLE32(header->ihdr.chunk_type);

                header->ihdr.width = __SWIZZLE32(header->ihdr.width);
                header->ihdr.height = __SWIZZLE32(header->ihdr.height);
                header->ihdr.bpp = __SWIZZLE8(header->ihdr.bpp);
                header->ihdr.color_type = __SWIZZLE8(header->ihdr.color_type);
                header->ihdr.compression_type = __SWIZZLE8(header->ihdr.compression_type);
                header->ihdr.filter_type = __SWIZZLE8(header->ihdr.filter_type);
                header->ihdr.interlacing_type = __SWIZZLE8(header->ihdr.interlacing_type);

                header->ihdr.crc = __SWIZZLE32(header->ihdr.crc);
              }
            }

#ifdef IMAGE_PNG_DETAIL_DEBUG
            spdlog::debug(
              "[{}] Finished reading IHDR section",
              signature
            );
#endif

            // Get the rest chunks
            while (!std::feof(file.p)) {
#ifdef IMAGE_PNG_DETAIL_DEBUG
              /*
              spdlog::debug(
                "[{}] Pointer: {}",
                signature,
                std::ftell(f)
              );
              */
#endif
              uint32_t length;
              char name[5];

              if (std::fread(&length, size::u32, 1, file.p) == 1) {
                length = __SWIZZLE32(length);
                std::fread(&name, sizeof(char), 4, file.p);
                name[4] = '\0';

#ifdef IMAGE_PNG_DETAIL_DEBUG
                /*
                spdlog::debug(
                  "[{}] Length: {}",
                  signature,
                  length
                );
                */
#endif

                if (std::strcmp(name, "IDAT") == 0) {
                  // There can be multiple IDAT chunks
                  header->chunks.idat += 1;
                  uint32_t idat_position = std::ftell(file.p) - 4u;
#ifdef IMAGE_PNG_DETAIL_DEBUG
                  if (header->chunks.idat <= IMAGE_PNG_DETAIL_MAXIMUM_IDAT_COUNT) {
                    std::fseek(f, -size::u32 * 2, SEEK_CUR);

                    uint32_t length;
                    std::fread(&length, size::u32, 1, f);

                    length = __SWIZZLE32(length);

                    spdlog::debug(
                      "[{}] IDAT found at offset {:X}/{} with length: {} bytes",
                      signature,
                      idat_position,
                      idat_position,
                      length
                    );

                    std::fseek(f, size::u32, SEEK_CUR);
                  }
                  else if (header->chunks.idat == IMAGE_PNG_DETAIL_MAXIMUM_IDAT_COUNT + 1) {
                    std::puts("...");
                  }
#endif
                  // Only determining DEFLATE-compressed information from the first IDAT section should be enough
                  if (header->chunks.idat == 1) {
                      uint8_t zlib_header[2];
                      std::fread(&zlib_header[0], size::u8, 2, file.p);
                      std::fseek(file.p, -size::u8 * 2, SEEK_CUR);

                      // http://www.libpng.org/pub/png/spec/1.2/PNG-Compression.html
                      // According to https://tools.ietf.org/html/rfc1950 (Section 2.2),
                      //
                      const auto cmf_set = std::bitset<8>(zlib_header[0]);
                      const auto flg_set = std::bitset<8>(zlib_header[1]);

#ifdef IMAGE_PNG_DETAIL_DEBUG
                      spdlog::debug(
                        "[{}] zlib bytes (CMF/FLG): {:X} ({}) {:X} ({})",
                        signature,
                        zlib_header[0],
                        cmf_set.to_string(),
                        zlib_header[1],
                        flg_set.to_string()
                      );
#endif

                      const uint8_t flevel = pack<uint8_t, 8>(
                        flg_set, {6, 7}
                      );

                      const uint8_t cm = pack<uint8_t, 8>(
                        cmf_set, {3, 2, 1, 0}
                      );

                      const uint8_t cinfo = pack<uint8_t, 8>(
                        cmf_set, {7, 6, 5, 4}
                      );

                      header->compression_level = flevel;
#ifdef IMAGE_PNG_DETAIL_DEBUG
                      spdlog::debug(
                        "[{}] CMF CM [{}] CMF CINFO [{}] FLG FLEVEL [{}]",
                        signature,
                        cm,
                        cinfo,
                        flevel
                      );
#endif
                  }
                }
                else if (std::strcmp(name, "PLTE") == 0) {
                  header->chunks.plte += 1;
                }
                else if (std::strcmp(name, "IEND") == 0) {
                  header->chunks.iend += 1;
                }
                else if (std::strcmp(name, "iCCP") == 0) {
                  header->chunks.iccp += 1;
                }
                else if (std::strcmp(name, "pHYs") == 0) {
                  header->chunks.phys += 1;
                }
                else if (std::strcmp(name, "sRGB") == 0) {
                  header->chunks.srgb += 1;
                }
                else if (std::strcmp(name, "tIME") == 0) {
                  header->chunks.time += 1;
                }
                else if (std::strcmp(name, "eXIf") == 0) {
                  header->chunks.exif += 1;
                }
                else if (std::strcmp(name, "gAMA") == 0) {
                  header->chunks.gama += 1;
                }
                else if (std::strcmp(name, "zTXt") == 0) {
                  header->chunks.ztxt += 1;
                }
                else if (std::strcmp(name, "hIST") == 0) {
                  header->chunks.hist += 1;
                }

                std::fseek(file.p, length + 4, SEEK_CUR);
#ifdef IMAGE_PNG_DETAIL_DEBUG
                /*
                spdlog::debug(
                  "[{}] Jumping to offset {}",
                  signature,
                  std::ftell(f)
                );
                */
#endif
              }
            }

            std::fclose(file.p);

            return error_t::None;
          }

          return error_t::Other;
        }
      } // namespace detail

      std::unordered_map<std::string, std::any> parse(const char *name)
      {
        std::unordered_map<std::string, std::any> r;

        detail::PNG_header_t header = {0};

        if (detail::read(&header, name) == error_t::None) {
            r.insert({ std::string("magic.s"), format(
              "[%02X] %02X %02X %02X %02X %02X %02X %02X",
              header.png[0],
              header.png[1],
              header.png[2],
              header.png[3],
              header.png[4],
              header.png[5],
              header.png[6],
              header.png[7]
            )});

            // PNG has no versioning information
            r.insert({ std::string("version.s"), std::string("1 (No versioning)") });
            r.insert({ std::string("version_sanitized.u16"), (uint16_t) 1u });

            // Explicit casting to uint32_t to avoid the "cannot bind packed field to reference" issue
            r.insert({ std::string("width.u32"), (uint32_t) header.ihdr.width });
            r.insert({ std::string("height.u32"), (uint32_t) header.ihdr.height });
            r.insert({ std::string("projected_aspect_ratio.f"), (float) header.ihdr.width / (float) header.ihdr.height });
            r.insert({ std::string("ihdr_crc.u32"), (uint32_t) header.ihdr.crc });
            r.insert({ std::string("compression_type.u8"), (uint8_t) header.ihdr.compression_type });

            // interlacing_type == 1 equals Adam7 interlacing, and 0 for none
            r.insert({ std::string("interlaced.b"), (bool) header.ihdr.interlacing_type == 1 });

            std::unordered_map<std::string, uint16_t> chunks;
            chunks.insert({ std::string("IHDR"), 1u });
            chunks.insert({ std::string("IDAT"), header.chunks.idat });
            chunks.insert({ std::string("PLTE"), header.chunks.plte });
            chunks.insert({ std::string("IEND"), header.chunks.iend });
            chunks.insert({ std::string("iCCP"), header.chunks.iccp });
            chunks.insert({ std::string("pHYs"), header.chunks.phys });
            chunks.insert({ std::string("sRGB"), header.chunks.srgb });
            chunks.insert({ std::string("tIME"), header.chunks.time });
            chunks.insert({ std::string("eXIf"), header.chunks.exif });
            chunks.insert({ std::string("gAMA"), header.chunks.gama });
            chunks.insert({ std::string("zTXt"), header.chunks.ztxt });
            chunks.insert({ std::string("hIST"), header.chunks.hist });
            r.insert({ std::string("chunks.unordered_map<s, u16>"), chunks });

            r.insert({ std::string("deflate_compression_level.u8"), header.compression_level });
        }

        return r;
      }
    } // namespace png
  } // namespace image
} // namespace doors

#endif
