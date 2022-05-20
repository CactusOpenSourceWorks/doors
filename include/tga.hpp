#if !defined(__IMAGE_TGA_DETAIL__)
#define __IMAGE_TGA_DETAIL__

// Pending issue(s):
//   Expose more extended information (v2.0)
//   Validation flags
//   Testing

// Test suite: https://www.fileformat.info/format/tga/sample/index.htm

#include <unordered_map>
#include <any>
#include <string>

#include <compiler.hpp>
using namespace compiler;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace doors {
  namespace image {
    namespace tga {
      namespace detail {
        __PACKED_STRUCT_START TGA_extension_header_t {
          uint16_t size;
          char author[41];
          char comment[324];
          uint16_t date[6]; // MM/DD/YY HH:MM:SS
          char job_ID[41];
          uint16_t job_time[3]; // HH:MM:SS
          char application_ID[41];
          uint8_t application_version[3];
          uint32_t key_color;
          uint32_t pixel_aspect_ratio;
          uint32_t gamma;
          uint32_t color_correction_offset;
          uint32_t postage_offset;
          uint32_t scan_line_offset;
          uint8_t attribute_type;
        };
        __PACKED_STRUCT_END

        // Because MSVC doesn't allow tight packing, (__pragma(pack(push, 1)) isn't the same as
        // __attribute__((packed))), this struct's size'd be screwed up utilizing MSVC convention...
        // Read out byte-by-byte instead.
        struct TGA_header_t {
          uint8_t version;
          uint8_t length;
          uint8_t paletted;
          uint8_t type;
          uint16_t palette_entry;  // If palette data is present, palette_entry points to the first 
                                   // entry of the palette data.
          uint16_t palette_colors; // If palette data is present, palette_colors contains the palette size.
          uint8_t palette_depth;  // If palette data is present, palette_depth contains the palette depth (15/16/24/32).
          uint16_t origin[2];
          uint16_t size[2];
          uint8_t bpp;
          uint8_t color_type;
          uint8_t descriptor;

          bool rle;
          TGA_extension_header_t extension;
        };

        error_t read(TGA_header_t *header, const char *name);
      } // namespace detail

      using namespace detail;

      std::unordered_map<std::string, std::any> parse(const char *name);
    } // namespace gif
  } // namespace image
} // namespace doors

#endif

#define IMAGE_TGA_DETAIL

#ifdef IMAGE_TGA_DETAIL
#undef IMAGE_TGA_DETAIL

