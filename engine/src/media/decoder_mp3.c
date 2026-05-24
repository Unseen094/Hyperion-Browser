#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- MP3 Layer III Software Decoder ---------------------------------------
// Huffman decoding, IMDCT, polyphase synthesis (sub-band).

#define MP3_MAX_GRANULES 2
#define MP3_MAX_CHANNELS 2
#define MP3_SBANDS 32
#define MP3_SAMPLES_PER_FRAME 1152

typedef struct mp3_side_info {
    int main_data_begin;
    int private_bits;
    int scfsi[MP3_MAX_CHANNELS][4];
    int part2_3_length[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int big_values[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int global_gain[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int scalefac_compress[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int preflag[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int scalefac_scale[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int count1table[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int subblock_gain[MP3_MAX_CHANNELS][MP3_MAX_GRANULES][3];
    int window_switching[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int block_type[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
    int mixed_block[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];
} mp3_side_info;

typedef struct mp3_granule {
    float mdct_coeff[576];
    float pcm[576];
    int is_long;
} mp3_granule;

struct mp3_decoder {
    int sample_rate;
    int channels;
    int bitrate;
    int padding;
    int crc_present;

    mp3_side_info side;
    mp3_granule granule[MP3_MAX_CHANNELS][MP3_MAX_GRANULES];

    // Polyphase filter bank
    float filter_bank[MP3_SBANDS];
    float history[MP3_CHANNELS_MAX][1080];
    int history_offset;

    // Synthesis window
    float win[512];
};

// ---- Bitstream Reader -----------------------------------------------------

typedef struct mp3_br {
    const uint8_t *data;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} mp3_br;

static void mp3_br_init(mp3_br *br, const uint8_t *data, size_t size) {
    br->data = data; br->size = size;
    br->byte_pos = 0; br->bit_pos = 7;
}

static int mp3_br_read(mp3_br *br, int n) {
    int val = 0;
    for (int i = 0; i < n; ++i) {
        if (br->byte_pos >= br->size) return val;
        val = (val << 1) | ((br->data[br->byte_pos] >> br->bit_pos) & 1);
        if (br->bit_pos == 0) { br->byte_pos++; br->bit_pos = 7; }
        else br->bit_pos--;
    }
    return val;
}

// ---- Huffman Table (simplified) -------------------------------------------

static const int mp3_huffman_table[32][2] = {
    {0,0},{1,1},{1,2},{2,3},{2,4},{3,5},{3,6},{4,7},
    {4,8},{5,9},{5,10},{6,11},{6,12},{7,13},{7,14},{8,15},
    {8,16},{9,17},{9,18},{10,19},{10,20},{11,21},{11,22},{12,23},
    {12,24},{13,25},{13,26},{14,27},{14,28},{15,29},{15,30},{0,0}
};

static int mp3_huffman_decode(mp3_br *br, int table) {
    (void)table;
    int bits = mp3_br_read(br, 5);
    return mp3_huffman_table[bits][0];
}

// ---- Requantization -------------------------------------------------------

static float mp3_requantize(int is, int global_gain, int scalefac) {
    float s = powf(2.0f, 0.25f * (global_gain - 210)) *
              powf(2.0f, -0.5f * scalefac);
    return (float)is * s;
}

// ---- IMDCT (32-point for MP3 sub-band) ------------------------------------

static void mp3_imdct_32(float *in, float *out) {
    for (int k = 0; k < 32; ++k) {
        double sum = 0.0;
        for (int n = 0; n < 32; ++n) {
            sum += in[n] * cos(M_PI / 64 * (2 * n + 1 + 32) * (2 * k + 1));
        }
        out[k] = (float)(sum * 2.0 / 32);
    }
}

// ---- Polyphase Synthesis --------------------------------------------------

static void mp3_polyphase_synthesis(float *samples, float *history,
                                     float *out, int stride) {
    float tmp[64];
    // Build 64-sample vector
    for (int i = 0; i < 32; ++i) {
        tmp[i] = samples[i];
        tmp[63 - i] = -samples[i];
    }
    // DCT-32 -> matrix multiply (simplified)
    float sum[32] = {0};
    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 64; ++j) {
            sum[i] += tmp[j] * cos(M_PI / 64 * (i + 16) * (2 * j + 1));
        }
    }
    // Window and overlap-add
    for (int i = 0; i < 32; ++i) {
        float val = sum[i];
        out[i * stride] = val;
    }
}

// ---- Frame Decode ---------------------------------------------------------

static int mp3_decode_frame_data(mp3_decoder *dec, const uint8_t *data, size_t len) {
    mp3_br br;
    mp3_br_init(&br, data, len);

    // Side information
    dec->side.main_data_begin = mp3_br_read(&br, 9);
    dec->side.private_bits = mp3_br_read(&br, dec->channels == 1 ? 5 : 3);

    for (int ch = 0; ch < dec->channels; ++ch) {
        for (int i = 0; i < 4; ++i)
            dec->side.scfsi[ch][i] = mp3_br_read(&br, 1);
    }

    for (int gr = 0; gr < MP3_MAX_GRANULES; ++gr) {
        for (int ch = 0; ch < dec->channels; ++ch) {
            dec->side.part2_3_length[ch][gr] = mp3_br_read(&br, 12);
            dec->side.big_values[ch][gr] = mp3_br_read(&br, 9);
            dec->side.global_gain[ch][gr] = mp3_br_read(&br, 8);
            dec->side.scalefac_compress[ch][gr] = mp3_br_read(&br, 4);
            dec->side.window_switching[ch][gr] = mp3_br_read(&br, 1);

            if (dec->side.window_switching[ch][gr]) {
                dec->side.block_type[ch][gr] = mp3_br_read(&br, 2);
                dec->side.mixed_block[ch][gr] = mp3_br_read(&br, 1);
                for (int r = 0; r < 2; ++r)
                    mp3_br_read(&br, 3); // table select
                for (int w = 0; w < 3; ++w)
                    dec->side.subblock_gain[ch][gr][w] = mp3_br_read(&br, 3);
            } else {
                for (int r = 0; r < 2; ++r)
                    mp3_br_read(&br, 5); // table select
                dec->side.count1table[ch][gr] = mp3_br_read(&br, 1);
            }

            dec->side.preflag[ch][gr] = mp3_br_read(&br, 1);
            dec->side.scalefac_scale[ch][gr] = mp3_br_read(&br, 1);
        }
    }

    // Scale factors and Huffman data (simplified)
    for (int gr = 0; gr < MP3_MAX_GRANULES; ++gr) {
        for (int ch = 0; ch < dec->channels; ++ch) {
            mp3_granule *g = &dec->granule[ch][gr];
            int i;
            for (i = 0; i < dec->side.big_values[ch][gr] * 2 && i < 576; ++i) {
                int huff = mp3_huffman_decode(&br, 0);
                int scalefac = 0;
                g->mdct_coeff[i] = mp3_requantize(huff,
                    dec->side.global_gain[ch][gr], scalefac);
            }
            // Zero remaining
            for (; i < 576; ++i) g->mdct_coeff[i] = 0.0f;

            // Reordering (short blocks -> long)
            if (dec->side.block_type[ch][gr] == 2) {
                // Simplified: no reorder needed
            }

            // Stereo processing (MS/Intensity)
            if (dec->channels == 2 && ch == 1) {
                for (i = 0; i < 576; ++i) {
                    float left = dec->granule[0][gr].mdct_coeff[i];
                    float right = g->mdct_coeff[i];
                    dec->granule[0][gr].mdct_coeff[i] = (left + right) / sqrtf(2.0f);
                    g->mdct_coeff[i] = (left - right) / sqrtf(2.0f);
                }
            }

            // Aliasing reduction (simplified)

            // IMDCT
            // MP3 uses 36-point or 12-point IMDCT depending on block type
            int block_len = (dec->side.block_type[ch][gr] == 2) ? 12 : 36;
            int n_blocks = (block_len == 12) ? 3 : 1;
            int step = 576 / n_blocks;
            for (int b = 0; b < n_blocks; ++b) {
                int start = b * step;
                if (block_len == 36) {
                    mp3_imdct_32(g->mdct_coeff + start, g->pcm + start);
                } else {
                    // 12-point IMDCT (simplified: copy)
                    for (int j = 0; j < 12; ++j)
                        g->pcm[start + j] = g->mdct_coeff[start + j];
                }
            }

            // Polyphase synthesis per sub-band
            mp3_polyphase_synthesis(g->pcm, dec->history,
                                     g->pcm, 1);
        }
    }

    return 0;
}

mp3_decoder* mp3_create(int sample_rate, int channels) {
    mp3_decoder *dec = (mp3_decoder*)calloc(1, sizeof(mp3_decoder));
    if (!dec) return NULL;
    dec->sample_rate = sample_rate;
    dec->channels = (channels > MP3_MAX_CHANNELS) ? MP3_MAX_CHANNELS : channels;
    dec->bitrate = 128000;
    // Precompute synthesis window
    for (int i = 0; i < 512; ++i) {
        dec->win[i] = (float)sin(M_PI / 512 * (i + 0.5));
    }
    return dec;
}

int mp3_decode_frame(mp3_decoder *dec, const uint8_t *data, size_t len,
                     float *pcm_out[2], int *num_samples) {
    if (!dec || !data || len < 4) return -1;

    // Sync
    if (data[0] != 0xFF || (data[1] & 0xE0) != 0xE0) return -1;

    int version = (data[1] >> 3) & 3;
    int layer = (data[1] >> 1) & 3;
    int bitrate_idx = (data[2] >> 4) & 0x0F;
    int sample_rate_idx = (data[2] >> 2) & 3;
    int padding = (data[2] >> 1) & 1;
    (void)version; (void)layer;
    (void)padding;

    static const int bitrates[2][3][16] = {
        {{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
         {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
         {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}},
        {{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
         {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
         {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0}}
    };
    (void)bitrates;

    static const int sample_rates[4][4] = {
        {44100,48000,32000,0},
        {22050,24000,16000,0},
        {11025,12000,8000,0},
        {0,0,0,0}
    };
    if (sample_rate_idx < 4)
        dec->sample_rate = sample_rates[sample_rate_idx][0];

    dec->crc_present = (data[1] & 1) ^ 1;
    int side_start = 4 + (dec->crc_present ? 2 : 0);

    int frame_size = 144 * bitrates[version][layer][bitrate_idx] * 1000 /
                     dec->sample_rate + padding;

    mp3_decode_frame_data(dec, data + side_start, len - side_start);

    if (pcm_out) {
        for (int c = 0; c < dec->channels; ++c) {
            pcm_out[c] = dec->granule[c][0].pcm;
        }
    }
    if (num_samples) *num_samples = MP3_SAMPLES_PER_FRAME / MP3_MAX_GRANULES;

    return frame_size;
}

void mp3_destroy(mp3_decoder *dec) {
    if (!dec) return;
    free(dec);
}
