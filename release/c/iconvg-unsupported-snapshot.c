#ifndef ICONVG_INCLUDE_GUARD
#define ICONVG_INCLUDE_GUARD

// Copyright 2021 The IconVG Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ----------------

// IconVG ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define ICONVG_IMPLEMENTATION before #include'ing or
// compiling it.

// ---------------- Version

// ICONVG_VERSION is the major.minor.patch version, as per https://semver.org/,
// as a uint64_t. The major number is the high 32 bits. The minor number is the
// middle 16 bits. The patch number is the low 16 bits. The pre-release label
// and build metadata are part of the string representation (such as
// "1.2.3-beta+456.20181231") but not the uint64_t representation.
//
// ICONVG_VERSION_PRE_RELEASE_LABEL (such as "", "beta" or "rc.1") being
// non-empty denotes a developer preview, not a release version, and has no
// backwards or forwards compatibility guarantees.
//
// ICONVG_VERSION_BUILD_METADATA_XXX, if non-zero, are the number of commits
// and the last commit date in the repository used to build this library. In
// each major.minor branch, the commit count should increase monotonically.
//
// Some code generation programs can override ICONVG_VERSION.
#define ICONVG_VERSION 0
#define ICONVG_VERSION_MAJOR 0
#define ICONVG_VERSION_MINOR 0
#define ICONVG_VERSION_PATCH 0
#define ICONVG_VERSION_PRE_RELEASE_LABEL "unsupported.snapshot"
#define ICONVG_VERSION_BUILD_METADATA_COMMIT_COUNT 0
#define ICONVG_VERSION_BUILD_METADATA_COMMIT_DATE 0
#define ICONVG_VERSION_STRING "0.0.0+0.00000000"

// -------------------------------- #include "./aaa_public.h"

#include <stdint.h>
#include <string.h>

// Functions that return a "const char*" typically use that to denote success
// (returning NULL) or failure (returning non-NULL). On failure, that C string
// is a human-readable but non-localized error message. It can also be compared
// (by the == operator, not just by strcmp) to an iconvg_error_etc constant.

extern const char iconvg_error_bad_magic_identifier[];
extern const char iconvg_error_bad_metadata[];
extern const char iconvg_error_bad_metadata_viewbox[];
extern const char iconvg_error_null_argument[];

// iconvg_rectangle is an axis-aligned rectangle with float32 co-ordinates.
//
// It is valid for a minimum co-ordinate to be greater than or equal to the
// corresponding maximum, or for any co-ordinate to be NaN, in which case the
// rectangle is empty. There are multiple ways to represent an empty rectangle
// but the canonical representation has all fields set to positive zero.
typedef struct iconvg_rectangle_struct {
  float min_x;
  float min_y;
  float max_x;
  float max_y;
} iconvg_rectangle;

#ifdef __cplusplus
extern "C" {
#endif

// iconvg_decode_viewbox sets *dst_viewbox to the ViewBox Metadata from the src
// IconVG-formatted data.
//
// An explicit ViewBox is optional in the IconVG file format. If not present in
// src, *dst_viewbox will be set to the default ViewBox: {-32, -32, +32, +32}.
//
// dst_viewbox may be NULL, in which case the function merely validates src's
// ViewBox.
const char*  //
iconvg_decode_viewbox(iconvg_rectangle* dst_viewbox,
                      const uint8_t* src_ptr,
                      size_t src_len);

// iconvg_rectangle__width returns self's width.
float  //
iconvg_rectangle__width(const iconvg_rectangle* self);

// iconvg_rectangle__height returns self's height.
float  //
iconvg_rectangle__height(const iconvg_rectangle* self);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef ICONVG_IMPLEMENTATION
// -------------------------------- #include "./aaa_private.h"

static inline uint16_t  //
iconvg_private_peek_u16le(const uint8_t* p) {
  return (uint16_t)(((uint16_t)(p[0]) << 0) | ((uint16_t)(p[1]) << 8));
}

static inline uint32_t  //
iconvg_private_peek_u32le(const uint8_t* p) {
  return ((uint32_t)(p[0]) << 0) | ((uint32_t)(p[1]) << 8) |
         ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24);
}

