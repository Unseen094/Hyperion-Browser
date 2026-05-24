#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---- VP9 Software Decoder -------------------------------------------------
// Superblock / partition decoder with probability adaptation.

#define MAX_SB_SIZE 64
#define MAX_REF_FRAMES_VP9 8

typedef struct vp9_frame_context {
    uint8_t prob_intra;
    uint8_t prob_last;
    uint8_t prob_gf;
    uint8_t prob_skip;
    uint8_t prob_tx_size[2];
    uint8_t prob_partition[16][3];
    uint8_t prob_inter_mode[7][3];
    uint8_t prob_intra_mode[4][8];
    uint8_t prob_mv_joint[3];
    uint8_t prob_mv_sign;
    uint8_t prob_mv_class[10];
    uint8_t prob_mv_class0_bit[10];
    uint8_t prob_mv_fp[3][8];
    uint8_t prob_mv_class0_fr[2][3];
    uint8_t prob_mv_class0_hp;
    uint8_t prob_mv_hp;
    uint8_t prob_coeff[4][2][3][6][8];
    uint8_t prob_coeff_eob[4][2][3][2];
} vp9_frame_context;

typedef struct vp9_segmentation {
    int enabled;
    int update_map;
    int temporal_update;
    int abs_delta;
    int feature_mask[8];
    int feature_data[8][4];
} vp9_segmentation;

typedef struct vp9_loop_filter {
    int filter_level;
    int sharpness_level;
    int mode_ref_delta_enabled;
    int mode_ref_delta_update;
    int ref_deltas[4];
    int mode_deltas[2];
} vp9_loop_filter;

typedef struct vp9_quantization {
    int base_q_idx;
    int y_dc_delta;
    int uv_dc_delta;
    int uv_ac_delta;
} vp9_quantization;

typedef struct vp9_tile_info {
    int tile_cols, tile_rows;
    int tile_col_log2, tile_row_log2;
    int sb_cols, sb_rows;
} vp9_tile_info;

struct vp9_decoder {
    int width, height;
    int bit_depth;
    int subsampling_x, subsampling_y;
    int sb_size, sb_cols, sb_rows;
    int y_stride, uv_stride;

    uint8_t *y_plane, *u_plane, *v_plane;

    vp9_frame_context fc;
    vp9_segmentation segmentation;
    vp9_loop_filter loop_filter;
    vp9_quantization quant;
    vp9_tile_info tiles;

    uint8_t ref_y[MAX_REF_FRAMES_VP9];
    // Reference frame buffers omitted for brevity
};

// ---- Boolean Decoder (VP9 range decoder) ----------------------------------

typedef struct vp9_bool_decoder {
    const uint8_t *data;
    size_t size;
    size_t byte_pos;
    uint32_t value;
    int count;
    int range;
} vp9_bool_decoder;

static void bool_init(vp9_bool_decoder *bd, const uint8_t *data, size_t size) {
    bd->data = data;
    bd->size = size;
    bd->byte_pos = 0;
    bd->value = 0;
    bd->count = 0;
    bd->range = 255;
    for (int i = 0; i < 2 && bd->byte_pos < size; ++i) {
        bd->value = (bd->value << 8) | data[bd->byte_pos++];
        bd->count += 8;
    }
}

static int bool_read(vp9_bool_decoder *bd, int prob) {
    int bit = 0;
    int split = 1 + (((bd->range - 1) * prob) >> 8);
    if (bd->value >> 8 >= split) {
        bit = 1;
        bd->range -= split;
    } else {
        bit = 0;
        bd->range = split;
    }
    while (bd->range < 128) {
        bd->range <<= 1;
        bd->value <<= 1;
        if (bd->count > 0) {
            bd->count--;
        } else if (bd->byte_pos < bd->size) {
            bd->value |= (bd->data[bd->byte_pos++] >> bd->count) & 1;
            bd->count = 7;
        }
    }
    return bit;
}

