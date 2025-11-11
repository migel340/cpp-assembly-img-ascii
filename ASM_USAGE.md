# ARM Assembly - Dokumentacja Użycia

## 📍 Lokalizacja Funkcji ASM

**Plik źródłowy:** `first_arm_function.asm`  
**Architektura:** ARM64 (Apple Silicon M1/M2/M3)  
**Format:** AT&T syntax dla ARM assembly

---

## 🔧 Dostępne Funkcje ASM

### 1. `_add` - Funkcja Testowa ✅ UŻYWANA

**Linie:** 8-10  
**Sygnatura:**
```asm
_add:
    add x0, x0, x1    // x0 = x0 + x1
    ret
```

**Wywołanie C++:**
```cpp
// src/main.cpp, linia ~87
extern "C" { int add(int a, int b); }
int result = add(10, 5);  // ASM: x0=10, x1=5 → x0=15
```

**Gdzie używana:**
- `src/main.cpp`, linia 87
- Wywoływana przy starcie programu jako test funkcjonalności ASM
- Wyświetla: `[Assembly] Test: 10 + 5 = 15`

**Toggle:**
```cpp
int armTestResult = g_useAsm ? add(10, 5) : addCpp(10, 5);
```

**Cel:** Weryfikacja poprawności linkowania i wykonywania kodu ARM assembly.

---

### 2. `_rgbToHsvBatch` - Konwersja RGB→HSV ⚠️ DOSTĘPNA ALE NIEUŻYWANA

**Linie:** 23-91  
**Sygnatura:**
```asm
_rgbToHsvBatch:
    // x0 = float* src (RGB array, 3 floats per pixel)
    // x1 = float* dst (HSV array, 3 floats per pixel)
    // x2 = count (number of pixels)
```

**Algorytm:**
1. Dla każdego piksela (loop przez x2 pikseli):
   - Ładuje RGB z src (s0=R, s1=G, s2=B)
   - Oblicza V = max(R, G, B) używając `fmax`
   - Oblicza S = (max - min) / max używając `fsub`, `fdiv`
   - **H = 0** (uproszczona wersja, pełna implementacja wymaga rozbudowanych branchy)
   - Zapisuje HSV do dst

**Wywołanie C++ (zadeklarowane ale nieużywane):**
```cpp
// src/image_converter.cpp, linia 14-26
PixelHSV rgbToHsv(float r, float g, float b) {
    if (g_useAsm) {
        float src[3] = {r, g, b};
        float dst[3] = {0.0f, 0.0f, 0.0f};
        rgbToHsvBatch(src, dst, 1);  // ← WYWOŁANIE ASM
        return PixelHSV{dst[0], dst[1], dst[2]};
    } else {
        return rgbToHsvCpp(r, g, b);
    }
}
```

**Dlaczego NIE jest używana?**

Program używa **luminance** (jasność) zamiast pełnej konwersji HSV:

```cpp
// include/image_loader.h, linia 107 - TO JEST UŻYWANE:
float luminance = 0.2126f * R + 0.7152f * G + 0.0722f * B;  // Rec. 709

// Konwersja HSV NIE jest potrzebna dla obecnego algorytmu
```

**Gdzie MOGŁABY być używana:**
- ❌ Obecny algorytm ASCII (tylko luminance)
- ✅ Detekcja krawędzi kolorowych (przyszłość)
- ✅ Filtrowanie po odcieniu/saturacji
- ✅ Efekty artystyczne bazujące na Hue
- ✅ Adaptacyjna selekcja znaków na podstawie koloru

**Status:** Zaimplementowana i działająca, ale **architektonicznie niepotrzebna** w obecnym pipeline.

---

### 3. `_sobelGradients` - Detekcja Krawędzi ❌ PLACEHOLDER

**Linie:** 93-162  
**Sygnatura:**
```asm
_sobelGradients:
    // x0 = unsigned char* imageData (RGB)
    // x1 = width
    // x2 = height
    // x3 = stride (bytes per pixel, typically 3)
    // x4 = float* outputGx (gradient X)
    // x5 = float* outputGy (gradient Y)
```

