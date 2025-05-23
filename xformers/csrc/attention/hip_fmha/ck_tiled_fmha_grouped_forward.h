/*
 * Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include <algorithm>
#include "ck_tiled_fmha_fwd_setting.h"
#include "ck_tiled_fmha_fwd_splitkv_smallq_selector.h"
#include "ck_tiled_fmha_grouped_forward_dispatch.h"
#include "ck_tiled_fmha_grouped_forward_splitkv_dispatch.h"
#include "ck_tiled_fmha_grouped_forward_splitkv_smallq_dispatch.h"
#include "ck_tiled_fmha_seqlen_q_switch.h"

template <
    typename ScalarType,
    bool kHasMask,
    bool kHasBias,
    bool kHasDropout,
    ck_tile::index_t MaxK>
void run_grouped_forward_mask_bias_dropout_dispatch(
    GroupedForwardParams& param,
    hipStream_t stream) {
  // currently split-kv implementation does not support:
  // (*) dropout
  // (*) head dimension > 256
  if constexpr (!kHasDropout) {
    if (param.use_split_kv && MaxK <= 256) {
      if constexpr (MaxK <= 256) {
        if (use_splitkv_smallq(
                param.max_seqlen_q, std::max(param.K, param.Kv))) {
          grouped_forward_splitkv_smallq_mask_bias_dropout_dispatch<
              ScalarType,
              kHasMask,
              kHasBias,
              MaxK>::Run(param, stream);
        } else {
          FMHA_FWD_SEQLEN_Q_SWITCH(param.max_seqlen_q, MaxSeqlenQ, [&] {
            grouped_forward_splitkv_mask_bias_dropout_dispatch<
                ScalarType,
                kHasMask,
                kHasBias,
                MaxK,
                MaxSeqlenQ>::Run(param, stream);
          });
        }
      } else {
        // Unreachable. Do not instantiate split-kv pipelines with head
        // dimension > 256
      }
    } else {
      if (get_fmha_fwd_mtile(param.num_batches, param.Hq, param.max_seqlen_q) ==
          128)
        grouped_forward_mask_bias_dropout_dispatch<
            ScalarType,
            kHasMask,
            kHasBias,
            kHasDropout,
            MaxK,
            128>::Run(param, stream);
      else
        grouped_forward_mask_bias_dropout_dispatch<
            ScalarType,
            kHasMask,
            kHasBias,
            kHasDropout,
            MaxK,
            64>::Run(param, stream);
    }
  } else {
    // at present, dropout of fwd kernel requires 32x32 WarpTile
    grouped_forward_mask_bias_dropout_dispatch<
        ScalarType,
        kHasMask,
        kHasBias,
        kHasDropout,
        MaxK,
        128>::Run(param, stream);
  }
};