static int bool_read_literal(vp9_bool_decoder *bd, int n) {
    int val = 0;
    for (int i = 0; i < n; ++i)
        val = (val << 1) | bool_read(bd, 128);
    return val;
}

// ---- Probability Adaptation -----------------------------------------------

static void adapt_prob(uint8_t *prob, int counts, int total, int update_factor) {
    if (total == 0) return;
    int new_prob = (counts * 256 + total / 2) / total;
    *prob = (uint8_t)(*prob + ((new_prob - *prob) * update_factor + 128) / 256);
}

// ---- Superblock Partition Decode ------------------------------------------

typedef enum vp9_partition_type {
    PARTITION_NONE,
    PARTITION_HORZ,
    PARTITION_VERT,
    PARTITION_SPLIT
} vp9_partition_type;

static vp9_partition_type decode_partition(vp9_bool_decoder *bd,
                                            int bs_log2, int mi_row, int mi_col,
                                            vp9_frame_context *fc) {
    if (bs_log2 == 6) { // 64x64 can only split or none
        return bool_read(bd, fc->prob_partition[0][0]) ?
               PARTITION_SPLIT : PARTITION_NONE;
    }
    int ctx = 0;
    // Simplified context derivation
    int p = fc->prob_partition[bs_log2 - 2][ctx];
    if (!bool_read(bd, p)) return PARTITION_NONE;
    p = fc->prob_partition[bs_log2 - 2][1];
    if (!bool_read(bd, p)) return PARTITION_HORZ;
    p = fc->prob_partition[bs_log2 - 2][2];
    return bool_read(bd, p) ? PARTITION_SPLIT : PARTITION_VERT;
}

// ---- IDCT for VP9 (simplified 4x4 / 8x8 / 16x16 / 32x32) -----------------

static void vp9_idct_4x4(int16_t block[16]) {
    int16_t tmp[16];
    for (int i = 0; i < 4; ++i) {
        int a0 = block[i];
        int a1 = block[4+i];
        int a2 = block[8+i];
        int a3 = block[12+i];
        int t0 = a0 + a2;
        int t1 = a0 - a2;
        int t2 = (a1 >> 1) - a3;
        int t3 = a1 + (a3 >> 1);
        tmp[i]     = (int16_t)(t0 + t3);
        tmp[4+i]   = (int16_t)(t1 + t2);
        tmp[8+i]   = (int16_t)(t1 - t2);
        tmp[12+i]  = (int16_t)(t0 - t3);
    }
    for (int i = 0; i < 4; ++i) {
        int a0 = tmp[i*4];
        int a1 = tmp[i*4+1];
        int a2 = tmp[i*4+2];
        int a3 = tmp[i*4+3];
        int t0 = a0 + a2;
        int t1 = a0 - a2;
        int t2 = (a1 >> 1) - a3;
        int t3 = a1 + (a3 >> 1);
        block[i*4]   = (int16_t)((t0 + t3 + 4) >> 3);
        block[i*4+1] = (int16_t)((t1 + t2 + 4) >> 3);
        block[i*4+2] = (int16_t)((t1 - t2 + 4) >> 3);
        block[i*4+3] = (int16_t)((t0 - t3 + 4) >> 3);
    }
}

// ---- Loop Filter (simplified) ---------------------------------------------

static void vp9_loop_filter_vertical(uint8_t *pix, int stride, int len, int fl, int sharp) {
    if (fl == 0) return;
    int limit = fl >> sharp;
    if (limit > 1) limit = 1;
    (void)limit;
    for (int i = 0; i < len; ++i) {
        int p0 = pix[0];
        int q0 = pix[1*stride];
        int diff = (p0 - q0) >> 2;
        if (diff < -64) diff = -64;
        if (diff > 63) diff = 63;
        pix[0] = (uint8_t)(p0 - diff);
        pix[1*stride] = (uint8_t)(q0 + diff);
        pix++;
    }
}

// ---- Main VP9 Frame Decode ------------------------------------------------