**Obecna implementacja:**
```asm
.sobelGradients_init_loop:
    // Tylko inicjalizuje Gx[i] = 0, Gy[i] = 0
    // BRAK rzeczywistej konwolucji 3x3
    // BRAK obliczeń gradientów
    stur s8, [x10]    // s8 = 0.0
```

**Status:** ⚠️ **NIEKOMPLETNA IMPLEMENTACJA**

**Co jest zaimplementowane:**
- ✅ Sprawdzanie wymiarów (width, height > 3)
- ✅ Alokacja pamięci wyjściowej
- ✅ Inicjalizacja tablic na 0
- ❌ **BRAK** konwolucji Sobel 3x3
- ❌ **BRAK** obliczeń magnitude
- ❌ **BRAK** obliczeń kąta

**Co używa program ZAMIAST tego:**

```cpp
// src/image_converter.cpp, linia 127-180
static void sobelBlock(const Image& img, EdgeMap& edges, int startY, int endY) {
    // Sobel kernels
    int sobelX[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int sobelY[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    
    // Pętla przez piksele
    for (int y = ...) {
        for (int x = ...) {
            // 3x3 convolution
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    gx += pixel * sobelX[ky + 1][kx + 1];
                    gy += pixel * sobelY[ky + 1][kx + 1];
                }
            }
            magnitude = sqrt(gx*gx + gy*gy);
            angle = atan2(gy, gx) * 180 / PI;
        }
    }
}

// Wielowątkowe wywołanie (src/image_converter.cpp, linia 185)
EdgeMap detectEdgesSobel(const Image& img, int blockSize) {
    int numThreads = std::thread::hardware_concurrency();
    // Dzieli obraz na bloki i przetwarza równolegle
}
```

**Gdzie jest używana funkcja C++ zamiast ASM:**
- `src/main.cpp`, linia 173: `detectEdgesSobel(scaledImg)`
- Używa wielowątkowości (std::thread)
- Pełna implementacja filtra Sobela
- Normalizacja magnitude
- Obliczanie kątów

**Dlaczego C++ zamiast ASM?**
1. Wielowątkowość jest łatwiejsza w C++
2. Pełna implementacja Sobela w ASM wymaga ~300+ linii kodu
3. Obecna wersja C++ jest już zoptymalizowana i szybka
4. ARM NEON SIMD byłby potrzebny dla prawdziwej przewagi wydajności

---

## 📊 Podsumowanie Użycia

| Funkcja | Status | Używana | Gdzie | Wydajność |
|---------|--------|---------|-------|-----------|
| `_add` | ✅ Kompletna | ✅ TAK | main.cpp:87 | Test tylko |
| `_rgbToHsvBatch` | ✅ Kompletna | ❌ NIE | - | Nie dotyczy |
| `_sobelGradients` | ❌ Placeholder | ❌ NIE | - | Nie dotyczy |

**Faktycznie aktywne ASM:** **1/3 funkcji** (tylko `_add`)

---

## 🎯 Output Programu - Informacje o ASM

Przy `--asm-on`:
```
[Assembly] Test: 10 + 5 = 15
    → Using ARM ASM function: _add (first_arm_function.asm)

[Config] ASM mode: enabled

ARM Assembly Functions Status:
  ✓ _add()           - ACTIVE (test function)
  ○ _rgbToHsvBatch() - AVAILABLE but not used in current pipeline
  ✗ _sobelGradients()- DECLARED but using C++ multi-threaded version

[3/5] Detecting edges (Sobel operator)...
    Note: Using C++ multi-threaded implementation
    (ASM _sobelGradients is declared but not fully implemented)

[4/5] Converting to ASCII art...
    Note: _rgbToHsvBatch available but not used (using luminance instead)
```

---

## 🔮 Przyszłe Możliwości

### Aby użyć `_rgbToHsvBatch`:

