# IM-0

Convert text into vintage punch cards and ticker tapes.

## About

**IM-0** is a lightweight systems programming project designed to bridge modern text inputs with vintage computing mechanics like punch cards and ticker tape codes. Inspired by classic electronics and historical computing archives, this utility lets you encode messages into authentic format representations, render them visually via terminal or web interfaces, and explore legacy data structures.

## Features

* **Punch Card Encoder & Decoder**: Transform text strings into 80-column IBM-style punch card representations with binary hole mapping.
* **Ticker Tape & Morse Code**: Convert messages into continuous ticker tape formats using standardized Morse code sequences.
* **Interactive TUI & CLI**: Built with a modular C implementation supporting both command-line execution and an interactive terminal dashboard.
* **Vector & Image Support**: Generate standalone SVG graphics and process visual data layouts.

## Getting Started

### Prerequisites

* A C compiler (e.g., GCC or Clang)
* The standard math library (`-lm`)

### Compilation

Clone the repository and compile the core binary using GCC:

    git clone [https://github.com/phantekzy/IM-0.git](https://github.com/phantekzy/IM-0.git)
    cd IM-0
    gcc -Wall -Wextra -O2 IM.c -o im -lm

## Usage

Launch the interactive Terminal User Interface (TUI) menu by running the binary with no arguments:

    ./im

From the interactive menu, you can select options to:
1. Encode text into Punch Cards (Terminal view & SVG output)
2. Decode Punch Cards from image files
3. Encode Ticker Tapes using Morse code logic

## Project Structure

* `IM.c` - Core C source code containing encoding tables, mask translation logic, and terminal rendering routines.
* `card.html` - Web-based frontend prototype for browser-based rendering and interactive manipulation.
* `stb_image.h` - Single-header library for native image decoding support.

## Contributing

Contributions, bug reports, and architectural improvements are welcome. Feel free to check out the repository or submit pull requests on [GitHub](https://github.com/phantekzy/IM-0).

## License

Distributed under the MIT License.
