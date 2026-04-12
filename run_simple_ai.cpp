#include "simple_ai.hpp"
#include "tetris_core.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void print_help(const char* program) {
  std::cout << "Usage: " << program << " --rows N --cols N --seed N\n"
            << "Options:\n"
            << "  --rows N   Board rows, range 1-31\n"
            << "  --cols N   Board cols, range 1-14\n"
            << "  --seed N   Random seed\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  int rows = 20;
  int cols = 10;
  int seed = 0;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      print_help(argv[0]);
      return 0;
    }
    if (i + 1 >= argc) {
      std::cerr << "Missing value for argument: " << arg << "\n";
      return 1;
    }
    const int value = std::atoi(argv[++i]);
    if (arg == "--rows") {
      rows = value;
    } else if (arg == "--cols") {
      cols = value;
    } else if (arg == "--seed") {
      seed = value;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      print_help(argv[0]);
      return 1;
    }
  }

  try {
    SimpleAi ai;
    TetrisAiGame game(ai, rows, cols, seed);
    const GameResult result = game.play();
    std::cout << "pieces=" << result.pieces_used << " lines=" << result.lines_cleared << "\n";
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
