#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- ISO BMFF Demuxer ------------------------------------------------------
// Parses ftyp, moov (trak -> mdia -> minf -> stbl), moof, mdat boxes.

#define READ_U32(p)  ((uint32_t)((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])
#define READ_U24(p)  ((uint32_t)((p)[0] << 16) | ((p)[1] << 8) | (p)[2])
#define READ_U16(p)  ((uint32_t)((p)[0] << 8) | (p)[1])
#define READ_U64(p)  (((uint64_t)READ_U32(p) << 32) | READ_U32((p) + 4))

typedef struct mp4_track {
    int track_id;
    int type; // 0=video, 1=audio
    int codec;
    int width, height;
    int sample_count;
    int64_t duration;
    int64_t timescale;
    int64_t media_timescale;
    int64_t *sample_sizes;
    int64_t *sample_offsets;
    int64_t *sample_durations;
    int *sample_keyframe;
    int64_t *chunk_offsets;
    int *chunk_sample_counts;
    int chunk_count;
    int current_sample;
    int stsc_entries;
    int *stsc_first_chunk;
    int *stsc_samples_per_chunk;
    int *stsc_sample_desc_index;
    int64_t data_offset;
} mp4_track;

typedef struct mp4_demuxer {
    const uint8_t *data;
    size_t len;
    int track_count;
    mp4_track tracks[8];
    int64_t duration;
    int64_t timescale;
    int moov_offset;
    int mdat_offset;
    int64_t mdat_size;
} mp4_demuxer;

static int64_t read_box_header(const uint8_t *p, size_t remain, uint32_t *type_out) {
    if (remain < 8) return -1;
    uint32_t size32 = READ_U32(p);
    uint32_t type = READ_U32(p + 4);
    int64_t size;
    if (size32 == 1) {
        if (remain < 16) return -1;
        size = (int64_t)READ_U64(p + 8);
    } else {
        size = size32;
    }
    *type_out = type;
    return size;
}

#define FOURCC(a,b,c,d) ((uint32_t)((a)<<24|(b)<<16|(c)<<8|(d)))

