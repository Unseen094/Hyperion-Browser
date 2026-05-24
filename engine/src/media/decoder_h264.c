#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---- H.264 / AVC Software Decoder -----------------------------------------
// NAL unit parser, slice header, CAVLC entropy decode,
// motion compensation, deblocking filter.

#define MAX_REF_FRAMES 16
#define MAX_MB_PER_FRAME 8160
#define MB_SIZE 16

typedef struct h264_pps {
    int pic_parameter_set_id;
    int seq_parameter_set_id;
    int entropy_coding_mode_flag;
    int num_slice_groups_minus1;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int weighted_pred_flag;
    int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int deblocking_filter_control_present_flag;
    int disable_deblocking_filter_idc;
    int chroma_qp_index_offset;
} h264_pps;

typedef struct h264_sps {
    int profile_idc;
    int level_idc;
    int seq_parameter_set_id;
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb_minus4;
    int max_num_ref_frames;
    int gaps_in_frame_num_value_allowed_flag;
    int pic_width_in_mbs_minus1;
    int pic_height_in_map_units_minus1;
    int frame_mbs_only_flag;
    int mb_adaptive_frame_field_flag;
    int direct_8x8_inference_flag;
    int frame_cropping_flag;
    int frame_crop_left_offset, frame_crop_right_offset;
    int frame_crop_top_offset, frame_crop_bottom_offset;
    int vui_parameters_present_flag;
    int chroma_format_idc;
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int width, height;
    int mb_width, mb_height;
} h264_sps;

typedef struct h264_slice_header {
    int first_mb_in_slice;
    int slice_type;
    int pic_parameter_set_id;
    int colour_plane_id;
    int frame_num;
    int field_pic_flag;
    int bottom_field_flag;
    int idr_pic_id;
    int pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom;
    int delta_pic_order_cnt[2];
    int sp_for_switch_flag;
    int slice_qp_delta;
    int ref_pic_list_modification_flag_l0;
    int ref_pic_list_modification_flag_l1;
    int cabac_init_idc;
    int slice_qs_delta;
    int chroma_qp_index_offset;
    int deblocking_filter_override_flag;
    int disable_deblocking_filter_idc;
    int slice_alpha_c0_offset_div2;
    int slice_beta_offset_div2;
    int num_ref_idx_active_override_flag;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int weighted_pred_flag;
    int weighted_bipred_idc;
    int direct_spatial_mv_pred_flag;
    int cabac_init_flag;
} h264_slice_header;

typedef struct h264_mb {
    int mb_type;
    int mb_x, mb_y;
    int16_t mv_l0[4][2];
    int16_t mv_l1[4][2];
    int ref_idx_l0[4];
    int ref_idx_l1[4];
    int intra_pred_mode;
    int16_t coeff_luma[16][16];
    int16_t coeff_chroma_cb[16][4];
    int16_t coeff_chroma_cr[16][4];
    int qp_y;
    int cbp;
} h264_mb;

struct h264_decoder {
    int width, height;
    int mb_width, mb_height;
    int bit_depth;

    // SPS/PPS state
    h264_sps sps;
    h264_pps pps;
    h264_slice_header slice;

    // Decoded frames (YUV 4:2:0)
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
    int y_stride, uv_stride;

    // Reference frames
    uint8_t *ref_y[MAX_REF_FRAMES];
    uint8_t *ref_u[MAX_REF_FRAMES];
    uint8_t *ref_v[MAX_REF_FRAMES];
    int ref_count;

    // Current macroblock data
    h264_mb mbs[MAX_MB_PER_FRAME];
    int current_mb;

    // Deblocking filter parameters
    int disable_deblocking;
    int alpha_c0_offset;
    int beta_offset;
};

// ---- NAL Unit Bitstream Reader --------------------------------------------

typedef struct bit_reader {
    const uint8_t *data;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} bit_reader;

static void br_init(bit_reader *br, const uint8_t *data, size_t size) {
    br->data = data;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 7;
}

static int br_read_bit(bit_reader *br) {
    if (br->byte_pos >= br->size) return 0;
    int bit = (br->data[br->byte_pos] >> br->bit_pos) & 1;
    if (br->bit_pos == 0) { br->byte_pos++; br->bit_pos = 7; }
    else br->bit_pos--;
    return bit;
}

