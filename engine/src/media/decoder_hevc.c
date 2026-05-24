#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---- HEVC / H.265 Software Decoder ----------------------------------------
// VPS / SPS / PPS / slice segment decoder with simplified CTU reconstruction.

#define MAX_CTU_ROWS 136
#define MAX_CTU_COLS 120
#define CTU_SIZE 64

typedef struct hevc_vps {
    int vps_id;
    int max_layers;
    int max_sub_layers;
    int temporal_id_nesting;
    int vps_max_dec_pic_buffering_minus1[8];
    int vps_num_reorder_pics[8];
    int vps_max_latency_increase[8];
} hevc_vps;

typedef struct hevc_sps {
    int sps_id;
    int vps_id;
    int max_sub_layers;
    int chroma_format_idc;
    int pic_width_in_luma_samples;
    int pic_height_in_luma_samples;
    int conformance_window_flag;
    int conf_win_left, conf_win_right, conf_win_top, conf_win_bottom;
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int log2_max_pic_order_cnt_lsb_minus4;
    int log2_min_luma_coding_block_size_minus3;
    int log2_diff_max_min_luma_coding_block_size;
    int log2_min_transform_block_size_minus2;
    int log2_diff_max_min_transform_block_size;
    int max_transform_hierarchy_depth_inter;
    int max_transform_hierarchy_depth_intra;
    int scaling_list_enabled_flag;
    int amp_enabled_flag;
    int sample_adaptive_offset_enabled_flag;
    int pcm_enabled_flag;
    int sps_strong_intra_smoothing_enable_flag;
    int width, height;
    int ctb_size;
    int log2_ctb_size;
    int ctb_width, ctb_height;
} hevc_sps;

typedef struct hevc_pps {
    int pps_id;
    int sps_id;
    int dependent_slice_segments_enabled_flag;
    int output_flag_present_flag;
    int num_extra_slice_header_bits;
    int sign_data_hiding_flag;
    int cabac_init_present_flag;
    int num_ref_idx_l0_default_active_minus1;
    int num_ref_idx_l1_default_active_minus1;
    int init_qp_minus26;
    int constrained_intra_pred_flag;
    int transform_skip_enabled_flag;
    int cu_qp_delta_enabled_flag;
    int diff_cu_qp_delta_depth;
    int pps_loop_filter_across_slices_enabled_flag;
    int deblocking_filter_control_present_flag;
    int deblocking_filter_override_enabled_flag;
    int pps_deblocking_filter_disabled_flag;
    int pps_beta_offset_div2;
    int pps_tc_offset_div2;
    int tiles_enabled_flag;
    int entropy_coding_sync_enabled_flag;
    int uniform_spacing_flag;
    int num_tile_columns_minus1;
    int num_tile_rows_minus1;
} hevc_pps;

typedef struct hevc_slice_segment {
    int first_ctb_in_slice;
    int slice_type;
    int pps_id;
    int slice_segment_address;
    int dependent_slice_segment_flag;
    int slice_qp_delta;
    int num_ref_idx_l0_active;
    int num_ref_idx_l1_active;
    int collocated_ref_idx;
    int five_minus_max_num_merge_cand;
} hevc_slice_segment;

typedef struct hevc_ctu {
    int ctu_x, ctu_y;
    int qp_y;
    int split_depth;
    int pred_mode;
    int part_mode;
    int intra_mode[4];
    int merge_idx;
    int mvp_idx;
    int16_t mv[4][2];
    int ref_idx[4];
    int16_t coeff_luma[64][64];
    int16_t coeff_cb[32][32];
    int16_t coeff_cr[32][32];
} hevc_ctu;

struct hevc_decoder {
    int width, height;
    int bit_depth;
    int ctb_size, ctb_width, ctb_height;

    hevc_vps vps;
    hevc_sps sps;
    hevc_pps pps;
    hevc_slice_segment slice;

    uint8_t *y_plane, *u_plane, *v_plane;
    int y_stride, uv_stride;

    hevc_ctu ctus[MAX_CTU_ROWS * MAX_CTU_COLS];
};

