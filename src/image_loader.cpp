// filepath: /Users/spacedesk2/CLionProjects/img-to-ascii/src/image_loader.cpp
#include "../include/image_loader.h"
#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

Image::~Image() {
    if (data != nullptr) {
        stbi_image_free(data);
        data = nullptr;
    }
}

Image::Image(Image&& other) noexcept
    : data(other.data)
    , width(other.width)
    , height(other.height)
    , channels(other.channels)
{
    other.data = nullptr;
    other.width = 0;
    other.height = 0;
    other.channels = 0;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        if (data != nullptr) {
            stbi_image_free(data);
        }

        data = other.data;
        width = other.width;
        height = other.height;
        channels = other.channels;

        other.data = nullptr;
        other.width = 0;
        other.height = 0;
        other.channels = 0;
    }
    return *this;
}

Image ImageLoader::loadImage(const std::string& filepath, int desiredChannels) {
    Image img;

    if (!fileExists(filepath)) {
        std::cerr << "Błąd: Plik nie istnieje: " << filepath << std::endl;
        return img;
    }

    img.data = stbi_load(filepath.c_str(), &img.width, &img.height, &img.channels, desiredChannels);

    if (img.data == nullptr) {
        std::cerr << "Błąd: Nie można wczytać obrazu: " << filepath << std::endl;
        std::cerr << "Powód: " << stbi_failure_reason() << std::endl;
        return img;
    }

    if (desiredChannels > 0) {
        img.channels = desiredChannels;
    }

    std::cout << "Obraz wczytany pomyślnie:" << std::endl;
    std::cout << "  Ścieżka: " << filepath << std::endl;
    std::cout << "  Wymiary: " << img.width << "x" << img.height << std::endl;
    std::cout << "  Kanały: " << img.channels << std::endl;

    return img;
}

bool ImageLoader::fileExists(const std::string& filepath) {
    std::ifstream file(filepath);
    return file.good();
}

