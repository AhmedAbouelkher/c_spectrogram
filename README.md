# c_spectrogram

A simple Short-Time Fourier Transform (STFT) spectrogram visualizer built in C using [Raylib](https://www.raylib.com/) and [libsndfile](http://libsndfile.github.io/libsndfile/).

This project is a sub-problem and a precursor to my ongoing [waview](https://github.com/AhmedAbouelkher/waview) project. It was born out of a passion for audio processing and the challenge of implementing these algorithms from scratch in C.

## Tech Stack & Libraries

- **C**: The core language used for performance and low-level control.
- **Raylib**: Used for window management, 2D graphics rendering, and UI components.
- **libsndfile**: Handles reading and decoding various audio file formats.
- **Standard Math Library (`math.h`)**: Leveraged for trigonometric and logarithmic calculations essential for FFT and scaling.
- **Complex Math Library (`complex.h`)**: Used for handling complex numbers in the FFT implementation.

## Features

- **Real-time Spectrogram**: Visualizes audio frequency content over time.
- **Custom FFT Implementation**: Recursive Cooley-Tukey FFT (radix-2) implemented from scratch.
- **Windowing**: Applies a Hamming window to reduce spectral leakage.
- **Heatmap Visualization**: Supports both colored (heat) and grayscale modes.
- **Wide Format Support**: Thanks to `libsndfile`, this tool supports most popular audio formats (WAV, FLAC, OGG, etc.) as input.
- **Export**: Save the generated spectrogram as a high-quality `.ppm` image.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Prerequisites

- **Raylib**: For windowing and graphics.
- **libsndfile**: For reading various audio formats.
- **C Compiler**: GCC or Clang.
- **pkg-config**: For managing library dependencies.

## Building

To build the project, simply run:

```bash
make
```

For a debug build:

```bash
make debug
```

## Usage

Run the executable and provide the path to an audio file:

```bash
./spectrogram path/to/your/audio.wav
```

### Controls

- **Save as Raw Image**: Exports the current view to the `./output` directory as a `.ppm` file.
- **Grayscale/Colored**: Toggles between different visualization modes.

## About

This project is a deep dive into the mathematical foundations of digital signal processing (DSP), specifically focusing on the transition from time-domain signals to frequency-domain representations.

Check out the main project here: [waview](https://github.com/AhmedAbouelkher/waview)