static inline float  //
iconvg_private_reinterpret_from_u32_to_f32(uint32_t u) {
  float f = 0;
  if (sizeof(uint32_t) == sizeof(float)) {
    memcpy(&f, &u, sizeof(uint32_t));
  }
  return f;
}

// ----

typedef struct iconvg_private_decoder_struct {
  const uint8_t* ptr;
  size_t len;
} iconvg_private_decoder;

// -------------------------------- #include "./decoder.c"

static void  //
iconvg_private_decoder__init(iconvg_private_decoder* self,
                             const uint8_t* ptr,
                             size_t len) {
  self->ptr = ptr;
  self->len = len;
}

// ----

static void  //
iconvg_private_decoder__advance_to_ptr(iconvg_private_decoder* self,
                                       const uint8_t* new_ptr) {
  if (new_ptr >= self->ptr) {
    size_t delta = ((size_t)(new_ptr - self->ptr));
    if (delta <= self->len) {
      self->ptr += delta;
      self->len -= delta;
      return;
    }
  }
  self->ptr = NULL;
  self->len = 0;
}

static iconvg_private_decoder  //
iconvg_private_decoder__limit_u32(iconvg_private_decoder* self,
                                  uint32_t limit) {
  iconvg_private_decoder d;
  d.ptr = self->ptr;
  d.len = (limit < self->len) ? limit : self->len;
  return d;
}

static void  //
iconvg_private_decoder__skip_to_the_end(iconvg_private_decoder* self) {
  self->ptr += self->len;
  self->len = 0;
}

// ----

static bool  //
iconvg_private_decoder__decode_coordinate_number(iconvg_private_decoder* self,
                                                 float* dst) {
  if (self->len >= 1) {
    uint8_t v = self->ptr[0];
    if ((v & 0x01) == 0) {  // 1-byte encoding.
      int32_t i = (int32_t)(v >> 1);
      *dst = ((float)(i - 64));
      self->ptr += 1;
      self->len -= 1;
      return true;

    } else if ((v & 0x02) == 0) {  // 2-byte encoding.
      if (self->len >= 2) {
        int32_t i = (int32_t)(iconvg_private_peek_u16le(self->ptr) >> 2);
        *dst = ((float)(i - (128 * 64))) / 64.0f;
        self->ptr += 2;
        self->len -= 2;
        return true;
      }

    } else {  // 4-byte encoding.
      if (self->len >= 4) {
        *dst = iconvg_private_reinterpret_from_u32_to_f32(
            0xFFFFFFFCu & iconvg_private_peek_u32le(self->ptr));
        self->ptr += 4;
        self->len -= 4;
        return true;
      }
    }
  }
  return false;
}

static bool  //
iconvg_private_decoder__decode_natural_number(iconvg_private_decoder* self,
                                              uint32_t* dst) {
  if (self->len >= 1) {
    uint8_t v = self->ptr[0];
    if ((v & 0x01) == 0) {  // 1-byte encoding.
      *dst = v >> 1;
      self->ptr += 1;
      self->len -= 1;
      return true;

    } else if ((v & 0x02) == 0) {  // 2-byte encoding.
      if (self->len >= 2) {
        *dst = iconvg_private_peek_u16le(self->ptr) >> 2;
        self->ptr += 2;
        self->len -= 2;
        return true;
      }

    } else {  // 4-byte encoding.
      if (self->len >= 4) {
        *dst = iconvg_private_peek_u32le(self->ptr) >> 2;
        self->ptr += 4;
        self->len -= 4;
        return true;
      }
    }
  }
  return false;
}

// ----

