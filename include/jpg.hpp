#if !defined(__IMAGE_JPG_DETAIL__)
#define __IMAGE_JPG_DETAIL__

// Pending issue(s):
//   Due to the nature of EXIFs, the SOF0 block could be incorrectly identified as not containing the real
//   size of the image. (fix this!)
//   Convert the few remaining std::f*(FILE *) calls to utilize scoped_file instead.
//   Testing

// Test suite: https://code.google.com/archive/p/imagetestsuite/downloads
// Only JFIF are supported for now - support for EXIF, TIFF and other JPEG-based formats is pending.

#include <unordered_map>
#include <any>
#include <string>

#include <type_traits>

#include <compiler.hpp>
using namespace compiler;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace doors {
  namespace image {
    namespace jpg {
      enum class JPG_validate_flags {
        SOI = 1 << 0,
        APP0 = 1 << 1,
        magic = 1 << 2,
        unrecognized_SOFn = 1 << 3,
        everything = SOI | APP0 | magic | unrecognized_SOFn
      };

      namespace detail {
        struct JPG_FFC0_header_t {
          uint8_t bpp;
          uint16_t width;
          uint16_t height;
          uint8_t color_space;
        };

        struct JPG_header_t {
          uint8_t soi[2];
          uint8_t app0[2];
          uint16_t app0_length;
          char jfif[5];
          uint8_t version[2];
          uint16_t version_sanitized;
          uint8_t density_unit;
          uint16_t density_width;
          uint16_t density_height;
          uint8_t thumbnail_width;
          uint8_t thumbnail_height;

          // SOF0 block
          JPG_FFC0_header_t ffc0;
        };

        const JPG_validate_flags get_default_flags();
        std::unordered_map<std::string, std::any> get_default_struct();

        error_t read(JPG_header_t *header, const char *name, const JPG_validate_flags flags = get_default_flags());
      } // namespace detail

      using namespace detail;

      std::unordered_map<std::string, std::any> parse(const char *name);
    } // namespace jpg
  } // namespace image
} // namespace doors

#endif

// #define IMAGE_JPG_DETAIL

#ifdef IMAGE_JPG_DETAIL
#undef IMAGE_JPG_DETAIL

namespace doors {
  namespace image {
    namespace jpg {
      namespace detail {
        const JPG_validate_flags get_default_flags()
        {
          return JPG_validate_flags::everything;
        }

        std::unordered_map<std::string, std::any> get_default_struct()
        {
          std::unordered_map<std::string, std::any> r = {
            { std::string("magic.s"), constant::qmark },
            { std::string("version.s"), constant::qmark },
            { std::string("version_sanitized.u16"), (uint16_t) 0u },
            { std::string("width.u32"), (uint32_t) 0u },
            { std::string("height.u32"), (uint32_t) 0u },
            { std::string("projected_aspect_ratio.f"), (float) 0.0f },
            { std::string("bits_per_pixel.u8"), (uint8_t) 0u },
            { std::string("color_space.u8"), (uint8_t) 0u },
            { std::string("color_space_sanitized.s"), constant::qmark }
          };

          return r;
        }

