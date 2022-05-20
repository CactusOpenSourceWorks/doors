#if !defined(__IMAGE_PSD_DETAIL__)
#define __IMAGE_PSD_DETAIL__

#include <unordered_map>
#include <any>
#include <string>

namespace doors {
  namespace image {
    namespace psd {
      namespace detail {
        struct PSD_header_t {
          char psd[5];
          uint32_t width;
          uint32_t height;
          uint16_t channels;
          uint8_t bpp;
          uint8_t color_space;

          uint16_t layers;
        };

        error_t read(PSD_header_t *header, const char *name);
      } // namespace detail

      using namespace detail;

      std::unordered_map<std::string, std::any> parse(const char *name);
    } // namespace psd
  } // namespace image
} // namespace doors

#endif

// #ifdef IMAGE_PSD_DETAIL
// #undef IMAGE_PSD_DETAIL

namespace doors {
  namespace image {
    namespace psd {
      namespace detail {
        error_t read(PSD_header_t *header, const char *name)
        {
          std::FILE *f = std::fopen(name, "rb");

          if (f && header) {
            if(std::fread(&header->psd[0], sizeof(char), 4, f) == 4) {
              header->psd[4] = '\0';
            }

            uint16_t dummy;
            if (std::fread(&dummy, sizeof(uint16_t), 1, f) != 1) {
              return error_t::InvalidPSD;
            }

            std::printf("%i\n", __SWIZZLE16(dummy));

            std::fseek(f, 6, SEEK_CUR);
            if (std::fread(&header->channels, sizeof(uint16_t), 1, f) == 1)
              header->channels = __SWIZZLE16(header->channels);

            if (std::fread(&header->height, sizeof(uint32_t), 1, f) == 1)
              header->height = __SWIZZLE32(header->height);

            if (std::fread(&header->width, sizeof(uint32_t), 1, f) == 1)
              header->width = __SWIZZLE32(header->width);

            uint16_t bpp;
            if (std::fread(&bpp, sizeof(uint16_t), 1, f) == 1)
              header->bpp = (uint8_t) __SWIZZLE16(bpp);

            uint16_t color_space;
            if (std::fread(&color_space, sizeof(uint16_t), 1, f) == 1)
              header->color_space = (uint8_t) __SWIZZLE16(color_space);

            // Color Mode Data Section
            uint32_t color_data_length;
            std::fread(&color_data_length, sizeof(uint32_t), 1, f);
            std::fseek(f, color_data_length, SEEK_CUR);

            // Image Resources Section
            bool lul = false;
            int count = 0;

            while (!lul) {
              printf("Current location: %i\n", std::ftell(f));
              uint32_t size = 0u;
              std::fread(&size, sizeof(uint32_t), 1, f);

              char image_resource_header[5];
              std::fread(&image_resource_header, sizeof(char), 4, f);
              image_resource_header[4] = '\0';

              count += 1;
              printf("Count: %i (at %i)\n", count, std::ftell(f));
              printf("Section length: %i\n", __SWIZZLE32(size));
              printf("First 4 bytes: %s\n", image_resource_header);

              if (std::string(image_resource_header) == "8BIM") {
                /*
                std::fseek(f, 2, SEEK_CUR);
                std::fseek(f, 2, SEEK_CUR);

                uint32_t image_resource_length;
                std::fread(&image_resource_length, sizeof(uint32_t), 1, f);

                std::printf("%i\n", image_resource_length);

                std::fseek(f, image_resource_length, SEEK_CUR);
                */
                std::fseek(f, -4, SEEK_CUR);
                std::fseek(f, __SWIZZLE32(size) + 8, SEEK_CUR);
              }
              else {
                lul = true;
              }
            }

            // Layer and Mask Information Section
            uint32_t layer_block_length;
            std::fread(&layer_block_length, sizeof(uint32_t), 1, f);

            uint32_t layer_section_length;
            std::fread(&layer_section_length, sizeof(uint32_t), 1, f);

            uint16_t layer_length;
            std::fread(&layer_length, sizeof(uint16_t), 1, f);

            printf("%i %i\n", __SWIZZLE16(layer_length), std::ftell(f));

            header->layers = __SWIZZLE16(layer_length);

            std::fclose(f);

            return error_t::None;
          }

          return error_t::Other;
        }
      } // namespace detail

      std::unordered_map<std::string, std::any> parse(const char *name)
      {
        std::unordered_map<std::string, std::any> r;

        detail::PSD_header_t header = {0};

        if (detail::read(&header, name) == error_t::None) {
            r.insert({ std::string("magic.s"), std::string(header.psd) });
            r.insert({ std::string("version.s"), std::string("") });
            r.insert({ std::string("version_sanitized.u16"), (uint16_t) 0u });
            r.insert({ std::string("width.u32"), header.width });
            r.insert({ std::string("height.u32"), header.height });
            r.insert({ std::string("projected_aspect_ratio.f"), (float) header.width / (float) header.height });

            r.insert({ std::string("bits_per_pixel.u8"), header.bpp });
            r.insert({ std::string("color_space.u8"), header.color_space });

            std::string color_space_sanitized = "";
            switch (header.color_space) {
              case 0:
                color_space_sanitized = "Bitmap";
                break;
              case 1:
                color_space_sanitized = "Grayscale";
                break;
              case 2:
                color_space_sanitized = "Indexed";
                break;
              case 3:
                color_space_sanitized = "RGB";
                break;
              case 4:
                color_space_sanitized = "CMYK";
                break;
              case 7:
                color_space_sanitized = "Multichannel";
                break;
              case 8:
                color_space_sanitized = "Duotone";
                break;
              case 9:
                color_space_sanitized = "Lab";
                break;
              default:
                color_space_sanitized = "(?)";
                break;
            }

            r.insert({ std::string("color_space_sanitized.s"), color_space_sanitized });

            r.insert({ std::string("layers.u16"), header.layers });
        }

        return r;
      }
    } // namespace psd
  } // namespace image
} // namespace doors

// #endif