static bool  //
iconvg_private_decoder__decode_magic_identifier(iconvg_private_decoder* self) {
  if ((self->len < 4) &&         //
      (self->ptr[0] != 0x89) &&  //
      (self->ptr[1] != 0x49) &&  //
      (self->ptr[2] != 0x56) &&  //
      (self->ptr[3] != 0x47)) {
    return false;
  }
  self->ptr += 4;
  self->len -= 4;
  return true;
}

static bool  //
iconvg_private_decoder__decode_metadata_viewbox(iconvg_private_decoder* self,
                                                iconvg_rectangle* dst) {
  return iconvg_private_decoder__decode_coordinate_number(self, &dst->min_x) &&
         iconvg_private_decoder__decode_coordinate_number(self, &dst->min_y) &&
         iconvg_private_decoder__decode_coordinate_number(self, &dst->max_x) &&
         iconvg_private_decoder__decode_coordinate_number(self, &dst->max_y);
}

// ----

const char*  //
iconvg_decode_viewbox(iconvg_rectangle* dst_viewbox,
                      const uint8_t* src_ptr,
                      size_t src_len) {
  iconvg_private_decoder d;
  iconvg_private_decoder__init(&d, src_ptr, src_len);
  if (!iconvg_private_decoder__decode_magic_identifier(&d)) {
    return iconvg_error_bad_magic_identifier;
  }
  uint32_t num_metadata_chunks;
  if (!iconvg_private_decoder__decode_natural_number(&d,
                                                     &num_metadata_chunks)) {
    return iconvg_error_bad_metadata;
  }

  bool use_default_viewbox = true;
  for (; num_metadata_chunks > 0; num_metadata_chunks--) {
    uint32_t chunk_length;
    if (!iconvg_private_decoder__decode_natural_number(&d, &chunk_length) ||
        (chunk_length > d.len)) {
      return iconvg_error_bad_metadata;
    }
    iconvg_private_decoder chunk =
        iconvg_private_decoder__limit_u32(&d, chunk_length);
    uint32_t metadata_id;
    if (!iconvg_private_decoder__decode_natural_number(&chunk, &metadata_id)) {
      return iconvg_error_bad_metadata;
    }

    if (metadata_id == 0) {  // MID 0 (ViewBox).
      iconvg_rectangle r;
      if (!iconvg_private_decoder__decode_metadata_viewbox(&chunk, &r) ||
          (chunk.len != 0)) {
        return iconvg_error_bad_metadata_viewbox;
      } else if (dst_viewbox) {
        *dst_viewbox = r;
      }
      use_default_viewbox = false;
    }

    iconvg_private_decoder__skip_to_the_end(&chunk);
    iconvg_private_decoder__advance_to_ptr(&d, chunk.ptr);
  }

  if (use_default_viewbox && dst_viewbox) {
    dst_viewbox->min_x = -32.0f;
    dst_viewbox->min_y = -32.0f;
    dst_viewbox->max_x = +32.0f;
    dst_viewbox->max_y = +32.0f;
  }
  return NULL;
}

// -------------------------------- #include "./errors.c"

const char iconvg_error_bad_magic_identifier[] =  //
    "iconvg: bad magic identifier";
const char iconvg_error_bad_metadata[] =  //
    "iconvg: bad metadata";
const char iconvg_error_bad_metadata_viewbox[] =  //
    "iconvg: bad metadata (viewbox)";
const char iconvg_error_null_argument[] =  //
    "iconvg: null argument";

// -------------------------------- #include "./rectangle.c"

float  //
iconvg_rectangle__width(const iconvg_rectangle* self) {
  // Note that max_x or min_x may be NaN.
  if (self && (self->max_x > self->min_x)) {
    return self->max_x - self->min_x;
  }
  return 0.0f;
}

float  //
iconvg_rectangle__height(const iconvg_rectangle* self) {
  // Note that max_y or min_y may be NaN.
  if (self && (self->max_y > self->min_y)) {
    return self->max_y - self->min_y;
  }
  return 0.0f;
}

#endif  // ICONVG_IMPLEMENTATION

#endif  // ICONVG_INCLUDE_GUARD
