#include "JPEGRtp.h"
#include "JPEG.h"

using namespace std;
using namespace mediakit;

#define AV_WB24(p, d)                                                                                                  \
    do {                                                                                                               \
        ((uint8_t *)(p))[2] = (d);                                                                                     \
        ((uint8_t *)(p))[1] = (d) >> 8;                                                                                \
        ((uint8_t *)(p))[0] = (d) >> 16;                                                                               \
    } while (0)

#define AV_WB16(p, d)                                                                                                  \
    do {                                                                                                               \
        ((uint8_t *)(p))[1] = (d);                                                                                     \
        ((uint8_t *)(p))[0] = (d) >> 8;                                                                           \
    } while (0)

#define AV_WB8(p, d)  do { ((uint8_t*)(p))[0] = (d); } while(0)

/* JPEG marker codes */
enum JpegMarker {
    /* start of frame */
    SOF0 = 0xc0,       /* baseline */
    SOF1 = 0xc1,       /* extended sequential, huffman */
    SOF2 = 0xc2,       /* progressive, huffman */
    SOF3 = 0xc3,       /* lossless, huffman */

    SOF5 = 0xc5,       /* differential sequential, huffman */
    SOF6 = 0xc6,       /* differential progressive, huffman */
    SOF7 = 0xc7,       /* differential lossless, huffman */
    JPG = 0xc8,       /* reserved for JPEG extension */
    SOF9 = 0xc9,       /* extended sequential, arithmetic */
    SOF10 = 0xca,       /* progressive, arithmetic */
    SOF11 = 0xcb,       /* lossless, arithmetic */

    SOF13 = 0xcd,       /* differential sequential, arithmetic */
    SOF14 = 0xce,       /* differential progressive, arithmetic */
    SOF15 = 0xcf,       /* differential lossless, arithmetic */

    DHT = 0xc4,       /* define huffman tables */

    DAC = 0xcc,       /* define arithmetic-coding conditioning */

    /* restart with modulo 8 count "m" */
    RST0 = 0xd0,
    RST1 = 0xd1,
    RST2 = 0xd2,
    RST3 = 0xd3,
    RST4 = 0xd4,
    RST5 = 0xd5,
    RST6 = 0xd6,
    RST7 = 0xd7,

    SOI = 0xd8,       /* start of image */
    EOI = 0xd9,       /* end of image */
    SOS = 0xda,       /* start of scan */
    DQT = 0xdb,       /* define quantization tables */
    DNL = 0xdc,       /* define number of lines */
    DRI = 0xdd,       /* define restart interval */
    DHP = 0xde,       /* define hierarchical progression */
    EXP = 0xdf,       /* expand reference components */

    APP0 = 0xe0,
    APP1 = 0xe1,
    APP2 = 0xe2,
    APP3 = 0xe3,
    APP4 = 0xe4,
    APP5 = 0xe5,
    APP6 = 0xe6,
    APP7 = 0xe7,
    APP8 = 0xe8,
    APP9 = 0xe9,
    APP10 = 0xea,
    APP11 = 0xeb,
    APP12 = 0xec,
    APP13 = 0xed,
    APP14 = 0xee,
    APP15 = 0xef,

    JPG0 = 0xf0,
    JPG1 = 0xf1,
    JPG2 = 0xf2,
    JPG3 = 0xf3,
    JPG4 = 0xf4,
    JPG5 = 0xf5,
    JPG6 = 0xf6,
    SOF48 = 0xf7,       ///< JPEG-LS
    LSE = 0xf8,       ///< JPEG-LS extension parameters
    JPG9 = 0xf9,
    JPG10 = 0xfa,
    JPG11 = 0xfb,
    JPG12 = 0xfc,
    JPG13 = 0xfd,

    COM = 0xfe,       /* comment */

    TEM = 0x01,       /* temporary private use for arithmetic coding */

    /* 0x02 -> 0xbf reserved */
};

typedef struct PutByteContext {
    uint8_t *buffer, *buffer_end, *buffer_start;
    int eof;
} PutByteContext;

static void bytestream2_init_writer(PutByteContext *p, uint8_t *buf, int buf_size) {
    assert(buf_size >= 0);
    p->buffer = buf;
    p->buffer_start = buf;
    p->buffer_end = buf + buf_size;
    p->eof = 0;
}

