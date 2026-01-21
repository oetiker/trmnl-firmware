#ifndef __PLANAR_G5__
#define __PLANAR_G5__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/bb_epaper/src/Group5.h"

//
// Planar G5 - Multi-bit image compression using G5 per bit plane
// Extends 1-bit G5 to support 2-bit (4 gray) and 4-bit (16 gray) images
//
// Format structure:
// [Header: 8 bytes]
//   - Magic: 2 bytes "P5" (0x5035)
//   - Width: 2 bytes (little-endian)
//   - Height: 2 bytes (little-endian)
//   - Bit depth: 1 byte (2 or 4)
//   - Plane count: 1 byte (2 or 4)
// [Plane sizes: 2 bytes * plane_count]
//   - Size of each compressed plane (little-endian)
// [Compressed plane data]
//   - Plane 0 (LSB) G5 data
//   - Plane 1 G5 data
//   - ...
//

#define PLANAR_G5_MAGIC 0x5035  // "P5" in little-endian

// Return codes
enum {
    PG5_SUCCESS = 0,
    PG5_INVALID_PARAMETER,
    PG5_ENCODE_ERROR,
    PG5_DECODE_ERROR,
    PG5_BUFFER_OVERFLOW,
    PG5_INVALID_FORMAT,
    PG5_UNSUPPORTED_DEPTH
};

// Header structure (8 bytes + plane sizes)
typedef struct {
    uint16_t magic;
    uint16_t width;
    uint16_t height;
    uint8_t bitDepth;    // 2 or 4
    uint8_t planeCount;  // 2 or 4
} PG5_HEADER;

// Encoder state
typedef struct {
    int width;
    int height;
    int bitDepth;
    int planeCount;
    uint8_t *planeBuf;     // Temporary buffer for one plane line
    int planePitch;        // Bytes per line for one plane
    G5ENCIMAGE encoders[4]; // One encoder per plane
} PG5_ENCODER;

// Decoder state
typedef struct {
    int width;
    int height;
    int bitDepth;
    int planeCount;
    uint8_t *planeBufs[4]; // Line buffer per plane
    int planePitch;
    G5DECIMAGE decoders[4];
    uint8_t *planeData[4]; // Pointers to compressed plane data
    uint16_t planeSizes[4];
} PG5_DECODER;

#ifdef __cplusplus
extern "C" {
#endif

// Encoder functions
int pg5_encoder_init(PG5_ENCODER *enc, int width, int height, int bitDepth);
int pg5_encode_line(PG5_ENCODER *enc, const uint8_t *pixels, uint8_t *planeOutputs[4]);
int pg5_encoder_finish(PG5_ENCODER *enc, uint8_t *output, int maxSize, int *outSize);
void pg5_encoder_free(PG5_ENCODER *enc);

// Decoder functions
int pg5_decoder_init(PG5_DECODER *dec, const uint8_t *data, int dataSize);
int pg5_decode_line(PG5_DECODER *dec, uint8_t *pixels);
void pg5_decoder_free(PG5_DECODER *dec);

// Utility: Extract bit plane from multi-bit line
void pg5_extract_plane(const uint8_t *src, uint8_t *dst, int width, int bitDepth, int plane);

// Utility: Combine bit planes into multi-bit line
void pg5_combine_planes(const uint8_t *planes[4], uint8_t *dst, int width, int bitDepth);

#ifdef __cplusplus
}
#endif

//
// Implementation
//

#ifdef PLANAR_G5_IMPLEMENTATION

// Extract a single bit plane from a 2-bit or 4-bit image line
void pg5_extract_plane(const uint8_t *src, uint8_t *dst, int width, int bitDepth, int plane) {
    memset(dst, 0, (width + 7) / 8);

    if (bitDepth == 2) {
        // 2-bit: 4 pixels per byte
        int srcIdx = 0;
        for (int x = 0; x < width; x++) {
            int bytePos = x / 4;
            int bitPos = (3 - (x & 3)) * 2;  // MSB first within byte
            uint8_t pixel = (src[bytePos] >> bitPos) & 0x03;

            // Extract the specified bit from the 2-bit pixel
            uint8_t bit = (pixel >> plane) & 1;

            // Pack into output (MSB first)
            int outByte = x / 8;
            int outBit = 7 - (x & 7);
            dst[outByte] |= (bit << outBit);
        }
    } else if (bitDepth == 4) {
        // 4-bit: 2 pixels per byte
        for (int x = 0; x < width; x++) {
            int bytePos = x / 2;
            int nibblePos = (1 - (x & 1)) * 4;  // High nibble first
            uint8_t pixel = (src[bytePos] >> nibblePos) & 0x0F;

            // Extract the specified bit from the 4-bit pixel
            uint8_t bit = (pixel >> plane) & 1;

            // Pack into output (MSB first)
            int outByte = x / 8;
            int outBit = 7 - (x & 7);
            dst[outByte] |= (bit << outBit);
        }
    }
}

// Combine bit planes back into 2-bit or 4-bit image line
void pg5_combine_planes(const uint8_t *planes[4], uint8_t *dst, int width, int bitDepth) {
    int planeCount = (bitDepth == 2) ? 2 : 4;

    if (bitDepth == 2) {
        // 2-bit: 4 pixels per byte
        int outBytes = (width + 3) / 4;
        memset(dst, 0, outBytes);

        for (int x = 0; x < width; x++) {
            int inByte = x / 8;
            int inBit = 7 - (x & 7);

            uint8_t pixel = 0;
            for (int p = 0; p < 2; p++) {
                uint8_t bit = (planes[p][inByte] >> inBit) & 1;
                pixel |= (bit << p);
            }

            int outByte = x / 4;
            int outShift = (3 - (x & 3)) * 2;
            dst[outByte] |= (pixel << outShift);
        }
    } else if (bitDepth == 4) {
        // 4-bit: 2 pixels per byte
        int outBytes = (width + 1) / 2;
        memset(dst, 0, outBytes);

        for (int x = 0; x < width; x++) {
            int inByte = x / 8;
            int inBit = 7 - (x & 7);

            uint8_t pixel = 0;
            for (int p = 0; p < 4; p++) {
                uint8_t bit = (planes[p][inByte] >> inBit) & 1;
                pixel |= (bit << p);
            }

            int outByte = x / 2;
            int outShift = (1 - (x & 1)) * 4;
            dst[outByte] |= (pixel << outShift);
        }
    }
}

#endif // PLANAR_G5_IMPLEMENTATION

#endif // __PLANAR_G5__