static uint32_t hevc_br_read_bits(const uint8_t **pp, int n) {
    uint32_t val = 0;
    const uint8_t *p = *pp;
    static int bit_pos = 7;
    for (int i = 0; i < n; ++i) {
        val = (val << 1) | ((p[0] >> bit_pos) & 1);
        if (bit_pos == 0) { p++; bit_pos = 7; }
        else bit_pos--;
    }
    *pp = p;
    return val;
}

static uint32_t hevc_br_read_ue(const uint8_t **pp) {
    int leading = 0;
    while (hevc_br_read_bits(pp, 1) == 0 && leading < 32) leading++;
    if (leading >= 32) return 0;
    return (1u << leading) - 1 + hevc_br_read_bits(pp, leading);
}

static int hevc_br_read_se(const uint8_t **pp) {
    uint32_t ue = hevc_br_read_ue(pp);
    return (ue & 1) ? -(int)((ue + 1) >> 1) : (int)((ue + 1) >> 1);
}

static int parse_hevc_vps(const uint8_t **pp, hevc_vps *vps) {
    memset(vps, 0, sizeof(hevc_vps));
    vps->vps_id = (int)hevc_br_read_bits(pp, 4);
    vps->max_layers = 1 + (int)hevc_br_read_bits(pp, 6);
    vps->max_sub_layers = 1 + (int)hevc_br_read_bits(pp, 3);
    vps->temporal_id_nesting = (int)hevc_br_read_bits(pp, 1);
    hevc_br_read_bits(pp, 16); // vps_reserved_0xffff
    for (int i = 0; i < vps->max_sub_layers; ++i) {
        vps->vps_max_dec_pic_buffering_minus1[i] = (int)hevc_br_read_ue(pp);
        vps->vps_num_reorder_pics[i] = (int)hevc_br_read_ue(pp);
        vps->vps_max_latency_increase[i] = (int)hevc_br_read_ue(pp);
    }
    return 1;
}

static int parse_hevc_sps(const uint8_t **pp, hevc_sps *sps) {
    memset(sps, 0, sizeof(hevc_sps));
    sps->sps_id = (int)hevc_br_read_bits(pp, 4);
    sps->vps_id = (int)hevc_br_read_bits(pp, 4);
    sps->max_sub_layers = 1 + (int)hevc_br_read_bits(pp, 3);
    sps->chroma_format_idc = (int)hevc_br_read_bits(pp, 2);
    sps->pic_width_in_luma_samples = (int)hevc_br_read_bits(pp, 16);
    sps->pic_height_in_luma_samples = (int)hevc_br_read_bits(pp, 16);
    sps->conformance_window_flag = (int)hevc_br_read_bits(pp, 1);
    if (sps->conformance_window_flag) {
        sps->conf_win_left = (int)hevc_br_read_ue(pp);
        sps->conf_win_right = (int)hevc_br_read_ue(pp);
        sps->conf_win_top = (int)hevc_br_read_ue(pp);
        sps->conf_win_bottom = (int)hevc_br_read_ue(pp);
    }
    sps->bit_depth_luma_minus8 = (int)hevc_br_read_ue(pp);
    sps->bit_depth_chroma_minus8 = (int)hevc_br_read_ue(pp);
    sps->log2_max_pic_order_cnt_lsb_minus4 = (int)hevc_br_read_ue(pp);
    sps->log2_min_luma_coding_block_size_minus3 = (int)hevc_br_read_ue(pp);
    sps->log2_diff_max_min_luma_coding_block_size = (int)hevc_br_read_ue(pp);
    sps->log2_min_transform_block_size_minus2 = (int)hevc_br_read_ue(pp);
    sps->log2_diff_max_min_transform_block_size = (int)hevc_br_read_ue(pp);
    sps->max_transform_hierarchy_depth_inter = (int)hevc_br_read_ue(pp);
    sps->max_transform_hierarchy_depth_intra = (int)hevc_br_read_ue(pp);
    sps->scaling_list_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    sps->amp_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    sps->sample_adaptive_offset_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    sps->pcm_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    sps->sps_strong_intra_smoothing_enable_flag = (int)hevc_br_read_bits(pp, 1);

    sps->width = sps->pic_width_in_luma_samples;
    sps->height = sps->pic_height_in_luma_samples;
    sps->log2_ctb_size = 6; // CTB 64
    sps->ctb_size = 1 << sps->log2_ctb_size;
    sps->ctb_width = (sps->width + sps->ctb_size - 1) / sps->ctb_size;
    sps->ctb_height = (sps->height + sps->ctb_size - 1) / sps->ctb_size;
    return 1;
}

