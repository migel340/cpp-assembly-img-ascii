// filepath: /Users/spacedesk2/CLionProjects/img-to-ascii/include/image_loader.h
#pragma once

#include <string>
#include <array>

struct Image {
    unsigned char* data;
    int width;
    int height;
    int channels;

    Image() : data(nullptr), width(0), height(0), channels(0) {}
    ~Image();

    // Disable copying
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    // Enable moving
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    [[nodiscard]] bool isValid() const { return data != nullptr && width > 0 && height > 0; }
};

class ImageLoader {
public:
    /**
     * Ładuje obraz z podanej ścieżki
     * @param filepath Ścieżka do pliku obrazu
     * @param desiredChannels Liczba kanałów (0 = auto, 3 = RGB, 4 = RGBA)
     * @return Struktura Image z danymi obrazu
     */
    static Image loadImage(const std::string& filepath, int desiredChannels = 0);

    /**
     * Sprawdza czy plik istnieje
     * @param filepath Ścieżka do pliku
     * @return true jeśli plik istnieje
     */
    static bool fileExists(const std::string& filepath);
};

// -------------------- PIXEL HELPERS --------------------
// Lightweight helpers for reading pixels from Image buffers.

struct PixelRGB {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct PixelRGBA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

// Check coordinates inside image
inline bool inBounds(const Image& img, int x, int y) {
    return (x >= 0 && x < img.width && y >= 0 && y < img.height);
}

// Compute base index for pixel (x,y)
inline int pixelBaseIndex(const Image& img, int x, int y) {
    return (y * img.width + x) * img.channels;
}

// Safe RGB getter (assumes at least 3 channels). If out of bounds or channels < 3, returns zeros.
inline PixelRGB getPixelRGB(const Image& img, int x, int y) {
    PixelRGB p{0,0,0};
    if (!img.isValid() || !inBounds(img, x, y) || img.channels < 3) return p;
    int idx = pixelBaseIndex(img, x, y);
    p.r = img.data[idx + 0];
    p.g = img.data[idx + 1];
    p.b = img.data[idx + 2];
    return p;
}

// Safe RGBA getter (fills alpha=255 if not present)
inline PixelRGBA getPixelRGBA(const Image& img, int x, int y) {
    PixelRGBA p{0,0,0,255};
    if (!img.isValid() || !inBounds(img, x, y) || img.channels < 1) return p;
    int idx = pixelBaseIndex(img, x, y);
    p.r = img.data[idx + 0];
    if (img.channels >= 2) p.g = img.data[idx + 1];
    if (img.channels >= 3) p.b = img.data[idx + 2];
    if (img.channels >= 4) p.a = img.data[idx + 3];
    return p;
}

// Return normalized RGB as floats in range [0,1]
inline std::array<float,3> getPixelRGBf(const Image& img, int x, int y) {
    std::array<float,3> out{0.f,0.f,0.f};
    if (!img.isValid() || !inBounds(img, x, y) || img.channels < 3) return out;
    int idx = pixelBaseIndex(img, x, y);
    out[0] = img.data[idx + 0] / 255.0f;
    out[1] = img.data[idx + 1] / 255.0f;
    out[2] = img.data[idx + 2] / 255.0f;
    return out;
}

// Compute luminance (perceptual) from RGB bytes -> float [0,1]
// Using Rec. 709 / ITU-R BT.709 weights
inline float getLuminance(const Image& img, int x, int y) {
    auto f = getPixelRGBf(img, x, y);
    return 0.2126f * f[0] + 0.7152f * f[1] + 0.0722f * f[2];
}

// Compute index-safe total bytes
inline size_t imageByteSize(const Image& img) {
    if (!img.isValid()) return 0;
    return static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * static_cast<size_t>(img.channels);
}

// -------------------- end PIXEL HELPERS --------------------
