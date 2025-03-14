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

#include "./aaa_private.h"

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
        // TODO: reject NaNs?
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

static bool  //
iconvg_private_decoder__decode_real_number(iconvg_private_decoder* self,
                                           float* dst) {
  if (self->len >= 1) {
    uint8_t v = self->ptr[0];
    if ((v & 0x01) == 0) {  // 1-byte encoding.
      *dst = (float)(v >> 1);
      self->ptr += 1;
      self->len -= 1;
      return true;

    } else if ((v & 0x02) == 0) {  // 2-byte encoding.
      if (self->len >= 2) {
        *dst = (float)(iconvg_private_peek_u16le(self->ptr) >> 2);
        self->ptr += 2;
        self->len -= 2;
        return true;
      }

    } else {  // 4-byte encoding.
      if (self->len >= 4) {
        // TODO: reject NaNs?
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
iconvg_private_decoder__decode_zero_to_one_number(iconvg_private_decoder* self,
                                                  float* dst) {
  if (self->len >= 1) {
    uint8_t v = self->ptr[0];
    if ((v & 0x01) == 0) {  // 1-byte encoding.
      *dst = (float)(((double)(v >> 1)) / 120.0);
      self->ptr += 1;
      self->len -= 1;
      return true;

    } else if ((v & 0x02) == 0) {  // 2-byte encoding.
      if (self->len >= 2) {
        *dst = (float)(((double)(iconvg_private_peek_u16le(self->ptr) >> 2)) /
                       15120.0);
        self->ptr += 2;
        self->len -= 2;
        return true;
      }

    } else {  // 4-byte encoding.
      if (self->len >= 4) {
        // TODO: reject NaNs?
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

// ----

static bool  //
iconvg_private_decoder__decode_magic_identifier(iconvg_private_decoder* self) {
  if ((self->len < 4) ||         //
      (self->ptr[0] != 0x89) ||  //
      (self->ptr[1] != 0x49) ||  //
      (self->ptr[2] != 0x56) ||  //
      (self->ptr[3] != 0x47)) {
    return false;
  }
  self->ptr += 4;
  self->len -= 4;
  return true;
}

static bool  //
iconvg_private_decoder__decode_metadata_viewbox(iconvg_private_decoder* self,
                                                iconvg_rectangle_f32* dst) {
  return iconvg_private_decoder__decode_coordinate_number(self, &dst->min_x) &&
         iconvg_private_decoder__decode_coordinate_number(self, &dst->min_y) &&
         iconvg_private_decoder__decode_coordinate_number(self, &dst->max_x) &&
         iconvg_private_decoder__decode_coordinate_number(self, &dst->max_y) &&
         (-INFINITY < dst->min_x) &&    //
         (dst->min_x <= dst->max_x) &&  //
         (dst->max_x < +INFINITY) &&    //
         (-INFINITY < dst->min_y) &&    //
         (dst->min_y <= dst->max_y) &&  //
         (dst->max_y < +INFINITY);
}

static bool  //
iconvg_private_decoder__decode_metadata_suggested_palette(
    iconvg_private_decoder* self,
    iconvg_palette* dst) {
  if (self->len == 0) {
    return false;
  }
  uint8_t spec = self->ptr[0];
  self->ptr += 1;
  self->len -= 1;

  size_t n = 1 + (spec & 0x3F);
  size_t bytes_per_elem = 1 + (spec >> 6);
  if (self->len != (n * bytes_per_elem)) {
    return false;
  }
  const uint8_t* p = self->ptr;
  self->ptr += self->len;
  self->len = 0;

  iconvg_premul_color* c = &dst->colors[0];
  switch (bytes_per_elem) {
    case 1:
      for (; n > 0; n--) {
        uint8_t u = *p++;
        uint32_t rgba =
            (u < 0x80) ? iconvg_private_peek_u32le(
                             &iconvg_private_one_byte_colors[4 * ((size_t)u)])
                       : 0xFF000000u;
        iconvg_private_poke_u32le(&c->rgba[0], rgba);
        c++;
      }
      break;

    case 2:
      for (; n > 0; n--) {
        uint8_t rg = *p++;
        c->rgba[0] = 0x11 * (rg >> 4);
        c->rgba[1] = 0x11 * (rg & 0x0F);
        uint8_t ba = *p++;
        c->rgba[2] = 0x11 * (ba >> 4);
        c->rgba[3] = 0x11 * (ba & 0x0F);
        c++;
      }
      break;

    case 3:
      for (; n > 0; n--) {
        c->rgba[0] = *p++;
        c->rgba[1] = *p++;
        c->rgba[2] = *p++;
        c->rgba[3] = 0xFF;
        c++;
      }
      break;

    case 4:
      for (; n > 0; n--) {
        c->rgba[0] = *p++;
        c->rgba[1] = *p++;
        c->rgba[2] = *p++;
        c->rgba[3] = *p++;
        c++;
      }
      break;
  }
  return true;
}

// ----

static const char*  //
iconvg_private_execute_bytecode(iconvg_canvas* c_arg,
                                iconvg_rectangle_f32 r,
                                iconvg_private_decoder* d,
                                iconvg_paint* state) {
  // adjustments are the ADJ values from the IconVG spec.
  static const uint32_t adjustments[8] = {0, 1, 2, 3, 4, 5, 6, 0};

  iconvg_canvas no_op_canvas = iconvg_make_broken_canvas(NULL);
  iconvg_canvas* c = &no_op_canvas;

  // Drawing ops will typically set curr_x and curr_y. They also set x1 and y1
  // in case the subsequent op is smooth and needs an implicit point.
  float curr_x = +0.0f;
  float curr_y = +0.0f;
  float x1 = +0.0f;
  float y1 = +0.0f;
  float x2 = +0.0f;
  float y2 = +0.0f;
  float x3 = +0.0f;
  float y3 = +0.0f;
  uint32_t flags = 0;

  double scale_x = +1.0;
  double bias_x = +0.0;
  double scale_y = +1.0;
  double bias_y = +0.0;
  {
    double rw = iconvg_rectangle_f32__width_f64(&r);
    double rh = iconvg_rectangle_f32__height_f64(&r);
    double vw = iconvg_rectangle_f32__width_f64(&state->viewbox);
    double vh = iconvg_rectangle_f32__height_f64(&state->viewbox);
    if ((rw > 0) && (rh > 0) && (vw > 0) && (vh > 0)) {
      scale_x = rw / vw;
      scale_y = rh / vh;
      bias_x = r.min_x - (state->viewbox.min_x * scale_x);
      bias_y = r.min_y - (state->viewbox.min_y * scale_y);
    }
  }
  state->s2d_scale_x = scale_x;
  state->s2d_bias_x = bias_x;
  state->s2d_scale_y = scale_y;
  state->s2d_bias_y = bias_y;
  state->d2s_scale_x = 1.0 / scale_x;
  state->d2s_bias_x = -bias_x * state->d2s_scale_x;
  state->d2s_scale_y = 1.0 / scale_y;
  state->d2s_bias_y = -bias_y * state->d2s_scale_y;

  // sel[0] and sel[1] are the CSEL and NSEL registers.
  uint32_t sel[2] = {0};
  double lod[2];
  lod[0] = 0.0;
  lod[1] = INFINITY;

styling_mode:
  while (true) {
    if (d->len == 0) {
      return NULL;
    }
    uint8_t opcode = d->ptr[0];
    d->ptr += 1;
    d->len -= 1;

    if (opcode < 0x80) {
      sel[opcode >> 6] = opcode & 0x3F;
      continue;

    } else if (opcode < 0x88) {  // Set CREG[etc]; 1 byte color.
      if (d->len < 1) {
        return iconvg_error_bad_color;
      }
      uint8_t creg_index = (sel[0] - adjustments[opcode & 0x07]) & 0x3F;
      uint8_t* rgba = &state->creg.colors[creg_index].rgba[0];
      iconvg_private_set_one_byte_color(rgba, &state->custom_palette,
                                        &state->creg, d->ptr[0]);
      d->ptr += 1;
      d->len -= 1;
      sel[0] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0x90) {  // Set CREG[etc]; 2 byte color.
      if (d->len < 2) {
        return iconvg_error_bad_color;
      }
      uint8_t creg_index = (sel[0] - adjustments[opcode & 0x07]) & 0x3F;
      uint8_t* rgba = &state->creg.colors[creg_index].rgba[0];
      rgba[0] = 0x11 * (d->ptr[0] >> 4);
      rgba[1] = 0x11 * (d->ptr[0] & 0x0F);
      rgba[2] = 0x11 * (d->ptr[1] >> 4);
      rgba[3] = 0x11 * (d->ptr[1] & 0x0F);
      d->ptr += 2;
      d->len -= 2;
      sel[0] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0x98) {  // Set CREG[etc]; 3 byte (direct) color.
      if (d->len < 3) {
        return iconvg_error_bad_color;
      }
      uint8_t creg_index = (sel[0] - adjustments[opcode & 0x07]) & 0x3F;
      uint8_t* rgba = &state->creg.colors[creg_index].rgba[0];
      rgba[0] = d->ptr[0];
      rgba[1] = d->ptr[1];
      rgba[2] = d->ptr[2];
      rgba[3] = 0xFF;
      d->ptr += 3;
      d->len -= 3;
      sel[0] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0xA0) {  // Set CREG[etc]; 4 byte color.
      if (d->len < 4) {
        return iconvg_error_bad_color;
      }
      uint8_t creg_index = (sel[0] - adjustments[opcode & 0x07]) & 0x3F;
      uint8_t* rgba = &state->creg.colors[creg_index].rgba[0];
      rgba[0] = d->ptr[0];
      rgba[1] = d->ptr[1];
      rgba[2] = d->ptr[2];
      rgba[3] = d->ptr[3];
      d->ptr += 4;
      d->len -= 4;
      sel[0] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0xA8) {  // Set CREG[etc]; 3 byte (indirect) color.
      if (d->len < 3) {
        return iconvg_error_bad_color;
      }
      uint8_t creg_index = (sel[0] - adjustments[opcode & 0x07]) & 0x3F;
      uint8_t* rgba = &state->creg.colors[creg_index].rgba[0];
      uint8_t p[4] = {0};
      uint8_t q[4] = {0};
      iconvg_private_set_one_byte_color(&p[0], &state->custom_palette,
                                        &state->creg, d->ptr[1]);
      iconvg_private_set_one_byte_color(&q[0], &state->custom_palette,
                                        &state->creg, d->ptr[2]);
      uint32_t q_blend = d->ptr[0];
      uint32_t p_blend = 255 - q_blend;
      rgba[0] = (uint8_t)(((p_blend * p[0]) + (q_blend * q[0]) + 128) / 255);
      rgba[1] = (uint8_t)(((p_blend * p[1]) + (q_blend * q[1]) + 128) / 255);
      rgba[2] = (uint8_t)(((p_blend * p[2]) + (q_blend * q[2]) + 128) / 255);
      rgba[3] = (uint8_t)(((p_blend * p[3]) + (q_blend * q[3]) + 128) / 255);
      d->ptr += 3;
      d->len -= 3;
      sel[0] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0xB0) {  // Set NREG[etc]; real number.
      uint8_t nreg_index = (sel[1] - adjustments[opcode & 0x07]) & 0x3F;
      float* num = &state->nreg[nreg_index];
      if (!iconvg_private_decoder__decode_real_number(d, num)) {
        return iconvg_error_bad_number;
      }
      sel[1] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0xB8) {  // Set NREG[etc]; coordinate number.
      uint8_t nreg_index = (sel[1] - adjustments[opcode & 0x07]) & 0x3F;
      float* num = &state->nreg[nreg_index];
      if (!iconvg_private_decoder__decode_coordinate_number(d, num)) {
        return iconvg_error_bad_coordinate;
      }
      sel[1] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0xC0) {  // Set NREG[etc]; zero-to-one number.
      uint8_t nreg_index = (sel[1] - adjustments[opcode & 0x07]) & 0x3F;
      float* num = &state->nreg[nreg_index];
      if (!iconvg_private_decoder__decode_zero_to_one_number(d, num)) {
        return iconvg_error_bad_number;
      }
      sel[1] += ((opcode & 0x07) == 0x07) ? 1 : 0;
      continue;

    } else if (opcode < 0xC7) {  // Switch to the drawing mode.
      uint8_t creg_index = (sel[0] - adjustments[opcode & 0x07]) & 0x3F;
      memcpy(&state->paint_rgba, &state->creg.colors[creg_index],
             sizeof(state->paint_rgba));
      if (iconvg_paint__type(state) == ICONVG_PAINT_TYPE__INVALID) {
        return iconvg_error_invalid_paint_type;
      }
      if (!iconvg_private_decoder__decode_coordinate_number(d, &curr_x) ||
          !iconvg_private_decoder__decode_coordinate_number(d, &curr_y)) {
        return iconvg_error_bad_coordinate;
      }
      double h = (double)state->height_in_pixels;
      c = ((lod[0] <= h) && (h < lod[1])) ? c_arg : &no_op_canvas;
      ICONVG_PRIVATE_TRY((*c->vtable->begin_drawing)(c));
      ICONVG_PRIVATE_TRY(
          (*c->vtable->begin_path)(c,                            //
                                   (curr_x * scale_x) + bias_x,  //
                                   (curr_y * scale_y) + bias_y));
      x1 = curr_x;
      y1 = curr_y;
      goto drawing_mode;

    } else if (opcode < 0xC8) {  // Set Level of Detail bounds.
      float lod0;
      float lod1;
      if (!iconvg_private_decoder__decode_real_number(d, &lod0) ||
          !iconvg_private_decoder__decode_real_number(d, &lod1)) {
        return iconvg_error_bad_number;
      }
      lod[0] = (double)lod0;
      lod[1] = (double)lod1;
      continue;
    }

    return iconvg_error_bad_styling_opcode;
  }

drawing_mode:
  while (true) {
    if (d->len == 0) {
      return iconvg_error_bad_path_unfinished;
    }
    uint8_t opcode = d->ptr[0];
    d->ptr += 1;
    d->len -= 1;

    switch (opcode >> 4) {
      case 0x00:
      case 0x01: {  // 'L' mnemonic: absolute line_to.
        for (int reps = opcode & 0x1F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &curr_x) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &curr_y)) {
            return iconvg_error_bad_coordinate;
          }
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_line_to)(c,                            //
                                         (curr_x * scale_x) + bias_x,  //
                                         (curr_y * scale_y) + bias_y));
          x1 = curr_x;
          y1 = curr_y;
        }
        continue;
      }

      case 0x02:
      case 0x03: {  // 'l' mnemonic: relative line_to.
        for (int reps = opcode & 0x1F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1)) {
            return iconvg_error_bad_coordinate;
          }
          curr_x += x1;
          curr_y += y1;
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_line_to)(c,                            //
                                         (curr_x * scale_x) + bias_x,  //
                                         (curr_y * scale_y) + bias_y));
          x1 = curr_x;
          y1 = curr_y;
        }
        continue;
      }

      case 0x04: {  // 'T' mnemonic: absolute smooth quad_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2)) {
            return iconvg_error_bad_coordinate;
          }
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_quad_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y));
          curr_x = x2;
          curr_y = y2;
          x1 = (2 * curr_x) - x1;
          y1 = (2 * curr_y) - y1;
        }
        continue;
      }

      case 0x05: {  // 't' mnemonic: relative smooth quad_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2)) {
            return iconvg_error_bad_coordinate;
          }
          x2 += curr_x;
          y2 += curr_y;
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_quad_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y));
          curr_x = x2;
          curr_y = y2;
          x1 = (2 * curr_x) - x1;
          y1 = (2 * curr_y) - y1;
        }
        continue;
      }

      case 0x06: {  // 'Q' mnemonic: absolute quad_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2)) {
            return iconvg_error_bad_coordinate;
          }
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_quad_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y));
          curr_x = x2;
          curr_y = y2;
          x1 = (2 * curr_x) - x1;
          y1 = (2 * curr_y) - y1;
        }
        continue;
      }

      case 0x07: {  // 'q' mnemonic: relative quad_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2)) {
            return iconvg_error_bad_coordinate;
          }
          x1 += curr_x;
          y1 += curr_y;
          x2 += curr_x;
          y2 += curr_y;
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_quad_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y));
          curr_x = x2;
          curr_y = y2;
          x1 = (2 * curr_x) - x1;
          y1 = (2 * curr_y) - y1;
        }
        continue;
      }

      case 0x08: {  // 'S' mnemonic: absolute smooth cube_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x3) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y3)) {
            return iconvg_error_bad_coordinate;
          }
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_cube_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y,  //
                                         (x3 * scale_x) + bias_x,  //
                                         (y3 * scale_y) + bias_y));
          curr_x = x3;
          curr_y = y3;
          x1 = (2 * curr_x) - x2;
          y1 = (2 * curr_y) - y2;
        }
        continue;
      }

      case 0x09: {  // 's' mnemonic: relative smooth cube_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x3) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y3)) {
            return iconvg_error_bad_coordinate;
          }
          x2 += curr_x;
          y2 += curr_y;
          x3 += curr_x;
          y3 += curr_y;
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_cube_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y,  //
                                         (x3 * scale_x) + bias_x,  //
                                         (y3 * scale_y) + bias_y));
          curr_x = x3;
          curr_y = y3;
          x1 = (2 * curr_x) - x2;
          y1 = (2 * curr_y) - y2;
        }
        continue;
      }

      case 0x0A: {  // 'C' mnemonic: absolute cube_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x3) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y3)) {
            return iconvg_error_bad_coordinate;
          }
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_cube_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y,  //
                                         (x3 * scale_x) + bias_x,  //
                                         (y3 * scale_y) + bias_y));
          curr_x = x3;
          curr_y = y3;
          x1 = (2 * curr_x) - x2;
          y1 = (2 * curr_y) - y2;
        }
        continue;
      }

      case 0x0B: {  // 'c' mnemonic: relative cube_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y2) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x3) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y3)) {
            return iconvg_error_bad_coordinate;
          }
          x1 += curr_x;
          y1 += curr_y;
          x2 += curr_x;
          y2 += curr_y;
          x3 += curr_x;
          y3 += curr_y;
          ICONVG_PRIVATE_TRY(
              (*c->vtable->path_cube_to)(c,                        //
                                         (x1 * scale_x) + bias_x,  //
                                         (y1 * scale_y) + bias_y,  //
                                         (x2 * scale_x) + bias_x,  //
                                         (y2 * scale_y) + bias_y,  //
                                         (x3 * scale_x) + bias_x,  //
                                         (y3 * scale_y) + bias_y));
          curr_x = x3;
          curr_y = y3;
          x1 = (2 * curr_x) - x2;
          y1 = (2 * curr_y) - y2;
        }
        continue;
      }

      case 0x0C: {  // 'A' mnemonic: absolute arc_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          float x0 = curr_x;
          float y0 = curr_y;
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1) ||
              !iconvg_private_decoder__decode_zero_to_one_number(d, &x2) ||
              !iconvg_private_decoder__decode_natural_number(d, &flags) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &curr_x) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &curr_y)) {
            return iconvg_error_bad_coordinate;
          }
          ICONVG_PRIVATE_TRY(iconvg_private_path_arc_to(
              c, scale_x, bias_x, scale_y, bias_y, x0, y0, x1, y1, x2,
              flags & 0x01, flags & 0x02, curr_x, curr_y));
          x1 = curr_x;
          y1 = curr_y;
        }
        continue;
      }

      case 0x0D: {  // 'a' mnemonic: relative arc_to.
        for (int reps = opcode & 0x0F; reps >= 0; reps--) {
          float x0 = curr_x;
          float y0 = curr_y;
          if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y1) ||
              !iconvg_private_decoder__decode_zero_to_one_number(d, &x2) ||
              !iconvg_private_decoder__decode_natural_number(d, &flags) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &x3) ||
              !iconvg_private_decoder__decode_coordinate_number(d, &y3)) {
            return iconvg_error_bad_coordinate;
          }
          curr_x += x3;
          curr_y += y3;
          ICONVG_PRIVATE_TRY(iconvg_private_path_arc_to(
              c, scale_x, bias_x, scale_y, bias_y, x0, y0, x1, y1, x2,
              flags & 0x01, flags & 0x02, curr_x, curr_y));
          x1 = curr_x;
          y1 = curr_y;
        }
        continue;
      }
    }

    switch (opcode) {
      case 0xE1: {  // 'z' mnemonic: close_path.
        ICONVG_PRIVATE_TRY((*c->vtable->end_path)(c));
        ICONVG_PRIVATE_TRY((*c->vtable->end_drawing)(c, state));
        goto styling_mode;
      }

      case 0xE2: {  // 'z; M' mnemonics: close_path; absolute move_to.
        ICONVG_PRIVATE_TRY((*c->vtable->end_path)(c));
        if (!iconvg_private_decoder__decode_coordinate_number(d, &curr_x) ||
            !iconvg_private_decoder__decode_coordinate_number(d, &curr_y)) {
          return iconvg_error_bad_coordinate;
        }
        ICONVG_PRIVATE_TRY(
            (*c->vtable->begin_path)(c,                            //
                                     (curr_x * scale_x) + bias_x,  //
                                     (curr_y * scale_y) + bias_y));
        x1 = curr_x;
        y1 = curr_y;
        continue;
      }

      case 0xE3: {  // 'z; m' mnemonics: close_path; relative move_to.
        ICONVG_PRIVATE_TRY((*c->vtable->end_path)(c));
        if (!iconvg_private_decoder__decode_coordinate_number(d, &x1) ||
            !iconvg_private_decoder__decode_coordinate_number(d, &y1)) {
          return iconvg_error_bad_coordinate;
        }
        curr_x += x1;
        curr_y += y1;
        ICONVG_PRIVATE_TRY(
            (*c->vtable->begin_path)(c,                            //
                                     (curr_x * scale_x) + bias_x,  //
                                     (curr_y * scale_y) + bias_y));
        x1 = curr_x;
        y1 = curr_y;
        continue;
      }

      case 0xE6: {  // 'H' mnemonic: absolute horizontal line_to.
        if (!iconvg_private_decoder__decode_coordinate_number(d, &curr_x)) {
          return iconvg_error_bad_coordinate;
        }
        ICONVG_PRIVATE_TRY(
            (*c->vtable->path_line_to)(c,                            //
                                       (curr_x * scale_x) + bias_x,  //
                                       (curr_y * scale_y) + bias_y));
        x1 = curr_x;
        y1 = curr_y;
        continue;
      }

      case 0xE7: {  // 'h' mnemonic: relative horizontal line_to.
        if (!iconvg_private_decoder__decode_coordinate_number(d, &x1)) {
          return iconvg_error_bad_coordinate;
        }
        curr_x += x1;
        ICONVG_PRIVATE_TRY(
            (*c->vtable->path_line_to)(c,                            //
                                       (curr_x * scale_x) + bias_x,  //
                                       (curr_y * scale_y) + bias_y));
        x1 = curr_x;
        y1 = curr_y;
        continue;
      }

      case 0xE8: {  // 'V' mnemonic: absolute vertical line_to.
        if (!iconvg_private_decoder__decode_coordinate_number(d, &curr_y)) {
          return iconvg_error_bad_coordinate;
        }
        ICONVG_PRIVATE_TRY(
            (*c->vtable->path_line_to)(c,                            //
                                       (curr_x * scale_x) + bias_x,  //
                                       (curr_y * scale_y) + bias_y));
        x1 = curr_x;
        y1 = curr_y;
        continue;
      }

      case 0xE9: {  // 'v' mnemonic: relative vertical line_to.
        if (!iconvg_private_decoder__decode_coordinate_number(d, &y1)) {
          return iconvg_error_bad_coordinate;
        }
        curr_y += y1;
        ICONVG_PRIVATE_TRY(
            (*c->vtable->path_line_to)(c,                            //
                                       (curr_x * scale_x) + bias_x,  //
                                       (curr_y * scale_y) + bias_y));
        x1 = curr_x;
        y1 = curr_y;
        continue;
      }
    }

    return iconvg_error_bad_drawing_opcode;
  }
  return iconvg_private_internal_error_unreachable;
}

