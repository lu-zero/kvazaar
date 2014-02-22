/*****************************************************************************
 * This file is part of Kvazaar HEVC encoder.
 *
 * Copyright (C) 2013-2014 Tampere University of Technology and others (see
 * COPYING file).
 *
 * Kvazaar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Kvazaar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kvazaar.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

/*
 * \file
 */

#include "search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "bitstream.h"
#include "picture.h"
#include "intra.h"
#include "inter.h"
#include "filter.h"


// Temporarily for debugging.
#define USE_INTRA_IN_P 1
//#define RENDER_CU encoder->frame==2
#define RENDER_CU 0
#define SEARCH_MV_FULL_RADIUS 0

#define IN_FRAME(x, y, width, height, block_width, block_height) \
  ((x) >= 0 && (y) >= 0 \
  && (x) + (block_width) <= (width) \
  && (y) + (block_height) <= (height))

/**
 * This is used in the hexagon_search to select 3 points to search.
 *
 * The start of the hexagonal pattern has been repeated at the end so that
 * the indices between 1-6 can be used as the start of a 3-point list of new
 * points to search.
 *
 *   6 o-o 1 / 7
 *    /   \
 * 5 o  0  o 2 / 8
 *    \   /
 *   4 o-o 3
 */
const vector2d large_hexbs[10] = {
  { 0, 0 },
  { 1, -2 }, { 2, 0 }, { 1, 2 }, { -1, 2 }, { -2, 0 }, { -1, -2 },
  { 1, -2 }, { 2, 0 }
};

/**
 * This is used as the last step of the hexagon search.
 */
const vector2d small_hexbs[5] = {
  { 0, 0 },
  { -1, -1 }, { -1, 0 }, { 1, 0 }, { 1, 1 }
};

static int calc_mvd_cost(int x, int y, const vector2d *pred)
{
  int cost = 0;

  // Get the absolute difference vector and count the bits.
  x = abs(abs(x) - abs(pred->x));
  y = abs(abs(y) - abs(pred->y));
  while (x >>= 1) {
    ++cost;
  }
  while (y >>= 1) {
    ++cost;
  }

  // I don't know what is a good cost function for this. It probably doesn't
  // have to aproximate the actual cost of encoding the vector, but it's a
  // place to start.

  // Add two for quarter pixel resolution and multiply by two for Exp-Golomb.
  return (cost ? (cost + 2) << 1 : 0);
}

/**
 * \brief Do motion search using the HEXBS algorithm.
 *
 * \param depth      log2 depth of the search
 * \param pic        Picture motion vector is searched for.
 * \param ref        Picture motion vector is searched from.
 * \param orig       Top left corner of the searched for block.
 * \param mv_in_out  Predicted mv in and best out. Quarter pixel precision.
 *
 * \returns  Cost of the motion vector.
 *
 * Motion vector is searched by first searching iteratively with the large
 * hexagon pattern until the best match is at the center of the hexagon.
 * As a final step a smaller hexagon is used to check the adjacent pixels.
 *
 * If a non 0,0 predicted motion vector predictor is given as mv_in_out,
 * the 0,0 vector is also tried. This is hoped to help in the case where
 * the predicted motion vector is way off. In the future even more additional
 * points like 0,0 might be used, such as vectors from top or left.
 */
