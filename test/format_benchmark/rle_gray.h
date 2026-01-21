#ifndef __RLE_GRAY__
#define __RLE_GRAY__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//
// Simple RLE compression for 2-bit and 4-bit grayscale images
// Optimized for e-ink display content (large solid areas)
//
// Format for 4-bit (RLE-4):
//   Each byte: [count:4][value:4]
//   - count 1-15: repeat value count times
//   - count 0: escape, next byte is extended count (16-271), then value nibble
//
// Format for 2-bit (RLE-2):
//   Each byte: [count:6][value:2]
//   - count 1-63: repeat value count times
//   - count 0: escape, next byte is extended count (64-319), then value
//
// File structure:
// [Header: 8 bytes]
//   - Magic: 2 bytes "RL" (0x4C52)
//   - Width: 2 bytes (little-endian)
//   - Height: 2 bytes (little-endian)
//   - Bit depth: 1 byte (2 or 4)
//   - Reserved: 1 byte
// [Compressed data]
//

#define RLE_GRAY_MAGIC 0x4C52  // "RL" in little-endian

// Return codes
enum {
    RLE_SUCCESS = 0,
    RLE_INVALID_PARAMETER,
    RLE_ENCODE_ERROR,
    RLE_DECODE_ERROR,
    RLE_BUFFER_OVERFLOW
};

typedef struct {
    uint16_t magic;
    uint16_t width;
    uint16_t height;
    uint8_t bitDepth;
    uint8_t reserved;
} RLE_HEADER;

#ifdef __cplusplus
extern "C" {
#endif

// Encode a complete image
// Input: pixels in packed format (4 pixels/byte for 2-bit, 2 pixels/byte for 4-bit)
// Output: compressed data including header
int rle_encode(const uint8_t *pixels, int width, int height, int bitDepth,
               uint8_t *output, int maxSize, int *outSize);

// Decode a complete image
// Input: compressed data including header
// Output: pixels in packed format
int rle_decode(const uint8_t *data, int dataSize,
               uint8_t *pixels, int maxPixels, int *width, int *height, int *bitDepth);

// Encode a single line (no header)
int rle_encode_line(const uint8_t *pixels, int width, int bitDepth,
                    uint8_t *output, int maxSize, int *outSize);

// Decode a single line (no header)
int rle_decode_line(const uint8_t *data, int dataSize, int width, int bitDepth,
                    uint8_t *pixels, int *bytesRead);

#ifdef __cplusplus
}
#endif

//
// Implementation
//

#ifdef RLE_GRAY_IMPLEMENTATION

// Get pixel value at position x
static inline uint8_t get_pixel(const uint8_t *pixels, int x, int bitDepth) {
    if (bitDepth == 2) {
        int bytePos = x / 4;
        int shift = (3 - (x & 3)) * 2;
        return (pixels[bytePos] >> shift) & 0x03;
    } else { // 4-bit
        int bytePos = x / 2;
        int shift = (1 - (x & 1)) * 4;
        return (pixels[bytePos] >> shift) & 0x0F;
    }
}

// Set pixel value at position x
static inline void set_pixel(uint8_t *pixels, int x, uint8_t value, int bitDepth) {
    if (bitDepth == 2) {
        int bytePos = x / 4;
        int shift = (3 - (x & 3)) * 2;
        pixels[bytePos] &= ~(0x03 << shift);
        pixels[bytePos] |= (value & 0x03) << shift;
    } else { // 4-bit
        int bytePos = x / 2;
        int shift = (1 - (x & 1)) * 4;
        pixels[bytePos] &= ~(0x0F << shift);
        pixels[bytePos] |= (value & 0x0F) << shift;
    }
}

int rle_encode_line(const uint8_t *pixels, int width, int bitDepth,
                    uint8_t *output, int maxSize, int *outSize) {
    int outIdx = 0;
    int x = 0;

    while (x < width) {
        uint8_t value = get_pixel(pixels, x, bitDepth);
        int runLen = 1;

        // Count run length
        while (x + runLen < width && get_pixel(pixels, x + runLen, bitDepth) == value) {
            runLen++;
        }

        if (bitDepth == 4) {
            // RLE-4: [count:4][value:4]
            while (runLen > 0) {
                if (runLen <= 15) {
                    if (outIdx >= maxSize) return RLE_BUFFER_OVERFLOW;
                    output[outIdx++] = (runLen << 4) | value;
                    runLen = 0;
                } else if (runLen <= 271) {
                    // Extended: escape + count + value
                    if (outIdx + 2 >= maxSize) return RLE_BUFFER_OVERFLOW;
                    int extCount = (runLen > 271) ? 271 : runLen;
                    output[outIdx++] = 0x00 | value;  // escape (count=0)
                    output[outIdx++] = extCount - 16;  // 0-255 maps to 16-271
                    runLen -= extCount;
                } else {
                    // Max run, will loop
                    if (outIdx + 2 >= maxSize) return RLE_BUFFER_OVERFLOW;
                    output[outIdx++] = 0x00 | value;
                    output[outIdx++] = 255;  // 271 pixels
                    runLen -= 271;
                }
            }
        } else { // 2-bit
            // RLE-2: [count:6][value:2]
            while (runLen > 0) {
                if (runLen <= 63) {
                    if (outIdx >= maxSize) return RLE_BUFFER_OVERFLOW;
                    output[outIdx++] = (runLen << 2) | value;
                    runLen = 0;
                } else if (runLen <= 319) {
                    // Extended: escape + count
                    if (outIdx + 2 >= maxSize) return RLE_BUFFER_OVERFLOW;
                    int extCount = (runLen > 319) ? 319 : runLen;
                    output[outIdx++] = 0x00 | value;  // escape (count=0)
                    output[outIdx++] = extCount - 64;  // 0-255 maps to 64-319
                    runLen -= extCount;
                } else {
                    if (outIdx + 2 >= maxSize) return RLE_BUFFER_OVERFLOW;
                    output[outIdx++] = 0x00 | value;
                    output[outIdx++] = 255;  // 319 pixels
                    runLen -= 319;
                }
            }
        }

        x += (x < width) ? 0 : runLen;  // runLen already consumed
        // Actually advance x by original run
        while (x < width && get_pixel(pixels, x, bitDepth) == value) x++;
        // Correction: we already counted, so just use the run
    }

    *outSize = outIdx;
    return RLE_SUCCESS;
}