// ----

const char*  //
iconvg_decode_viewbox(iconvg_rectangle_f32* dst_viewbox,
                      const uint8_t* src_ptr,
                      size_t src_len) {
  iconvg_private_decoder d;
  d.ptr = src_ptr;
  d.len = src_len;

  if (!iconvg_private_decoder__decode_magic_identifier(&d)) {
    return iconvg_error_bad_magic_identifier;
  }
  uint32_t num_metadata_chunks;
  if (!iconvg_private_decoder__decode_natural_number(&d,
                                                     &num_metadata_chunks)) {
    return iconvg_error_bad_metadata;
  }

  bool use_default_viewbox = true;
  int32_t previous_metadata_id = -1;
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
    } else if (previous_metadata_id >= ((int32_t)metadata_id)) {
      return iconvg_error_bad_metadata_id_order;
    }

    if (metadata_id == 0) {  // MID 0 (ViewBox).
      use_default_viewbox = false;
      iconvg_rectangle_f32 r;
      if (!iconvg_private_decoder__decode_metadata_viewbox(&chunk, &r) ||
          (chunk.len != 0)) {
        return iconvg_error_bad_metadata_viewbox;
      } else if (dst_viewbox) {
        *dst_viewbox = r;
      }
    }

    iconvg_private_decoder__skip_to_the_end(&chunk);
    iconvg_private_decoder__advance_to_ptr(&d, chunk.ptr);
    previous_metadata_id = ((int32_t)metadata_id);
  }

  if (use_default_viewbox && dst_viewbox) {
    *dst_viewbox = iconvg_private_default_viewbox();
  }
  return NULL;
}