static unsigned hexagon_search(unsigned depth,
                               const picture *pic, const picture *ref,
                               const vector2d *orig, vector2d *mv_in_out)
{
  vector2d mv = { mv_in_out->x >> 2, mv_in_out->y >> 2 };
  int block_width = CU_WIDTH_FROM_DEPTH(depth);
  unsigned best_cost = UINT32_MAX;
  unsigned i;
  unsigned best_index = 0; // Index of large_hexbs or finally small_hexbs.

  // Search the initial 7 points of the hexagon.
  for (i = 0; i < 7; ++i) {
    const vector2d *pattern = &large_hexbs[i];
    unsigned cost = calc_sad(pic, ref, orig->x, orig->y,
                             orig->x + mv.x + pattern->x, orig->y + mv.y + pattern->y,
                             block_width, block_width);
    cost += calc_mvd_cost(mv.x + pattern->x, mv.y + pattern->y, mv_in_out);

    if (cost < best_cost) {
      best_cost = cost;
      best_index = i;
    }
  }

  // Try the 0,0 vector.
  if (!(mv.x == 0 && mv.y == 0)) {
    unsigned cost = calc_sad(pic, ref, orig->x, orig->y,
                             orig->x, orig->y,
                             block_width, block_width);
    cost += calc_mvd_cost(0, 0, mv_in_out);

    // If the 0,0 is better, redo the hexagon around that point.
    if (cost < best_cost) {
      best_cost = cost;
      best_index = 0;
      mv.x = 0;
      mv.y = 0;

      for (i = 1; i < 7; ++i) {
        const vector2d *pattern = &large_hexbs[i];
        unsigned cost = calc_sad(pic, ref, orig->x, orig->y,
                                 orig->x + pattern->x,
                                 orig->y + pattern->y,
                                 block_width, block_width);
        cost += calc_mvd_cost(pattern->x, pattern->y, mv_in_out);

        if (cost < best_cost) {
          best_cost = cost;
          best_index = i;
        }
      }
    }
  }

  // Iteratively search the 3 new points around the best match, until the best
  // match is in the center.
  while (best_index != 0) {
    unsigned start; // Starting point of the 3 offsets to be searched.
    if (best_index == 1) {
      start = 6;
    } else if (best_index == 8) {
      start = 1;
    } else {
      start = best_index - 1;
    }

    // Move the center to the best match.
    mv.x += large_hexbs[best_index].x;
    mv.y += large_hexbs[best_index].y;
    best_index = 0;

    // Iterate through the next 3 points.
    for (i = 0; i < 3; ++i) {
      const vector2d *offset = &large_hexbs[start + i];
      unsigned cost = calc_sad(pic, ref, orig->x, orig->y,
                               orig->x + mv.x + offset->x,
                               orig->y + mv.y + offset->y,
                               block_width, block_width);
      cost += calc_mvd_cost(mv.x + offset->x, mv.y + offset->y, mv_in_out);

      if (cost < best_cost) {
        best_cost = cost;
        best_index = start + i;
      }
      ++offset;
    }
  }

  // Move the center to the best match.
  mv.x += large_hexbs[best_index].x;
  mv.y += large_hexbs[best_index].y;
  best_index = 0;

  // Do the final step of the search with a small pattern.
  for (i = 1; i < 5; ++i) {
    const vector2d *offset = &small_hexbs[i];
    unsigned cost = calc_sad(pic, ref, orig->x, orig->y,
                             orig->x + mv.x + offset->x,
                             orig->y + mv.y + offset->y,
                             block_width, block_width);
    cost += calc_mvd_cost(mv.x + offset->x, mv.y + offset->y, mv_in_out);

    if (cost > 0 && cost < best_cost) {
      best_cost = cost;
      best_index = i;
    }
  }

  // Adjust the movement vector according to the final best match.
  mv.x += small_hexbs[best_index].x;
  mv.y += small_hexbs[best_index].y;

  // Return final movement vector in quarter-pixel precision.
  mv_in_out->x = mv.x << 2;
  mv_in_out->y = mv.y << 2;

  return best_cost;
}

