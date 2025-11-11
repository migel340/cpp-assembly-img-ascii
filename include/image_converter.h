#pragma once

#include "image_loader.h"
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

// Global flag to control ASM usage (defined in main.cpp)
extern bool g_useAsm;

// ============================================================================
// HSV CONVERSION STRUCTURES AND HELPERS
// ============================================================================

struct PixelHSV {
    float h;  // Hue [0, 360)
    float s;  // Saturation [0, 1]
    float v;  // Value [0, 1]
};

// Convert RGB (0-1 normalized) to HSV
// Implemented in C++ and also available in assembly for performance
PixelHSV rgbToHsv(float r, float g, float b);

// C++ version for reference
inline PixelHSV rgbToHsvCpp(float r, float g, float b) {
    PixelHSV hsv{0.f, 0.f, 0.f};

    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float delta = max_val - min_val;

    // Value
    hsv.v = max_val;

    // Saturation
    hsv.s = (max_val != 0.0f) ? (delta / max_val) : 0.0f;

    // Hue
    if (delta == 0.0f) {
        hsv.h = 0.0f;
    } else if (max_val == r) {
        hsv.h = 60.0f * std::fmod((g - b) / delta + 6.0f, 6.0f);
    } else if (max_val == g) {
        hsv.h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        hsv.h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    return hsv;
}

// ============================================================================
// SOBEL EDGE DETECTION
// ============================================================================

// Stores edge magnitude and direction for a pixel
struct EdgeInfo {
    float magnitude;  // [0, 1] normalized
    float angle;      // [0, 360) in degrees, direction of edge
};

// Sobel kernel directions
// 0 = horizontal edge, 90 = vertical edge
struct SobelResult {
    float gx;  // Gradient in X direction
    float gy;  // Gradient in Y direction
};

// Character mappings for ASCII art with edge directions
struct AsciiCharMap {
    // Brightness levels with better gradation (16 levels)
    static constexpr const char* densityChars = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
    static constexpr int densityLevels = 70;

    // Simplified density for better readability
    static constexpr const char* simpleDensityChars = " .:-=+*#%@";
    static constexpr int simpleDensityLevels = 10;

    // Edge direction characters based on angle
    // Maps angle (0-360) to edge representation
    static char getEdgeChar(float angle) {
        // Normalize angle to 0-180 (since gradients are symmetric)
        angle = std::fmod(angle, 180.0f);
        if (angle < 0) angle += 180.0f;

        // Map angle ranges to directional characters
        if (angle < 22.5f || angle >= 157.5f) return '-';  // Horizontal
        if (angle >= 22.5f && angle < 67.5f) return '/';   // Diagonal /
        if (angle >= 67.5f && angle < 112.5f) return '|';  // Vertical
        return '\\'; // Diagonal \ (for 112.5 <= angle < 157.5)
    }
};

// ============================================================================
// IMAGE SCALING
// ============================================================================

// Scale image to target dimensions using bilinear interpolation
// Accounts for terminal character aspect ratio (typically 1:2)
Image scaleImage(const Image& src, int targetWidth, int targetHeight, float aspectRatio = 0.5f);

// ============================================================================
// THREADED SOBEL EDGE DETECTION
// ============================================================================

// Result of edge detection for entire image
struct EdgeMap {
    float* magnitudes;  // Normalized [0,1]
    float* angles;      // [0, 360)
    int width;
    int height;

    EdgeMap(int w, int h);
    ~EdgeMap();

    // Disable copying
    EdgeMap(const EdgeMap&) = delete;
    EdgeMap& operator=(const EdgeMap&) = delete;

    // Enable moving
    EdgeMap(EdgeMap&& other) noexcept
        : magnitudes(other.magnitudes)
        , angles(other.angles)
        , width(other.width)
        , height(other.height)
    {
        other.magnitudes = nullptr;
        other.angles = nullptr;
        other.width = 0;
        other.height = 0;
    }

    EdgeMap& operator=(EdgeMap&& other) noexcept {
        if (this != &other) {
            delete[] magnitudes;
            delete[] angles;

            magnitudes = other.magnitudes;
            angles = other.angles;
            width = other.width;
            height = other.height;

            other.magnitudes = nullptr;
            other.angles = nullptr;
            other.width = 0;
            other.height = 0;
        }
        return *this;
    }

    [[nodiscard]] bool isValid() const {
        return magnitudes != nullptr && angles != nullptr && width > 0 && height > 0;
    }

    // Get edge info at pixel
    [[nodiscard]] EdgeInfo getEdgeAt(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return {0.f, 0.f};
        int idx = y * width + x;
        return {magnitudes[idx], angles[idx]};
    }
};

// Detect edges using Sobel operator with threading
// blockSize: number of pixels per thread block (larger = fewer threads)
EdgeMap detectEdgesSobel(const Image& img, int blockSize = 64);

// ============================================================================
// ASCII CONVERSION
// ============================================================================

struct AsciiPixel {
    char character;
    unsigned char r, g, b;  // RGB color values (0-255) for ANSI color support
};

// Convert processed image to ASCII art
// Uses brightness for character density and edge info for special characters
std::vector<AsciiPixel> convertToAscii(
    const Image& scaledImg,
    const EdgeMap* edges = nullptr,
    bool useEdges = true
);

// Print ASCII art to console with optional colors
void printAsciiArt(
    const std::vector<AsciiPixel>& ascii,
    int width,
    int height,
    bool useColors = false
);

// ============================================================================
// ASSEMBLY FUNCTIONS (ARM64)
// ============================================================================

extern "C" {
    // Fast RGB to HSV conversion for batches
    // Input: src pointer to RGB floats (3 floats per pixel)
    // Output: dst pointer to HSV floats (3 floats per pixel)
    // count: number of pixels to convert
    void rgbToHsvBatch(const float* src, float* dst, int count);

    // Fast Sobel edge detection on a region
    // Computes gradients for interior pixels (borders set to 0)
    // imageData: pointer to image buffer (RGB, 3 bytes per pixel)
    // width, height: image dimensions
    // outputGx, outputGy: output arrays for gradients
    void sobelGradients(
        const unsigned char* imageData,
        int width,
        int height,
        int stride,  // bytes per pixel
        float* outputGx,
        float* outputGy
    );
}