static uint32_t br_read_bits(bit_reader *br, int n) {
    uint32_t val = 0;
    for (int i = 0; i < n; ++i) val = (val << 1) | br_read_bit(br);
    return val;
}

static uint32_t br_read_ue(bit_reader *br) {
    int leading = 0;
    while (br_read_bit(br) == 0 && leading < 32) leading++;
    if (leading == 32) return 0;
    return (1u << leading) - 1 + br_read_bits(br, leading);
}

static int br_read_se(bit_reader *br) {
    uint32_t ue = br_read_ue(br);
    int se = (int)((ue + 1) >> 1);
    return (ue & 1) ? -se : se;
}

// ---- NAL Unit Parser ------------------------------------------------------

static int find_nal_start_code(const uint8_t *data, size_t len, size_t *nal_start, size_t *nal_size) {
    size_t i = *nal_start;
    while (i + 3 < len) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            // Found start at i
            if (*nal_start > 0) {
                *nal_size = i - *nal_start;
                return 1;
            }
            i += 4;
        } else {
            i++;
        }
    }
    return 0;
}

static int remove_emul_prevention(const uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) {
    size_t j = 0;
    for (size_t i = 0; i < in_len; ++i) {
        if (i + 2 < in_len && in[i] == 0 && in[i + 1] == 0 && in[i + 2] == 3) {
            out[j++] = 0; out[j++] = 0;
            i += 2;
        } else {
            out[j++] = in[i];
        }
    }
    *out_len = j;
    return 1;
}

static int parse_sps(bit_reader *br, h264_sps *sps) {
    memset(sps, 0, sizeof(h264_sps));
    sps->profile_idc = (int)br_read_bits(br, 8);
    br_read_bits(br, 1); // constraint_set0_flag
    br_read_bits(br, 1); // constraint_set1_flag
    br_read_bits(br, 1); // constraint_set2_flag
    br_read_bits(br, 1); // constraint_set3_flag
    br_read_bits(br, 1); // constraint_set4_flag
    br_read_bits(br, 1); // constraint_set5_flag
    br_read_bits(br, 2); // reserved
    sps->level_idc = (int)br_read_bits(br, 8);
    sps->seq_parameter_set_id = (int)br_read_ue(br);
    if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244 ||
        sps->profile_idc == 44 || sps->profile_idc == 83 ||
        sps->profile_idc == 86 || sps->profile_idc == 118 ||
        sps->profile_idc == 128) {
        sps->chroma_format_idc = (int)br_read_ue(br);
        if (sps->chroma_format_idc == 3) br_read_bits(br, 1); // residual_colour_transform_flag
        sps->bit_depth_luma_minus8 = (int)br_read_ue(br);
        sps->bit_depth_chroma_minus8 = (int)br_read_ue(br);
        br_read_bits(br, 1); // qpprime_y_zero_transform_bypass_flag
        int seq_scaling_matrix_present_flag = br_read_bits(br, 1);
        if (seq_scaling_matrix_present_flag) {
            for (int i = 0; i < 8; ++i) {
                if (br_read_bits(br, 1)) {
                    int size = (i < 6) ? 16 : 64;
                    for (int j = 0; j < size; ++j) br_read_ue(br);
                }
            }
        }
    }
    sps->log2_max_frame_num_minus4 = (int)br_read_ue(br);
    sps->pic_order_cnt_type = (int)br_read_ue(br);
    if (sps->pic_order_cnt_type == 0) {
        sps->log2_max_pic_order_cnt_lsb_minus4 = (int)br_read_ue(br);
    } else if (sps->pic_order_cnt_type == 1) {
        br_read_bits(br, 1); // delta_pic_order_always_zero_flag
        br_read_se(br);      // offset_for_non_ref_pic
        br_read_se(br);      // offset_for_top_to_bottom_field
        int num_ref_frames_in_pic_order_cnt_cycle = (int)br_read_ue(br);
        for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i)
            br_read_se(br);
    }
    sps->max_num_ref_frames = (int)br_read_ue(br);
    sps->gaps_in_frame_num_value_allowed_flag = br_read_bits(br, 1);
    sps->pic_width_in_mbs_minus1 = (int)br_read_ue(br);
    sps->pic_height_in_map_units_minus1 = (int)br_read_ue(br);
    sps->frame_mbs_only_flag = br_read_bits(br, 1);
    if (!sps->frame_mbs_only_flag)
        sps->mb_adaptive_frame_field_flag = br_read_bits(br, 1);
    br_read_bits(br, 1); // direct_8x8_inference_flag
    sps->frame_cropping_flag = br_read_bits(br, 1);
    if (sps->frame_cropping_flag) {
        sps->frame_crop_left_offset = (int)br_read_ue(br);
        sps->frame_crop_right_offset = (int)br_read_ue(br);
        sps->frame_crop_top_offset = (int)br_read_ue(br);
        sps->frame_crop_bottom_offset = (int)br_read_ue(br);
    }
    sps->vui_parameters_present_flag = br_read_bits(br, 1);
    if (sps->vui_parameters_present_flag) {
        (void)br_read_bits(br, 1); // aspect_ratio_info_present_flag
        // simplified: skip VUI
    }
    sps->mb_width = sps->pic_width_in_mbs_minus1 + 1;
    sps->mb_height = (2 - sps->frame_mbs_only_flag) * (sps->pic_height_in_map_units_minus1 + 1);
    sps->width = sps->mb_width * 16;
    sps->height = sps->mb_height * 16;
    if (sps->frame_cropping_flag) {
        sps->width -= (sps->frame_crop_left_offset + sps->frame_crop_right_offset) * 2;
        sps->height -= (sps->frame_crop_top_offset + sps->frame_crop_bottom_offset) * 2;
    }
    return 1;
}