static int parse_hevc_pps(const uint8_t **pp, hevc_pps *pps, const hevc_sps *sps) {
    memset(pps, 0, sizeof(hevc_pps));
    pps->pps_id = (int)hevc_br_read_bits(pp, 6);
    pps->sps_id = (int)hevc_br_read_bits(pp, 4);
    pps->dependent_slice_segments_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    pps->output_flag_present_flag = (int)hevc_br_read_bits(pp, 1);
    pps->num_extra_slice_header_bits = (int)hevc_br_read_bits(pp, 3);
    pps->sign_data_hiding_flag = (int)hevc_br_read_bits(pp, 1);
    pps->cabac_init_present_flag = (int)hevc_br_read_bits(pp, 1);
    pps->num_ref_idx_l0_default_active_minus1 = (int)hevc_br_read_ue(pp);
    pps->num_ref_idx_l1_default_active_minus1 = (int)hevc_br_read_ue(pp);
    pps->init_qp_minus26 = (int)hevc_br_read_se(pp);
    pps->constrained_intra_pred_flag = (int)hevc_br_read_bits(pp, 1);
    pps->transform_skip_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    pps->cu_qp_delta_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    if (pps->cu_qp_delta_enabled_flag)
        pps->diff_cu_qp_delta_depth = (int)hevc_br_read_ue(pp);
    pps->pps_loop_filter_across_slices_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    pps->deblocking_filter_control_present_flag = (int)hevc_br_read_bits(pp, 1);
    if (pps->deblocking_filter_control_present_flag) {
        pps->deblocking_filter_override_enabled_flag = (int)hevc_br_read_bits(pp, 1);
        pps->pps_deblocking_filter_disabled_flag = (int)hevc_br_read_bits(pp, 1);
        if (!pps->pps_deblocking_filter_disabled_flag) {
            pps->pps_beta_offset_div2 = (int)hevc_br_read_se(pp);
            pps->pps_tc_offset_div2 = (int)hevc_br_read_se(pp);
        }
    }
    pps->tiles_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    pps->entropy_coding_sync_enabled_flag = (int)hevc_br_read_bits(pp, 1);
    if (pps->tiles_enabled_flag) {
        pps->uniform_spacing_flag = (int)hevc_br_read_bits(pp, 1);
        pps->num_tile_columns_minus1 = (int)hevc_br_read_ue(pp);
        pps->num_tile_rows_minus1 = (int)hevc_br_read_ue(pp);
    }
    return 1;
}

// ---- HEVC IDCT 4x4 / 8x8 / 16x16 / 32x32 (simplified DCT) ----------------

static void hevc_idct_4x4(int16_t block[16]) {
    int16_t tmp[16];
    for (int i = 0; i < 4; ++i) {
        int a0 = block[i];
        int a1 = block[4+i];
        int a2 = block[8+i];
        int a3 = block[12+i];
        tmp[i]     = (int16_t)(a0 + a1 + a2 + a3);
        tmp[4+i]   = (int16_t)(2*a0 + a1 - a2 - 2*a3);
        tmp[8+i]   = (int16_t)(a0 - a1 - a2 + a3);
        tmp[12+i]  = (int16_t)(a0 - 2*a1 + 2*a2 - a3);
    }
    for (int i = 0; i < 4; ++i) {
        int a0 = tmp[i*4];
        int a1 = tmp[i*4+1];
        int a2 = tmp[i*4+2];
        int a3 = tmp[i*4+3];
        block[i*4]   = (int16_t)((a0 + a1 + a2 + a3 + 64) >> 7);
        block[i*4+1] = (int16_t)((2*a0 + a1 - a2 - 2*a3 + 64) >> 7);
        block[i*4+2] = (int16_t)((a0 - a1 - a2 + a3 + 64) >> 7);
        block[i*4+3] = (int16_t)((a0 - 2*a1 + 2*a2 - a3 + 64) >> 7);
    }
}

// ---- Intra Prediction ------------------------------------------------------

