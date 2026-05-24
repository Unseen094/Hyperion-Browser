#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- WebM / Matroska EBML Demuxer -----------------------------------------
// Parses EBML header, Segment -> Info, Tracks, Cluster blocks.

#define EBML_VINT_SIZE(byte) ((byte) == 0 ? 0 : (int)(8 - ((32 - __builtin_clz((byte))) / 8)))
#define EBML_VINT_MASK(size) ((1ULL << (7 + (size) * 7)) - 1)

static uint64_t ebml_read_vint(const uint8_t *data, size_t remain, int *size_out) {
    if (remain < 1) { *size_out = 0; return 0; }
    uint8_t lead = data[0];
    if (lead == 0) { *size_out = 0; return 0; } // unknown length
    int size = 1;
    while (size < 9 && !(lead & (1 << (8 - size)))) size++;
    if (size > 9 || (size_t)size > remain) { *size_out = 0; return 0; }
    uint64_t val = lead & ((1 << (8 - size)) - 1);
    for (int i = 1; i < size; ++i) val = (val << 8) | data[i];
    *size_out = size;
    return val;
}

static uint64_t ebml_read_uint(const uint8_t *data, int size) {
    uint64_t v = 0;
    for (int i = 0; i < size; ++i) v = (v << 8) | data[i];
    return v;
}

static double ebml_read_float(const uint8_t *data, int size) {
    if (size == 4) {
        uint32_t bits = (uint32_t)ebml_read_uint(data, 4);
        float f;
        memcpy(&f, &bits, sizeof(f));
        return (double)f;
    } else if (size == 8) {
        uint64_t bits = ebml_read_uint(data, 8);
        double d;
        memcpy(&d, &bits, sizeof(d));
        return d;
    }
    return 0.0;
}

// EBML Element IDs
#define EBML_ID_EBML          0x1A45DFA3ULL
#define EBML_ID_SEGMENT       0x18538067ULL
#define EBML_ID_INFO          0x1549A966ULL
#define EBML_ID_TIMECODESCALE 0x2AD7B1ULL
#define EBML_ID_DURATION      0x4489ULL
#define EBML_ID_TRACKS        0x1654AE6BULL
#define EBML_ID_TRACKENTRY    0xAEULL
#define EBML_ID_TRACKNUMBER   0xD7ULL
#define EBML_ID_TRACKTYPE     0x83ULL
#define EBML_ID_CODECID       0x86ULL
#define EBML_ID_PIXELWIDTH    0xB0ULL
#define EBML_ID_PIXELHEIGHT   0xBAULL
#define EBML_ID_CLUSTER       0x1F43B675ULL
#define EBML_ID_SIMPLEBLOCK   0xA3ULL
#define EBML_ID_BLOCKGROUP    0xA0ULL
#define EBML_ID_BLOCK         0xA1ULL
#define EBML_ID_TIMECODE      0xE7ULL
#define EBML_ID_BLOCKDURATION 0x9BULL
#define EBML_ID_REFERENCEBLOCK 0xFBULL

#define TRACK_TYPE_VIDEO 1
#define TRACK_TYPE_AUDIO 2

typedef struct webm_track {
    int track_number;
    int type;
    int codec; // 1=VP9, 2=VP8, 3=Opus, 4=Vorbis
    int width, height;
    int64_t codec_private_size;
    uint8_t *codec_private;
    int64_t default_duration;
} webm_track;

typedef struct webm_demuxer {
    const uint8_t *data;
    size_t len;
    int track_count;
    webm_track tracks[8];
    int64_t timecode_scale;
    double duration;
    int64_t segment_start;
    int64_t segment_end;
    int64_t cluster_pos;
    int64_t cluster_timecode;
    int current_cluster_block;
    int blocks_parsed;
    int64_t next_cluster_pos;
} webm_demuxer;