static int parse_pps(bit_reader *br, h264_pps *pps) {
    memset(pps, 0, sizeof(h264_pps));
    pps->pic_parameter_set_id = (int)br_read_ue(br);
    pps->seq_parameter_set_id = (int)br_read_ue(br);
    pps->entropy_coding_mode_flag = br_read_bits(br, 1);
    pps->num_slice_groups_minus1 = (int)br_read_ue(br);
    if (pps->num_slice_groups_minus1 > 0) {
        int slice_group_map_type = (int)br_read_ue(br);
        if (slice_group_map_type == 0) {
            for (int i = 0; i < pps->num_slice_groups_minus1 + 1; ++i)
                br_read_ue(br);
        } else if (slice_group_map_type == 2) {
            for (int i = 0; i < pps->num_slice_groups_minus1 + 1; ++i) {
                br_read_ue(br); br_read_ue(br); br_read_ue(br);
            }
        }
    }
    pps->num_ref_idx_l0_active_minus1 = (int)br_read_ue(br);
    pps->num_ref_idx_l1_active_minus1 = (int)br_read_ue(br);
    pps->weighted_pred_flag = br_read_bits(br, 1);
    pps->weighted_bipred_idc = (int)br_read_bits(br, 2);
    pps->pic_init_qp_minus26 = (int)br_read_se(br);
    pps->chroma_qp_index_offset = (int)br_read_se(br);
    pps->deblocking_filter_control_present_flag = br_read_bits(br, 1);
    pps->constrained_intra_pred_flag = br_read_bits(br, 1);
    pps->redundant_pic_cnt_present_flag = br_read_bits(br, 1);
    return 1;
}