static void hevc_intra_dc(uint8_t *dst, int stride, int size, const uint8_t *left, const uint8_t *top) {
    int sum = 0;
    for (int i = 0; i < size; ++i) sum += top[i] + left[i];
    int dc = (sum + size) / (2 * size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            dst[y * stride + x] = (uint8_t)dc;
}

static void hevc_intra_planar(uint8_t *dst, int stride, int size, const uint8_t *left, const uint8_t *top, const uint8_t *topleft) {
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int v = (size - 1 - x) * left[y] + (x + 1) * top[size];
            int h = (size - 1 - y) * top[x] + (y + 1) * left[size];
            dst[y * stride + x] = (uint8_t)((v + h + size) / (2 * size));
        }
    }
    (void)topleft;
}

// ---- Deblocking Filter (simplified) ----------------------------------------

static void hevc_deblock_vertical(uint8_t *pix, int stride, int qp, int beta_offset, int tc_offset) {
    int q = qp + beta_offset;
    if (q < 0) q = 0;
    if (q > 51) q = 51;
    const uint8_t beta_table[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,3,3,3,3,4,4,4,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18};
    int beta = beta_table[q];
    (void)beta; (void)tc_offset;
    for (int i = 0; i < 4; ++i) {
        int p0 = pix[0];
        int q0 = pix[1*stride];
        if (abs(p0 - q0) < beta) {
            pix[0] = (uint8_t)((p0 + q0 + 1) >> 1);
        }
        pix++;
    }
}

// ---- Main Decode Function -------------------------------------------------

hevc_decoder* hevc_create(int width, int height) {
    hevc_decoder *dec = (hevc_decoder*)calloc(1, sizeof(hevc_decoder));
    if (!dec) return NULL;
    dec->width = width;
    dec->height = height;
    dec->bit_depth = 8;
    dec->ctb_size = 64;
    dec->ctb_width = (width + 63) / 64;
    dec->ctb_height = (height + 63) / 64;
    dec->y_stride = (width + 63) & ~63;
    dec->uv_stride = dec->y_stride / 2;

    dec->y_plane = (uint8_t*)calloc(1, (size_t)dec->y_stride * height);
    dec->u_plane = (uint8_t*)calloc(1, (size_t)dec->uv_stride * (height/2));
    dec->v_plane = (uint8_t*)calloc(1, (size_t)dec->uv_stride * (height/2));

    dec->sps.ctb_size = 64;
    dec->sps.log2_ctb_size = 6;
    dec->sps.ctb_width = dec->ctb_width;
    dec->sps.ctb_height = dec->ctb_height;
    dec->sps.width = width;
    dec->sps.height = height;
    return dec;
}

