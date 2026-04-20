#include <exception>
#include <iostream>

#include "http_server.hpp"

int main() {
    try {
        HttpServer server(8080);
        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Failed to start simulator: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