#if SEARCH_MV_FULL_RADIUS
static unsigned search_mv_full(unsigned depth,
                               const picture *pic, const picture *ref,
                               const vector2d *orig, vector2d *mv_in_out)
{
  vector2d mv = { mv_in_out->x >> 2, mv_in_out->y >> 2 };
  int block_width = CU_WIDTH_FROM_DEPTH(depth);
  unsigned best_cost = UINT32_MAX;
  int x, y;
  vector2d min_mv, max_mv;

  /*if (abs(mv.x) > SEARCH_MV_FULL_RADIUS || abs(mv.y) > SEARCH_MV_FULL_RADIUS) {
    best_cost = calc_sad(pic, ref, orig->x, orig->y,
                         orig->x, orig->y,
                         block_width, block_width);
    mv.x = 0;
    mv.y = 0;
  }*/

  min_mv.x = mv.x - SEARCH_MV_FULL_RADIUS;
  min_mv.y = mv.y - SEARCH_MV_FULL_RADIUS;
  max_mv.x = mv.x + SEARCH_MV_FULL_RADIUS;
  max_mv.y = mv.y + SEARCH_MV_FULL_RADIUS;

  for (y = min_mv.y; y < max_mv.y; ++y) {
    for (x = min_mv.x; x < max_mv.x; ++x) {
      unsigned cost = calc_sad(pic, ref, orig->x, orig->y,
                               orig->x + x,
                               orig->y + y,
                               block_width, block_width);
      cost += calc_mvd_cost(x, y, mv_in_out);
      if (cost < best_cost) {
        best_cost = cost;
        mv.x = x;
        mv.y = y;
      }
    }
  }

  mv_in_out->x = mv.x << 2;
  mv_in_out->y = mv.y << 2;

  return best_cost;
}
#endif

static void search_inter(encoder_control *encoder, uint16_t x_ctb,
                         uint16_t y_ctb, uint8_t depth)
{
  picture *cur_pic = encoder->in.cur_pic;
  uint32_t ref_idx = 0;
  cu_info *cur_cu = &cur_pic->cu_array[depth][x_ctb + y_ctb * (encoder->in.width_in_lcu << MAX_DEPTH)];
  cur_cu->inter.cost = UINT_MAX;

  for (ref_idx = 0; ref_idx < encoder->ref->used_size; ref_idx++) {
    picture *ref_pic = encoder->ref->pics[ref_idx];
    unsigned width_in_scu = NO_SCU_IN_LCU(ref_pic->width_in_lcu);
    cu_info *ref_cu = &ref_pic->cu_array[MAX_DEPTH][y_ctb * width_in_scu + x_ctb];
    uint32_t temp_cost = (int)(g_lambda_cost[encoder->QP] * ref_idx);
    vector2d orig, mv;
    orig.x = x_ctb * CU_MIN_SIZE_PIXELS;
    orig.y = y_ctb * CU_MIN_SIZE_PIXELS;
    mv.x = 0;
    mv.y = 0;
    if (ref_cu->type == CU_INTER) {
      mv.x = ref_cu->inter.mv[0];
      mv.y = ref_cu->inter.mv[1];
    }

  #if SEARCH_MV_FULL_RADIUS
    cur_cu->inter.cost = search_mv_full(depth, cur_pic, ref_pic, &orig, &mv);
  #else
    temp_cost += hexagon_search(depth, cur_pic, ref_pic, &orig, &mv);
  #endif
    if(temp_cost < cur_cu->inter.cost) {
      cur_cu->inter.mv_ref = ref_idx;
      cur_cu->inter.mv_dir = 1;
      cur_cu->inter.mv[0] = (int16_t)mv.x;
      cur_cu->inter.mv[1] = (int16_t)mv.y;
      cur_cu->inter.cost = temp_cost;
    }
  }

}

