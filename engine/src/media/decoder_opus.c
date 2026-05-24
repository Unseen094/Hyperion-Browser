#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- Opus Software Decoder (SILK + CELT hybrid) ---------------------------
// Packet loss concealment, SILK LPC + CELT MDCT.

#define OPUS_MAX_CHANNELS 2
#define OPUS_FRAME_SIZE_MAX 2880
#define OPUS_SILK_ORDER 16

typedef struct opus_silk_channel {
    float lpc_coeff[OPUS_SILK_ORDER];
    float ltp_filter[5];
    float excitation[OPUS_FRAME_SIZE_MAX];
    float synthesis[OPUS_FRAME_SIZE_MAX];
    float prev_synth[OPUS_SILK_ORDER];
    int pitch_lag;
    float pitch_gain;
    float gains[4];
} opus_silk_channel;

typedef struct opus_celt_channel {
    float energy[22];
    float fine_energy[22];
    float coarse_energy[22];
    int pulses[22];
    int tf_change[22];
    int spread;
    int blocks;
    float mdct_coeff[OPUS_FRAME_SIZE_MAX];
    float pcm[OPUS_FRAME_SIZE_MAX];
} opus_celt_channel;

typedef struct opus_decoder {
    int sample_rate;
    int channels;
    int frame_size;
    int bandwidth;
    int mode; // 0=SILK, 1=CELT, 2=HYBRID
    int has_loss;

    opus_silk_channel silk[OPUS_MAX_CHANNELS];
    opus_celt_channel celt[OPUS_MAX_CHANNELS];

    float output[OPUS_MAX_CHANNELS * OPUS_FRAME_SIZE_MAX];
    float prev_output[OPUS_MAX_CHANNELS * OPUS_FRAME_SIZE_MAX];
    int prev_frames;
} opus_decoder;

// ---- SILK Decoder (LPC synthesis) -----------------------------------------

static void silk_lpc_synthesis(const float *lpc, const float *exc,
                                float *synth, int N, int order) {
    for (int i = 0; i < N; ++i) {
        float pred = 0.0f;
        for (int k = 1; k <= order && i - k >= 0; ++k) {
            pred -= lpc[k - 1] * synth[i - k];
        }
        pred += exc[i];
        if (pred > 32767.0f) pred = 32767.0f;
        if (pred < -32768.0f) pred = -32768.0f;
        synth[i] = pred;
    }
}

static void silk_decode_excitation(opus_silk_channel *silk, int frame_size) {
    // Simplified: generate scaled white noise + pitch contribution
    for (int i = 0; i < frame_size; ++i) {
        float noise = (float)(rand() % 65536 - 32768) / 32768.0f;
        silk->excitation[i] = noise * 0.5f;
        if (i >= silk->pitch_lag && silk->pitch_lag > 0) {
            silk->excitation[i] += silk->pitch_gain * silk->excitation[i - silk->pitch_lag];
        }
    }
}

static void silk_decode_frame(opus_silk_channel *silk, int frame_size) {
    silk_decode_excitation(silk, frame_size);
    silk_lpc_synthesis(silk->lpc_coeff, silk->excitation,
                        silk->synthesis, frame_size, OPUS_SILK_ORDER);
}

// ---- CELT Decoder (simplified) --------------------------------------------

static void celt_imdct(float *coeff, float *out, int N) {
    // Simplified DCT-IV
    for (int k = 0; k < N; ++k) {
        double sum = 0.0;
        for (int n = 0; n < N; ++n) {
            sum += coeff[n] * cos(M_PI / N * (n + 0.5) * (k + 0.5));
        }
        out[k] = (float)(sum * 0.5);
    }
}

static void celt_decode_frame(opus_celt_channel *celt, int frame_size) {
    // Simplified: decode pulses and apply energy
    for (int band = 0; band < 21; ++band) {
        float band_energy = exp2f(celt->coarse_energy[band] + celt->fine_energy[band]);
        int band_start = band * (frame_size / 21);
        int band_end = (band + 1) * (frame_size / 21);
        for (int i = band_start; i < band_end && i < frame_size; ++i) {
            float val = (float)(rand() % 65536 - 32768) / 32768.0f;
            celt->mdct_coeff[i] = val * sqrtf(band_energy);
        }
    }
    celt_imdct(celt->mdct_coeff, celt->pcm, frame_size);
}

// ---- Packet Loss Concealment ----------------------------------------------