static int parse_slice_header(bit_reader *br, h264_slice_header *sh, const h264_sps *sps, const h264_pps *pps) {
    memset(sh, 0, sizeof(h264_slice_header));
    sh->first_mb_in_slice = (int)br_read_ue(br);
    sh->slice_type = (int)br_read_ue(br);
    sh->pic_parameter_set_id = (int)br_read_ue(br);
    int frame_num_bits = sps->log2_max_frame_num_minus4 + 4;
    sh->frame_num = (int)br_read_bits(br, frame_num_bits);
    if (!sps->frame_mbs_only_flag) {
        sh->field_pic_flag = br_read_bits(br, 1);
        if (sh->field_pic_flag) sh->bottom_field_flag = br_read_bits(br, 1);
    }
    if (sh->slice_type == 2 || sh->slice_type == 7) {
        sh->idr_pic_id = (int)br_read_ue(br);
    }
    if (sps->pic_order_cnt_type == 0) {
        sh->pic_order_cnt_lsb = (int)br_read_bits(br, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        if (pps->pic_order_present_flag && !sh->field_pic_flag)
            sh->delta_pic_order_cnt_bottom = (int)br_read_se(br);
    }
    if (pps->redundant_pic_cnt_present_flag)
        br_read_ue(br);
    // Simplified: skip additional slice header fields
    return 1;
}

// ---- CAVLC Entropy Decode -------------------------------------------------

static const uint8_t cavlc_coeff_token_table[4][17] = {
    {5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5},
    {3,3,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {4,3,3,2,2,1,1,1,1,1,1,1,1,1,1,1,1},
    {6,5,4,3,2,1,1,1,1,1,1,1,1,1,1,1,1}
};

static int cavlc_decode_coeff_token(bit_reader *br, int nC) {
    int ctx = (nC < 2) ? 0 : (nC < 4) ? 1 : (nC < 8) ? 2 : 3;
    const uint8_t *table = cavlc_coeff_token_table[ctx];
    int bits = 0;
    for (int i = 0; i < 6; ++i) {
        bits = (bits << 1) | br_read_bit(br);
        if (bits <= 6 && table[bits] < 6) return (int)table[bits];
        if (bits > 6) break;
    }
    return 0;
}

static int cavlc_decode_total_zeros(bit_reader *br, int max_coeff) {
    (void)max_coeff;
    return (int)br_read_ue(br);
}

static int cavlc_decode_run_before(bit_reader *br, int zeros_left) {
    (void)zeros_left;
    return (int)br_read_ue(br);
}

static void cavlc_decode_residual(bit_reader *br, int16_t *coeff, int max_coeff, int nC) {
    int total_coeff = cavlc_decode_coeff_token(br, nC);
    if (total_coeff == 0) return;
    int trailing_ones = 0;
    if (total_coeff > 0 && total_coeff <= 2) trailing_ones = total_coeff;
    else if (total_coeff > 2) trailing_ones = 3;
    for (int i = 0; i < trailing_ones; ++i) {
        coeff[i] = br_read_bit(br) ? -1 : 1;
    }
    for (int i = trailing_ones; i < total_coeff; ++i) {
        int level = (int)br_read_ue(br);
        coeff[i] = br_read_bit(br) ? -level : level;
    }
    int total_zeros = cavlc_decode_total_zeros(br, max_coeff);
    int coeff_idx = 0;
    int zeros_left = total_zeros;
    for (int i = 0; i < total_coeff - 1 && zeros_left > 0; ++i) {
        int run = cavlc_decode_run_before(br, zeros_left);
        coeff[coeff_idx++] = 0;
        zeros_left -= run;
        (void)run;
    }
}

// ---- Inverse Transform (IDCT 4x4) -----------------------------------------

static void idct_4x4(int16_t block[16]) {
    int16_t tmp[16];
    for (int i = 0; i < 4; ++i) {
        int a0 = block[i + 0*4];
        int a1 = block[i + 1*4];
        int a2 = block[i + 2*4];
        int a3 = block[i + 3*4];
        int b0 = a0 + a2;
        int b1 = a0 - a2;
        int b2 = (a1 >> 1) - a3;
        int b3 = a1 + (a3 >> 1);
        tmp[i + 0*4] = (int16_t)(b0 + b3);
        tmp[i + 1*4] = (int16_t)(b1 + b2);
        tmp[i + 2*4] = (int16_t)(b1 - b2);
        tmp[i + 3*4] = (int16_t)(b0 - b3);
    }
    for (int i = 0; i < 4; ++i) {
        int a0 = tmp[i*4 + 0];
        int a1 = tmp[i*4 + 1];
        int a2 = tmp[i*4 + 2];
        int a3 = tmp[i*4 + 3];
        int b0 = a0 + a2;
        int b1 = a0 - a2;
        int b2 = (a1 >> 1) - a3;
        int b3 = a1 + (a3 >> 1);
        block[i*4 + 0] = (int16_t)((b0 + b3 + 32) >> 6);
        block[i*4 + 1] = (int16_t)((b1 + b2 + 32) >> 6);
        block[i*4 + 2] = (int16_t)((b1 - b2 + 32) >> 6);
        block[i*4 + 3] = (int16_t)((b0 - b3 + 32) >> 6);
    }
}

// ---- Intra Prediction ------------------------------------------------------

static void intra_predict_4x4_luma(uint8_t *dst, int stride, int mode,
                                    const uint8_t *left, const uint8_t *top) {
    int16_t pred[16];
    switch (mode) {
        case 0: // Vertical
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) pred[i*4+j] = top[j];
            break;
        case 1: // Horizontal
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) pred[i*4+j] = left[i];
            break;
        case 2: // DC
            { int s = 0;
              for (int i = 0; i < 4; ++i) s += top[i] + left[i];
              s = (s + 4) >> 3;
              for (int i = 0; i < 16; ++i) pred[i] = (int16_t)s; }
            break;
        case 3: // Diagonal Down-Left
            { int a = top[0], b = top[1], c = top[2], d = top[3], e = top[4];
              pred[0] = (int16_t)((a + c + 2*(b) + 2) >> 2);
              pred[1] = (int16_t)((b + d + 2*(c) + 2) >> 2);
              pred[2] = (int16_t)((c + e + 2*(d) + 2) >> 2);
              pred[3] = (int16_t)((d + e + 1) >> 1);
              pred[4] = (int16_t)((a + 2*b + c + 2) >> 2);
              pred[5] = (int16_t)((b + 2*c + d + 2) >> 2);
              pred[6] = (int16_t)((c + 2*d + e + 2) >> 2);
              pred[7] = (int16_t)((d + 2*e + f + 2) >> 2);
              pred[8] = (int16_t)((b + 2*c + d + 2) >> 2);
              pred[9] = (int16_t)((c + 2*d + e + 2) >> 2);
              pred[10] = (int16_t)((d + 2*e + f + 2) >> 2);
              pred[11] = (int16_t)((e + 2*f + g + 2) >> 2);
              pred[12] = (int16_t)((c + 2*d + e + 2) >> 2);
              pred[13] = (int16_t)((d + 2*e + f + 2) >> 2);
              pred[14] = (int16_t)((e + 2*f + g + 2) >> 2);
              pred[15] = (int16_t)((f + 2*g + h + 2) >> 2); }
            break;
        default:
            memset(pred, 128, sizeof(pred));
            break;
    }
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            dst[i * stride + j] = (uint8_t)pred[i*4+j];
}