static inline void bytestream2_put_byte(PutByteContext *p, uint8_t value) {
    if (!p->eof && (p->buffer_end - p->buffer >= 1)) {
        p->buffer[0] = value;
        p->buffer += 1;
    } else {
        p->eof = 1;
    }
}

static inline void bytestream2_put_be16(PutByteContext *p, uint16_t value) {
    if (!p->eof && (p->buffer_end - p->buffer >= 2)) {
        p->buffer[0] = value >> 8;
        p->buffer[1] = value & 0x00FF;
        p->buffer += 2;
    } else {
        p->eof = 1;
    }
}

static inline void bytestream2_put_be24(PutByteContext *p, uint16_t value) {
    if (!p->eof && (p->buffer_end - p->buffer >= 2)) {
        p->buffer[0] = value >> 16;
        p->buffer[1] = value >> 8;
        p->buffer[2] = value & 0x00FF;
        p->buffer += 2;
    } else {
        p->eof = 1;
    }
}

static unsigned int bytestream2_put_buffer(PutByteContext *p, const uint8_t *src, unsigned int size) {
    int size2 = 0;
    if (p->eof) {
        return 0;
    }
    size2 = MIN(p->buffer_end - p->buffer, size);
    if (size2 != (int)size) {
        p->eof = 1;
    }
    memcpy(p->buffer, src, size2);
    p->buffer += size2;
    return size2;
}

static inline int bytestream2_tell_p(PutByteContext *p) {
    return (int) (p->buffer - p->buffer_start);
}

