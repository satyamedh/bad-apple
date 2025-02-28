#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "lcdgfx.h"

// Include the header file that contains the RLE-encoded video data.
// The header file should define: 
//   const char* video_data = "...";
#include "video_data.h"

// Define video frame dimensions.
#define FRAME_WIDTH  128
#define FRAME_HEIGHT 64
#define FRAME_SIZE   (FRAME_WIDTH * FRAME_HEIGHT)
#define PACKED_BUFFER_SIZE     (FRAME_WIDTH * (FRAME_HEIGHT / 8))

#define TARGET_FPS 12

DisplaySH1106_128x64_I2C display(-1, {I2C_NUM_1, 0x3C, 17, 18, 1000000}); // -1 means no reset pin

// Global state for RLE decoding.
static const char *g_rlePtr = video_data;  // pointer to current position in the RLE string
static int  g_leftoverRunCount = 0;          // remaining run count from previous parsing
static uint8_t g_leftoverBit = 0;             // bit value associated with the leftover run
static bool g_hasLeftover = false;

/**
 * @brief Decode one frame from the RLE-encoded video data.
 *
 * This function fills the provided frameBuffer with FRAME_SIZE bits
 * (each represented as 0 or 1) by parsing the global RLE string.
 *
 * @param frameBuffer Preallocated buffer of size FRAME_SIZE (in bytes).
 * @return true if a full frame was decoded, false if there isn’t enough data.
 */
bool decodeNextFrame(uint8_t *frameBuffer) {
    size_t decodedCount = 0;

    // Continue until we've filled one frame.
    while (decodedCount < FRAME_SIZE) {
        // If we have a leftover run from the previous parse, use it.
        if (g_hasLeftover) {
            int available = g_leftoverRunCount;
            int toWrite = (available <= (FRAME_SIZE - decodedCount)) ? available : (FRAME_SIZE - decodedCount);
            for (int i = 0; i < toWrite; i++) {
                frameBuffer[decodedCount + i] = g_leftoverBit;
            }
            decodedCount += toWrite;
            g_leftoverRunCount -= toWrite;
            if (g_leftoverRunCount == 0) {
                g_hasLeftover = false;
            }
        } else {
            // Parse a new run from the RLE string.
            if (*g_rlePtr == '\0') {
                // End of data reached before filling the frame.
                return false;
            }

            // Parse the run length (digits until colon).
            int runLength = 0;
            while (*g_rlePtr != ':' && *g_rlePtr != '\0') {
                if (isdigit(*g_rlePtr)) {
                    runLength = runLength * 10 + (*g_rlePtr - '0');
                }
                g_rlePtr++;
            }
            if (*g_rlePtr == ':') {
                g_rlePtr++;  // Skip the colon.
            } else {
                // Malformed data: no colon found.
                return false;
            }
            // Next character should be '0' or '1'.
            if (*g_rlePtr != '0' && *g_rlePtr != '1') {
                // Malformed data.
                return false;
            }
            uint8_t bit = (*g_rlePtr == '1') ? 1 : 0;
            g_rlePtr++;

            // Skip any whitespace after the run.
            while (*g_rlePtr == ' ' || *g_rlePtr == '\t' || *g_rlePtr == '\n') {
                g_rlePtr++;
            }

            // Write as many bits as we can for this run.
            int toWrite = (runLength <= (FRAME_SIZE - decodedCount)) ? runLength : (FRAME_SIZE - decodedCount);
            for (int i = 0; i < toWrite; i++) {
                frameBuffer[decodedCount + i] = bit;
            }
            decodedCount += toWrite;
            int remaining = runLength - toWrite;
            if (remaining > 0) {
                // Save the leftover run for the next frame.
                g_leftoverRunCount = remaining;
                g_leftoverBit = bit;
                g_hasLeftover = true;
            }
        }
    }
    return true;
}

/**
 * @brief Display a frame to the serial console.
 *
 * For demonstration purposes, each pixel is printed as a block if set,
 * or a space if not. This may flood the serial console for large frames.
 *
 * @param frameBuffer The decoded frame buffer (size FRAME_SIZE).
 */
// void displayFrame(const uint8_t *frameBuffer) {
//     for (int y = 0; y < FRAME_HEIGHT; y++) {
//         for (int x = 0; x < FRAME_WIDTH; x++) {
//             // add to frame buffer
//         }
//         printf("\n");
//     }
//     display.display();
// }


void packFrameBuffer(const uint8_t *frameBuffer, uint8_t *packedBuffer)
{
    memset(packedBuffer, 0, PACKED_BUFFER_SIZE);

    for (int x = 0; x < FRAME_WIDTH; x++) {
        for (int page = 0; page < (FRAME_HEIGHT / 8); page++) {
            uint8_t byteVal = 0;
            for (int bit = 0; bit < 8; bit++) {
                int y = page * 8 + bit;
                if (frameBuffer[y * FRAME_WIDTH + x]) {
                    byteVal |= (1 << bit);
                }
            }
            packedBuffer[page * FRAME_WIDTH + x] = byteVal;
        }
    }
}

/**
 * @brief Main application entry point.
 *
 * This function decodes the video from the RLE-encoded string and "displays"
 * each frame by printing it to the Serial console with a delay between frames.
 */
extern "C" void app_main(void)
{
    printf("Starting RLE video decoder...\n");

    // Initialize the display
    display.begin();
    display.clear();

    // Allocate memory for one frame.
    // For larger frames or if you want to buffer more data, use PSRAM.
    uint8_t *frameBuffer = (uint8_t *)heap_caps_malloc(FRAME_SIZE * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    if (!frameBuffer) {
        printf("Failed to allocate memory for frame buffer.\n");
        return;
    }

    // Reset the global pointer and leftover state.
    g_rlePtr = video_data;
    g_leftoverRunCount = 0;
    g_hasLeftover = false;

    uint8_t packedBuffer[PACKED_BUFFER_SIZE];

    // Decode frames one by one until the RLE data is exhausted.
    int frameCount = 0;
    while (decodeNextFrame(frameBuffer)) {
        uint32_t frameStart = esp_timer_get_time();
        printf("Displaying frame %d...\n", frameCount++);
        packFrameBuffer(frameBuffer, packedBuffer);
        display.drawBuffer1Fast(0, 0, FRAME_WIDTH, FRAME_HEIGHT, packedBuffer);
        uint32_t frameEnd = esp_timer_get_time();
        uint32_t frameTime = frameEnd - frameStart;
        uint32_t frameDelay = 1000000 / TARGET_FPS - frameTime;
        if (frameDelay > 0) {
            vTaskDelay(pdMS_TO_TICKS(frameDelay / 1000));
        }    
    }

    printf("Finished decoding video.\n");
    free(frameBuffer);
}
