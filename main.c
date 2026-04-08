#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "sndfile.h"

#ifdef _WIN32
#define PATH_JOIN_SEPARATOR "\\"
#else
#define PATH_JOIN_SEPARATOR "/"
#endif

#define ARRAY_LEN(x) ((int)(sizeof(x) / sizeof(x[0])))

#define MAX_FILEPATH_LENGTH 1 << 12
#define RAYLIB_WINDOW_ASPECT_RATION 1200.0 / 600.0

size_t WINDOW_SIZE = 1 << 10;

bool global_isGrayscale = false;

void hammingWindow(float in[], size_t size) {
  // Apply a tapering window to reduce spectral leakage before FFT.
  for (size_t n = 0; n < size; n++) {
    float t = (float)n / (size - 1);
    float coef = 0.54 - 0.46 * cosf(2 * PI * t);
    in[n] *= coef;
  }
}

void fft(float in[], size_t stride, float complex out[], size_t size) {
  // Recursive Cooley-Tukey FFT (radix-2, decimation-in-time).
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

/* t in [0,1]: blue → cyan → green → yellow → red */
static void heatColor(float t, unsigned char rgb[3]) {
  if (t < 0.0f)
    t = 0.0f;
  else if (t > 1.0f)
    t = 1.0f;
  float r, g, b;
  if (t < 0.25f) {
    r = 0.0f;
    g = 0.0f;
    b = 4.0f * t;
  } else if (t < 0.5f) {
    r = 0.0f;
    g = 4.0f * (t - 0.25f);
    b = 1.0f;
  } else if (t < 0.75f) {
    r = 4.0f * (t - 0.5f);
    g = 1.0f;
    b = 1.0f - 4.0f * (t - 0.5f);
  } else {
    r = 1.0f;
    g = 1.0f - 4.0f * (t - 0.75f);
    b = 0.0f;
  }
  rgb[0] = (unsigned char)(255.0f * r);
  rgb[1] = (unsigned char)(255.0f * g);
  rgb[2] = (unsigned char)(255.0f * b);
}

int saveImage(const char *filename, float *data, int width, int height,
              bool isGrayscale) {
  FILE *f = fopen(filename, "wb");
  if (!f)
    return -1;
  fprintf(f, "P6\n%d %d\n255\n", width, height);
  float min_val = 1000.0f, max_val = -1000.0f;
  // Find global min/max for normalization to the heatmap range [0, 1].
  for (int i = 0; i < width * height; i++) {
    if (data[i] < min_val)
      min_val = data[i];
    if (data[i] > max_val)
      max_val = data[i];
  }
  float range = max_val - min_val;
  if (range < 1e-6f)
    range = 1.0f;
  // Write rows from top to bottom so low frequencies appear at the bottom.
  for (int y = height - 1; y >= 0; y--) {
    for (int x = 0; x < width; x++) {
      float val = data[y * width + x];
      float t = (val - min_val) / range;
      unsigned char rgb[3];
      if (isGrayscale) {
        rgb[0] = (unsigned char)(t * 255.0f);
        rgb[1] = (unsigned char)(t * 255.0f);
        rgb[2] = (unsigned char)(t * 255.0f);
      } else {
        heatColor(t, rgb);
      }
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
  return 0;
}

int createRayImage(Image *dst, float *data, int width, int height,
                   bool isGrayscale) {
  float min_val = 1000.0f, max_val = -1000.0f;
  // Find global min/max for normalization to the heatmap range [0, 1].
  for (int i = 0; i < width * height; i++) {
    if (data[i] < min_val)
      min_val = data[i];
    if (data[i] > max_val)
      max_val = data[i];
  }
  float range = max_val - min_val;
  if (range < 1e-6f)
    range = 1.0f;
  for (int y = 0; y < height; y++) {
    int srcY = height - 1 - y; // to flip the image vertically
    for (int x = 0; x < width; x++) {
      float val = data[srcY * width + x];
      float t = (val - min_val) / range;
      Color color = {0};
      if (isGrayscale) {
        unsigned char gray = (unsigned char)(t * 255.0f);
        color.r = gray, color.g = gray, color.b = gray, color.a = 255;
      } else {
        unsigned char rgb[3];
        heatColor(t, rgb);
        color.r = rgb[0], color.g = rgb[1], color.b = rgb[2], color.a = 255;
      }
      ImageDrawPixel(dst, x, y, color);
    }
  }

  return 0;
}

// result manual deallocation is required
float *processAudio(char *srcPath, int *dstImageWidth, int *dstImageHeight) {
  size_t STEP_SIZE = (WINDOW_SIZE) / 2;

  SF_INFO fileInfo;
  memset(&fileInfo, 0, sizeof(fileInfo));

  SNDFILE *file = sf_open(srcPath, SFM_READ, &fileInfo);
  if (!file) {
    printf("Error opening file: %s\n", sf_strerror(NULL));
    return NULL;
  }

  printf("Processing the audio file: %s\n", srcPath);

  float totalDuration = (float)fileInfo.frames / fileInfo.samplerate;

  const int pickedChannel = 0;

  float *frameBuffer = malloc(WINDOW_SIZE * fileInfo.channels * sizeof(float));
  int channelArrSize = fileInfo.frames * sizeof(float);
  float *channelBuffer = malloc(channelArrSize);

  sf_count_t framesRead;
  size_t channelBufferIndex = 0;

  while ((framesRead = sf_readf_float(file, frameBuffer, WINDOW_SIZE)) > 0) {
    // Extract one channel from interleaved multi-channel input.
    for (int i = 0; i < framesRead; i++) {
      channelBuffer[channelBufferIndex++] =
          frameBuffer[i * fileInfo.channels + pickedChannel];
    }
  }

  free(frameBuffer);
  if (sf_close(file) < 0) {
    printf("Failed to close audio file: %s\n", sf_strerror(file));
    return NULL;
  }

  size_t totalWindows = ((fileInfo.frames - WINDOW_SIZE) / STEP_SIZE) + 1;
  float *spectrogram = calloc(totalWindows * WINDOW_SIZE, sizeof(float));
  size_t spectrogramIndex = 0;

  for (size_t begin = 0; begin <= fileInfo.frames - WINDOW_SIZE;
       begin += STEP_SIZE) {

    float FFT_IN_BUFF[WINDOW_SIZE];
    float complex FFT_OUT_BUFF[WINDOW_SIZE];
    size_t fftInBuffIndex = 0;

    size_t end = begin + WINDOW_SIZE;
    for (size_t i = begin; i < end; i++) {
      FFT_IN_BUFF[fftInBuffIndex++] = channelBuffer[i];
    }

    hammingWindow(FFT_IN_BUFF, WINDOW_SIZE);
    fft(FFT_IN_BUFF, 1, FFT_OUT_BUFF, WINDOW_SIZE);

    // Convert FFT magnitude to log scale (dB-like) for better contrast.
    for (size_t y = 0; y < WINDOW_SIZE; y++) {
      float a = 20 + log10f(0.0001f + cabsf(FFT_OUT_BUFF[y]));
      spectrogram[spectrogramIndex * WINDOW_SIZE + y] = a;
    }

    spectrogramIndex++;
  }

  // MARK:- Image
  size_t imageWidth = spectrogramIndex;
  size_t imageHeight = (WINDOW_SIZE / 2);
  float *transposedData = calloc(imageWidth * WINDOW_SIZE, sizeof(float));
  float *imageData = calloc(imageWidth * imageHeight, sizeof(float));

  // >>> Transpose
  for (int x = 0; x < imageWidth; x++) {
    for (int y = 0; y < WINDOW_SIZE; y++) {
      transposedData[y * imageWidth + x] = spectrogram[x * WINDOW_SIZE + y];
    }
  }

  // >>> Copy only one half of the signal (because of FFT)
  for (int y = 0; y < imageHeight; y++) {
    memcpy(imageData + y * imageWidth, transposedData + y * imageWidth,
           imageWidth * sizeof(float));
  }

  *dstImageWidth = imageWidth;
  *dstImageHeight = imageHeight;

  free(transposedData);
  free(spectrogram);
  free(channelBuffer);

  return imageData;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("You should provide the audio file path\n");
    return -1;
  }

  // MARK:- Audio
  int imageWidth = 0, imageHeight = 0;

  float *imageData = processAudio(argv[1], &imageWidth, &imageHeight);

  if (imageData == NULL || imageWidth == 0 || imageHeight == 0) {
    printf("Failed to process the audio file\n");
    printf("is image null %d\n", imageData == NULL);
    printf("is imageWidth = 0 %d\n", imageWidth == 0);
    printf("is imageHeight = 0 %d\n", imageHeight == 0);
    return -2;
  }

  // MARK:- Raylib

  size_t windowH = 600;
  size_t windowW = windowH * RAYLIB_WINDOW_ASPECT_RATION;

  InitWindow(windowW, windowH, "STFT");
  SetTargetFPS(60);

  Image img = GenImageColor(imageWidth, imageHeight, WHITE);

  if (createRayImage(&img, imageData, imageWidth, imageHeight,
                     global_isGrayscale) < 0) {
    printf("FAILED TO Create THE IMAGE for raylib\n");
    return -1;
  }

  Texture2D texture = LoadTextureFromImage(img);
  if (!IsTextureValid(texture)) {
    printf("FAILED TO load texture\n");
    return -1;
  }

  float fontSize = 12;

  const char *msgText = NULL;
  double msgTime = 0;

  Rectangle saveBtnRect = {0, 10, 125, 35};
  const char *saveBtnText = "Save as Raw Image";

  Rectangle gsToggleBtnRect = {10, 10, 80, 30};

  while (!WindowShouldClose()) {
    // START: Save button
    saveBtnRect.x = GetScreenWidth() - saveBtnRect.width - 10;
    Vector2 mouse = GetMousePosition();
    if (1) {
      bool btnHovering = CheckCollisionPointRec(mouse, saveBtnRect);
      bool btnClicked = btnHovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

      if (btnHovering) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
      } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
      }
      if (btnClicked) {
        const char *fileNoExt = GetFileNameWithoutExt(argv[1]);
        char outputDir[MAX_FILEPATH_LENGTH];
        strcpy(outputDir, TextFormat(".%soutput", PATH_JOIN_SEPARATOR));
        if (MakeDirectory(outputDir) != 0) {
          printf("Failed to create output directory. Using the current "
                 "directory...\n");
          strcpy(outputDir, GetWorkingDirectory());
        }
        char *imgColorType = "_colored";
        if (global_isGrayscale) {
          imgColorType = "_grayscale";
        }
        const char *fp =
            TextFormat("%s%s%s%s%s", outputDir, PATH_JOIN_SEPARATOR, fileNoExt,
                       imgColorType, ".ppm");
        printf("Saving image to: %s...\n", fp);
        if (saveImage(fp, imageData, imageWidth, imageHeight,
                      global_isGrayscale) < 0) {
          printf("FAILED TO SAVE THE IMAGE FILE: %s\n", fp);
          continue;
        }
        printf("Image saved: %s\n", fp);
        // "Image saved at "
        msgText = TextFormat("Image saved at: %s", fp);
        msgTime = GetTime();
      }
    }
    // END: save button

    // START: Grayscale toggle button
    if (1) {
      gsToggleBtnRect.x = saveBtnRect.x - gsToggleBtnRect.width - 10;
      bool btnHovering = CheckCollisionPointRec(mouse, gsToggleBtnRect);
      bool btnClicked = btnHovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
      if (btnHovering) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
      } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
      }
      if (btnClicked) {
        global_isGrayscale = !global_isGrayscale;
        if (createRayImage(&img, imageData, imageWidth, imageHeight,
                           global_isGrayscale) < 0) {
          printf("FAILED TO Create THE IMAGE for raylib\n");
          continue;
        }

        texture = LoadTextureFromImage(img);
        if (!IsTextureValid(texture)) {
          printf("FAILED TO load texture\n");
          continue;
        }
      }
    }
    // END: Grayscale toggle button

    // Clear the msgText after 5 seconds
    if (msgText != NULL && (GetTime() - msgTime) > 5.0) {
      msgText = NULL;
    }

    BeginDrawing();
    ClearBackground(BLACK);

    // START: Draw Spectrogram Image Texture
    if (1) {
      Rectangle src = {0, 0, texture.width, texture.height};
      float scaledWidth = (float)texture.height * RAYLIB_WINDOW_ASPECT_RATION;
      if (scaledWidth < windowW)
        scaledWidth = windowW;
      Rectangle dest = {
          (GetScreenWidth() - scaledWidth) * 0.5f,
          (GetScreenHeight() - texture.height) * 0.5f,
          scaledWidth,
          texture.height,
      };
      Vector2 origin = {0.0f, 0.0f};
      DrawTexturePro(texture, src, dest, origin, 0, WHITE);
    }
    // END: Draw Spectrogram Image Texture

    // START: Draw Save Button
    if (1) {
      DrawRectangleRounded(saveBtnRect, 0.3f, 12, DARKPURPLE);
      float saveBtnTextWidth = MeasureText(saveBtnText, fontSize);
      DrawText(
          saveBtnText,
          (int)(saveBtnRect.x + saveBtnRect.width / 2 - saveBtnTextWidth / 2),
          (int)(saveBtnRect.y + saveBtnRect.height / 2 - fontSize / 2),
          fontSize, WHITE);

      // END: Draw Save Button

      // START: Draw Grayscale Toggle Button
      char *gsToggleBtnText = "Grayscale";
      Color gsToggleBgColor = GRAY;
      Color gsToggleTxtColor = WHITE;
      if (global_isGrayscale) {
        gsToggleBtnText = "Colored";
        gsToggleBgColor = ORANGE;
        gsToggleTxtColor = BLACK;
      }
      DrawRectangleRec(gsToggleBtnRect, gsToggleBgColor);
      float gsTextWidth = MeasureText(gsToggleBtnText, fontSize);
      DrawText(
          gsToggleBtnText,
          (int)(gsToggleBtnRect.x + gsToggleBtnRect.width / 2 -
                gsTextWidth / 2),
          (int)(gsToggleBtnRect.y + gsToggleBtnRect.height / 2 - fontSize / 2),
          fontSize, gsToggleTxtColor);
    }
    // END: Draw Grayscale Toggle Button

    if (msgText != NULL) {
      int msgTextFontSize = 17;
      float msgTextWidth = MeasureText(msgText, msgTextFontSize);
      size_t xPos = (GetScreenWidth() - msgTextWidth) / 2;
      DrawText(msgText, xPos, 10, msgTextFontSize, RED);
    }

    EndDrawing();
  }

  UnloadTexture(texture);
  free(imageData);
  CloseWindow();

  return 0;
}