#pragma once

enum class error_t {
  None,
  InvalidFormat,
  Other,

  InvalidGIF,
  InvalidJPG,
  InvalidPNG,
  InvalidPSD,
  InvalidTGA
};

struct error_message_t {
  error_t error;
  char *message;
};

error_message_t errors[] = {
  error_message_t {
    error_t::None,
    "Nothing In Your Eyes"
  }
};
