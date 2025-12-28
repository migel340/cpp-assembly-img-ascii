#include "../include/image_converter.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>

// Global luminance buffer used by ASM path. Defined here.
float* g_lumaBuffer = nullptr;
// last HSV time in milliseconds
double g_lastHsvMs = std::nan("");


// ============================================================================
// HSV CONVERSION IMPLEMENTATION
// ============================================================================

PixelHSV rgbToHsv(float r, float g, float b) {
    // Toggle between ASM and C++ implementation
    if (g_hsvAsm) {
        // Use assembly batch function for single pixel
        float src[3] = {r, g, b};
        float dst[3] = {0.0f, 0.0f, 0.0f};
        rgbToHsvBatch(src, dst, 1);
        return PixelHSV{dst[0], dst[1], dst[2]};
    } else {
        // Use C++ implementation
        return rgbToHsvCpp(r, g, b);
    }
}

// ============================================================================
// IMAGE SCALING IMPLEMENTATION
// ============================================================================

Image scaleImage(const Image& src, int targetWidth, int targetHeight, float aspectRatio) {
    if (!src.isValid() || targetWidth <= 0 || targetHeight <= 0) {
        return Image();
    }

    Image dst;
    dst.width = targetWidth;
    dst.height = targetHeight;
    dst.channels = src.channels;
    dst.data = new unsigned char[targetWidth * targetHeight * src.channels]{};

    // Calculate scale factors
    // Aspect ratio correction: terminal chars are ~2x taller than wide
    // We sample the source image with adjusted coordinates
    float scaleX = static_cast<float>(src.width) / targetWidth;
    float scaleY = static_cast<float>(src.height) / targetHeight;

    for (int y = 0; y < targetHeight; ++y) {
        for (int x = 0; x < targetWidth; ++x) {
            float srcX = x * scaleX;
            float srcY = y * scaleY;

            // Skip if out of bounds
            if (srcY >= src.height - 1 || srcX >= src.width - 1) {
                // Fill with black/background
                for (int c = 0; c < src.channels; ++c) {
                    int dstIdx = (y * targetWidth + x) * src.channels + c;
                    dst.data[dstIdx] = 0;
                }
                continue;
            }

            // Clamp coordinates to valid range
            int x0 = static_cast<int>(srcX);
            int y0 = static_cast<int>(srcY);

            x0 = std::max(0, std::min(x0, src.width - 2));
            y0 = std::max(0, std::min(y0, src.height - 2));

            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float fx = srcX - x0;
            float fy = srcY - y0;

            // Clamp fractions to [0, 1]
            fx = std::max(0.0f, std::min(1.0f, fx));
            fy = std::max(0.0f, std::min(1.0f, fy));

            // Bilinear interpolation
            for (int c = 0; c < src.channels; ++c) {
                int idx00 = (y0 * src.width + x0) * src.channels + c;
                int idx10 = (y0 * src.width + x1) * src.channels + c;
                int idx01 = (y1 * src.width + x0) * src.channels + c;
                int idx11 = (y1 * src.width + x1) * src.channels + c;

                float v00 = src.data[idx00] / 255.0f;
                float v10 = src.data[idx10] / 255.0f;
                float v01 = src.data[idx01] / 255.0f;
                float v11 = src.data[idx11] / 255.0f;

                float v0 = v00 * (1 - fx) + v10 * fx;
                float v1 = v01 * (1 - fx) + v11 * fx;
                float v = v0 * (1 - fy) + v1 * fy;

                int dstIdx = (y * targetWidth + x) * src.channels + c;
                dst.data[dstIdx] = static_cast<unsigned char>(v * 255.0f);
            }
        }
    }

    return dst;
}

// ============================================================================
// EDGE MAP IMPLEMENTATION
// ============================================================================

EdgeMap::EdgeMap(int w, int h) : width(w), height(h) {
    size_t size = static_cast<size_t>(w) * static_cast<size_t>(h);
    magnitudes = new float[size]{};
    angles = new float[size]{};
}

EdgeMap::~EdgeMap() {
    delete[] magnitudes;
    delete[] angles;
}

// ============================================================================
// SOBEL EDGE DETECTION IMPLEMENTATION
// ============================================================================