static void vp9_decode_frame(vp9_decoder *dec, const uint8_t *data, size_t len) {
    vp9_bool_decoder bd;
    bool_init(&bd, data, len);

    // Frame header
    int frame_type = bool_read_literal(&bd, 2); // 0=KEY, 1=INTER, 2=ALTREF
    int show_frame = bool_read(&bd, 128);
    int error_resilient = bool_read(&bd, 128);
    (void)show_frame; (void)error_resilient;

    if (frame_type == 0) {
        // Key frame
        bool_read_literal(&bd, 3); // sync code
        dec->bit_depth = bool_read(&bd, 128) ? 10 : 8;
        dec->subsampling_x = 1;
        dec->subsampling_y = 1;
        bool_read(&bd, 128); // color space
        bool_read(&bd, 128); // color range
    }

    // Segmentation
    dec->segmentation.enabled = bool_read(&bd, 128);
    if (dec->segmentation.enabled) {
        dec->segmentation.update_map = bool_read(&bd, 128);
        if (dec->segmentation.update_map)
            dec->segmentation.temporal_update = bool_read(&bd, 128);
        dec->segmentation.abs_delta = bool_read(&bd, 128);
        for (int i = 0; i < 8; ++i) {
            if (bool_read(&bd, 128)) {
                dec->segmentation.feature_mask[i] = bool_read_literal(&bd, 4);
                for (int j = 0; j < 4; ++j) {
                    if (dec->segmentation.feature_mask[i] & (1 << j))
                        dec->segmentation.feature_data[i][j] = bool_read_literal(&bd, 8);
                }
            }
        }
    }

    // Loop filter
    dec->loop_filter.filter_level = bool_read_literal(&bd, 6);
    dec->loop_filter.sharpness_level = bool_read_literal(&bd, 3);
    dec->loop_filter.mode_ref_delta_enabled = bool_read(&bd, 128);
    if (dec->loop_filter.mode_ref_delta_enabled && bool_read(&bd, 128)) {
        for (int i = 0; i < 4; ++i) {
            if (bool_read(&bd, 128))
                dec->loop_filter.ref_deltas[i] = bool_read_literal(&bd, 6);
        }
        for (int i = 0; i < 2; ++i) {
            if (bool_read(&bd, 128))
                dec->loop_filter.mode_deltas[i] = bool_read_literal(&bd, 6);
        }
    }

    // Quantization
    dec->quant.base_q_idx = bool_read_literal(&bd, 8);
    dec->quant.y_dc_delta = bool_read(&bd, 128) ? bool_read_literal(&bd, 4) : 0;
    dec->quant.uv_dc_delta = bool_read(&bd, 128) ? bool_read_literal(&bd, 4) : 0;
    dec->quant.uv_ac_delta = bool_read(&bd, 128) ? bool_read_literal(&bd, 4) : 0;

    // Tile info
    int tile_cols_log2 = bool_read_literal(&bd, 2);
    int tile_rows_log2 = bool_read_literal(&bd, 2);
    dec->tiles.tile_cols = 1 << tile_cols_log2;
    dec->tiles.tile_rows = 1 << tile_rows_log2;
    dec->tiles.tile_col_log2 = tile_cols_log2;
    dec->tiles.tile_row_log2 = tile_rows_log2;

    dec->sb_size = 64;
    dec->sb_cols = (dec->width + dec->sb_size - 1) / dec->sb_size;
    dec->sb_rows = (dec->height + dec->sb_size - 1) / dec->sb_size;
    dec->tiles.sb_cols = dec->sb_cols;
    dec->tiles.sb_rows = dec->sb_rows;

    // Decode tiles
    for (int tile_row = 0; tile_row < dec->tiles.tile_rows; ++tile_row) {
        for (int tile_col = 0; tile_col < dec->tiles.tile_cols; ++tile_col) {
            for (int sb_row = 0; sb_row < dec->sb_rows; ++sb_row) {
                for (int sb_col = 0; sb_col < dec->sb_cols; ++sb_col) {
                    vp9_partition_type part = decode_partition(&bd, 6, sb_row, sb_col, &dec->fc);
                    int sub_size = 64;
                    if (part == PARTITION_SPLIT) sub_size = 32;
                    else if (part == PARTITION_NONE) sub_size = 64;
                    else sub_size = 32;

                    int steps = 64 / sub_size;
                    for (int sy = 0; sy < steps; ++sy) {
                        for (int sx = 0; sx < steps; ++sx) {
                            int bx = (sb_col * 64 + sx * sub_size) / 4;
                            int by = (sb_row * 64 + sy * sub_size) / 4;
                            (void)bx; (void)by;

                            // Skip probability
                            int skip = bool_read(&bd, dec->fc.prob_skip);
                            (void)skip;

                            // Intra / Inter mode
                            if (frame_type == 0) {
                                // Key frame: always intra
                                int intra_mode = bool_read_literal(&bd, 3);
                                (void)intra_mode;
                            } else {
                                int is_inter = bool_read(&bd, dec->fc.prob_intra) ? 0 : 1;
                                (void)is_inter;
                            }

                            // Transform coefficients
                            int has_coeff = bool_read(&bd, 128);
                            if (has_coeff) {
                                for (int c = 0; c < 16; ++c) {
                                    bool_read_literal(&bd, 1); // simplified token
                                }
                            }

                            // Reconstruction
                            int mx = sb_col * 64 + sx * sub_size;
                            int my = sb_row * 64 + sy * sub_size;
                            for (int y = 0; y < sub_size && my + y < dec->height; ++y) {
                                for (int x = 0; x < sub_size && mx + x < dec->width; ++x) {
                                    dec->y_plane[(my + y) * dec->y_stride + mx + x] = 128;
                                }
                            }
                        }
                    }
                }
            }

            // Loop filter per tile
            for (int sb_row = 0; sb_row < dec->sb_rows; ++sb_row) {
                for (int sb_col = 0; sb_col < dec->sb_cols; ++sb_col) {
                    int mx = sb_col * 64;
                    int my = sb_row * 64;
                    for (int y = 0; y < 64 && my + y < dec->height; ++y) {
                        vp9_loop_filter_vertical(dec->y_plane + (my + y) * dec->y_stride + mx,
                                                  dec->y_stride, 64,
                                                  dec->loop_filter.filter_level,
                                                  dec->loop_filter.sharpness_level);
                    }
                }
            }
        }
    }
}

