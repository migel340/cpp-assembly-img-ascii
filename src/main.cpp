#include <iostream>
#include <string>
#include <chrono>
#include "../include/image_loader.h"
#include "../include/image_converter.h"

extern "C" {
    int add(int a, int b);
}

// Global flag to control ASM usage. Default is OFF; runtime flags control usage.
bool g_useAsm = false;

// Global thread count for processing (0 = auto). Clamped to [1,64] when used.
int g_threadCount = 0;

// C++ implementation of add function
int addCpp(int a, int b) {
    return a + b;
}

void printUsage(const char* programName) {
    std::cout << "==================================================" << std::endl;
    std::cout << "       Image to ASCII Art Converter" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << programName << " <image_path> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Supported formats: JPG, JPEG, PNG, BMP, TGA, GIF" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -w <width>       Target ASCII art width (default: 120)" << std::endl;
    std::cout << "  -h <height>      Target ASCII art height (default: 60)" << std::endl;
    std::cout << "  --no-edges       Disable edge detection" << std::endl;
    std::cout << "  --colors         Enable ANSI 24-bit true color output" << std::endl;
    std::cout << "  --asm-on         Enable ARM assembly optimizations (default)" << std::endl;
    std::cout << "  --asm-off        Disable ARM assembly, use C++ only" << std::endl;
    std::cout << std::endl;
    std::cout << "Recommended sizes for different terminals:" << std::endl;
    std::cout << "  Small:  80x30   (fits in small terminals)" << std::endl;
    std::cout << "  Medium: 120x60  (default, good balance)" << std::endl;
    std::cout << "  Large:  160x80  (for wide terminals)" << std::endl;
    std::cout << "  XL:     200x100 (full screen terminals)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " image.jpg" << std::endl;
    std::cout << "  " << programName << " photo.png -w 80 -h 30" << std::endl;
    std::cout << "  " << programName << " image.jpg --no-edges" << std::endl;
    std::cout << "  " << programName << " image.jpg --colors" << std::endl;
    std::cout << "  " << programName << " image.jpg --asm-off" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "       Image to ASCII Art Converter v1.0" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    // Parse command line arguments
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string imagePath = argv[1];
    int targetWidth = 120;  // Smaller default to fit standard terminals (80-120 cols)
    int targetHeight = 60;
    bool useEdges = true;
    bool useColors = false;

    // Parse optional arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-w" && i + 1 < argc) {
            targetWidth = std::stoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            targetHeight = std::stoi(argv[++i]);
        } else if (arg == "--no-edges") {
            useEdges = false;
        } else if (arg == "--colors") {
            useColors = true;
        } else if (arg == "--asm-on") {
            g_useAsm = true;
        } else if (arg == "--asm-off") {
            g_useAsm = false;
        } else if (arg == "--threads" && i + 1 < argc) {
            try {
                g_threadCount = std::stoi(argv[++i]);
                if (g_threadCount < 0) g_threadCount = 0;
            } catch (...) {
                g_threadCount = 0;
            }
        }
    }

    // Test add function based on ASM flag
    int armTestResult = g_useAsm ? add(10, 5) : addCpp(10, 5);
    std::cout << "[" << (g_useAsm ? "Assembly" : "C++") << "] Test: 10 + 5 = " << armTestResult << std::endl;
    std::cout << std::endl;

    std::cout << "[Config] Target dimensions: " << targetWidth << "x" << targetHeight << std::endl;
    std::cout << "[Config] Edge detection: " << (useEdges ? "enabled" : "disabled") << std::endl;
    std::cout << "[Config] Colors: " << (useColors ? "enabled" : "disabled") << std::endl;
    std::cout << "[Config] ASM mode: " << (g_useAsm ? "enabled" : "disabled (C++ only)") << std::endl;
    std::cout << std::endl;

    // Start total timer
    auto totalStart = std::chrono::high_resolution_clock::now();

    // ========================================================================
    // STEP 1: Load Image
    // ========================================================================
    std::cout << "[1/5] Loading image..." << std::endl;
    auto loadStart = std::chrono::high_resolution_clock::now();

    Image originalImg = ImageLoader::loadImage(imagePath, 3);  // Load as RGB

    auto loadEnd = std::chrono::high_resolution_clock::now();
    auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count();

    if (!originalImg.isValid()) {
        std::cerr << "[ERROR] Failed to load image!" << std::endl;
        return 1;
    }

    std::cout << "[✓] Image loaded in " << loadTime << " ms" << std::endl;
    std::cout << "    Dimensions: " << originalImg.width << "x" << originalImg.height << std::endl;
    std::cout << "    Channels: " << originalImg.channels << std::endl;
    std::cout << "    Size: " << (imageByteSize(originalImg) / 1024) << " KB" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 2: Scale Image
    // ========================================================================
    std::cout << "[2/5] Scaling image..." << std::endl;
    auto scaleStart = std::chrono::high_resolution_clock::now();

    // Terminal characters are typically ~2x taller than wide
    // Adjust target height to compensate for aspect ratio
    // Using 0.75 to get better vertical coverage (not too squashed)
    int adjustedHeight = static_cast<int>(targetHeight * 0.75f);
    Image scaledImg = scaleImage(originalImg, targetWidth, adjustedHeight, 1.0f);

    auto scaleEnd = std::chrono::high_resolution_clock::now();
    auto scaleTime = std::chrono::duration_cast<std::chrono::milliseconds>(scaleEnd - scaleStart).count();
    auto scaleTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(scaleEnd - scaleStart).count();

    if (!scaledImg.isValid()) {
        std::cerr << "[ERROR] Failed to scale image!" << std::endl;
        return 1;
    }

    if (scaleTime > 0) {
        std::cout << "[✓] Image scaled in " << scaleTime << " ms" << std::endl;
    } else {
        std::cout << "[✓] Image scaled in " << scaleTimeMicro << " μs" << std::endl;
    }
    std::cout << "    New dimensions: " << scaledImg.width << "x" << scaledImg.height << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 3: Detect Edges (Optional)
    // ========================================================================
    EdgeMap* edges = nullptr;
    auto edgeTime = 0LL;
    auto edgeTimeMicro = 0LL;
    if (useEdges) {
        std::cout << "[3/5] Detecting edges (Sobel operator)..." << std::endl;
        auto edgeStart = std::chrono::high_resolution_clock::now();

        EdgeMap edgeResult = detectEdgesSobel(scaledImg);
        edges = new EdgeMap(std::move(edgeResult));

        auto edgeEnd = std::chrono::high_resolution_clock::now();
        edgeTime = std::chrono::duration_cast<std::chrono::milliseconds>(edgeEnd - edgeStart).count();
        edgeTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(edgeEnd - edgeStart).count();

        if (edgeTime > 0) {
            std::cout << "[✓] Edge detection completed in " << edgeTime << " ms" << std::endl;
        } else {
            std::cout << "[✓] Edge detection completed in " << edgeTimeMicro << " μs" << std::endl;
        }
        std::cout << std::endl;
    } else {
        std::cout << "[3/5] Skipping edge detection..." << std::endl;
        std::cout << std::endl;
    }

    // ========================================================================
    // STEP 4: Convert to ASCII
    // ========================================================================
    std::cout << "[4/5] Converting to ASCII art..." << std::endl;
    auto asciiStart = std::chrono::high_resolution_clock::now();

    std::vector<AsciiPixel> asciiArt = convertToAscii(scaledImg, edges, useEdges);

    auto asciiEnd = std::chrono::high_resolution_clock::now();
    auto asciiTime = std::chrono::duration_cast<std::chrono::milliseconds>(asciiEnd - asciiStart).count();
    auto asciiTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(asciiEnd - asciiStart).count();

    if (asciiTime > 0) {
        std::cout << "[✓] ASCII conversion completed in " << asciiTime << " ms" << std::endl;
    } else {
        std::cout << "[✓] ASCII conversion completed in " << asciiTimeMicro << " μs" << std::endl;
    }
    std::cout << "    Generated " << asciiArt.size() << " characters" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 5: Display Result
    // ========================================================================
    std::cout << "[5/5] Rendering ASCII art..." << std::endl;
    auto renderStart = std::chrono::high_resolution_clock::now();

    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    printAsciiArt(asciiArt, scaledImg.width, scaledImg.height, useColors);

    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;

    auto renderEnd = std::chrono::high_resolution_clock::now();
    auto renderTime = std::chrono::duration_cast<std::chrono::milliseconds>(renderEnd - renderStart).count();

    std::cout << "[✓] Conversion completed successfully!" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Performance Summary
    // ========================================================================
    auto totalEnd = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart).count();

    std::cout << "Performance Summary:" << std::endl;
    std::cout << "  Loading: " << loadTime << " ms" << std::endl;

    if (scaleTime > 0) {
        std::cout << "  Scaling: " << scaleTime << " ms" << std::endl;
    } else {
        std::cout << "  Scaling: " << scaleTimeMicro << " μs" << std::endl;
    }

    if (useEdges) {
        if (edgeTime > 0) {;
            std::cout << "  Edge Detection: " << edgeTime << " ms" << std::endl;
        } else {
            std::cout << "  Edge Detection: " << edgeTimeMicro << " μs" << std::endl;
        }
    }

    if (asciiTime > 0) {
        std::cout << "  ASCII Conversion: " << asciiTime << " ms" << std::endl;
    } else {
        std::cout << "  ASCII Conversion: " << asciiTimeMicro << " μs" << std::endl;
    }

    if (renderTime > 0) {
        std::cout << "  Rendering: " << renderTime << " ms" << std::endl;
    } else {
        auto renderTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart).count();
        std::cout << "  Rendering: " << renderTimeMicro << " μs" << std::endl;
    }

    std::cout << "  ----------------------------------------" << std::endl;
    std::cout << "  TOTAL: " << totalTime << " ms" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Cleanup
    // ========================================================================
    if (edges) {
        delete edges;
    }

    return 0;
}