static inline void avio_write(string &str, const void *ptr, size_t size) {
    str.append((char *) ptr, size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static const uint8_t default_quantizers[128] = {
        /* luma table */
        16, 11, 12, 14, 12, 10, 16, 14,
        13, 14, 18, 17, 16, 19, 24, 40,
        26, 24, 22, 22, 24, 49, 35, 37,
        29, 40, 58, 51, 61, 60, 57, 51,
        56, 55, 64, 72, 92, 78, 64, 68,
        87, 69, 55, 56, 80, 109, 81, 87,
        95, 98, 103, 104, 103, 62, 77, 113,
        121, 112, 100, 120, 92, 101, 103, 99,

        /* chroma table */
        17, 18, 18, 24, 21, 24, 47, 26,
        26, 47, 99, 66, 56, 66, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
};


/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
const uint8_t avpriv_mjpeg_bits_dc_luminance[17] =
        { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
const uint8_t avpriv_mjpeg_val_dc[12] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

const uint8_t avpriv_mjpeg_bits_dc_chrominance[17] =
        { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};

const uint8_t avpriv_mjpeg_bits_ac_luminance[17] =
        { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};
const uint8_t avpriv_mjpeg_val_ac_luminance[] =
        {0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
         0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
         0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
         0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
         0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
         0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
         0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
         0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
         0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
         0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
         0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
         0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
         0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
         0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
         0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
         0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
         0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
         0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
         0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
         0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
         0xf9, 0xfa
        };

const uint8_t avpriv_mjpeg_bits_ac_chrominance[17] =
        { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};

const uint8_t avpriv_mjpeg_val_ac_chrominance[] =
        {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
         0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
         0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
         0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
         0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
         0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
         0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
         0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
         0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
         0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
         0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
         0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
         0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
         0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
         0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
         0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
         0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
         0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
         0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
         0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
         0xf9, 0xfa
        };

static int jpeg_create_huffman_table(PutByteContext *p, int table_class,
                                     int table_id, const uint8_t *bits_table,
                                     const uint8_t *value_table) {
    int i = 0, n = 0;

    bytestream2_put_byte(p, table_class << 4 | table_id);

    for (i = 1; i <= 16; i++) {
        n += bits_table[i];
        bytestream2_put_byte(p, bits_table[i]);
    }

    for (i = 0; i < n; i++) {
        bytestream2_put_byte(p, value_table[i]);
    }
    return n + 17;
}

static void jpeg_put_marker(PutByteContext *pbc, int code) {
    bytestream2_put_byte(pbc, 0xff);
    bytestream2_put_byte(pbc, code);
}

static int jpeg_create_header(uint8_t *buf, int size, uint32_t type, uint32_t w,
                              uint32_t h, const uint8_t *qtable, int nb_qtable,
                              int dri) {
    PutByteContext pbc;
    uint8_t *dht_size_ptr;
    int dht_size = 0, i = 0;

    bytestream2_init_writer(&pbc, buf, size);

    /* Convert from blocks to pixels. */
    w <<= 3;
    h <<= 3;

    /* SOI */
    jpeg_put_marker(&pbc, SOI);

    /* JFIF header */
    jpeg_put_marker(&pbc, APP0);
    bytestream2_put_be16(&pbc, 16);
    bytestream2_put_buffer(&pbc, (const uint8_t *) "JFIF", 5);
    bytestream2_put_be16(&pbc, 0x0201);
    bytestream2_put_byte(&pbc, 0);
    bytestream2_put_be16(&pbc, 1);
    bytestream2_put_be16(&pbc, 1);
    bytestream2_put_byte(&pbc, 0);
    bytestream2_put_byte(&pbc, 0);

    if (dri) {
        jpeg_put_marker(&pbc, DRI);
        bytestream2_put_be16(&pbc, 4);
        bytestream2_put_be16(&pbc, dri);
    }

    /* DQT */
    jpeg_put_marker(&pbc, DQT);
    bytestream2_put_be16(&pbc, 2 + nb_qtable * (1 + 64));

    for (i = 0; i < nb_qtable; i++) {
        bytestream2_put_byte(&pbc, i);

        /* Each table is an array of 64 values given in zig-zag
         * order, identical to the format used in a JFIF DQT
         * marker segment. */
        bytestream2_put_buffer(&pbc, qtable + 64 * i, 64);
    }

    /* DHT */
    jpeg_put_marker(&pbc, DHT);
    dht_size_ptr = pbc.buffer;
    bytestream2_put_be16(&pbc, 0);

    dht_size = 2;
    dht_size += jpeg_create_huffman_table(&pbc, 0, 0, avpriv_mjpeg_bits_dc_luminance,
                                          avpriv_mjpeg_val_dc);
    dht_size += jpeg_create_huffman_table(&pbc, 0, 1, avpriv_mjpeg_bits_dc_chrominance,
                                          avpriv_mjpeg_val_dc);
    dht_size += jpeg_create_huffman_table(&pbc, 1, 0, avpriv_mjpeg_bits_ac_luminance,
                                          avpriv_mjpeg_val_ac_luminance);
    dht_size += jpeg_create_huffman_table(&pbc, 1, 1, avpriv_mjpeg_bits_ac_chrominance,
                                          avpriv_mjpeg_val_ac_chrominance);
    AV_WB16(dht_size_ptr, dht_size);

    /* SOF0 */
    jpeg_put_marker(&pbc, SOF0);
    bytestream2_put_be16(&pbc, 17); /* size */
    bytestream2_put_byte(&pbc, 8); /* bits per component */
    bytestream2_put_be16(&pbc, h);
    bytestream2_put_be16(&pbc, w);
    bytestream2_put_byte(&pbc, 3); /* number of components */
    bytestream2_put_byte(&pbc, 1); /* component number */
    bytestream2_put_byte(&pbc, (2 << 4) | (type ? 2 : 1)); /* hsample/vsample */
    bytestream2_put_byte(&pbc, 0); /* matrix number */
    bytestream2_put_byte(&pbc, 2); /* component number */
    bytestream2_put_byte(&pbc, 1 << 4 | 1); /* hsample/vsample */
    bytestream2_put_byte(&pbc, nb_qtable == 2 ? 1 : 0); /* matrix number */
    bytestream2_put_byte(&pbc, 3); /* component number */
    bytestream2_put_byte(&pbc, 1 << 4 | 1); /* hsample/vsample */
    bytestream2_put_byte(&pbc, nb_qtable == 2 ? 1 : 0); /* matrix number */

    /* SOS */
    jpeg_put_marker(&pbc, SOS);
    bytestream2_put_be16(&pbc, 12);
    bytestream2_put_byte(&pbc, 3);
    bytestream2_put_byte(&pbc, 1);
    bytestream2_put_byte(&pbc, 0);
    bytestream2_put_byte(&pbc, 2);
    bytestream2_put_byte(&pbc, 17);
    bytestream2_put_byte(&pbc, 3);
    bytestream2_put_byte(&pbc, 17);
    bytestream2_put_byte(&pbc, 0);
    bytestream2_put_byte(&pbc, 63);
    bytestream2_put_byte(&pbc, 0);

    /* Return the length in bytes of the JPEG header. */
    return bytestream2_tell_p(&pbc);
}

static inline int av_clip(int a, int amin, int amax) {
    if (a < amin) { return amin; }
    else if (a > amax) { return amax; }
    else { return a; }
}

static void create_default_qtables(uint8_t *qtables, uint8_t q) {
    int factor = q;
    int i = 0;
    uint16_t S;

    factor = av_clip(q, 1, 99);

    if (q < 50) {
        S = 5000 / factor;
    } else {
        S = 200 - factor * 2;
    }

    for (i = 0; i < 128; i++) {
        int val = (default_quantizers[i] * S + 50) / 100;

        /* Limit the quantizers to 1 <= q <= 255. */
        val = av_clip(val, 1, 255);
        qtables[i] = val;
    }
}

#define AVERROR_INVALIDDATA -1
#define AVERROR_PATCHWELCOME -2
#define AVERROR_EAGAIN -3
#define RTP_FLAG_KEY    0x1 ///< RTP packet contains a keyframe
#define RTP_FLAG_MARKER 0x2 ///< RTP marker bit was set for this packet
#define av_log(ctx, level, ...)  PrintD(__VA_ARGS__)

#ifndef AV_RB24
#   define AV_RB24(x)                           \
    ((((const uint8_t*)(x))[0] << 16) |         \
     (((const uint8_t*)(x))[1] <<  8) |         \
     ((const uint8_t*)(x))[2])
#endif

#define AV_RB8(x)     (((const uint8_t*)(x))[0])

#ifndef AV_RB16
#   define AV_RB16(x)    ((((const uint8_t*)(x))[0] << 8) | (((const uint8_t*)(x))[1] ))
#endif

static int jpeg_parse_packet(void *ctx, PayloadContext *jpeg, uint32_t *timestamp, const uint8_t *buf, int len,
                             uint16_t seq, int flags, uint8_t *type) {
    uint8_t q = 0, width = 0, height = 0;
    const uint8_t *qtables = NULL;
    uint16_t qtable_len = 0;
    uint32_t off = 0;
    int ret = 0, dri = 0;

    if (len < 8) {
        av_log(ctx, AV_LOG_ERROR, "Too short RTP/JPEG packet.\n");
        return AVERROR_INVALIDDATA;
    }

    /* Parse the main JPEG header. */
    off = AV_RB24(buf + 1);  /* fragment byte offset */
    *type = AV_RB8(buf + 4);   /* id of jpeg decoder params */
    q = AV_RB8(buf + 5);   /* quantization factor (or table id) */
    width = AV_RB8(buf + 6);   /* frame width in 8 pixel blocks */
    height = AV_RB8(buf + 7);   /* frame height in 8 pixel blocks */
    buf += 8;
    len -= 8;

    if (*type & 0x40) {
        if (len < 4) {
            av_log(ctx, AV_LOG_ERROR, "Too short RTP/JPEG packet.\n");
            return AVERROR_INVALIDDATA;
        }
        dri = AV_RB16(buf);
        buf += 4;
        len -= 4;
        *type &= ~0x40;
    }
    if (*type > 1) {
        av_log(ctx, AV_LOG_ERROR, "RTP/JPEG type %d", (int) *type);
        return AVERROR_PATCHWELCOME;
    }

    /* Parse the quantization table header. */
    if (off == 0) {
        /* Start of JPEG data packet. */
        uint8_t new_qtables[128];
        uint8_t hdr[1024];

        if (q > 127) {
            uint8_t precision;
            if (len < 4) {
                av_log(ctx, AV_LOG_ERROR, "Too short RTP/JPEG packet.\n");
                return AVERROR_INVALIDDATA;
            }

            /* The first byte is reserved for future use. */
            precision = AV_RB8(buf + 1);    /* size of coefficients */
            qtable_len = AV_RB16(buf + 2);   /* length in bytes */
            buf += 4;
            len -= 4;

            if (precision) {
                av_log(ctx, AV_LOG_WARNING, "Only 8-bit precision is supported.\n");
            }

            if (qtable_len > 0) {
                if (len < qtable_len) {
                    av_log(ctx, AV_LOG_ERROR, "Too short RTP/JPEG packet.\n");
                    return AVERROR_INVALIDDATA;
                }
                qtables = buf;
                buf += qtable_len;
                len -= qtable_len;
                if (q < 255) {
                    if (jpeg->qtables_len[q - 128] &&
                        (jpeg->qtables_len[q - 128] != qtable_len ||
                         memcmp(qtables, &jpeg->qtables[q - 128][0], qtable_len))) {
                        av_log(ctx, AV_LOG_WARNING,
                               "Quantization tables for q=%d changed\n", q);
                    } else if (!jpeg->qtables_len[q - 128] && qtable_len <= 128) {
                        memcpy(&jpeg->qtables[q - 128][0], qtables,
                               qtable_len);
                        jpeg->qtables_len[q - 128] = qtable_len;
                    }
                }
            } else {
                if (q == 255) {
                    av_log(ctx, AV_LOG_ERROR,
                           "Invalid RTP/JPEG packet. Quantization tables not found.\n");
                    return AVERROR_INVALIDDATA;
                }
                if (!jpeg->qtables_len[q - 128]) {
                    av_log(ctx, AV_LOG_ERROR,
                           "No quantization tables known for q=%d yet.\n", q);
                    return AVERROR_INVALIDDATA;
                }
                qtables = &jpeg->qtables[q - 128][0];
                qtable_len = jpeg->qtables_len[q - 128];
            }
        } else { /* q <= 127 */
            if (q == 0 || q > 99) {
                av_log(ctx, AV_LOG_ERROR, "Reserved q value %d\n", q);
                return AVERROR_INVALIDDATA;
            }
            create_default_qtables(new_qtables, q);
            qtables = new_qtables;
            qtable_len = sizeof(new_qtables);
        }

        /* Skip the current frame in case of the end packet
         * has been lost somewhere. */
        jpeg->frame.clear();
        jpeg->frame.reserve(1024 + len);
        jpeg->timestamp = *timestamp;

        /* Generate a frame and scan headers that can be prepended to the
         * RTP/JPEG data payload to produce a JPEG compressed image in
         * interchange format. */
        jpeg->hdr_size = jpeg_create_header(hdr, sizeof(hdr), *type, width,
                                            height, qtables,
                                            qtable_len / 64, dri);

        /* Copy JPEG header to frame buffer. */
        avio_write(jpeg->frame, hdr, jpeg->hdr_size);
    }

    if (jpeg->frame.empty()) {
        av_log(ctx, AV_LOG_ERROR,
               "Received packet without a start chunk; dropping frame.\n");
        return AVERROR_EAGAIN;
    }

    if (jpeg->timestamp != *timestamp) {
        /* Skip the current frame if timestamp is incorrect.
         * A start packet has been lost somewhere. */
        jpeg->frame.clear();
        av_log(ctx, AV_LOG_ERROR, "RTP timestamps don't match.\n");
        return AVERROR_INVALIDDATA;
    }

    if (off != jpeg->frame.size() - jpeg->hdr_size) {
        av_log(ctx, AV_LOG_ERROR,
               "Missing packets; dropping frame.\n");
        return AVERROR_EAGAIN;
    }

    /* Copy data to frame buffer. */
    avio_write(jpeg->frame, buf, len);

    if (flags & RTP_FLAG_MARKER) {
        /* End of JPEG data packet. */
        uint8_t buf[2] = {0xff, EOI};

        /* Put EOI marker. */
        avio_write(jpeg->frame, buf, sizeof(buf));
        return 0;
    }

    return AVERROR_EAGAIN;
}

//----------------------------------------------------------------------------------
#define DEF(type, name, bytes, write)                                                                                  \
    static inline void bytestream_put_##name(uint8_t **b, const type value) {                                          \
        write(*b, value);                                                                                              \
        (*b) += bytes;                                                                                                 \
    }

DEF(unsigned int, be24, 3, AV_WB24)
DEF(unsigned int, be16, 2, AV_WB16)
DEF(unsigned int, byte, 1, AV_WB8)

static inline void bytestream_put_buffer(uint8_t **b, const uint8_t *src, unsigned int size) {
    memcpy(*b, src, size);
    (*b) += size;
}

void JPEGRtpEncoder::rtpSendJpeg(const uint8_t *buf, int size, uint64_t pts, uint8_t type)
{
    const uint8_t *qtables[4] = { NULL };
    int nb_qtables = 0;
    uint8_t w, h;
    uint8_t *p;
    int off = 0; /* fragment offset of the current JPEG frame */
    int len;
    int i;
    int default_huffman_tables = 0;
    uint8_t *out = nullptr;

    /* preparse the header for getting some info */
    for (i = 0; i < size; i++) {
        if (buf[i] != 0xff)
            continue;

        if (buf[i + 1] == DQT) {
            int tables, j;
            if (buf[i + 4] & 0xF0)
                av_log(s1, AV_LOG_WARNING,
                       "Only 8-bit precision is supported.\n");

            /* a quantization table is 64 bytes long */
            tables = AV_RB16(&buf[i + 2]) / 65;
            if (i + 5 + tables * 65 > size) {
                av_log(s1, AV_LOG_ERROR, "Too short JPEG header. Aborted!\n");
                return;
            }
            if (nb_qtables + tables > 4) {
                av_log(s1, AV_LOG_ERROR, "Invalid number of quantisation tables\n");
                return;
            }

            for (j = 0; j < tables; j++)
                qtables[nb_qtables + j] = buf + i + 5 + j * 65;
            nb_qtables += tables;
            // 大致忽略DQT/qtable所占字节数，提高搜寻速度
            i += tables << 6;
        } else if (buf[i + 1] == SOF0) {
            if (buf[i + 14] != 17 || buf[i + 17] != 17) {
                av_log(s1, AV_LOG_ERROR,
                       "Only 1x1 chroma blocks are supported. Aborted!\n");
                return;
            }
            h = (buf[i + 5] * 256 + buf[i + 6]) / 8;
            w = (buf[i + 7] * 256 + buf[i + 8]) / 8;
            // 大致忽略SOF0所占字节数，提高搜寻速度
            i += 16;
        } else if (buf[i + 1] == DHT) {
            int dht_size = AV_RB16(&buf[i + 2]);
            default_huffman_tables |= 1 << 4;
            i += 3;
            dht_size -= 2;
            if (i + dht_size >= size)
                continue;
            while (dht_size > 0)
                switch (buf[i + 1]) {
                case 0x00:
                    if (   dht_size >= 29
                        && !memcmp(buf + i +  2, avpriv_mjpeg_bits_dc_luminance + 1, 16)
                        && !memcmp(buf + i + 18, avpriv_mjpeg_val_dc, 12)) {
                        default_huffman_tables |= 1;
                        i += 29;
                        dht_size -= 29;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                case 0x01:
                    if (   dht_size >= 29
                        && !memcmp(buf + i +  2, avpriv_mjpeg_bits_dc_chrominance + 1, 16)
                        && !memcmp(buf + i + 18, avpriv_mjpeg_val_dc, 12)) {
                        default_huffman_tables |= 1 << 1;
                        i += 29;
                        dht_size -= 29;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                case 0x10:
                    if (   dht_size >= 179
                        && !memcmp(buf + i +  2, avpriv_mjpeg_bits_ac_luminance   + 1, 16)
                        && !memcmp(buf + i + 18, avpriv_mjpeg_val_ac_luminance, 162)) {
                        default_huffman_tables |= 1 << 2;
                        i += 179;
                        dht_size -= 179;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                case 0x11:
                    if (   dht_size >= 179
                        && !memcmp(buf + i +  2, avpriv_mjpeg_bits_ac_chrominance + 1, 16)
                        && !memcmp(buf + i + 18, avpriv_mjpeg_val_ac_chrominance, 162)) {
                        default_huffman_tables |= 1 << 3;
                        i += 179;
                        dht_size -= 179;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                default:
                    i += dht_size;
                    dht_size = 0;
                    continue;
            }
        } else if (buf[i + 1] == SOS) {
            /* SOS is last marker in the header */
            i += AV_RB16(&buf[i + 2]) + 2;
            if (i > size) {
                av_log(s1, AV_LOG_ERROR,
                       "Insufficient data. Aborted!\n");
                return;
            }
            break;
        }
    }
    if (default_huffman_tables && default_huffman_tables != 31) {
        av_log(s1, AV_LOG_ERROR,
               "RFC 2435 requires standard Huffman tables for jpeg\n");
        return;
    }
    if (nb_qtables && nb_qtables != 2)
        av_log(s1, AV_LOG_WARNING,
               "RFC 2435 suggests two quantization tables, %d provided\n",
               nb_qtables);

    /* skip JPEG header */
    buf  += i;
    size -= i;

    for (i = size - 2; i >= 0; i--) {
        if (buf[i] == 0xff && buf[i + 1] == EOI) {
            /* Remove the EOI marker */
            size = i;
            break;
        }
    }

    while (size > 0) {
        int hdr_size = 8;

        if (off == 0 && nb_qtables)
            hdr_size += 4 + 64 * nb_qtables;

        /* payload max in one packet */
        len = MIN(size, (int)getMaxSize() - hdr_size);

        /* marker bit is last packet in frame */
        auto rtp_packet = makeRtp(getTrackType(), nullptr, len + hdr_size, size == len, pts);
        p = rtp_packet->getPayload();

        /* set main header */
        bytestream_put_byte(&p, 0);
        bytestream_put_be24(&p, off);
        bytestream_put_byte(&p, type);
        bytestream_put_byte(&p, 255);
        bytestream_put_byte(&p, w);
        bytestream_put_byte(&p, h);

        if (off == 0 && nb_qtables) {
            /* set quantization tables header */
            bytestream_put_byte(&p, 0);
            bytestream_put_byte(&p, 0);
            bytestream_put_be16(&p, 64 * nb_qtables);

            for (i = 0; i < nb_qtables; i++)
                bytestream_put_buffer(&p, qtables[i], 64);
        }

        /* copy payload data */
        memcpy(p, buf, len);

        // output rtp packet
        RtpCodec::inputRtp(std::move(rtp_packet), false);

        buf  += len;
        size -= len;
        off  += len;
    }
    free(out);
}

////////////////////////////////////////////////////////////

JPEGRtpDecoder::JPEGRtpDecoder() {
    memset(&_ctx.timestamp, 0, sizeof(_ctx) - offsetof(decltype(_ctx), timestamp));
}

CodecId JPEGRtpDecoder::getCodecId() const {
    return CodecJPEG;
}

bool JPEGRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool) {
    auto payload = rtp->getPayload();
    auto size = rtp->getPayloadSize();
    auto stamp = rtp->getStamp();
    auto seq = rtp->getSeq();
    auto marker = rtp->getHeader()->mark;
    if (size <= 0) {
        //无实际负载
        return false;
    }

    uint8_t type;
    if (0 == jpeg_parse_packet(nullptr, &_ctx, &stamp, payload, size, seq, marker ? RTP_FLAG_MARKER : 0, &type)) {
        auto buffer = std::make_shared<toolkit::BufferString>(std::move(_ctx.frame));
        // JFIF头固定20个字节长度
        auto frame = std::make_shared<JPEGFrame>(std::move(buffer), stamp / 90, type, 20);
        _ctx.frame.clear();
        RtpCodec::inputFrame(std::move(frame));
    }

    return false;
}

////////////////////////////////////////////////////////////////////////

JPEGRtpEncoder::JPEGRtpEncoder(
    uint32_t ssrc, uint32_t mtu, uint32_t sample_rate, uint8_t payload_type, uint8_t interleaved)
    : RtpInfo(ssrc, mtu, sample_rate, payload_type, interleaved) {}


bool JPEGRtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = (uint8_t *)frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto pts = frame->pts();
    auto type = 1;
    auto jpeg = dynamic_pointer_cast<JPEGFrame>(frame);
    if (jpeg) {
        type = jpeg->pixType();
    }
    rtpSendJpeg(ptr, len, pts, type);
    return len > 0;
}
