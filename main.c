#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "sndfile.h"

#define ARRAY_LEN(x) ((int)(sizeof(x) / sizeof(x[0])))

void hammingWindow(float in[], size_t size) {
  for (size_t n = 0; n < size; n++) {
    float t = (float)n / (size - 1);
    float coef = 0.54 - 0.46 * cosf(2 * PI * t);
    in[n] *= coef;
  }
}

void fft(float in[], size_t stride, float complex out[], size_t size) {
  if (size == 1) {
    out[0] = in[0];
    return;
  }
  fft(in, stride * 2, out, size / 2);
  fft(in + stride, stride * 2, out + size / 2, size / 2);
  for (size_t k = 0; k < size / 2; ++k) {
    float t = (float)k / size;
    float complex v = cexp(-2 * I * PI * t) * out[k + size / 2];
    float complex e = out[k];
    out[k] = e + v;
    out[k + size / 2] = e - v;
  }
}

int savePPM(const char *filename, float *spectrogram, int width, int height) {
  FILE *f = fopen(filename, "wb");
  if (!f)
    return -1;
  // P5 = Binary Grayscale, Width, Height, Max Gray Value
  // Note: We use height/2 because the second half of FFT is redundant
  int img_height = height / 2;
  fprintf(f, "P5\n%d %d\n255\n", width, img_height);
  // Find min/max for normalization
  float min_val = 1000.0f, max_val = -1000.0f;
  for (int i = 0; i < width * height; i++) {
    if (spectrogram[i] < min_val)
      min_val = spectrogram[i];
    if (spectrogram[i] > max_val)
      max_val = spectrogram[i];
  }
  // Write rows (Frequency) from top to bottom
  // We iterate i from img_height-1 down to 0 so low frequencies are at the
  // bottom
  for (int i = img_height - 1; i >= 0; i--) {
    for (int j = 0; j < width; j++) {
      float val = spectrogram[j * height + i];
      // Normalize to 0-255
      unsigned char pixel =
          (unsigned char)(255 * (val - min_val) / (max_val - min_val));
      fputc(pixel, f);
    }
  }
  fclose(f);
  return 0;
}

int main(int argc, char *argv[]) {
  size_t WINDOW_SIZE = 1 << 10;

  if (argc != 2) {
    printf("You should provide the file path\n");
    return -1;
  }
  char *filePath = argv[1];
  printf("Playing the audio file: %s\n", filePath);

  SF_INFO fileInfo;
  memset(&fileInfo, 0, sizeof(fileInfo));

  SNDFILE *file = sf_open(filePath, SFM_READ, &fileInfo);
  if (!file) {
    printf("Error opening file: %s\n", sf_strerror(NULL));
    return 1;
  }

  float totalDuration = (float)fileInfo.frames / fileInfo.samplerate;

  int channelsNum = fileInfo.channels;
  float ABUFFER[WINDOW_SIZE * channelsNum];
  sf_count_t framesRead;

  float MONO_BUFFER[WINDOW_SIZE];
  float complex FFT_BUFFER[WINDOW_SIZE];

  // Calculate total windows (assuming no overlap for now)
  size_t total_windows = fileInfo.frames / WINDOW_SIZE;
  float *spectrogram = malloc(total_windows * WINDOW_SIZE * sizeof(float));

  size_t index = 0;

  while ((framesRead = sf_readf_float(file, ABUFFER, WINDOW_SIZE)) > 0) {
    printf("[%zu] Read %lld frames ", index, (long long)framesRead);

    for (sf_count_t i = 0; i < framesRead; i++) {
      if (channelsNum == 1) {
        MONO_BUFFER[i] = ABUFFER[i];
      } else {
        float sum = 0.0f;
        for (int ch = 0; ch < channelsNum; ch++)
          sum += ABUFFER[i * channelsNum + ch];
        MONO_BUFFER[i] = sum / channelsNum;
      }
    }

    if (framesRead < WINDOW_SIZE) {
      size_t remaining = WINDOW_SIZE - framesRead;
      memset(MONO_BUFFER + framesRead, 0, remaining * sizeof(float));
    }

    printf("length of mono buffer: %d\n", ARRAY_LEN(MONO_BUFFER));

    hammingWindow(MONO_BUFFER, WINDOW_SIZE);
    fft(MONO_BUFFER, 1, FFT_BUFFER, WINDOW_SIZE);

    for (int i = 0; i < WINDOW_SIZE; i++) {
      float a = 20 + log10f(0.00001f + cabsf(FFT_BUFFER[i]));
      spectrogram[index * WINDOW_SIZE + i] = a;
    }

    index++;
  }

  if (savePPM("./image5.ppm", spectrogram, index, WINDOW_SIZE) < 0) {
    printf("FAILED TO SAVE THE IMAGE FILE\n");
    return -1;
  }

  free(spectrogram);
  sf_close(file);

  return 0;

  InitWindow(800, 600, "STFT");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
  }

  CloseWindow();

  return 0;
}