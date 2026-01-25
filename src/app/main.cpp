
#include "first_app.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
  try {
    std::cout << "Starting app..." << std::endl;
    lve::FirstApp app{};
    std::cout << "App created, running..." << std::endl;
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}