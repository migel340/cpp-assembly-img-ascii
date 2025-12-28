#include <iostream>
#include <string>
#include <chrono>
#include "../include/image_loader.h"
#include "../include/image_converter.h"

extern "C" {
    int add(int a, int b);
}

// Global flag to control ASM usage. Default is OFF; runtime flags control usage.
// Global flags to control which ASM implementations are enabled
bool g_sobelAsm = false;
bool g_hsvAsm = false;

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
    std::cout << "  --width <cols>   Target ASCII art width (default: 120)" << std::endl;
    std::cout << "  --height <rows>  Target ASCII art height (default: 60)" << std::endl;
    std::cout << "  --edges          Enable edge detection" << std::endl;
    std::cout << "  --no-edges       Disable edge detection" << std::endl;
    std::cout << "  --colors         Enable ANSI 24-bit true color output" << std::endl;
    std::cout << "  --no-colors      Disable ANSI colors" << std::endl;
    std::cout << "  --sobel-asm      Use assembly implementation for Sobel (alias: --sobel-asm)" << std::endl;
    std::cout << "  --no-sobel-asm   Disable assembly Sobel (alias: --no-sobel-asm)" << std::endl;
    std::cout << "  --hsv-asm        Use assembly implementation for HSV batch (alias: --hsv-asm)" << std::endl;
    std::cout << "  --no-hsv-asm     Disable assembly HSV batch (alias: --no-hsv-asm)" << std::endl;
    std::cout << "  --hsv            Use RGB->HSV batch conversion and hue-based filtering (also enables --hsv-asm by default)" << std::endl;
    std::cout << "  --no-hsv         Disable HSV conversion and disable HSV ASM" << std::endl;
    std::cout << std::endl;
    std::cout << "Recommended sizes for different terminals:" << std::endl;
    std::cout << "  Small:  80x30   (fits in small terminals)" << std::endl;
    std::cout << "  Medium: 120x60  (default, good balance)" << std::endl;
    std::cout << "  Large:  160x80  (for wide terminals)" << std::endl;
    std::cout << "  XL:     200x100 (full screen terminals)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " image.jpg" << std::endl;
    std::cout << "  " << programName << " photo.png --width 80 --height 30" << std::endl;
    std::cout << "  " << programName << " image.jpg --no-edges" << std::endl;
    std::cout << "  " << programName << " image.jpg --colors" << std::endl;
    std::cout << "  " << programName << " image.jpg --no-colors" << std::endl;
    std::cout << "  " << programName << " image.jpg --no-sobel-asm --no-hsv-asm" << std::endl;
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
    bool useHsv = false;
    bool noRender = false;
    // Track which required flags were explicitly provided
    bool edgesFlagSpecified = false;
    bool hsvFlagSpecified = false;
    bool sobelAsmFlagSpecified = false;
    bool hsvAsmFlagSpecified = false;
    bool colorsFlagSpecified = false;

    // Parse optional arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            targetWidth = std::stoi(argv[++i]);
        } else if ((arg == "-h" || arg == "--height") && i + 1 < argc) {
            targetHeight = std::stoi(argv[++i]);
        } else if (arg == "--no-edges") {
            useEdges = false;
            edgesFlagSpecified = true;
        } else if (arg == "--edges") {
            useEdges = true;
            edgesFlagSpecified = true;
        } else if (arg == "--colors") {
            useColors = true;
            colorsFlagSpecified = true;
        } else if (arg == "--no-colors") {
            useColors = false;
            colorsFlagSpecified = true;
        } else if (arg == "--asm-on" || arg == "--use-asm") {
            // legacy: enable all ASM backends
            g_sobelAsm = true;
            g_hsvAsm = true;
            sobelAsmFlagSpecified = true;
            hsvAsmFlagSpecified = true;
        } else if (arg == "--asm-off" || arg == "--no-asm") {
            // legacy: disable all ASM backends
            g_sobelAsm = false;
            g_hsvAsm = false;
            sobelAsmFlagSpecified = true;
            hsvAsmFlagSpecified = true;
        } else if (arg == "--use-hsv" || arg == "--hsv") {
            // Enable HSV conversion; by default also enable hsv ASM
            useHsv = true;
            g_hsvAsm = true;
            hsvFlagSpecified = true;
            hsvAsmFlagSpecified = true;
        } else if (arg == "--no-hsv") {
            // Disable HSV conversion and HSV ASM
            useHsv = false;
            g_hsvAsm = false;
            hsvFlagSpecified = true;
            hsvAsmFlagSpecified = true;
        } else if (arg == "--sobel-asm") {
            g_sobelAsm = true;
            sobelAsmFlagSpecified = true;
        } else if (arg == "--no-sobel-asm") {
            g_sobelAsm = false;
            sobelAsmFlagSpecified = true;
        } else if (arg == "--hsv-asm") {
            g_hsvAsm = true;
            hsvAsmFlagSpecified = true;
        } else if (arg == "--no-hsv-asm") {
            g_hsvAsm = false;
            hsvAsmFlagSpecified = true;
        } else if ((arg == "--threads" || arg == "--workers") && i + 1 < argc) {
            try {
                g_threadCount = std::stoi(argv[++i]);
                if (g_threadCount < 0) g_threadCount = 0;
            } catch (...) {
                g_threadCount = 0;
            }
        }
        else if (arg == "--no-render") {
            noRender = true;
        }
    }

    // Test add function if any ASM mode is enabled
    bool anyAsm = g_sobelAsm || g_hsvAsm;
    int armTestResult = anyAsm ? add(10, 5) : addCpp(10, 5);
    std::cout << "[" << (anyAsm ? "Assembly" : "C++") << "] Test: 10 + 5 = " << armTestResult << std::endl;
    std::cout << std::endl;

    // Enforce that required option groups were explicitly specified (colors may be omitted)
    // Required groups: edges, hsv choice, sobel-asm choice, hsv-asm choice
    std::vector<std::string> missing;
    if (!edgesFlagSpecified) missing.push_back("edges (use --edges or --no-edges)");
    if (!hsvFlagSpecified) missing.push_back("hsv (use --hsv or --no-hsv)");
    if (!sobelAsmFlagSpecified) missing.push_back("sobel asm (use --sobel-asm or --no-sobel-asm)");
    if (!hsvAsmFlagSpecified) missing.push_back("hsv asm (use --hsv-asm or --no-hsv-asm)");
    if (!colorsFlagSpecified) missing.push_back("colors (use --colors or --no-colors)");

    if (!missing.empty()) {
        std::cerr << "[ERROR] Missing required option groups:" << std::endl;
        for (auto &s : missing) std::cerr << "  - " << s << std::endl;
        std::cerr << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "[Config] Target dimensions: " << targetWidth << "x" << targetHeight << std::endl;
    std::cout << "[Config] Edge detection: " << (useEdges ? "enabled" : "disabled") << std::endl;
    std::cout << "[Config] Colors: " << (useColors ? "enabled" : "disabled") << std::endl;
    std::cout << "[Config] Sobel ASM: " << (g_sobelAsm ? "enabled" : "disabled") << std::endl;
    std::cout << "[Config] HSV ASM: " << (g_hsvAsm ? "enabled" : "disabled") << std::endl;
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

    std::cout << "[✓] Image loaded" << std::endl;
    std::cout << "    Dimensions: " << originalImg.width << "x" << originalImg.height << std::endl;
    std::cout << "    Channels: " << originalImg.channels << std::endl;
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

    std::cout << "[✓] Image scaled" << std::endl;
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

        std::cout << "[✓] Edge detection completed" << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << "[3/5] Skipping edge detection..." << std::endl;
        std::cout << std::endl;
    }

    // compute edge time in milliseconds (float) for metrics
    double edgeMs = -1.0;
    if (useEdges) {
        if (edgeTime > 0) edgeMs = static_cast<double>(edgeTime);
        else edgeMs = static_cast<double>(edgeTimeMicro) / 1000.0;
    }

    // ========================================================================
    // STEP 4: Convert to ASCII
    // ========================================================================
    std::cout << "[4/5] Converting to ASCII art..." << std::endl;
    auto asciiStart = std::chrono::high_resolution_clock::now();

    std::vector<AsciiPixel> asciiArt = convertToAscii(scaledImg, edges, useEdges, useHsv);

    auto asciiEnd = std::chrono::high_resolution_clock::now();
    auto asciiTime = std::chrono::duration_cast<std::chrono::milliseconds>(asciiEnd - asciiStart).count();
    auto asciiTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(asciiEnd - asciiStart).count();

    std::cout << "[✓] ASCII conversion completed" << std::endl;
    std::cout << "    Generated " << asciiArt.size() << " characters" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 5: Display Result
    // ========================================================================
    if (noRender) {
        std::cout << "[5/5] Skipping rendering (no-render)" << std::endl;
        std::cout << "[✓] Conversion completed successfully!" << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << "[5/5] Rendering ASCII art..." << std::endl;
        auto renderStart = std::chrono::high_resolution_clock::now();

        std::cout << "==================================================" << std::endl;
        std::cout << std::endl;

        printAsciiArt(asciiArt, scaledImg.width, scaledImg.height, useColors);

        std::cout << std::endl;
        std::cout << "==================================================" << std::endl;

        auto renderEnd = std::chrono::high_resolution_clock::now();
        (void)renderEnd; // renderEnd used below when computing total

        std::cout << "[✓] Conversion completed successfully!" << std::endl;
        std::cout << std::endl;
    }

    // ========================================================================
    // Performance Summary
    // ========================================================================
    // Decide end time: if rendering skipped, count total up to end of ASCII conversion
    std::chrono::high_resolution_clock::time_point totalEnd;
    if (noRender) {
        totalEnd = asciiEnd;
    } else {
        // renderEnd was declared only in non-noRender branch; compute now
        totalEnd = std::chrono::high_resolution_clock::now();
    }
    double totalTimeMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(totalEnd - totalStart).count();

    // Only show total in human-readable summary; detailed per-step timings removed.
    std::cout << "Performance Summary:" << std::endl;
    std::cout << "  TOTAL: " << totalTimeMs << " ms" << std::endl;
    std::cout << std::endl;

    // Machine-readable metrics for benchmark scripts
    if (useEdges) {
        printf("METRIC:EdgeDetection_ms:%.6f\n", edgeMs);
    } else {
        printf("METRIC:EdgeDetection_ms:nan\n");
    }
    printf("METRIC:TOTAL_ms:%.6f\n", totalTimeMs);
    // HSV metric (may be NaN if not used)
    extern double g_lastHsvMs;
    if (!std::isnan(g_lastHsvMs)) {
        printf("METRIC:HSV_ms:%.6f\n", g_lastHsvMs);
    } else {
        printf("METRIC:HSV_ms:nan\n");
    }

    // ========================================================================
    // Cleanup
    // ========================================================================
    if (edges) {
        delete edges;
    }

    return 0;
}