static void search_intra(encoder_control *encoder, uint16_t x_ctb,
                         uint16_t y_ctb, uint8_t depth)
{
  int16_t x = x_ctb * (LCU_WIDTH >> MAX_DEPTH);
  int16_t y = y_ctb * (LCU_WIDTH >> MAX_DEPTH);
  picture *cur_pic = encoder->in.cur_pic;
  uint8_t width = LCU_WIDTH >> depth;
  cu_info *cur_cu = &cur_pic->cu_array[depth][x_ctb + y_ctb * (encoder->in.width_in_lcu << MAX_DEPTH)];

  // INTRAPREDICTION
  pixel pred[LCU_WIDTH * LCU_WIDTH + 1];
  pixel rec[(LCU_WIDTH * 2 + 1) * (LCU_WIDTH * 2 + 1)];
  pixel *recShift = &rec[(LCU_WIDTH >> (depth)) * 2 + 8 + 1];

  // Build reconstructed block to use in prediction with extrapolated borders
  intra_build_reference_border(cur_pic, cur_pic->y_data,
                               x, y,
                               (int16_t)width * 2 + 8, rec, (int16_t)width * 2 + 8, 0);
  cur_cu->intra[0].mode = (int8_t)intra_prediction(encoder->in.cur_pic->y_data,
      encoder->in.width, recShift, width * 2 + 8, x, y,
      width, pred, width, &cur_cu->intra[0].cost);
  cur_cu->part_size = SIZE_2Nx2N;

  // Do search for NxN split.
  if (0 && depth == MAX_DEPTH) { //TODO: reactivate NxN when _something_ is done to make it better
    // Save 2Nx2N information to compare with NxN.
    int nn_cost = cur_cu->intra[0].cost;
    int8_t nn_mode = cur_cu->intra[0].mode;
    int i;
    int cost = (int)(g_lambda_cost[encoder->QP] * 4.5);  // round to nearest
    static vector2d offsets[4] = {{0,0},{1,0},{0,1},{1,1}};
    width = 4;
    recShift = &rec[width * 2 + 8 + 1];

    for (i = 0; i < 4; ++i) {
      int x_pos = x + offsets[i].x * width;
      int y_pos = y + offsets[i].y * width;
      intra_build_reference_border(cur_pic, cur_pic->y_data,
                                   x_pos, y_pos,
                                   (int16_t)width * 2 + 8, rec, (int16_t)width * 2 + 8, 0);
      cur_cu->intra[i].mode = (int8_t)intra_prediction(encoder->in.cur_pic->y_data,
          encoder->in.width, recShift, width * 2 + 8, (int16_t)x_pos, (int16_t)y_pos,
          width, pred, width, &cur_cu->intra[i].cost);
      cost += cur_cu->intra[i].cost;
    }

    // Choose between 2Nx2N and NxN.
    if (nn_cost <= cost) {
      cur_cu->intra[0].cost = nn_cost;
      cur_cu->intra[0].mode = nn_mode;
    } else {
      cur_cu->intra[0].cost = cost;
      cur_cu->part_size = SIZE_NxN;
    }
  }
}

/**
 * \brief Search best modes at each depth for the whole picture.
 *
 * This function fills the cur_pic->cu_array of the current picture
 * with the best mode and it's cost for each CU at each depth for the whole
 * frame.
 */
void search_tree(encoder_control *encoder,
                 int x, int y, uint8_t depth)
{
  int cu_width = LCU_WIDTH >> depth;
  uint16_t x_ctb = (uint16_t)x / (LCU_WIDTH >> MAX_DEPTH);
  uint16_t y_ctb = (uint16_t)y / (LCU_WIDTH >> MAX_DEPTH);

  // Stop recursion if the CU is completely outside the frame.
  if (x >= encoder->in.width || y >= encoder->in.height) {
    return;
  }

  // If the CU is partially outside the frame, split.
  if (x + cu_width > encoder->in.width ||
      y + cu_width > encoder->in.height)
  {
    int half_cu = cu_width / 2;

    search_tree(encoder, x,           y,           depth + 1);
    search_tree(encoder, x + half_cu, y,           depth + 1);
    search_tree(encoder, x,           y + half_cu, depth + 1);
    search_tree(encoder, x + half_cu, y + half_cu, depth + 1);

    return;
  }

  // CU is completely inside the frame, so search for best prediction mode at
  // this depth.
  {
    picture *cur_pic = encoder->in.cur_pic;

    if (cur_pic->slicetype != SLICE_I &&
        depth >= MIN_INTER_SEARCH_DEPTH &&
        depth <= MAX_INTER_SEARCH_DEPTH)
    {
      search_inter(encoder, x_ctb, y_ctb, depth);
    }

    if (depth >= MIN_INTRA_SEARCH_DEPTH &&
        depth <= MAX_INTRA_SEARCH_DEPTH)
    {
      search_intra(encoder, x_ctb, y_ctb, depth);
    }
  }

  // Recurse to max search depth.
  if (depth < MAX_INTRA_SEARCH_DEPTH && depth < MAX_INTER_SEARCH_DEPTH) {
    int half_cu = cu_width / 2;;

    search_tree(encoder, x,           y,           depth + 1);
    search_tree(encoder, x + half_cu, y,           depth + 1);
    search_tree(encoder, x,           y + half_cu, depth + 1);
    search_tree(encoder, x + half_cu, y + half_cu, depth + 1);
  }
}