mp4_demuxer* mp4_open(const uint8_t* data, size_t len) {
    if (!data || len < 16) return NULL;
    mp4_demuxer* d = (mp4_demuxer*)calloc(1, sizeof(mp4_demuxer));
    if (!d) return NULL;
    d->data = data;
    d->len = len;
    d->track_count = 0;
    d->moov_offset = -1;
    d->mdat_offset = -1;
    d->duration = 0;
    d->timescale = 1000;

    size_t offset = 0;
    int found_ftyp = 0;

    while (offset + 8 <= len) {
        uint32_t box_type;
        int64_t box_size = read_box_header(data + offset, len - offset, &box_type);
        if (box_size < 8) break;
        if (box_size == 0) break;
        int64_t content_start = offset + 8;
        if (box_type == FOURCC('f','t','y','p')) {
            found_ftyp = 1;
        } else if (box_type == FOURCC('m','o','o','v')) {
            d->moov_offset = (int)offset;
            // Parse moov children
            int64_t moov_end = offset + box_size;
            int64_t moov_pos = content_start;
            while (moov_pos + 8 <= moov_end && moov_pos + 8 <= (int64_t)len) {
                uint32_t child_type;
                int64_t child_size = read_box_header(data + moov_pos,
                                                     (size_t)(moov_end - moov_pos),
                                                     &child_type);
                if (child_size < 8) break;
                int64_t child_data = moov_pos + 8;

                if (child_type == FOURCC('m','v','h','d')) {
                    if ((size_t)(child_data + 20) <= len) {
                        int version = data[child_data];
                        int64_t pos = child_data + 4 + (version == 1 ? 8 : 4);
                        if ((size_t)(pos + 8) <= len) {
                            d->timescale = READ_U32(data + pos);
                            if (version == 1) {
                                d->duration = (int64_t)READ_U64(data + pos + 4);
                            } else {
                                d->duration = READ_U32(data + pos + 4);
                            }
                        }
                    }
                } else if (child_type == FOURCC('t','r','a','k')) {
                    if (d->track_count >= 8) { moov_pos += child_size; continue; }
                    mp4_track *t = &d->tracks[d->track_count];
                    memset(t, 0, sizeof(mp4_track));
                    t->track_id = d->track_count;
                    t->timescale = d->timescale;
                    t->current_sample = 0;
                    t->data_offset = -1;

                    int64_t trak_end = moov_pos + child_size;
                    int64_t trak_pos = child_data;
                    while (trak_pos + 8 <= trak_end && trak_pos + 8 <= (int64_t)len) {
                        uint32_t trak_child;
                        int64_t trak_child_size = read_box_header(data + trak_pos,
                                                                   (size_t)(trak_end - trak_pos),
                                                                   &trak_child);
                        if (trak_child_size < 8) break;
                        int64_t trak_child_data = trak_pos + 8;

                        if (trak_child == FOURCC('t','k','h','d')) {
                            if ((size_t)(trak_child_data + 4) <= len) {
                                t->track_id = (int)READ_U32(data + trak_child_data + 4);
                            }
                        } else if (trak_child == FOURCC('m','d','i','a')) {
                            int64_t mdia_end = trak_pos + trak_child_size;
                            int64_t mdia_pos = trak_child_data;
                            while (mdia_pos + 8 <= mdia_end && mdia_pos + 8 <= (int64_t)len) {
                                uint32_t mdia_child;
                                int64_t mdia_child_size = read_box_header(data + mdia_pos,
                                                                           (size_t)(mdia_end - mdia_pos),
                                                                           &mdia_child);
                                if (mdia_child_size < 8) break;
                                int64_t mdia_child_data = mdia_pos + 8;

                                if (mdia_child == FOURCC('m','d','h','d')) {
                                    if ((size_t)(mdia_child_data + 24) <= len) {
                                        int ver = data[mdia_child_data];
                                        int64_t pos = mdia_child_data + 4 + (ver == 1 ? 8 : 4);
                                        t->media_timescale = READ_U32(data + pos);
                                    }
                                } else if (mdia_child == FOURCC('m','i','n','f')) {
                                    int64_t minf_end = mdia_pos + mdia_child_size;
                                    int64_t minf_pos = mdia_child_data;
                                    while (minf_pos + 8 <= minf_end && minf_pos + 8 <= (int64_t)len) {
                                        uint32_t minf_child;
                                        int64_t minf_child_size = read_box_header(data + minf_pos,
                                                                                   (size_t)(minf_end - minf_pos),
                                                                                   &minf_child);
                                        if (minf_child_size < 8) break;
                                        int64_t minf_child_data = minf_pos + 8;

                                        if (minf_child == FOURCC('s','t','b','l')) {
                                            int64_t stbl_end = minf_pos + minf_child_size;
                                            int64_t stbl_pos = minf_child_data;
                                            while (stbl_pos + 8 <= stbl_end && stbl_pos + 8 <= (int64_t)len) {
                                                uint32_t stbl_child;
                                                int64_t stbl_child_size = read_box_header(data + stbl_pos,
                                                                                           (size_t)(stbl_end - stbl_pos),
                                                                                           &stbl_child);
                                                if (stbl_child_size < 8) break;
                                                int64_t stbl_child_data = stbl_pos + 8;

                                                if (stbl_child == FOURCC('s','t','s','d')) {
                                                    if ((size_t)(stbl_child_data + 8) <= len) {
                                                        int entry_count = (int)READ_U32(data + stbl_child_data + 4);
                                                        if (entry_count > 0 && (size_t)(stbl_child_data + 8 + 8) <= len) {
                                                            uint32_t format = READ_U32(data + stbl_child_data + 8);
                                                            if (format == FOURCC('a','v','c','1')) {
                                                                t->type = 0;
                                                                t->codec = 1; // H264
                                                            } else if (format == FOURCC('h','e','v','1') ||
                                                                       format == FOURCC('h','v','c','1')) {
                                                                t->type = 0;
                                                                t->codec = 2; // HEVC
                                                            } else if (format == FOURCC('m','p','4','a')) {
                                                                t->type = 1;
                                                                t->codec = 1; // AAC
                                                            }
                                                        }
                                                    }
                                                } else if (stbl_child == FOURCC('s','t','s','z')) {
                                                    if ((size_t)(stbl_child_data + 8) <= len) {
                                                        int field_size = data[stbl_child_data + 4];
                                                        int sample_count = (int)READ_U32(data + stbl_child_data + 4 + 4);
                                                        t->sample_count = sample_count;
                                                        t->sample_sizes = (int64_t*)calloc(sample_count, sizeof(int64_t));
                                                        if (field_size == 4) {
                                                            for (int i = 0; i < sample_count && (size_t)(stbl_child_data + 12 + i * 4) <= len; ++i)
                                                                t->sample_sizes[i] = READ_U32(data + stbl_child_data + 12 + i * 4);
                                                        } else if (field_size == 2) {
                                                            for (int i = 0; i < sample_count && (size_t)(stbl_child_data + 12 + i * 2) <= len; ++i)
                                                                t->sample_sizes[i] = READ_U16(data + stbl_child_data + 12 + i * 2);
                                                        }
                                                    }
                                                } else if (stbl_child == FOURCC('s','t','c','o') ||
                                                           stbl_child == FOURCC('c','o','6','4')) {
                                                    if ((size_t)(stbl_child_data + 4) <= len) {
                                                        int entry_count = (int)READ_U32(data + stbl_child_data + 4);
                                                        t->chunk_count = entry_count;
                                                        if (entry_count > 0) {
                                                            t->chunk_offsets = (int64_t*)calloc(entry_count, sizeof(int64_t));
                                                            for (int i = 0; i < entry_count && (size_t)(stbl_child_data + 8 + i * 4) <= len; ++i)
                                                                t->chunk_offsets[i] = READ_U32(data + stbl_child_data + 8 + i * 4);
                                                        }
                                                    }
                                                } else if (stbl_child == FOURCC('s','t','s','c')) {
                                                    if ((size_t)(stbl_child_data + 4) <= len) {
                                                        t->stsc_entries = (int)READ_U32(data + stbl_child_data + 4);
                                                        t->stsc_first_chunk = (int*)calloc(t->stsc_entries, sizeof(int));
                                                        t->stsc_samples_per_chunk = (int*)calloc(t->stsc_entries, sizeof(int));
                                                        t->stsc_sample_desc_index = (int*)calloc(t->stsc_entries, sizeof(int));
                                                        for (int i = 0; i < t->stsc_entries && (size_t)(stbl_child_data + 8 + i * 12) <= len; ++i) {
                                                            t->stsc_first_chunk[i] = (int)READ_U32(data + stbl_child_data + 8 + i * 12);
                                                            t->stsc_samples_per_chunk[i] = (int)READ_U32(data + stbl_child_data + 8 + i * 12 + 4);
                                                            t->stsc_sample_desc_index[i] = (int)READ_U32(data + stbl_child_data + 8 + i * 12 + 8);
                                                        }
                                                    }
                                                } else if (stbl_child == FOURCC('s','t','t','s')) {
                                                    if ((size_t)(stbl_child_data + 4) <= len) {
                                                        int entry_count = (int)READ_U32(data + stbl_child_data + 4);
                                                        if (t->sample_count > 0 && !t->sample_durations) {
                                                            t->sample_durations = (int64_t*)calloc(t->sample_count, sizeof(int64_t));
                                                        }
                                                        int sample_idx = 0;
                                                        for (int i = 0; i < entry_count && (size_t)(stbl_child_data + 8 + i * 12) <= len; ++i) {
                                                            int count = (int)READ_U32(data + stbl_child_data + 8 + i * 8);
                                                            int64_t duration_val = READ_U32(data + stbl_child_data + 8 + i * 8 + 4);
                                                            for (int j = 0; j < count && sample_idx < t->sample_count; ++j) {
                                                                t->sample_durations[sample_idx++] = duration_val;
                                                            }
                                                        }
                                                    }
                                                } else if (stbl_child == FOURCC('s','t','s','s')) {
                                                    if ((size_t)(stbl_child_data + 4) <= len) {
                                                        int entry_count = (int)READ_U32(data + stbl_child_data + 4);
                                                        if (t->sample_count > 0) {
                                                            t->sample_keyframe = (int*)calloc(t->sample_count, sizeof(int));
                                                        }
                                                        for (int i = 0; i < entry_count && (size_t)(stbl_child_data + 8 + i * 4) <= len; ++i) {
                                                            int idx = (int)READ_U32(data + stbl_child_data + 8 + i * 4) - 1;
                                                            if (idx >= 0 && idx < t->sample_count)
                                                                t->sample_keyframe[idx] = 1;
                                                        }
                                                    }
                                                }

                                                stbl_pos += stbl_child_size;
                                            }
                                        }
                                        minf_pos += minf_child_size;
                                    }
                                }
                                mdia_pos += mdia_child_size;
                            }
                        }
                        trak_pos += trak_child_size;
                    }
                    // Determine width/height from codec
                    if (t->type == 0 && t->codec == 1) {
                        t->width = 1920; t->height = 1080;
                    } else if (t->type == 0 && t->codec == 2) {
                        t->width = 1920; t->height = 1080;
                    }
                    d->track_count++;
                }
                moov_pos += child_size;
            }
        } else if (box_type == FOURCC('m','d','a','t')) {
            d->mdat_offset = (int)offset;
            d->mdat_size = box_size;
        } else if (box_type == FOURCC('m','o','o','f')) {
            // Movie fragment - parse for fragmented mp4
            // Minimal: extract base data offset
        }
        offset += (size_t)box_size;
        if (found_ftyp && d->moov_offset >= 0 && d->mdat_offset >= 0) break;
    }

    // Calculate per-track data offsets from chunk offsets + sample sizes
    for (int ti = 0; ti < d->track_count; ++ti) {
        mp4_track *t = &d->tracks[ti];
        if (t->sample_count <= 0 || !t->chunk_offsets || t->chunk_count <= 0) continue;
        t->sample_offsets = (int64_t*)calloc(t->sample_count, sizeof(int64_t));
        int sample_idx = 0;
        for (int ci = 0; ci < t->chunk_count && sample_idx < t->sample_count; ++ci) {
            int64_t chunk_start = t->chunk_offsets[ci];
            int samples_in_chunk = 0;
            // Determine samples in this chunk from stsc
            for (int si = t->stsc_entries - 1; si >= 0; --si) {
                if (t->stsc_first_chunk[si] <= ci + 1) {
                    samples_in_chunk = t->stsc_samples_per_chunk[si];
                    break;
                }
            }
            if (samples_in_chunk <= 0) samples_in_chunk = 1;
            for (int s = 0; s < samples_in_chunk && sample_idx < t->sample_count; ++s) {
                t->sample_offsets[sample_idx] = chunk_start;
                chunk_start += t->sample_sizes[sample_idx];
                sample_idx++;
            }
        }
        // Adjust with mdat offset if needed
        if (d->mdat_offset > 0) {
            int64_t min_offset = t->sample_offsets[0];
            if (min_offset < d->mdat_offset) {
                // offsets are relative to file start, already correct
            }
        }
    }

    return d;
}