// Single-threaded Sobel for a block of pixels
static void sobelBlock(
    const Image& img,
    EdgeMap& edges,
    int startY,
    int endY
) {
    if (!img.isValid() || !edges.isValid()) return;

    // Sobel kernels
    int sobelX[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    int sobelY[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}
    };

    float maxGradient = 0.0f;

    for (int y = std::max(1, startY); y < std::min(img.height - 1, endY); ++y) {
        for (int x = 1; x < img.width - 1; ++x) {
            float gx = 0.0f, gy = 0.0f;

            // Apply Sobel kernels
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    float pixel = getLuminance(img, x + kx, y + ky);
                    gx += pixel * sobelX[ky + 1][kx + 1];
                    gy += pixel * sobelY[ky + 1][kx + 1];
                }
            }

            float magnitude = std::sqrt(gx * gx + gy * gy);
            float angle = std::atan2(gy, gx) * 180.0f / M_PI;
            if (angle < 0) angle += 180.0f;

            int idx = y * img.width + x;
            edges.magnitudes[idx] = magnitude;
            edges.angles[idx] = angle;

            maxGradient = std::max(maxGradient, magnitude);
        }
    }

    // Normalize magnitudes
    if (maxGradient > 0.0f) {
        for (int y = std::max(1, startY); y < std::min(img.height - 1, endY); ++y) {
            for (int x = 1; x < img.width - 1; ++x) {
                int idx = y * img.width + x;
                edges.magnitudes[idx] /= maxGradient;
            }
        }
    }
}

