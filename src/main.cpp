// main.cpp

#include <iostream>
#include "../include/image_loader.h"

extern "C" {
    int add(int a, int b);
}

int main(int argc, char* argv[]) {
    std::cout << "Image to ASCII Converter" << std::endl << std::endl;

    int wynik = add(10, 5);
    std::cout << "ARM ASM Test:: 10 + 5 = " << wynik << std::endl << std::endl;

    if (argc < 2) {
        std::cout << "Użycie: " << argv[0] << " <ścieżka_do_obrazu>" << std::endl;
        std::cout << "Przykład: " << argv[0] << " image.jpg" << std::endl;
        return 1;
    }

    std::string imagePath = argv[1];
    std::cout << "Ładowanie obrazu: " << imagePath << std::endl << std::endl;

    Image img = ImageLoader::loadImage(imagePath, 3);

    if (!img.isValid()) {
        std::cerr << "Nie udało się wczytać obrazu!" << std::endl;
        return 1;
    }

    std::cout << std::endl << "Obraz wczytany poprawnie!" << std::endl;
    std::cout << "Wymiary: " << img.width << " x " << img.height << std::endl;
    std::cout << "Kanały: " << img.channels << std::endl;
    std::cout << "Rozmiar danych: " << imageByteSize(img) << " bajtów" << std::endl << std::endl;

    // Demo: wypisz kilka pierwszych pikseli i ich luminancję
    int sampleX[] = {0, 1, 2};
    int sampleY[] = {0, 1};
    for (int y : sampleY) {
        for (int x : sampleX) {
            if (!inBounds(img, x, y)) continue;
            PixelRGB p = getPixelRGB(img, x, y);
            float lum = getLuminance(img, x, y);
            std::cout << "Pixel (" << x << "," << y << ") - R:" << int(p.r) << " G:" << int(p.g) << " B:" << int(p.b)
                      << "  Luminance: " << lum << std::endl;
        }
    }

    // Możesz teraz przetwarzać piksela po pikselu: przykład pętli
    // for (int y = 0; y < img.height; ++y) {
    //     for (int x = 0; x < img.width; ++x) {
    //         float l = getLuminance(img, x, y);
    //         // mapuj l -> znak ASCII
    //     }
    // }

    return 0;
}