int rle_decode_line(const uint8_t *data, int dataSize, int width, int bitDepth,
                    uint8_t *pixels, int *bytesRead) {
    int inIdx = 0;
    int x = 0;

    // Clear output
    int outBytes = (bitDepth == 2) ? (width + 3) / 4 : (width + 1) / 2;
    memset(pixels, 0, outBytes);

    while (x < width && inIdx < dataSize) {
        if (bitDepth == 4) {
            uint8_t byte = data[inIdx++];
            int count = byte >> 4;
            uint8_t value = byte & 0x0F;

            if (count == 0) {
                // Extended count
                if (inIdx >= dataSize) return RLE_DECODE_ERROR;
                count = data[inIdx++] + 16;
            }

            for (int i = 0; i < count && x < width; i++, x++) {
                set_pixel(pixels, x, value, bitDepth);
            }
        } else { // 2-bit
            uint8_t byte = data[inIdx++];
            int count = byte >> 2;
            uint8_t value = byte & 0x03;

            if (count == 0) {
                // Extended count
                if (inIdx >= dataSize) return RLE_DECODE_ERROR;
                count = data[inIdx++] + 64;
            }

            for (int i = 0; i < count && x < width; i++, x++) {
                set_pixel(pixels, x, value, bitDepth);
            }
        }
    }

    *bytesRead = inIdx;
    return RLE_SUCCESS;
}

int rle_encode(const uint8_t *pixels, int width, int height, int bitDepth,
               uint8_t *output, int maxSize, int *outSize) {
    if (bitDepth != 2 && bitDepth != 4) return RLE_INVALID_PARAMETER;
    if (maxSize < 8) return RLE_BUFFER_OVERFLOW;

    // Write header
    RLE_HEADER *hdr = (RLE_HEADER *)output;
    hdr->magic = RLE_GRAY_MAGIC;
    hdr->width = width;
    hdr->height = height;
    hdr->bitDepth = bitDepth;
    hdr->reserved = 0;

    int bytesPerLine = (bitDepth == 2) ? (width + 3) / 4 : (width + 1) / 2;
    int outIdx = sizeof(RLE_HEADER);

    for (int y = 0; y < height; y++) {
        int lineSize;
        int result = rle_encode_line(pixels + y * bytesPerLine, width, bitDepth,
                                     output + outIdx, maxSize - outIdx, &lineSize);
        if (result != RLE_SUCCESS) return result;
        outIdx += lineSize;
    }

    *outSize = outIdx;
    return RLE_SUCCESS;
}

int rle_decode(const uint8_t *data, int dataSize,
               uint8_t *pixels, int maxPixels, int *width, int *height, int *bitDepth) {
    if (dataSize < 8) return RLE_INVALID_PARAMETER;

    const RLE_HEADER *hdr = (const RLE_HEADER *)data;
    if (hdr->magic != RLE_GRAY_MAGIC) return RLE_DECODE_ERROR;

    *width = hdr->width;
    *height = hdr->height;
    *bitDepth = hdr->bitDepth;

    int bytesPerLine = (*bitDepth == 2) ? (*width + 3) / 4 : (*width + 1) / 2;
    int totalBytes = bytesPerLine * *height;
    if (totalBytes > maxPixels) return RLE_BUFFER_OVERFLOW;

    int inIdx = sizeof(RLE_HEADER);

    for (int y = 0; y < *height; y++) {
        int bytesRead;
        int result = rle_decode_line(data + inIdx, dataSize - inIdx, *width, *bitDepth,
                                     pixels + y * bytesPerLine, &bytesRead);
        if (result != RLE_SUCCESS) return result;
        inIdx += bytesRead;
    }

    return RLE_SUCCESS;
}

#endif // RLE_GRAY_IMPLEMENTATION

#endif // __RLE_GRAY__