int hevc_decode(hevc_decoder *dec, const uint8_t *data, size_t len,
                uint8_t **yuv[3], int *strides) {
    if (!dec || !data || !len) return -1;
    size_t offset = 0;
    int nal_type;
    const uint8_t *nal_data;

    while (offset + 6 <= len) {
        if (data[offset] == 0 && data[offset+1] == 0 && data[offset+2] == 1) {
            offset += 3;
        } else if (data[offset] == 0 && data[offset+1] == 0 && data[offset+2] == 0 && data[offset+3] == 1) {
            offset += 4;
        } else {
            offset++; continue;
        }
        nal_data = data + offset;
        size_t nal_start = offset;

        size_t next_nal = offset;
        while (next_nal + 4 < len) {
            if ((data[next_nal] == 0 && data[next_nal+1] == 0 &&
                 data[next_nal+2] == 1) ||
                (data[next_nal] == 0 && data[next_nal+1] == 0 &&
                 data[next_nal+2] == 0 && data[next_nal+3] == 1))
                break;
            next_nal++;
        }
        size_t nal_size = next_nal - nal_start;
        offset = next_nal;

        nal_type = (nal_data[0] >> 1) & 0x3F;
        size_t ebsp_size = nal_size > 0 ? nal_size - 1 : 0;
        const uint8_t *ebsp = nal_data + 1;

        if (nal_type == 32) {
            parse_hevc_vps(&ebsp, &dec->vps);
        } else if (nal_type == 33) {
            parse_hevc_sps(&ebsp, &dec->sps);
            dec->width = dec->sps.width;
            dec->height = dec->sps.height;
            dec->ctb_width = dec->sps.ctb_width;
            dec->ctb_height = dec->sps.ctb_height;
        } else if (nal_type == 34) {
            parse_hevc_pps(&ebsp, &dec->pps, &dec->sps);
        } else if (nal_type >= 0 && nal_type <= 9) {
            // Slice segment
            dec->slice.first_ctb_in_slice = (int)hevc_br_read_ue(&ebsp);
            dec->slice.slice_type = (int)hevc_br_read_ue(&ebsp);
            dec->slice.pps_id = (int)hevc_br_read_bits(&ebsp, 6);
            dec->slice.slice_qp_delta = (int)hevc_br_read_se(&ebsp);

            int qp = 26 + dec->pps.init_qp_minus26 + dec->slice.slice_qp_delta;
            int ctb_count = dec->ctb_width * dec->ctb_height;

            for (int ctb_idx = dec->slice.first_ctb_in_slice;
                 ctb_idx < ctb_count; ++ctb_idx) {
                hevc_ctu *ctb = &dec->ctus[ctb_idx];
                ctb->ctu_x = (ctb_idx % dec->ctb_width) * dec->ctb_size;
                ctb->ctu_y = (ctb_idx / dec->ctb_width) * dec->ctb_size;
                ctb->qp_y = qp;

                // Simplified CU split and intra/inter decode
                int split_flag = (int)hevc_br_read_bits(&ebsp, 1);
                if (split_flag) {
                    ctb->split_depth = 1;
                    // In a real decoder: iterate sub-CUs
                }

                int pred_mode = (int)hevc_br_read_bits(&ebsp, 1);
                if (pred_mode == 0) {
                    // Intra
                    int luma_mode = (int)hevc_br_read_bits(&ebsp, 3);
                    (void)luma_mode;
                    uint8_t left[65] = {0};
                    uint8_t top[65] = {0};
                    uint8_t topleft = 0;
                    int stride = dec->y_stride;

                    for (int i = 0; i < 64 && ctb->ctu_y + i < (uint32_t)dec->height; ++i)
                        left[i] = (ctb->ctu_x > 0) ? dec->y_plane[(ctb->ctu_y + i) * stride + (ctb->ctu_x - 1)] : 128;
                    for (int i = 0; i < 64 && ctb->ctu_x + i < (uint32_t)dec->width; ++i)
                        top[i] = (ctb->ctu_y > 0) ? dec->y_plane[(ctb->ctu_y - 1) * stride + (ctb->ctu_x + i)] : 128;
                    topleft = (ctb->ctu_x > 0 && ctb->ctu_y > 0) ? dec->y_plane[(ctb->ctu_y - 1) * stride + (ctb->ctu_x - 1)] : 128;

                    hevc_intra_dc(dec->y_plane + ctb->ctu_y * stride + ctb->ctu_x,
                                  stride, 64, left, top);
                } else {
                    // Inter
                    for (int i = 0; i < 4; ++i) {
                        ctb->mv[i][0] = (int16_t)hevc_br_read_se(&ebsp);
                        ctb->mv[i][1] = (int16_t)hevc_br_read_se(&ebsp);
                    }
                }
            }

            // SAO (simplified)
            // Deblocking
            if (!dec->pps.pps_deblocking_filter_disabled_flag) {
                for (int ctb_idx = dec->slice.first_ctb_in_slice;
                     ctb_idx < ctb_count; ++ctb_idx) {
                    hevc_ctu *ctb = &dec->ctus[ctb_idx];
                    for (int row = 0; row < 64 && ctb->ctu_y + row < (uint32_t)dec->height; ++row) {
                        hevc_deblock_vertical(dec->y_plane + (ctb->ctu_y + row) * dec->y_stride + ctb->ctu_x,
                                              dec->y_stride, ctb->qp_y,
                                              dec->pps.pps_beta_offset_div2,
                                              dec->pps.pps_tc_offset_div2);
                    }
                }
            }
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

void hevc_destroy(hevc_decoder *dec) {
    if (!dec) return;
    free(dec->y_plane);
    free(dec->u_plane);
    free(dec->v_plane);
    free(dec);
}
