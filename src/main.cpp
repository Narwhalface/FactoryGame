#include "engine/Application.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <charconv>
#include <stdexcept>
#include <string>

namespace {
std::size_t parseSize(std::string_view value, const char* name) {
    std::size_t result{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size())
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + std::string(value));
    return result;
}
}

int main(int argc, char** argv) {
    try {
        std::size_t population = 0;
        std::size_t frames = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument(argv[i]);
            if (argument == "--benchmark" && i + 1 < argc) population = parseSize(argv[++i], "population");
            else if (argument == "--frames" && i + 1 < argc) frames = parseSize(argv[++i], "frame count");
            else throw std::invalid_argument("Usage: factory_game [--benchmark COUNT --frames COUNT]");
        }
        factory::engine::Application application;
        application.run(population, frames);
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