static const char*  //
iconvg_private_decode(iconvg_canvas* c,
                      iconvg_rectangle_f32 r,
                      iconvg_private_decoder* d,
                      const iconvg_decode_options* options) {
  iconvg_paint state;
  state.viewbox = iconvg_private_default_viewbox();
  if (options && options->height_in_pixels.has_value) {
    state.height_in_pixels = options->height_in_pixels.value;
  } else {
    double h = iconvg_rectangle_f32__height_f64(&r);
    // The 0x10_0000 = (1 << 20) = 1048576 limit is arbitrary but it's less
    // than MAX_INT32 and also ensures that conversion between integer and
    // float or double is lossless.
    if (h <= 0x100000) {
      state.height_in_pixels = (int64_t)h;
    } else {
      state.height_in_pixels = 0x100000;
    }
  }
  memset(&state.paint_rgba, 0, sizeof(state.paint_rgba));
  memcpy(&state.custom_palette, &iconvg_private_default_palette,
         sizeof(state.custom_palette));

  if (!iconvg_private_decoder__decode_magic_identifier(d)) {
    return iconvg_error_bad_magic_identifier;
  }
  uint32_t num_metadata_chunks;
  if (!iconvg_private_decoder__decode_natural_number(d, &num_metadata_chunks)) {
    return iconvg_error_bad_metadata;
  }

  int32_t previous_metadata_id = -1;
  for (; num_metadata_chunks > 0; num_metadata_chunks--) {
    uint32_t chunk_length;
    if (!iconvg_private_decoder__decode_natural_number(d, &chunk_length) ||
        (chunk_length > d->len)) {
      return iconvg_error_bad_metadata;
    }
    iconvg_private_decoder chunk =
        iconvg_private_decoder__limit_u32(d, chunk_length);
    uint32_t metadata_id;
    if (!iconvg_private_decoder__decode_natural_number(&chunk, &metadata_id)) {
      return iconvg_error_bad_metadata;
    } else if (previous_metadata_id >= ((int32_t)metadata_id)) {
      return iconvg_error_bad_metadata_id_order;
    }

    switch (metadata_id) {
      case 0:  // MID 0 (ViewBox).
        if (!iconvg_private_decoder__decode_metadata_viewbox(&chunk,
                                                             &state.viewbox) ||
            (chunk.len != 0)) {
          return iconvg_error_bad_metadata_viewbox;
        }
        break;

      case 1:  // MID 1 (Suggested Palette).
        if (!iconvg_private_decoder__decode_metadata_suggested_palette(
                &chunk, &state.custom_palette) ||
            (chunk.len != 0)) {
          return iconvg_error_bad_metadata_suggested_palette;
        }
        break;

      default:
        return iconvg_error_bad_metadata;
    }

    iconvg_private_decoder__skip_to_the_end(&chunk);
    iconvg_private_decoder__advance_to_ptr(d, chunk.ptr);
    previous_metadata_id = ((int32_t)metadata_id);
  }

  ICONVG_PRIVATE_TRY((*c->vtable->on_metadata_viewbox)(c, state.viewbox));
  ICONVG_PRIVATE_TRY(
      (*c->vtable->on_metadata_suggested_palette)(c, &state.custom_palette));

  if (options && options->palette) {
    memcpy(&state.custom_palette, options->palette,
           sizeof(state.custom_palette));
  }

  memcpy(&state.creg, &state.custom_palette, sizeof(state.creg));
  memset(&state.nreg[0], 0, sizeof(state.nreg));
  state.s2d_scale_x = +1.0;
  state.s2d_bias_x = +0.0;
  state.s2d_scale_y = +1.0;
  state.s2d_bias_y = +0.0;
  state.d2s_scale_x = +1.0;
  state.d2s_bias_x = +0.0;
  state.d2s_scale_y = +1.0;
  state.d2s_bias_y = +0.0;

  return iconvg_private_execute_bytecode(c, r, d, &state);
}

const char*  //
iconvg_decode(iconvg_canvas* dst_canvas,
              iconvg_rectangle_f32 dst_rect,
              const uint8_t* src_ptr,
              size_t src_len,
              const iconvg_decode_options* options) {
  iconvg_canvas fallback_canvas = iconvg_make_broken_canvas(NULL);
  if (!dst_canvas || !dst_canvas->vtable) {
    dst_canvas = &fallback_canvas;
  }

  if (dst_canvas->vtable->sizeof__iconvg_canvas_vtable !=
      sizeof(iconvg_canvas_vtable)) {
    // If we want to support multiple library versions (with dynamic linking),
    // we could detect older versions here (with smaller vtable sizes) and
    // substitute in an adapter implementation.
    return iconvg_error_unsupported_vtable;
  }

  iconvg_private_decoder d;
  d.ptr = src_ptr;
  d.len = src_len;

  const char* err_msg =
      (*dst_canvas->vtable->begin_decode)(dst_canvas, dst_rect);
  if (!err_msg) {
    err_msg = iconvg_private_decode(dst_canvas, dst_rect, &d, options);
  }
  return (*dst_canvas->vtable->end_decode)(dst_canvas, err_msg, src_len - d.len,
                                           d.len);
}