EdgeMap detectEdgesSobel(const Image& img, int blockSize) {
    EdgeMap edges(img.width, img.height);

    if (!img.isValid()) {
        return edges;
    }

    if (g_sobelAsm) {
        // Use assembly accelerated Sobel to compute Gx and Gy, then derive magnitudes/angles
        int w = img.width;
        int h = img.height;
        size_t total = static_cast<size_t>(w) * static_cast<size_t>(h);
        float* gx = new float[total]();
        float* gy = new float[total]();

        // Ensure luminance buffer exists for ASM implementation to use
        if (!g_lumaBuffer) {
            g_lumaBuffer = new float[total]();
        }

        // Determine thread count (0 = auto)
        int threadCount = g_threadCount;
        if (threadCount <= 0) {
            threadCount = static_cast<int>(std::thread::hardware_concurrency());
            if (threadCount <= 0) threadCount = 1;
        }
        if (threadCount > 64) threadCount = 64;

        if (threadCount == 1) {
            // Single worker: compute whole image
            sobelGradients(img.data, w, h, img.channels, 0, h, gx, gy, g_lumaBuffer);
        } else {
            // Partition inner rows [1, h-1) across threads (edges remain 0)
            int innerStart = 1;
            int innerEnd = std::max(1, h - 1);
            int rows = innerEnd - innerStart;
            if (rows > 0) {
                int block = (rows + threadCount - 1) / threadCount;
                std::vector<std::thread> workers;
                workers.reserve(threadCount);
                for (int t = 0; t < threadCount; ++t) {
                    int s = innerStart + t * block;
                    int e = s + block;
                    if (s >= innerEnd) break;
                    if (e > innerEnd) e = innerEnd;
                    // Launch worker computing rows [s, e)
                    workers.emplace_back([&img, s, e, gx, gy, w, h]() {
                        sobelGradients(img.data, w, h, img.channels, s, e, gx, gy, g_lumaBuffer);
                    });
                }
                for (auto& th : workers) th.join();
            }
        }

        // Compute magnitudes and angles, track max gradient
        float maxGradient = 0.0f;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int idx = y * w + x;
                float vx = gx[idx];
                float vy = gy[idx];
                float magnitude = std::sqrt(vx * vx + vy * vy);
                float angle = std::atan2(vy, vx) * 180.0f / M_PI;
                if (angle < 0) angle += 180.0f;
                edges.magnitudes[idx] = magnitude;
                edges.angles[idx] = angle;
                maxGradient = std::max(maxGradient, magnitude);
            }
        }

        // Normalize magnitudes
        if (maxGradient > 0.0f) {
            for (int y = 1; y < h - 1; ++y) {
                for (int x = 1; x < w - 1; ++x) {
                    int idx = y * w + x;
                    edges.magnitudes[idx] /= maxGradient;
                }
            }
        }

        delete[] gx;
        delete[] gy;
        // free luminance buffer
        if (g_lumaBuffer) {
            delete[] g_lumaBuffer;
            g_lumaBuffer = nullptr;
        }
        return edges;
    }

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 1;

    int blockHeight = std::max(1, img.height / numThreads);
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        int startY = t * blockHeight;
        int endY = (t == numThreads - 1) ? img.height : (t + 1) * blockHeight;

        threads.emplace_back([&img, &edges, startY, endY]() {
            sobelBlock(img, edges, startY, endY);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return edges;
}

// ============================================================================
// ASCII CONVERSION IMPLEMENTATION
// ============================================================================

std::vector<AsciiPixel> convertToAscii(
    const Image& scaledImg,
    const EdgeMap* edges,
    bool useEdges,
    bool useHsv
) {
    std::vector<AsciiPixel> ascii;

    if (!scaledImg.isValid()) {
        return ascii;
    }

    ascii.reserve(scaledImg.width * scaledImg.height);

    const int totalPixels = scaledImg.width * scaledImg.height;

    // If HSV path requested, run batch conversion (uses ASM batch if g_useAsm)
    std::vector<float> hsvDst;
    if (useHsv) {
        hsvDst.resize(static_cast<size_t>(totalPixels) * 3);
        std::vector<float> src;
        src.reserve(static_cast<size_t>(totalPixels) * 3);
        for (int y = 0; y < scaledImg.height; ++y) {
            for (int x = 0; x < scaledImg.width; ++x) {
                int idx = (y * scaledImg.width + x) * scaledImg.channels;
                float rf = scaledImg.data[idx] / 255.0f;
                float gf = (scaledImg.channels > 1) ? (scaledImg.data[idx + 1] / 255.0f) : rf;
                float bf = (scaledImg.channels > 2) ? (scaledImg.data[idx + 2] / 255.0f) : rf;
                src.push_back(rf);
                src.push_back(gf);
                src.push_back(bf);
            }
        }

        // Call batch HSV converter (assembly-accelerated when enabled)
        auto hsvStart = std::chrono::high_resolution_clock::now();
        rgbToHsvBatch(src.data(), hsvDst.data(), totalPixels);
        auto hsvEnd = std::chrono::high_resolution_clock::now();
        auto hsvMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(hsvEnd - hsvStart).count();
        g_lastHsvMs = hsvMs;
    }

    if (!useHsv) {
        g_lastHsvMs = std::nan("");
    }

    for (int y = 0; y < scaledImg.height; ++y) {
        for (int x = 0; x < scaledImg.width; ++x) {
            int idx = (y * scaledImg.width + x) * scaledImg.channels;
            unsigned char r = scaledImg.data[idx];
            unsigned char g = (scaledImg.channels > 1) ? scaledImg.data[idx + 1] : r;
            unsigned char b = (scaledImg.channels > 2) ? scaledImg.data[idx + 2] : r;

            float luminance = getLuminance(scaledImg, x, y);
            // Apply gamma correction for better contrast
            luminance = std::pow(luminance, 0.8f);
            // Clamp to [0, 1]
            luminance = std::max(0.0f, std::min(1.0f, luminance));

            int level = static_cast<int>(luminance * (AsciiCharMap::densityLevels - 1));
            level = std::max(0, std::min(AsciiCharMap::densityLevels - 1, level));

            char ch = AsciiCharMap::densityChars[level];

            // If HSV mode enabled, optionally override based on hue ranges
            if (useHsv) {
                int p = (y * scaledImg.width + x);
                float h = hsvDst[p * 3 + 0];
                float s = hsvDst[p * 3 + 1];
                float v = hsvDst[p * 3 + 2];
                // Use value from HSV as brightness instead
                float hvLum = std::max(0.0f, std::min(1.0f, v));
                int hvLevel = static_cast<int>(hvLum * (AsciiCharMap::densityLevels - 1));
                hvLevel = std::max(0, std::min(AsciiCharMap::densityLevels - 1, hvLevel));
                ch = AsciiCharMap::densityChars[hvLevel];

                // Example hue-based filtering: make blue hues prominent
                if (s > 0.15f && (h >= 180.0f && h <= 260.0f)) {
                    ch = '#';
                }
            }

            // Override with edge character if applicable
            if (useEdges && edges && edges->isValid()) {
                EdgeInfo edge = edges->getEdgeAt(x, y);
                if (edge.magnitude > 0.25f) {
                    ch = AsciiCharMap::getEdgeChar(edge.angle); 
                }
            }

            AsciiPixel pixel{ch, r, g, b};
            ascii.push_back(pixel);
        }
    }

    return ascii;
}

void printAsciiArt(
    const std::vector<AsciiPixel>& ascii,
    int width,
    int height,
    bool useColors
) {
    if (ascii.empty() || width <= 0 || height <= 0) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (idx >= ascii.size()) break;

            const AsciiPixel& pixel = ascii[idx];

            if (useColors) {
                // Print with ANSI 24-bit true color support
                // Format: \033[38;2;R;G;Bm (foreground color)
                std::cout << "\033[38;2;"
                          << static_cast<int>(pixel.r) << ";"
                          << static_cast<int>(pixel.g) << ";"
                          << static_cast<int>(pixel.b) << "m"
                          << pixel.character;
            } else {
                std::cout << pixel.character;
            }
        }

        if (useColors) {
            // Reset color at end of line
            std::cout << "\033[0m";
        }

        std::cout << '\n';
    }

    // Final color reset
    if (useColors) {
        std::cout << "\033[0m";
    }

    std::cout.flush();
}

