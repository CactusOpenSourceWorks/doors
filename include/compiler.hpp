#if !defined(__COMPILER_DETAIL__)
#define __COMPILER_DETAIL__

#include <iostream>
#include <bitset>
#include <memory>
#include <filesystem>
#include <type_traits>
#include <functional>

#if defined(__GNUC__)
  #define __SWIZZLE64 __builtin_bswap64
  #define __SWIZZLE32 __builtin_bswap32
  #define __SWIZZLE16 __builtin_bswap16
  #define __SWIZZLE8

  #define __PACKED_STRUCT_START struct __attribute__((__packed__))
  #define __PACKED_STRUCT_END

  #define __SIGNATURE __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
  #define __SWIZZLE64 _byteswap_uint64
  #define __SWIZZLE32 _byteswap_ulong
  #define __SWIZZLE16 _byteswap_ushort
  #define __SWIZZLE8

  #define __PACKED_STRUCT_START __pragma(pack(push, 1)) struct
  #define __PACKED_STRUCT_END __pragma(pack(pop))

  #define __SIGNATURE __FUNCSIG__
#endif

namespace compiler {
namespace size {
  constexpr const size_t c = sizeof(char);
  constexpr const size_t u8 = sizeof(uint8_t);
  constexpr const size_t u16 = sizeof(uint16_t);
  constexpr const size_t u32 = sizeof(uint32_t);
  constexpr const size_t u64 = sizeof(uint64_t);
}

namespace constant {
  // C++20 brings constexpr string, but for now we'll be sticking with const.
  const std::string qmark("(?)");
}

void traverse(const char *directory, const std::function<void(const char *)> &f)
{
  for (const auto &p : std::filesystem::recursive_directory_iterator(directory))
    f(p.path().string().c_str());
}

template <typename T, size_t size>
T pack(const std::bitset<size> &set, const std::initializer_list<uint8_t> &list)
{
  static_assert(std::is_integral<T>::value);

  T r{};
  for (const auto &i : list) {
    r <<= 1;
    r |= set[i];
  }

  return r;
}

template <typename... Args>
std::string format(const std::string& format, Args... args)
{
    size_t size = std::snprintf(nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'

    if (size <= 0) {
      return std::string();
    }

    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

// Thin wrapper around std::FILE * allowing the use of RAII.
// Wrapping under a std::unique_ptr<> is also an option, but a struct is more handy at keeping custom metadata information.
struct scoped_file {
    std::FILE *p = nullptr;

    scoped_file(const char *name)
    {
      p = std::fopen(name, "rb");
    }

    bool valid() const { return p != nullptr; }

    template <
      typename T, 
      typename = std::enable_if<!std::is_void<T>::value>
    >
    T byte(int count = 1)
    {
      T r{};
      std::fread(&r, sizeof r, count, p);

      return r;
    }

    std::string string(int length)
    {
      std::string s;
      s.resize(length);

      std::fread(&s[0], size::u8, length, p);

      return s;
    }

    bool skip(long offset = 0, int origin = SEEK_CUR)
    {
      return 0 == std::fseek(p, offset, origin);
    }

    bool operator!=(const void *p) const
    {
      return this->p != p;
    }

    ~scoped_file()
    {
      if (p != nullptr)
        std::fclose(p);
    }
};

template <typename T, typename = std::enable_if<std::is_enum<T>::value>>
auto operator|(T lhs, T rhs) -> const T
{
  using type = typename std::underlying_type<T>::type;

  return static_cast<T>(
    static_cast<type>(lhs) | static_cast<type>(rhs)
  );
}

template <typename T, typename = std::enable_if<std::is_enum<T>::value>>
auto operator&(T lhs, T rhs) -> bool
{
  using type = typename std::underlying_type<T>::type;

  return static_cast<type>(lhs) & static_cast<type>(rhs);
}

}

#endif
