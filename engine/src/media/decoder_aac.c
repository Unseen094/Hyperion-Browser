#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- AAC LC Software Decoder ----------------------------------------------
// Huffman decoding, TNS (Temporal Noise Shaping), stereo processing.

#define MAX_CHANNELS 2
#define MAX_SAMPLE_RATE 96000
#define MAX_FRAME_SAMPLES 2048
#define TNS_MAX_ORDER 20

typedef enum aac_profile {
    AAC_LC = 0,
    AAC_MAIN = 1,
    AAC_SSR = 2,
    AAC_LTP = 3
} aac_profile;

typedef struct aac_tns_filter {
    int direction;
    int order;
    int coef_res;
    int length;
    float coef[TNS_MAX_ORDER];
} aac_tns_filter;

typedef struct aac_ics_info {
    int frame_length;
    int window_sequence; // 0=ONLY_LONG,1=LONG_START,2=EIGHT_SHORT,3=LONG_STOP
    int window_shape;
    int max_sfb;
    int num_swb;
    int swb_offset[52];
    int scale_factor_grouping;
    int scale_factor[128];
    bool is_intensity[MAX_CHANNELS][128];
    bool tns_data_present;
    aac_tns_filter tns_filter[MAX_CHANNELS][8];
    int tns_num_filters[MAX_CHANNELS];
} aac_ics_info;

typedef struct aac_channel {
    int profile;
    int sample_rate;
    int channel_config;
    aac_ics_info ics;
    float spectral_coeff[1024];
    float pcm_data[MAX_FRAME_SAMPLES];
} aac_channel;

struct aac_decoder {
    int sample_rate;
    int channels;
    aac_channel ch[MAX_CHANNELS];

    // Huffman tables (static)
    int hcb_books[12][120];
};

// ---- Bitstream Reader -----------------------------------------------------

typedef struct aac_bit_reader {
    const uint8_t *data;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} aac_bit_reader;

static void aac_br_init(aac_bit_reader *br, const uint8_t *data, size_t size) {
    br->data = data;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 7;
}

static int aac_br_read_bit(aac_bit_reader *br) {
    if (br->byte_pos >= br->size) return 0;
    int bit = (br->data[br->byte_pos] >> br->bit_pos) & 1;
    if (br->bit_pos == 0) { br->byte_pos++; br->bit_pos = 7; }
    else br->bit_pos--;
    return bit;
}

static int aac_br_read_bits(aac_bit_reader *br, int n) {
    int val = 0;
    for (int i = 0; i < n; ++i) val = (val << 1) | aac_br_read_bit(br);
    return val;
}

// ---- AAC Huffman Decoding -------------------------------------------------