static void webm_parse_track_entry(webm_demuxer *d, const uint8_t *data, size_t remain) {
    if (d->track_count >= 8) return;
    webm_track *t = &d->tracks[d->track_count];
    memset(t, 0, sizeof(webm_track));
    t->codec_private = NULL;
    size_t pos = 0;
    while (pos + 1 < remain) {
        int id_size, data_size;
        uint64_t id = ebml_read_vint(data + pos, remain - pos, &id_size);
        if (id_size == 0) break;
        pos += id_size;
        uint64_t val_size = ebml_read_vint(data + pos, remain - pos, &data_size);
        if (data_size == 0) break;
        pos += data_size;
        if (pos + val_size > remain) break;

        if (id == EBML_ID_TRACKNUMBER) {
            t->track_number = (int)ebml_read_uint(data + pos, (int)val_size);
        } else if (id == EBML_ID_TRACKTYPE) {
            t->type = (int)ebml_read_uint(data + pos, (int)val_size);
        } else if (id == EBML_ID_CODECID) {
            char codec_str[64] = {0};
            int csize = (int)val_size < 63 ? (int)val_size : 63;
            memcpy(codec_str, data + pos, csize);
            if (strcmp(codec_str, "V_VP9") == 0) t->codec = 1;
            else if (strcmp(codec_str, "V_VP8") == 0) t->codec = 2;
            else if (strcmp(codec_str, "A_OPUS") == 0) t->codec = 3;
            else if (strcmp(codec_str, "A_VORBIS") == 0) t->codec = 4;
        } else if (id == EBML_ID_PIXELWIDTH) {
            t->width = (int)ebml_read_uint(data + pos, (int)val_size);
        } else if (id == EBML_ID_PIXELHEIGHT) {
            t->height = (int)ebml_read_uint(data + pos, (int)val_size);
        } else if (id == 0x63A2ULL) { // CodecPrivate
            t->codec_private_size = (int64_t)val_size;
            t->codec_private = (uint8_t*)malloc((size_t)val_size);
            if (t->codec_private) memcpy(t->codec_private, data + pos, (size_t)val_size);
        } else if (id == 0x23E383ULL) { // DefaultDuration
            t->default_duration = (int64_t)ebml_read_uint(data + pos, (int)val_size);
        }
        pos += (size_t)val_size;
    }
    d->track_count++;
}

static void webm_parse_cluster(webm_demuxer *d, const uint8_t *data, size_t remain) {
    size_t pos = 0;
    while (pos + 1 < remain) {
        int id_size, data_size;
        uint64_t id = ebml_read_vint(data + pos, remain - pos, &id_size);
        if (id_size == 0) break;
        pos += id_size;
        uint64_t val_size = ebml_read_vint(data + pos, remain - pos, &data_size);
        if (data_size == 0) break;
        pos += data_size;
        if (pos + val_size > remain) break;

        if (id == EBML_ID_TIMECODE) {
            d->cluster_timecode = (int64_t)ebml_read_uint(data + pos, (int)val_size);
        } else if (id == EBML_ID_SIMPLEBLOCK) {
            // Parse SimpleBlock header: track number (vint), timecode (int16), flags
            const uint8_t *bp = data + pos;
            size_t bremain = (size_t)val_size;
            int track_size;
            uint64_t block_track = ebml_read_vint(bp, bremain, &track_size);
            if (track_size > 0) {
                bp += track_size; bremain -= track_size;
                if (bremain >= 3) {
                    int16_t block_time = (int16_t)((bp[0] << 8) | bp[1]);
                    int flags = bp[2];
                    (void)flags;
                    int64_t pts = d->cluster_timecode + block_time;
                    pts = pts * d->timecode_scale / 1000000;
                    (void)pts;
                    bp += 3; bremain -= 3;
                    // Frame data starts at bp
                    // In a real demuxer we cache these for read_sample
                }
            }
        }
        pos += (size_t)val_size;
    }
}