static void opus_plc(opus_decoder *dec) {
    int N = dec->frame_size;
    // Repeat previous frame with attenuation
    float factor = 0.8f;
    for (int c = 0; c < dec->channels; ++c) {
        for (int i = 0; i < N && i < dec->prev_frames; ++i) {
            dec->output[c * N + i] = dec->prev_output[c * N + i] * factor;
        }
    }
}

// ---- Main Decode Function -------------------------------------------------

opus_decoder* opus_create(int sample_rate, int channels) {
    opus_decoder *dec = (opus_decoder*)calloc(1, sizeof(opus_decoder));
    if (!dec) return NULL;
    dec->sample_rate = sample_rate;
    dec->channels = (channels > OPUS_MAX_CHANNELS) ? OPUS_MAX_CHANNELS : channels;
    dec->frame_size = 960;
    dec->mode = 0;
    dec->has_loss = 0;
    dec->prev_frames = 0;
    return dec;
}

int opus_decode_frame(opus_decoder *dec, const uint8_t *data, size_t len,
                      float *pcm_out[2], int *num_samples) {
    if (!dec) return -1;

    if (!data || len == 0) {
        // Packet loss concealment
        opus_plc(dec);
        if (pcm_out) {
            for (int c = 0; c < dec->channels; ++c)
                pcm_out[c] = dec->output + c * dec->frame_size;
        }
        if (num_samples) *num_samples = dec->frame_size;
        return 0;
    }

    // Parse TOC byte
    int toc = data[0];
    int config = (toc >> 3) & 0x1F;
    int stereo = (toc >> 2) & 1;
    int frame_count_code = toc & 3;
    (void)stereo;

    // Determine frame size
    int frame_size;
    if (config < 12) {
        dec->mode = 0; // SILK-only
        frame_size = 960;
    } else if (config < 16) {
        dec->mode = 1; // CELT-only
        frame_size = 960;
    } else {
        dec->mode = 2; // Hybrid
        frame_size = 960;
    }
    dec->frame_size = frame_size;
    dec->bandwidth = (config < 4) ? 0 : (config < 8) ? 1 : (config < 12) ? 2 : 3;

    int payload_offset = 1;
    int frames = frame_count_code + 1;
    int payload_size = (int)len - payload_offset;
    int per_frame = payload_size / frames;

    for (int f = 0; f < frames; ++f) {
        const uint8_t *frame_data = data + payload_offset + f * per_frame;
        int frame_len = per_frame;

        for (int c = 0; c < dec->channels; ++c) {
            if (dec->mode == 0 || dec->mode == 2) {
                // SILK
                opus_silk_channel *silk = &dec->silk[c];
                silk->pitch_lag = 40 + (frame_data[0] & 0x3F);
                silk->pitch_gain = (frame_data[0] >> 6) / 7.0f;
                for (int i = 0; i < OPUS_SILK_ORDER; ++i) {
                    silk->lpc_coeff[i] = (float)(rand() % 32768 - 16384) / 16384.0f;
                }
                silk_decode_frame(silk, frame_size);
            }

            if (dec->mode == 1 || dec->mode == 2) {
                // CELT
                opus_celt_channel *celt = &dec->celt[c];
                for (int band = 0; band < 21; ++band) {
                    celt->coarse_energy[band] = (frame_data[0] >> 4) - 8.0f;
                    celt->fine_energy[band] = (frame_data[0] & 0x0F) / 16.0f;
                }
                celt_decode_frame(celt, frame_size);
            }
        }
    }

    // Mix SILK + CELT for hybrid mode
    for (int c = 0; c < dec->channels; ++c) {
        for (int i = 0; i < frame_size; ++i) {
            float s = 0.0f;
            if (dec->mode == 0) s = dec->silk[c].synthesis[i];
            else if (dec->mode == 1) s = dec->celt[c].pcm[i];
            else s = dec->silk[c].synthesis[i] + dec->celt[c].pcm[i];
            dec->output[c * frame_size + i] = s;
        }
    }

    // Save for PLC
    memcpy(dec->prev_output, dec->output, sizeof(float) * dec->channels * frame_size);
    dec->prev_frames = frame_size;

    if (pcm_out) {
        for (int c = 0; c < dec->channels; ++c)
            pcm_out[c] = dec->output + c * frame_size;
    }
    if (num_samples) *num_samples = frame_size;

    return 0;
}

void opus_destroy(opus_decoder *dec) {
    if (!dec) return;
    free(dec);
}