/**
 * \brief
 */
uint32_t search_best_mode(encoder_control *encoder,
                          uint16_t x_ctb, uint16_t y_ctb, uint8_t depth)
{
  cu_info *cur_cu = &encoder->in.cur_pic->cu_array[depth]
                     [x_ctb + y_ctb * (encoder->in.width_in_lcu << MAX_DEPTH)];
  uint32_t best_intra_cost = cur_cu->intra[0].cost;
  uint32_t best_inter_cost = cur_cu->inter.cost;
  uint32_t lambda_cost = (int)(4.5 * g_lambda_cost[encoder->QP]); //TODO: Correct cost calculation

  if (depth < MAX_INTRA_SEARCH_DEPTH && depth < MAX_INTER_SEARCH_DEPTH) {
    uint32_t cost = lambda_cost;
    uint8_t change = 1 << (MAX_DEPTH - 1 - depth);
    cost += search_best_mode(encoder, x_ctb,          y_ctb,          depth + 1);
    cost += search_best_mode(encoder, x_ctb + change, y_ctb,          depth + 1);
    cost += search_best_mode(encoder, x_ctb,          y_ctb + change, depth + 1);
    cost += search_best_mode(encoder, x_ctb + change, y_ctb + change, depth + 1);

    if (cost < best_intra_cost && cost < best_inter_cost)
    {
      // Better value was found at a lower level.
      return cost;
    }
  }

  // If search hasn't been peformed at all for this block, the cost will be
  // max value, so it is safe to just compare costs. It just has to be made
  // sure that no value overflows.
  if (best_inter_cost <= best_intra_cost) {
    inter_set_block(encoder->in.cur_pic, x_ctb, y_ctb, depth, cur_cu);
    return best_inter_cost;
  } else {
    intra_set_block_mode(encoder->in.cur_pic, x_ctb, y_ctb, depth,
        cur_cu->intra[0].mode, cur_cu->part_size);
    return best_intra_cost;
  }
}


/**
 * \brief
 */
void search_slice_data(encoder_control *encoder)
{
  int16_t x_lcu, y_lcu;

  // Initialize the costs in the cu-array used for searching.
  {
    int d, x_cu, y_cu;

    for (y_cu = 0; y_cu < encoder->in.height / CU_MIN_SIZE_PIXELS; ++y_cu) {
      for (x_cu = 0; x_cu < encoder->in.width / CU_MIN_SIZE_PIXELS; ++x_cu) {
        for (d = 0; d <= MAX_DEPTH; ++d) {
          picture *cur_pic = encoder->in.cur_pic;
          cu_info *cur_cu = &cur_pic->cu_array[d][x_cu + y_cu * (encoder->in.width_in_lcu << MAX_DEPTH)];
          cur_cu->intra[0].cost = UINT32_MAX;
          cur_cu->inter.cost = UINT32_MAX;
        }
      }
    }
  }

  // Loop through every LCU in the slice
  for (y_lcu = 0; y_lcu < encoder->in.height_in_lcu; y_lcu++) {
    for (x_lcu = 0; x_lcu < encoder->in.width_in_lcu; x_lcu++) {
      uint8_t depth = 0;

      // Recursive function for looping through all the sub-blocks
      search_tree(encoder, x_lcu * LCU_WIDTH, y_lcu * LCU_WIDTH, depth);

      // Decide actual coding modes
      search_best_mode(encoder, x_lcu << MAX_DEPTH, y_lcu << MAX_DEPTH, depth);

      encode_block_residual(encoder, x_lcu << MAX_DEPTH, y_lcu << MAX_DEPTH, depth);

    }
  }
}