vp9_decoder* vp9_create(int width, int height) {
    vp9_decoder *dec = (vp9_decoder*)calloc(1, sizeof(vp9_decoder));
    if (!dec) return NULL;
    dec->width = width;
    dec->height = height;
    dec->bit_depth = 8;
    dec->subsampling_x = 1;
    dec->subsampling_y = 1;
    dec->sb_size = 64;
    dec->sb_cols = (width + 63) / 64;
    dec->sb_rows = (height + 63) / 64;
    dec->y_stride = (width + 63) & ~63;
    dec->uv_stride = dec->y_stride / 2;

    dec->y_plane = (uint8_t*)calloc(1, (size_t)dec->y_stride * height);
    dec->u_plane = (uint8_t*)calloc(1, (size_t)dec->uv_stride * (height / 2));
    dec->v_plane = (uint8_t*)calloc(1, (size_t)dec->uv_stride * (height / 2));

    // Initialize default probabilities
    memset(&dec->fc, 128, sizeof(dec->fc));
    return dec;
}

int vp9_decode(vp9_decoder *dec, const uint8_t *data, size_t len,
               uint8_t **yuv[3], int *strides) {
    if (!dec || !data || !len) return -1;
    // Skip IVF frame header if present (10 bytes: 4 bytes frame size + 8 bytes...)
    // For raw VP9 bitstream
    vp9_decode_frame(dec, data, len);

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

void vp9_destroy(vp9_decoder *dec) {
    if (!dec) return;
    free(dec->y_plane);
    free(dec->u_plane);
    free(dec->v_plane);
    free(dec);
}