// ---- Motion Compensation (simple bilinear) --------------------------------

static void mc_bilinear_8x8(uint8_t *dst, int dst_stride,
                              const uint8_t *ref, int ref_stride,
                              int mv_x, int mv_y) {
    int frac_x = mv_x & 3;
    int frac_y = mv_y & 3;
    int int_x = mv_x >> 2;
    int int_y = mv_y >> 2;

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int rx = int_x + x;
            int ry = int_y + y;
            int a = ref[ry * ref_stride + rx];
            int b = ref[ry * ref_stride + rx + 1];
            int c = ref[(ry + 1) * ref_stride + rx];
            int d = ref[(ry + 1) * ref_stride + rx + 1];
            int val = ((a * (4 - frac_x) + b * frac_x) * (4 - frac_y) +
                       (c * (4 - frac_x) + d * frac_x) * frac_y + 8) >> 4;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            dst[y * dst_stride + x] = (uint8_t)val;
        }
    }
}

// ---- Deblocking Filter ----------------------------------------------------

static int deblock_qp_threshold(int qp, int offset) {
    int q = qp + offset;
    if (q < 0) q = 0;
    if (q > 51) q = 51;
    return q;
}

static const uint8_t alpha_table[52] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    4,4,5,6,7,8,9,10,12,13,15,17,20,22,25,28,
    32,36,40,45,50,56,63,71,80,90,101,113,127,144,162,182,
    203,226,255
};

static const uint8_t beta_table[52] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    2,2,2,3,3,3,3,4,4,4,6,6,7,7,8,8,
    9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
    17,17,18,18
};

static void deblock_h_bs4(uint8_t *pix, int stride, int qp, int alpha_offset, int beta_offset) {
    int q = deblock_qp_threshold(qp, alpha_offset);
    int alpha = alpha_table[q];
    q = deblock_qp_threshold(qp, beta_offset);
    int beta = beta_table[q];
    (void)alpha; (void)beta;
    for (int i = 0; i < 4; ++i) {
        int p1 = pix[-1*stride];
        int p0 = pix[0];
        int q0 = pix[1*stride];
        int q1 = pix[2*stride];
        if (abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta) {
            pix[0] = (uint8_t)((2*p1 + 3*p0 + 3*q0 + 2*q1 + 4) >> 3);
            pix[1*stride] = (uint8_t)((p1 + 2*p0 + 2*q0 + q1 + 4) >> 3);
        }
        pix++;
    }
}

// ---- Main Decode Frame ----------------------------------------------------

