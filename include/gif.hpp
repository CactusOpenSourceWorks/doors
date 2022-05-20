#if !defined(__IMAGE_GIF_DETAIL__)
#define __IMAGE_GIF_DETAIL__

// Pending issue(s):
//   Convert the few remaining std::f*(FILE *) calls to utilize scoped_file instead.
//   Validation flags
//   Testing

// Test suite: http://code.google.com/p/imagetestsuite/ (which also covers PNG, JFIF & TIFF)
// 
// Since GIF contains no frame count information, a whole-file read is required to approximate the frames,
// which is, probably slow on large files.

#include <unordered_map>
#include <any>
#include <string>

#include <type_traits>

#include "compiler.hpp"
using namespace compiler;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace doors {
  namespace image {
    namespace gif {
      namespace detail {
        struct GIF_LSD_header_t {
          uint16_t width;
          uint16_t height;
          uint8_t packed;
          uint8_t background_color;
          uint8_t pixel_aspect_ratio;
        };

        struct GIF_GCT_header_t {
          bool exists;
          uint16_t size; // The GCT's total size equals to (2 ^ (size + 1)) * 3
        };

        struct GIF_header_t {
          char gif[4];
          char version[4];
          uint16_t version_sanitized;
          uint16_t frames;
          GIF_LSD_header_t lsd;
          GIF_GCT_header_t gct;
        };

        error_t read(GIF_header_t *header, const char *name);
      } // namespace detail

      using namespace detail;

      std::unordered_map<std::string, std::any> parse(const char *name);
    } // namespace gif
  } // namespace image
} // namespace doors

#endif

// #define IMAGE_GIF_DETAIL

#ifdef IMAGE_GIF_DETAIL
#undef IMAGE_GIF_DETAIL

static constexpr uint64_t cpow(uint64_t base, uint64_t exp)
{
  uint64_t r = 1;
  while (exp) {
    if (exp & 1) {
      r *= base;
    }
    exp >>= 1;
    base *= base;
  }
  return r;
}

namespace doors {
  namespace image {
    namespace gif {
      namespace detail {
        // GIF is little endian
        error_t read(GIF_header_t *header, const char *name)
        {
#ifdef IMAGE_GIF_DETAIL_DEBUG
          spdlog::set_pattern("[%^%l%$] %v");
          spdlog::set_level(spdlog::level::debug);
#endif

          scoped_file file(name);
          const char *signature = __SIGNATURE;

          if (file.valid() && header) {
            if (std::fread(&header->gif[0], size::c, 3, file.p) == 3)
              header->gif[3] = '\0';
            else
              return error_t::InvalidGIF;

            if (std::fread(&header->version[0], size::c, 3, file.p) == 3) {
              header->version[3] = '\0';

              if (std::strcmp(header->version, "87a") == 0)
                header->version_sanitized = 1987;
              else if (std::strcmp(header->version, "89a") == 0)
                header->version_sanitized = 1989;
              else
                return error_t::InvalidGIF;
            }
            else
              return error_t::InvalidGIF;

#ifdef IMAGE_GIF_DETAIL_DEBUG
            spdlog::debug(
              "[{}] GIF version bytes: {}",
              signature,
              header->version
            );
#endif

            // LSD
            {
              // Canvas width, height, packed byte, background color & aspect ratio is all part of the
              // Logical Screen Descriptor (LSD) which follows the header block.
              std::fread(&header->lsd.width, size::u16, 1, file.p);
              std::fread(&header->lsd.height, size::u16, 1, file.p);

              std::fread(&header->lsd.packed, size::u8, 1, file.p);

              // Contents of the packed byte (LSB ordering):
              // Bit 0: global color table flag
              // Bit 1-3: color resolution (obsolete)
              // Bit 4: Sort flag (obsolete)
              // Bit 5-7: Size of the global color table
              const auto set = std::bitset<8>(header->lsd.packed);
              header->gct.exists = set[7] == 1;

              // Stuff the first 3 bits, which helps in determining how many GCT bytes to skip through
              header->gct.size = pack<uint8_t, 8>(set, {0, 1, 2});

#ifdef IMAGE_GIF_DETAIL_DEBUG
              spdlog::debug(
                "[{}] GCT size (0-2): {:d}{:d}{:d} ({})\n"
                "GCT sorting flag (3): {:d}\n"
                "Color resolution table size (4-6): {:d}{:d}{:d}\n"
                "GCT presence flag (7): {:d}",
                signature,
                set[0], set[1], set[2],
                header->gct.size,
                set[3],
                set[4], set[5], set[6],
                set[7]
              );
#endif

              // These bits goes mostly unused
              // background_color: Can be used as w color index in the GCT for... well, background color.
              std::fread(&header->lsd.background_color, size::u8, 1, file.p);
              std::fread(&header->lsd.pixel_aspect_ratio, size::u8, 1, file.p);
            }

            // GCT
            {
              if (header->gct.exists) {
                // Unimplemented: pass through
                const uint64_t bytes = cpow(2, header->gct.size + 1) * 3;
#ifdef IMAGE_GIF_DETAIL_DEBUG
                spdlog::debug(
                  "[{}] Skipping {} GCT bytes",
                  signature,
                  bytes
                );
#endif

                std::fseek(file.p, bytes, SEEK_CUR);
              }
            }

            // '89 also contains the following optional structure:
            // CE (Comment extension)
            // AE (Application extension)
            // GCE (Graphics control extension)
            if (header->version_sanitized == 1989) {
                // ...
            }

            // Reading frame data requires a thorough read of the whole file.
            while (!std::feof(file.p)) {
              uint8_t magic;
              std::fread(&magic, size::u8, 1, file.p);

              // A LZW-packed frame is preceded with an image descriptor, beginning with 
              // ',' (0x2C). The next 4 bytes contain starting origin, followed by 4 bytes of image size.
              // It's already encoded using Intel byte order, thus require no swizzling (on x86)
              if (magic == 0x2C) {
                uint16_t w, h;
                std::fseek(file.p, 4, SEEK_CUR);
                std::fread(&w, size::u16, 1, file.p);
                std::fread(&h, size::u16, 1, file.p);

                if (w == header->lsd.width && h == header->lsd.height)
                  header->frames += 1;
              }
            }

            return error_t::None;
          }

          return error_t::Other;
        }
      } // namespace detail

      std::unordered_map<std::string, std::any> parse(const char *name)
      {
        std::unordered_map<std::string, std::any> r;
        detail::GIF_header_t header = {0};

        if (detail::read(&header, name) == error_t::None) {
            r.insert({ std::string("magic.s"), std::string(header.gif) });
            r.insert({ std::string("version.s"), std::string(header.version) });
            r.insert({ std::string("version_sanitized.u16"), header.version_sanitized });
            r.insert({ std::string("width.u32"), (uint32_t) header.lsd.width });
            r.insert({ std::string("height.u32"), (uint32_t) header.lsd.height });
            r.insert({ std::string("projected_aspect_ratio.f"), (float) header.lsd.width / (float) header.lsd.height });

            r.insert({ std::string("frames.u16"), header.frames });
        }

        return r;
      }
    } // namespace gif
  } // namespace image
} // namespace doors

#endif