namespace doors {
  namespace image {
    namespace tga {
      namespace detail {
        error_t read(TGA_header_t *header, const char *name)
        {
#ifdef IMAGE_TGA_DETAIL_DEBUG
          spdlog::set_pattern("[%^%l%$] %v");
          spdlog::set_level(spdlog::level::debug);
#endif

          scoped_file file(name);
          const char *signature = __SIGNATURE;

          if (file.valid() && header) {
            header->length = file.byte<uint8_t>();
            header->paletted = file.byte<uint8_t>();
            header->type = file.byte<uint8_t>();

#ifdef IMAGE_TGA_DETAIL_DEBUG
            spdlog::debug(
              "[{}] length = {}, paletted = {}, type = {}",
              signature,
              header->length,
              header->paletted,
              header->type
            );
#endif

            if (header->paletted) {
              header->palette_entry = file.byte<uint16_t>();
              header->palette_colors = file.byte<uint16_t>();
              header->palette_depth = file.byte<uint8_t>();

#ifdef IMAGE_TGA_DETAIL_DEBUG
              spdlog::debug(
                "[{}] palette_entry = {:x}, palette_colors = {}, palette_depth = {}",
                signature,
                header->palette_entry,
                header->palette_colors,
                header->palette_depth
              );
#endif
            }
            else {
              file.skip(sizeof header->palette_entry + sizeof header->palette_colors + sizeof header->palette_depth);
            }

            header->origin[0] = file.byte<uint16_t>();
            header->origin[1] = file.byte<uint16_t>();

            header->size[0] = file.byte<uint16_t>();
            header->size[1] = file.byte<uint16_t>();

            header->bpp = file.byte<uint8_t>();

#ifdef IMAGE_TGA_DETAIL_DEBUG
            spdlog::debug(
              "[{}] origin = [{}; {}], size = {}x{}, bpp = {}",
              signature,
              header->origin[0], header->origin[1],
              header->size[0], header->size[1],
              header->bpp
            );
#endif

            // Identifying Targa version requires seeking through the whole file, because version 2 writes
            // the "TRUEVISION-XFILE." string at the end of the file (which lies within the optional footer section).
            // It is **optional**, so if the encoder doesn't write out the footer bytes, there would be no way of identifying
            // v2.0 Targa files.
            {
              constexpr long v2_footer_length = 18;
              file.skip((long) -v2_footer_length, SEEK_END);
              const std::string v2_footer = file.string(v2_footer_length);

              header->version = 1;
              if (0 == std::memcmp(v2_footer.c_str(), "TRUEVISION-XFILE.", v2_footer_length)) {
                header->version = 2;

                file.skip((long) - v2_footer_length - 8);
                uint32_t extension = file.byte<uint32_t>();
                printf("%i %i\n", extension, sizeof(TGA_extension_header_t));

                if (extension != 0) {
                  file.skip(extension, SEEK_SET);
                  header->extension = file.byte<decltype(header->extension)>();

                  // 495 is a fixed size of the extension area, mandated by the TGA v2.0 specification.
                  if (header->extension.size != 495u)
                    return error_t::InvalidTGA;
                }
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
        detail::TGA_header_t header = {0};

        const auto get_color_type_sanitized = [&header] (uint8_t type) -> const char * {
          switch (type) {
            case 0:
              header.rle = false;
              return "No Image Data";
            case 1: // Uncompressed color-mapped image
              header.rle = false;
              return "Palette";
            case 2: // Uncompressed RGB image
              header.rle = false;
              return "RGB";
            case 3: // Uncompressed grayscale image
              header.rle = false;
              return "Grayscale";
            case 9: // RLE color-mapped image
              header.rle = true;
              return "Palette";
            case 10: // RLE RGB
              header.rle = true;
              return "RGB";
            case 11: // RLE Grayscale
              header.rle = true;
              return "Grayscale";
          }

          return "Unknown";
        };

        if (detail::read(&header, name) == error_t::None) {
            r.insert({ std::string("magic.s"), std::string("None") });
            r.insert({ std::string("version.s"), std::string("Version ") + std::to_string(header.version) });
            r.insert({ std::string("version_sanitized.u16"), (uint16_t) header.version});
            r.insert({ std::string("width.u32"), (uint32_t) header.size[0] });
            r.insert({ std::string("height.u32"), (uint32_t) header.size[1] });
            r.insert({ std::string("projected_aspect_ratio.f"), (float) header.size[0] / (float) header.size[1] });

            r.insert({ std::string("bits_per_pixel.u8"), (uint8_t) header.bpp });
            r.insert({ std::string("color_type_sanitized.s"), std::string(get_color_type_sanitized(header.type)) });
            r.insert({ std::string("compression_type.u8"), (uint8_t) (header.rle == true) });

            bool v2 = header.version == 2;
            r.insert({ std::string("author.s"), std::string(v2 ? header.extension.author : "(?)") });
            r.insert({ std::string("comment.s"), std::string(v2 ? header.extension.comment : "(?)") });
            r.insert({ std::string("software.s"), std::string(v2 ? header.extension.application_ID : "(?)") });
            r.insert({ std::string("job.s"), std::string(v2 ? header.extension.job_ID : "(?)") });

            r.insert({ std::string("time.s"),
              v2 ? format(
                "%i/%i/%i %i:%i:%i",
                header.extension.date[0], header.extension.date[1], header.extension.date[2],
                header.extension.date[3], header.extension.date[4], header.extension.date[5]
              ) : std::string("(?)")
            });

            r.insert({ std::string("job_time.s"), std::string(
              v2 ? format("%i:%i:%i", header.extension.job_time[0], header.extension.job_time[1], header.extension.job_time[2]) : "(?)")
            });

            r.insert({ std::string("gamma.f"), v2 ? header.extension.gamma : -1.0f });
            r.insert({ std::string("pixel_aspect_ratio.f"), v2 ? header.extension.pixel_aspect_ratio : -1.0f });
        }

        return r;
      }
    } // namespace gif
  } // namespace image
} // namespace doors

#endif