static int decode_slice_data(h264_decoder *dec, const uint8_t *nal_data, size_t nal_size) {
    uint8_t *ebsp = (uint8_t*)malloc(nal_size + 4);
    size_t ebsp_len;
    remove_emul_prevention(nal_data, nal_size, ebsp, &ebsp_len);

    bit_reader br;
    br_init(&br, ebsp, ebsp_len);

    // Parse slice header first
    parse_slice_header(&br, &dec->slice, &dec->sps, &dec->pps);

    int mb_width = dec->sps.mb_width;
    int mb_height = dec->sps.mb_height;
    int total_mbs = mb_width * mb_height;

    for (int mb_idx = dec->slice.first_mb_in_slice; mb_idx < total_mbs; ++mb_idx) {
        h264_mb *mb = &dec->mbs[mb_idx];
        mb->mb_x = mb_idx % mb_width;
        mb->mb_y = mb_idx / mb_width;
        mb->mb_type = (int)br_read_ue(&br); // simplified

        // Decode MB prediction mode
        int mb_type_idx = mb->mb_type;
        if (mb_type_idx < 5) {
            // I_NxN or I_16x16
            mb->intra_pred_mode = (int)br_read_ue(&br);
            // Intra chroma prediction
            br_read_ue(&br);
        } else {
            // P or B skip/direct
        }

        // CAVLC coded block pattern
        mb->cbp = 0;
        if (mb_type_idx > 0) {
            mb->cbp = (int)br_read_ue(&br);
        }

        // Decode residual luma
        if (mb->cbp & 0x0F) {
            for (int b = 0; b < 16; ++b) {
                // nC estimation
                int nC = 0;
                cavlc_decode_residual(&br, mb->coeff_luma[b], 16, nC);
                idct_4x4(mb->coeff_luma[b]);
            }
        }

        // Decode residual chroma
        if (mb->cbp & 0xF0) {
            for (int b = 0; b < 4; ++b) {
                cavlc_decode_residual(&br, mb->coeff_chroma_cb[b], 16, 0);
                idct_4x4(mb->coeff_chroma_cb[b]);
            }
            for (int b = 0; b < 4; ++b) {
                cavlc_decode_residual(&br, mb->coeff_chroma_cr[b], 16, 0);
                idct_4x4(mb->coeff_chroma_cr[b]);
            }
        }

        // Reconstruct pixels
        int mx = mb->mb_x * 16;
        int my = mb->mb_y * 16;
        for (int by = 0; by < 16; by += 4) {
            for (int bx = 0; bx < 16; bx += 4) {
                int blk = (by/4) * 4 + (bx/4);
                intra_predict_4x4_luma(dec->y_plane + (my+by)*dec->y_stride + mx+bx,
                                        dec->y_stride, mb->intra_pred_mode,
                                        dec->y_plane + (my+by)*dec->y_stride + mx+bx - 1,
                                        dec->y_plane + (my+by-1)*dec->y_stride + mx+bx);
                // Add residual
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j) {
                        int idx = (my+by+i)*dec->y_stride + mx+bx+j;
                        int val = dec->y_plane[idx] + mb->coeff_luma[blk][i*4+j];
                        if (val < 0) val = 0;
                        if (val > 255) val = 255;
                        dec->y_plane[idx] = (uint8_t)val;
                    }
            }
        }
    }

    // Deblocking filter
    if (!dec->disable_deblocking) {
        for (int mb_idx = 0; mb_idx < total_mbs; ++mb_idx) {
            h264_mb *mb = &dec->mbs[mb_idx];
            int mx = mb->mb_x * 16;
            int my = mb->mb_y * 16;
            for (int row = 0; row < 16; ++row) {
                deblock_h_bs4(dec->y_plane + (my+row)*dec->y_stride + mx,
                              dec->y_stride, mb->qp_y,
                              dec->alpha_c0_offset, dec->beta_offset);
            }
        }
    }

    free(ebsp);
    return 1;
}