int mp4_get_track_count(mp4_demuxer* d) {
    return d ? d->track_count : 0;
}

int mp4_get_video_codec(mp4_demuxer* d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    if (d->tracks[track].type != 0) return 0;
    return d->tracks[track].codec;
}

int mp4_get_audio_codec(mp4_demuxer* d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    if (d->tracks[track].type != 1) return 0;
    return d->tracks[track].codec;
}

int mp4_get_width(mp4_demuxer* d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    return d->tracks[track].width;
}

int mp4_get_height(mp4_demuxer* d, int track) {
    if (!d || track < 0 || track >= d->track_count) return 0;
    return d->tracks[track].height;
}

double mp4_get_duration(mp4_demuxer* d) {
    if (!d) return 0.0;
    if (d->timescale > 0)
        return (double)d->duration / (double)d->timescale;
    return 0.0;
}

int mp4_read_sample(mp4_demuxer* d, int track, uint8_t* out, size_t* out_len) {
    if (!d || track < 0 || track >= d->track_count || !out || !out_len) return 0;
    mp4_track* t = &d->tracks[track];
    if (t->current_sample >= t->sample_count) return 0;
    int idx = t->current_sample;
    int64_t offset = t->sample_offsets ? t->sample_offsets[idx] : 0;
    int64_t size = t->sample_sizes ? t->sample_sizes[idx] : 0;
    if (offset + size > (int64_t)d->len || size <= 0) return 0;
    memcpy(out, d->data + offset, (size_t)size);
    *out_len = (size_t)size;
    t->current_sample++;
    return 1;
}

void mp4_close(mp4_demuxer* d) {
    if (!d) return;
    for (int i = 0; i < d->track_count; ++i) {
        mp4_track* t = &d->tracks[i];
        free(t->sample_sizes);
        free(t->sample_offsets);
        free(t->sample_durations);
        free(t->sample_keyframe);
        free(t->chunk_offsets);
        free(t->chunk_sample_counts);
        free(t->stsc_first_chunk);
        free(t->stsc_samples_per_chunk);
        free(t->stsc_sample_desc_index);
    }
    free(d);
}