        // JFIF is MSB
        error_t read(JPG_header_t *header, const char *name, const JPG_validate_flags flags)
        {
#ifdef IMAGE_JPG_DETAIL_DEBUG
          spdlog::set_pattern("[%^%l%$] %v");
          spdlog::set_level(spdlog::level::debug);
#endif

          scoped_file file(name);
          const char *signature = __SIGNATURE;

          if (file.valid() && header) {
            // SOI bytes (ff d8) along with APP0 segment
            std::fread(&header->soi[0], size::u8, 2, file.p);
#ifdef IMAGE_JPG_DETAIL_DEBUG
            spdlog::debug(
              "[{}] SOI bytes (should be FF D8): {:X} {:X}",
              signature,
              header->soi[0],
              header->soi[1]
            );
#endif

            if (flags & JPG_validate_flags::SOI) {
                if (header->soi[0] != 0xFF || header->soi[1] != 0xD8) {
#ifdef IMAGE_JPG_DETAIL_DEBUG
                    spdlog::critical(
                      "[{}] Incorrect SOI bytes (should be FF D8): {:X} {:X}",
                      signature,
                      header->soi[0],
                      header->soi[1]
                    );
#endif
                    return error_t::InvalidJPG;
                }
            }

            std::fread(&header->app0[0], size::u8, 2, file.p);
#ifdef IMAGE_JPG_DETAIL_DEBUG
            spdlog::debug(
              "[{}] APP0 bytes (should be FF E0): {:X} {:X}",
              signature,
              header->app0[0],
              header->app0[1]
            );
#endif

            if (flags & JPG_validate_flags::APP0) {
              if (header->app0[0] != 0xFF || header->app0[1] != 0xE0) {
#ifdef IMAGE_JPG_DETAIL_DEBUG
                  spdlog::critical(
                    "[{}] Incorrect APP0 bytes (should be FF E0): {:X} {:X}",
                    signature,
                    header->soi[0],
                    header->soi[1]
                  );
#endif
                  return error_t::InvalidJPG;
              }
            }

            std::fread(&header->app0_length, size::u16, 1, file.p);
            header->app0_length = __SWIZZLE16(header->app0_length);
#ifdef IMAGE_JPG_DETAIL_DEBUG
            spdlog::debug(
              "[{}] APP0 length: {} bytes",
              signature,
              header->app0_length
            );
#endif

            if (std::fread(&header->jfif[0], sizeof(char), 5, file.p) == 5) {
              // The JFIF identifier is already NULL-terminated.

              if (flags & JPG_validate_flags::magic) {
                if (std::memcmp(header->jfif, "JFIF", 5) != 0) {
#ifdef IMAGE_JPG_DETAIL_DEBUG
                  spdlog::critical(
                    "[{}] Incorrect JFIF magic (received {})",
                    signature,
                    header->jfif
                  );
#endif
                  return error_t::InvalidJPG;
                }
              }
            }

            if (std::fread(&header->version[0], size::u8, 2, file.p) == 2) {
              // Why doesn't C++11 have std::stoui()?
              header->version_sanitized = static_cast<uint16_t>(std::stoul(
                    std::to_string(header->version[0])
                  + "0"
                  + std::to_string(header->version[1])
                ));
#ifdef IMAGE_JPG_DETAIL_DEBUG
              spdlog::debug(
                "[{}] Version: {}",
                signature,
                header->version_sanitized
              );
#endif
            }

            std::fread(&header->density_unit, size::u8, 1, file.p);

            switch (header->density_unit) {
              case 0x01:
#ifdef IMAGE_JPG_DETAIL_DEBUG
                spdlog::debug("[{}] Density Unit: Pixels/Inch", signature);
#endif
                break;
              case 0x02:
#ifdef IMAGE_JPG_DETAIL_DEBUG
                spdlog::debug("[{}] Density Unit: Pixels/Centimeter", signature);
#endif

                break;
              default:
#ifdef IMAGE_JPG_DETAIL_DEBUG
                spdlog::debug("[{}] Density Unit: Undefined", signature);
#endif
                ;
            }

            std::fread(&header->density_width, size::u16, 1, file.p);
            header->density_width = __SWIZZLE16(header->density_width);

            std::fread(&header->density_height, size::u16, 1, file.p);
            header->density_height = __SWIZZLE16(header->density_height);
#ifdef IMAGE_JPG_DETAIL_DEBUG
            spdlog::debug(
              "[{}] density_width = {}/density_height = {}",
              signature,
              header->density_width,
              header->density_height
            );
#endif

            std::fread(&header->thumbnail_width, size::u8, 1, file.p);
            std::fread(&header->thumbnail_height, size::u8, 1, file.p);
#ifdef IMAGE_JPG_DETAIL_DEBUG
            spdlog::debug(
              "[{}] thumbnail_width = {}/thumbnail_height = {}",
              signature,
              header->thumbnail_width,
              header->thumbnail_height
            );
#endif
            // Scanning for the first SOF0 marker. For the case of EXIF JPEGs,
            // the SOF0 marker could also reside from within the APP1 block.
            while (!std::feof(file.p)) {
              uint8_t ff, c0;
              std::fread(&ff, size::u8, 1, file.p);

              if (ff == 0xFF) {
                std::fread(&c0, size::u8, 1, file.p);
                if (c0 == 0xC0 || c0 == 0xC2) { // Supports both baseline/progressive
                  std::fseek(file.p, 2, SEEK_CUR);

#ifdef IMAGE_JPG_DETAIL_DEBUG
                  spdlog::debug(
                    "[{}] SOF0/SOF2 section found at {:X}/{}",
                    signature,
                    std::ftell(file.p),
                    std::ftell(file.p)
                  );
#endif

                  std::fread(&header->ffc0.bpp, size::u8, 1, file.p);

                  uint16_t t;

                  std::fread(&t, size::u16, 1, file.p);
                  header->ffc0.height = __SWIZZLE16(t);

                  std::fread(&t, size::u16, 1, file.p);
                  header->ffc0.width = __SWIZZLE16(t);

                  std::fread(&header->ffc0.color_space, size::u8, 1, file.p);
                  return error_t::None;
                }
              }
            }

            if (flags & JPG_validate_flags::unrecognized_SOFn) {
#ifdef IMAGE_JPG_DETAIL_DEBUG
              spdlog::critical(
                "[{}] Baseline/progressive SOFn couldn't be found. Size/colorspace information couldn't be retrieved.",
                signature
              );
#endif
              return error_t::InvalidJPG;
            }

            return error_t::None;
          }

          return error_t::Other;
        }
      } // namespace detail

      std::unordered_map<std::string, std::any> parse(const char *name)
      {
        auto r = get_default_struct();
        detail::JPG_header_t header = {0};

        const auto get_color_space_sanitized = [] (uint8_t color) -> const char * {
          switch (color) {
            case 1:
              return "Grayscale";
            case 3:
              return "Colored YCbCr";
            case 4:
              return "Colored CYMK";
          }

          return "Unknown";
        };

        if (detail::read(&header, name) == error_t::None) {
            r["magic.s"] = std::string(header.jfif);
            r["version.s"] = std::to_string(header.version[0]) + ".0" + std::to_string(header.version[1]);
            r["version_sanitized.u16"] = header.version_sanitized;
            r["width.u32"] = (uint32_t) header.ffc0.width;
            r["height.u32"] = (uint32_t) header.ffc0.height;
            r["projected_aspect_ratio.f"] = (float) header.ffc0.width / (float) header.ffc0.height;

            r["bits_per_pixel.u8"] = header.ffc0.bpp;
            r["color_space.u8"] = header.ffc0.color_space;
            r["color_space_sanitized.s"] = std::string(get_color_space_sanitized(header.ffc0.color_space));
        }

        return r;
      }
    } // namespace jpg
  } // namespace image
} // namespace doors

#endif