h264_decoder* h264_create(int width, int height) {
    h264_decoder *dec = (h264_decoder*)calloc(1, sizeof(h264_decoder));
    if (!dec) return NULL;
    dec->width = width;
    dec->height = height;
    dec->mb_width = (width + 15) / 16;
    dec->mb_height = (height + 15) / 16;
    dec->y_stride = (width + 31) & ~31;
    dec->uv_stride = dec->y_stride / 2;
    dec->bit_depth = 8;

    dec->y_plane = (uint8_t*)calloc(1, (size_t)dec->y_stride * height);
    dec->u_plane = (uint8_t*)calloc(1, (size_t)dec->uv_stride * (height / 2));
    dec->v_plane = (uint8_t*)calloc(1, (size_t)dec->uv_stride * (height / 2));
    if (!dec->y_plane || !dec->u_plane || !dec->v_plane) {
        free(dec->y_plane); free(dec->u_plane); free(dec->v_plane);
        free(dec); return NULL;
    }

    // Default SPS
    dec->sps.width = width;
    dec->sps.height = height;
    dec->sps.mb_width = dec->mb_width;
    dec->sps.mb_height = dec->mb_height;
    dec->sps.frame_mbs_only_flag = 1;
    dec->sps.log2_max_frame_num_minus4 = 4;
    dec->sps.pic_order_cnt_type = 0;
    dec->sps.log2_max_pic_order_cnt_lsb_minus4 = 4;

    // Default PPS
    dec->pps.entropy_coding_mode_flag = 0;
    dec->pps.num_ref_idx_l0_active_minus1 = 0;
    dec->pps.pic_init_qp_minus26 = 0;
    dec->pps.chroma_qp_index_offset = 0;
    dec->pps.deblocking_filter_control_present_flag = 1;

    dec->ref_count = 0;
    dec->disable_deblocking = 0;
    return dec;
}

int h264_decode(h264_decoder *dec, const uint8_t *data, size_t len,
                uint8_t **yuv[3], int *strides) {
    if (!dec || !data || !len) return -1;
    size_t offset = 0;
    uint8_t nal_buf[512 * 1024];
    size_t nal_len;

    while (offset < len) {
        // Find NAL start code
        if (offset + 4 > len) break;
        if (data[offset] == 0 && data[offset+1] == 0 && data[offset+2] == 0 && data[offset+3] == 1) {
            offset += 4;
            size_t nal_start = offset;
            size_t next_nal = offset;
            while (next_nal + 4 < len) {
                if (data[next_nal] == 0 && data[next_nal+1] == 0 && data[next_nal+2] == 0 && data[next_nal+3] == 1)
                    break;
                if (data[next_nal] == 0 && data[next_nal+1] == 0 && data[next_nal+2] == 1) {
                    next_nal += 3;
                    break;
                }
                next_nal++;
            }
            nal_len = next_nal - nal_start;

            if (nal_len > sizeof(nal_buf)) continue;
            memcpy(nal_buf, data + nal_start, nal_len);

            uint8_t nal_type = nal_buf[0] & 0x1F;
            int nal_ref_idc = (nal_buf[0] >> 5) & 3;
            (void)nal_ref_idc;

            // Remove NAL header byte for EBSP
            size_t ebsp_len = nal_len - 1;
            if (ebsp_len > sizeof(nal_buf) - 1) continue;
            memmove(nal_buf, nal_buf + 1, ebsp_len);

            if (nal_type == 7) {
                bit_reader br;
                br_init(&br, nal_buf, ebsp_len);
                parse_sps(&br, &dec->sps);
                dec->width = dec->sps.width;
                dec->height = dec->sps.height;
                dec->mb_width = dec->sps.mb_width;
                dec->mb_height = dec->sps.mb_height;
            } else if (nal_type == 8) {
                bit_reader br;
                br_init(&br, nal_buf, ebsp_len);
                parse_pps(&br, &dec->pps);
            } else if (nal_type >= 1 && nal_type <= 5) {
                decode_slice_data(dec, nal_buf, ebsp_len);
            }

            offset = next_nal;
        } else {
            offset++;
        }
    }

    if (yuv) {
        yuv[0] = &dec->y_plane;
        yuv[1] = &dec->u_plane;
        yuv[2] = &dec->v_plane;
    }
    if (strides) {
        strides[0] = dec->y_stride;
        strides[1] = dec->uv_stride;
        strides[2] = dec->uv_stride;
    }
    return 0;
}

void h264_destroy(h264_decoder *dec) {
    if (!dec) return;
    free(dec->y_plane);
    free(dec->u_plane);
    free(dec->v_plane);
    for (int i = 0; i < dec->ref_count && i < MAX_REF_FRAMES; ++i) {
        free(dec->ref_y[i]);
        free(dec->ref_u[i]);
        free(dec->ref_v[i]);
    }
    free(dec);
}