webm_demuxer* webm_open(const uint8_t *data, size_t len) {
    if (!data || len < 64) return NULL;
    webm_demuxer *d = (webm_demuxer*)calloc(1, sizeof(webm_demuxer));
    if (!d) return NULL;
    d->data = data;
    d->len = len;
    d->timecode_scale = 1000000;
    d->duration = 0.0;
    d->segment_start = -1;
    d->cluster_pos = -1;
    d->next_cluster_pos = -1;

    size_t offset = 0;
    // Expect EBML header
    if (offset + 4 > len) { free(d); return NULL; }
    int id_size, data_size;
    uint64_t id = ebml_read_vint(data + offset, len - offset, &id_size);
    if (id != EBML_ID_EBML) { free(d); return NULL; }
    offset += id_size;
    uint64_t ebml_size = ebml_read_vint(data + offset, len - offset, &data_size);
    offset += data_size + (size_t)ebml_size;

    // Expect Segment
    if (offset + id_size > len) { free(d); return NULL; }
    id = ebml_read_vint(data + offset, len - offset, &id_size);
    if (id != EBML_ID_SEGMENT) { free(d); return NULL; }
    offset += id_size;
    uint64_t segment_size = ebml_read_vint(data + offset, len - offset, &data_size);
    offset += data_size;
    d->segment_start = (int64_t)offset;
    d->segment_end = (segment_size > 0) ? (int64_t)(offset + segment_size) : (int64_t)len;

    while (offset < (size_t)d->segment_end && offset + 1 < len) {
        id = ebml_read_vint(data + offset, len - offset, &id_size);
        if (id_size == 0) break;
        offset += id_size;
        uint64_t val_size = ebml_read_vint(data + offset, len - offset, &data_size);
        if (data_size == 0) break;
        offset += data_size;
        if (offset + val_size > len) break;

        if (id == EBML_ID_INFO) {
            size_t info_end = offset + (size_t)val_size;
            size_t ipos = offset;
            while (ipos + 1 < info_end && ipos < len) {
                int si, ss;
                uint64_t sid = ebml_read_vint(data + ipos, info_end - ipos, &si);
                if (si == 0) break;
                ipos += si;
                uint64_t sv = ebml_read_vint(data + ipos, info_end - ipos, &ss);
                if (ss == 0) break;
                ipos += ss;
                if (ipos + sv > info_end) break;
                if (sid == EBML_ID_TIMECODESCALE && sv >= 4) {
                    d->timecode_scale = (int64_t)ebml_read_uint(data + ipos, (int)sv);
                } else if (sid == EBML_ID_DURATION && (sv == 4 || sv == 8)) {
                    d->duration = ebml_read_float(data + ipos, (int)sv);
                }
                ipos += (size_t)sv;
            }
        } else if (id == EBML_ID_TRACKS) {
            size_t tracks_end = offset + (size_t)val_size;
            size_t tpos = offset;
            while (tpos + 1 < tracks_end && tpos < len) {
                int si, ss;
                uint64_t tid = ebml_read_vint(data + tpos, tracks_end - tpos, &si);
                if (si == 0 || tid != EBML_ID_TRACKENTRY) break;
                tpos += si;
                uint64_t tv = ebml_read_vint(data + tpos, tracks_end - tpos, &ss);
                if (ss == 0) break;
                tpos += ss;
                if (tpos + tv > tracks_end) break;
                webm_parse_track_entry(d, data + tpos, (size_t)tv);
                tpos += (size_t)tv;
            }
        } else if (id == EBML_ID_CLUSTER) {
            d->cluster_pos = (int64_t)offset - data_size - id_size;
            d->next_cluster_pos = (int64_t)(offset + val_size);
            webm_parse_cluster(d, data + offset, (size_t)val_size);
            break; // parse first cluster only
        }
        offset += (size_t)val_size;
    }

    return d;
}

int webm_get_track_count(webm_demuxer *d) {
    return d ? d->track_count : 0;
}

int webm_get_video_codec(webm_demuxer *d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    if (d->tracks[track].type != TRACK_TYPE_VIDEO) return 0;
    return d->tracks[track].codec;
}

int webm_get_audio_codec(webm_demuxer *d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    if (d->tracks[track].type != TRACK_TYPE_AUDIO) return 0;
    return d->tracks[track].codec;
}

int webm_get_width(webm_demuxer *d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    return d->tracks[track].width;
}

int webm_get_height(webm_demuxer *d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    return d->tracks[track].height;
}

double webm_get_duration(webm_demuxer *d) {
    if (!d) return 0.0;
    return d->duration;
}

int webm_read_sample(webm_demuxer *d, int track, uint8_t *out, size_t *out_len) {
    if (!d || track < 0 || track >= d->track_count || !out || !out_len) return 0;
    (void)track;
    // In a real implementation, iterate clusters and cache blocks per track
    return 0;
}

void webm_close(webm_demuxer *d) {
    if (!d) return;
    for (int i = 0; i < d->track_count; ++i) {
        free(d->tracks[i].codec_private);
    }
    free(d);
}