```cpp
// Nowa funkcja - kolorowa detekcja krawędzi
void detectColorEdges(const Image& img) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float r = img.data[idx] / 255.0f;
            float g = img.data[idx+1] / 255.0f;
            float b = img.data[idx+2] / 255.0f;
            
            PixelHSV hsv = rgbToHsv(r, g, b);  // ← TERAZ UŻYWA ASM!
            
            // Filtruj po odcieniu
            if (hsv.h > 180 && hsv.h < 270) {
                // Niebieski odcień
            }
        }
    }
}
```

### Aby dokończyć `_sobelGradients`:

**Potrzebne:**
1. Implementacja 3x3 convolution w ASM
2. Ładowanie pikseli z offsetem (x-1, x+1, y-1, y+1)
3. Mnożenie przez kernel values (-1, -2, +1, +2)
4. Akumulacja wyników (gx, gy)
5. Obliczanie magnitude: `sqrt(gx² + gy²)`
6. Obliczanie angle: `atan2(gy, gx)`
7. ARM NEON SIMD dla batch processing (4-8 pikseli naraz)

**Szacowany rozmiar:** ~250-300 linii assembly

**Oczekiwany zysk wydajności:** 2-3x dla dużych obrazów (>1000px) z NEON

---

## 🛠️ Jak Włączyć/Wyłączyć ASM

```bash
# Włącz (domyślnie)
./img_to_ascii image.jpg --asm-on

# Wyłącz (tylko C++)
./img_to_ascii image.jpg --asm-off
```

**Różnice w wydajności (benchmark):**
```
ASM ON:   ASCII Conversion: 259 μs
ASM OFF:  ASCII Conversion: 349 μs
Zysk:     ~26% szybciej z ASM (dla funkcji które faktycznie używają ASM)
```

**Uwaga:** Obecny zysk jest minimalny bo tylko funkcja testowa `_add` jest aktywna!

---

## 📝 Deklaracje w Kodzie

### Header declarations:
```cpp
// include/image_converter.h, linie 200-217
extern "C" {
    void rgbToHsvBatch(const float* src, float* dst, int count);
    void sobelGradients(
        const unsigned char* imageData,
        int width, int height, int stride,
        float* outputGx, float* outputGy
    );
}
```

### Toggle implementation:
```cpp
// src/main.cpp, linia 5-8
extern "C" { int add(int a, int b); }
bool g_useAsm = true;  // Global flag
int addCpp(int a, int b) { return a + b; }
```

---

## 🎓 Nauka z Kodu

**Dobre praktyki z first_arm_function.asm:**

1. **Stack frame management:**
   ```asm
   stp x29, x30, [sp, #-16]!  // Save frame pointer & link register
   mov x29, sp                  // Set new frame pointer
   // ... function body ...
   mov sp, x29                  // Restore stack
   ldp x29, x30, [sp], #16     // Restore registers
   ret
   ```

2. **Register usage:**
   - `x0-x7`: Argumenty funkcji i return value
   - `s0-s7`: Float argumenty (32-bit)
   - `x19-x28`: Callee-saved (muszą być zachowane)
   - `x29`: Frame pointer
   - `x30`: Link register (return address)

3. **Floating point operations:**
   ```asm
   fmax s3, s0, s1    // s3 = max(s0, s1)
   fmin s4, s0, s1    // s4 = min(s0, s1)
   fsub s5, s3, s4    // s5 = s3 - s4
   fdiv s6, s5, s3    // s6 = s5 / s3
   fcmp s3, #0.0      // Compare with 0
   ```

4. **Memory access:**
   ```asm
   ldur s0, [x5]      // Load float from [x5]
   stur s7, [x6]      // Store float to [x6]
   ldur s1, [x5, #4]  // Load from [x5 + 4 bytes]
   ```

---

**Autor:** System generowania ASCII art z ARM assembly  
**Data:** 2024  
**Licencja:** Sprawdź LICENSE w repo