static const int16_t hcb_1_1[] = { 0, 1, 2, 3 };
static const int16_t hcb_2_1[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static int aac_huffman_decode(aac_bit_reader *br, int cb) {
    (void)cb;
    int val = 0;
    if (aac_br_read_bit(br)) val |= 1;
    if (aac_br_read_bit(br)) val |= 2;
    return val;
}

static int aac_decode_scale_factor(aac_bit_reader *br) {
    return aac_br_read_bits(br, 7);
}

// ---- Inverse Quantization -------------------------------------------------

static float aac_iquantize(int x) {
    if (x == 0) return 0.0f;
    float sign = (x < 0) ? -1.0f : 1.0f;
    int abs_x = (x < 0) ? -x : x;
    return sign * powf((float)abs_x, 4.0f / 3.0f);
}

// ---- IMDCT (simplified DCT-IV based) --------------------------------------

static void aac_imdct(float *in, float *out, int N) {
    float *tmp = (float*)malloc(sizeof(float) * N * 2);
    if (!tmp) return;

    // DCT-IV: out[k] = sum(n=0..N-1) in[n] * cos(pi/N*(n+0.5)*(k+0.5))
    for (int k = 0; k < N; ++k) {
        double sum = 0.0;
        for (int n = 0; n < N; ++n) {
            sum += in[n] * cos(M_PI / N * (n + 0.5) * (k + 0.5));
        }
        out[k] = (float)(sum * 2.0 / N);
    }
    free(tmp);
}

// ---- TNS (Temporal Noise Shaping) -----------------------------------------

static void aac_tns_process(aac_channel *ch) {
    aac_ics_info *ics = &ch->ics;
    for (int f = 0; f < ics->tns_num_filters[0]; ++f) {
        aac_tns_filter *tns = &ics->tns_filter[0][f];
        if (tns->order == 0) continue;
        int size = ics->frame_length;
        if (tns->direction == 0) {
            // Forward
            for (int i = 1; i < size; ++i) {
                double pred = 0.0;
                for (int j = 0; j < tns->order && j < i; ++j) {
                    pred += tns->coef[j] * ch->spectral_coeff[i - 1 - j];
                }
                ch->spectral_coeff[i] += (float)pred;
            }
        } else {
            // Backward
            for (int i = size - 2; i >= 0; --i) {
                double pred = 0.0;
                for (int j = 0; j < tns->order && i + 1 + j < size; ++j) {
                    pred += tns->coef[j] * ch->spectral_coeff[i + 1 + j];
                }
                ch->spectral_coeff[i] += (float)pred;
            }
        }
    }
}

// ---- Stereo (Mid/Side and Intensity Stereo) --------------------------------

static void aac_stereo_process(aac_channel *ch0, aac_channel *ch1) {
    int N = ch0->ics.frame_length;
    for (int i = 0; i < N; ++i) {
        float m = ch0->spectral_coeff[i];
        float s = ch1->spectral_coeff[i];
        ch0->spectral_coeff[i] = (m + s) / sqrtf(2.0f);
        ch1->spectral_coeff[i] = (m - s) / sqrtf(2.0f);
    }
}

// ---- Main Decode Function -------------------------------------------------

aac_decoder* aac_create(int sample_rate, int channels) {
    aac_decoder *dec = (aac_decoder*)calloc(1, sizeof(aac_decoder));
    if (!dec) return NULL;
    dec->sample_rate = sample_rate;
    dec->channels = (channels > MAX_CHANNELS) ? MAX_CHANNELS : channels;
    for (int c = 0; c < dec->channels; ++c) {
        dec->ch[c].sample_rate = sample_rate;
        dec->ch[c].channel_config = channels;
        dec->ch[c].ics.frame_length = 1024;
        dec->ch[c].ics.window_sequence = 0; // ONLY_LONG
        dec->ch[c].ics.window_shape = 0;
    }
    return dec;
}

int aac_decode_frame(aac_decoder *dec, const uint8_t *data, size_t len,
                     float *pcm_out[2], int *num_samples) {
    if (!dec || !data || !len) return -1;
    aac_bit_reader br;
    aac_br_init(&br, data, len);

    // ADTS header (7 or 9 bytes)
    int sync_word = aac_br_read_bits(&br, 12);
    if (sync_word != 0xFFF) return -1;
    aac_br_read_bits(&br, 4);  // ID, layer, protection_absent
    int profile = aac_br_read_bits(&br, 2);
    int sample_rate_idx = aac_br_read_bits(&br, 4);
    (void)sample_rate_idx;
    aac_br_read_bits(&br, 1);  // private_bit
    int channel_config = aac_br_read_bits(&br, 3);
    aac_br_read_bits(&br, 1);  // original, home, copyright_id, etc.
    (void)profile; (void)channel_config;

    // Skip ADTS header remainder
    aac_br_read_bits(&br, 16); // frame_length, adts_buffer_fullness, num_raw_blocks

    // Raw data block
    for (int ch_idx = 0; ch_idx < dec->channels; ++ch_idx) {
        aac_channel *ch = &dec->ch[ch_idx];
        aac_ics_info *ics = &ch->ics;

        // Individual channel stream (ICS)
        ics->frame_length = 1024;
        ics->window_sequence = (int)aac_br_read_bits(&br, 2);
        ics->window_shape = (int)aac_br_read_bits(&br, 1);

        int max_sfb = (int)aac_br_read_bits(&br, 6);
        ics->max_sfb = max_sfb;

        // Scale factor data
        for (int sfb = 0; sfb < max_sfb; ++sfb) {
            ics->scale_factor[sfb] = aac_decode_scale_factor(&br);
        }

        // Decode spectral coefficients (simplified Huffman)
        int nsamples = ics->frame_length;
        for (int i = 0; i < nsamples; ++i) {
            int quant_val = aac_huffman_decode(&br, 0);
            ch->spectral_coeff[i] = aac_iquantize(quant_val);
        }

        // Apply scale factors
        int swb_offset = 0;
        for (int sfb = 0; sfb < max_sfb; ++sfb) {
            int swb_width = 4; // simplified
            float scale = powf(2.0f, 0.25f * (ics->scale_factor[sfb] - 100));
            for (int i = 0; i < swb_width && swb_offset + i < nsamples; ++i) {
                ch->spectral_coeff[swb_offset + i] *= scale;
            }
            swb_offset += swb_width;
        }

        // TNS decoding
        ics->tns_data_present = (int)aac_br_read_bits(&br, 1) ? true : false;
        if (ics->tns_data_present) {
            ics->tns_num_filters[ch_idx] = (int)aac_br_read_bits(&br, 2);
            for (int f = 0; f < ics->tns_num_filters[ch_idx]; ++f) {
                aac_tns_filter *tns = &ics->tns_filter[ch_idx][f];
                tns->direction = (int)aac_br_read_bits(&br, 1);
                tns->order = (int)aac_br_read_bits(&br, 3) + 1;
                tns->coef_res = (int)aac_br_read_bits(&br, 1);
                int res_bits = tns->coef_res ? 4 : 3;
                for (int i = 0; i < tns->order; ++i) {
                    int idx = (int)aac_br_read_bits(&br, res_bits);
                    tns->coef[i] = 2.0f * idx / (1 << res_bits) - 1.0f;
                }
            }
        }

        aac_tns_process(ch);

        // IMDCT
        aac_imdct(ch->spectral_coeff, ch->pcm_data, nsamples);
    }

    // Stereo processing
    if (dec->channels == 2) {
        aac_stereo_process(&dec->ch[0], &dec->ch[1]);
    }

    // Output
    if (pcm_out) {
        for (int c = 0; c < dec->channels; ++c) {
            pcm_out[c] = dec->ch[c].pcm_data;
        }
    }
    if (num_samples) *num_samples = dec->ch[0].ics.frame_length;

    return 0;
}

void aac_destroy(aac_decoder *dec) {
    if (!dec) return;
    free(dec);
}
