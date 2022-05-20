#if !defined(__IMAGE_BMP_DETAIL__)
#define __IMAGE_BMP_DETAIL__

// Pending issue(s):
//   Convert the few remaining std::f*(FILE *) calls to utilize scoped_file instead.
//   Validation flags
//   Testing

// Test suite: http://entropymine.com/jason/bmpsuite/bmpsuite/html/bmpsuite.html

#include <unordered_map>
#include <any>
#include <string>

#include <compiler.hpp>
using namespace compiler;

namespace doors {
  namespace image {
    namespace bmp {
      namespace detail {
        struct DIB_header_t {
          uint32_t width;
          uint32_t height;
          uint16_t planes;
          uint8_t bpp;
        };

        struct BMP_header_t {
          char bmp[3];
          char version[3];
          uint16_t version_sanitized;

          uint32_t size;

          DIB_header_t dib;
        };

        error_t read(BMP_header_t *header, const char *name);
      } // namespace detail

      using namespace detail;

      std::unordered_map<std::string, std::any> parse(const char *name);
    } // namespace bmp
  } // namespace image
} // namespace doors

#endif

#define IMAGE_BMP_DETAIL

#ifdef IMAGE_BMP_DETAIL
#undef IMAGE_BMP_DETAIL

namespace doors {
  namespace image {
    namespace bmp {
      namespace detail {
        error_t read(BMP_header_t *header, const char *name)
        {
          scoped_file file(name);

          if (file.valid() && header) {
            if (std::fread(&header->bmp[0], size::c, 2, file.p) == 2)
                header->bmp[2] = '\0';

            header->size = file.byte<uint32_t>();
            file.skip(8);

            uint32_t header_size = file.byte<uint32_t>();
            header->version[0] = 'V';

            switch (header_size) {
              case 40: // BITMAPINFOHEADER
                header->version[1] = '1';
                header->version_sanitized = 1;
                break;

              case 52: // BITMAPV2INFOHEADER, which is very, VERY unlikely
                header->version[1] = '2';
                header->version_sanitized = 2;
                break;

              case 56: // BITMAPV3INFOHEADER, ditto
                header->version[1] = '3';
                header->version_sanitized = 3;
                break;

              case 108: // BITMAPV4HEADER
                header->version[1] = '4';
                header->version_sanitized = 4;
                break;

              case 124: // BITMAPV5HEADER
                header->version[1] = '5';
                header->version_sanitized = 5;
                break;

              default:
                header->version[1] = '?';
                header->version_sanitized = -1;
                break;
            }

            header->version[2] = '\0';

            header->dib.width = file.byte<uint32_t>();
            header->dib.height = file.byte<uint32_t>();
            header->dib.planes = file.byte<uint16_t>();
            header->dib.bpp = file.byte<uint16_t>();

            return error_t::None;
          }

          return error_t::Other;
        }
      } // namespace detail

      std::unordered_map<std::string, std::any> parse(const char *name)
      {
        std::unordered_map<std::string, std::any> r;

        detail::BMP_header_t header = {0};

        if (detail::read(&header, name) == error_t::None) {
            r.insert({ std::string("magic.s"), std::string(header.bmp) });
            r.insert({ std::string("version.s"), std::string(header.version) });
            r.insert({ std::string("version_sanitized.u16"), header.version_sanitized });
            r.insert({ std::string("width.u32"), (uint32_t) header.dib.width });
            r.insert({ std::string("height.u32"), (uint32_t) header.dib.height });
            r.insert({ std::string("projected_aspect_ratio.f"), (float) header.dib.width / (float) header.dib.height });

            r.insert({ std::string("encoded_size.u32"), header.size });
            r.insert({ std::string("bits_per_pixel.u8"), header.dib.bpp });
            r.insert({ std::string("planes.u16"), header.dib.planes });
        }

        return r;
      }
    } // namespace bmp
  } // namespace image
} // namespace doors

#endif
