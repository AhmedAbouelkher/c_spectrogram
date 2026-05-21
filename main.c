#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#ifdef PLATFORM_WEB
#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "./libs/dr_libs/dr_flac.h"
#include "./libs/dr_libs/dr_mp3.h"
#include "./libs/dr_libs/dr_wav.h"
#else
#include "sndfile.h"
#endif

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define RAYGUI_IMPLEMENTATION
#include "./libs/raygui/src/raygui.h"

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
float *processAudio(char *srcPath, int *dstImageWidth, int *dstImageHeight);

const char *audioExt(const char *fileExtension) {
  return fileExtension[0] == '.' ? fileExtension + 1 : fileExtension;
}

float *buildSpectrogram(float *channelBuffer, size_t frameCount,
                        int *dstImageWidth, int *dstImageHeight);

void UpdateDrawFrame(void);

char *audioFilePath = NULL;
const char *audioSourceFiles[] = {
    "resources/file_example_WAV_1MG.wav",
    "resources/alexgrohl-energetic-action-sport.mp3"};
int activeComboBoxIndex = 0;
int newDropdownBoxIndex = 0;

int imageWidth = 0, imageHeight = 0;
float *imageData = NULL;

bool audioDropdownBoxEditMode = false;
size_t windowH = 600;
size_t windowW = 0;

Texture2D texture = {0};
Image img = {0};
const char *msgText = NULL;
double msgTime = 0;

int main(int argc, char *argv[]) {

#ifdef PLATFORM_WEB
  activeComboBoxIndex = 0;
  newDropdownBoxIndex = activeComboBoxIndex;
  audioFilePath = (char *)audioSourceFiles[activeComboBoxIndex];
#else
  if (argc != 2) {
    printf("You should provide the audio file path\n");
    return -1;
  }
  audioFilePath = argv[1];
#endif

  windowW = windowH * RAYLIB_WINDOW_ASPECT_RATION;
  InitWindow(windowW, windowH, "STFT");

  // MARK:- Audio

  imageData = processAudio(audioFilePath, &imageWidth, &imageHeight);

  if (imageData == NULL || imageWidth == 0 || imageHeight == 0) {
    printf("Failed to process the audio file\n");
    printf("is image null %d\n", imageData == NULL);
    printf("is imageWidth = 0 %d\n", imageWidth == 0);
    printf("is imageHeight = 0 %d\n", imageHeight == 0);
    return -2;
  }

  // MARK:- Raylib

  img = GenImageColor(imageWidth, imageHeight, WHITE);

  if (createRayImage(&img, imageData, imageWidth, imageHeight,
                     global_isGrayscale) < 0) {
    printf("FAILED TO Create THE IMAGE for raylib\n");
    return -1;
  }

  texture = LoadTextureFromImage(img);
  if (!IsTextureValid(texture)) {
    printf("FAILED TO load texture\n");
    return -1;
  }

#if defined(PLATFORM_WEB)
  GuiSetStyle(DEFAULT, TEXT_SIZE, 25);
#endif

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    UpdateDrawFrame();
  }
#endif

  UnloadTexture(texture);
  free(imageData);
  CloseWindow();

  return 0;
}

void UpdateDrawFrame(void) {
  Rectangle saveBtnRect = {0, 15, 260, 40};
  Rectangle gsToggleBtnRect = {10, 15, 150, 40};
  // top left corner of the window
  Rectangle audioDropdownBoxRect = {10, 15, 160, 50};

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

  saveBtnRect.x = GetScreenWidth() - saveBtnRect.width - 10;
  if (GuiButton(saveBtnRect, "Save as Raw Image")) {
    const char *fileNoExt = GetFileNameWithoutExt(audioFilePath);
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
    const char *fp = TextFormat("%s%s%s%s%s", outputDir, PATH_JOIN_SEPARATOR,
                                fileNoExt, imgColorType, ".ppm");
    printf("Saving image to: %s...\n", fp);
    if (saveImage(fp, imageData, imageWidth, imageHeight, global_isGrayscale) <
        0) {
      printf("FAILED TO SAVE THE IMAGE FILE: %s\n", fp);
      goto endDrawing;
    }
    printf("Image saved: %s\n", fp);
    // "Image saved at "
    msgText = TextFormat("Image saved at: %s", fp);
    msgTime = GetTime();
  }

  gsToggleBtnRect.x = saveBtnRect.x - gsToggleBtnRect.width - 10;
  bool prevIsGrayscale = global_isGrayscale;
  GuiToggle(gsToggleBtnRect, global_isGrayscale ? "Colored" : "Grayscale",
            &global_isGrayscale);
  if (prevIsGrayscale != global_isGrayscale) {
    if (createRayImage(&img, imageData, imageWidth, imageHeight,
                       global_isGrayscale) < 0) {
      printf("FAILED TO Create THE IMAGE for raylib\n");
      goto endDrawing;
    }

    texture = LoadTextureFromImage(img);
    if (!IsTextureValid(texture)) {
      printf("FAILED TO load texture\n");
      goto endDrawing;
    }
  }

#ifdef PLATFORM_WEB
  if (GuiDropdownBox(audioDropdownBoxRect, "WAV;MP3", &newDropdownBoxIndex,
                     audioDropdownBoxEditMode)) {
    audioDropdownBoxEditMode = !audioDropdownBoxEditMode;

    if (newDropdownBoxIndex != activeComboBoxIndex) {
      activeComboBoxIndex = newDropdownBoxIndex;
      const char *path = audioSourceFiles[activeComboBoxIndex];
      imageData = processAudio((char *)path, &imageWidth, &imageHeight);

      if (imageData != NULL) {
        UnloadImage(img);
        img = GenImageColor(imageWidth, imageHeight, WHITE);
        if (createRayImage(&img, imageData, imageWidth, imageHeight,
                           global_isGrayscale) < 0) {
          printf("FAILED TO Create THE IMAGE for raylib\n");
        } else {
          UnloadTexture(texture);
          texture = LoadTextureFromImage(img);
          if (!IsTextureValid(texture)) {
            printf("FAILED TO load texture\n");
          }
        }
      }
    }
  }
#endif

  // Clear the msgText after 5 seconds
  if (msgText != NULL && (GetTime() - msgTime) > 5.0) {
    msgText = NULL;
  }
  // Raygui draws controls.

  if (msgText != NULL) {
    int msgTextFontSize = 17;
    float msgTextWidth = MeasureText(msgText, msgTextFontSize);
    size_t xPos = (GetScreenWidth() - msgTextWidth) / 2;
    DrawText(msgText, xPos, 10, msgTextFontSize, RED);
  }

endDrawing:
  EndDrawing();
}

float *buildSpectrogram(float *channelBuffer, size_t frameCount,
                        int *dstImageWidth, int *dstImageHeight) {
  size_t STEP_SIZE = WINDOW_SIZE / 2;
  if (frameCount < WINDOW_SIZE)
    return NULL;

  size_t totalWindows = ((frameCount - WINDOW_SIZE) / STEP_SIZE) + 1;
  float *spectrogram = calloc(totalWindows * WINDOW_SIZE, sizeof(float));
  if (!spectrogram)
    return NULL;

  size_t spectrogramIndex = 0;
  for (size_t begin = 0; begin <= frameCount - WINDOW_SIZE;
       begin += STEP_SIZE) {
    float FFT_IN_BUFF[WINDOW_SIZE];
    float complex FFT_OUT_BUFF[WINDOW_SIZE];
    for (size_t i = 0; i < WINDOW_SIZE; i++)
      FFT_IN_BUFF[i] = channelBuffer[begin + i];

    hammingWindow(FFT_IN_BUFF, WINDOW_SIZE);
    fft(FFT_IN_BUFF, 1, FFT_OUT_BUFF, WINDOW_SIZE);

    for (size_t y = 0; y < WINDOW_SIZE; y++)
      spectrogram[spectrogramIndex * WINDOW_SIZE + y] =
          20 + log10f(0.0001f + cabsf(FFT_OUT_BUFF[y]));
    spectrogramIndex++;
  }

  size_t imageWidth = spectrogramIndex;
  size_t imageHeight = WINDOW_SIZE / 2;
  float *transposedData = calloc(imageWidth * WINDOW_SIZE, sizeof(float));
  float *imageData = calloc(imageWidth * imageHeight, sizeof(float));
  if (!transposedData || !imageData) {
    free(transposedData);
    free(imageData);
    free(spectrogram);
    return NULL;
  }

  for (int x = 0; x < imageWidth; x++) {
    for (int y = 0; y < WINDOW_SIZE; y++)
      transposedData[y * imageWidth + x] = spectrogram[x * WINDOW_SIZE + y];
  }
  for (int y = 0; y < imageHeight; y++) {
    memcpy(imageData + y * imageWidth, transposedData + y * imageWidth,
           imageWidth * sizeof(float));
  }

  *dstImageWidth = imageWidth;
  *dstImageHeight = imageHeight;

  free(transposedData);
  free(spectrogram);
  return imageData;
}

#ifdef PLATFORM_WEB
float *processAudio(char *srcPath, int *dstImageWidth, int *dstImageHeight) {
  const char *fileExtension = GetFileExtension(srcPath);
  const char *ext = audioExt(fileExtension);
  float *channelBuffer = NULL;
  size_t channelBufferIndex = 0;
  if (strcmp(ext, "wav") == 0) {
    printf("Processing WAV file: %s\n", srcPath);
    drwav wav;
    if (!drwav_init_file(&wav, srcPath, NULL)) {
      printf("Failed to initialize WAV file.\n");
      return NULL;
    }
    size_t frameCount = (size_t)wav.totalPCMFrameCount;
    channelBuffer = malloc(frameCount * sizeof(float));
    float *frameBuffer = malloc(WINDOW_SIZE * wav.channels * sizeof(float));
    if (!channelBuffer || !frameBuffer) {
      free(channelBuffer);
      free(frameBuffer);
      drwav_uninit(&wav);
      return NULL;
    }
    while (channelBufferIndex < frameCount) {
      size_t framesToRead = frameCount - channelBufferIndex;
      if (framesToRead > WINDOW_SIZE)
        framesToRead = WINDOW_SIZE;
      size_t framesRead =
          drwav_read_pcm_frames_f32(&wav, framesToRead, frameBuffer);
      if (framesRead == 0)
        break;
      for (size_t i = 0; i < framesRead; i++)
        channelBuffer[channelBufferIndex++] = frameBuffer[i * wav.channels];
    }
    free(frameBuffer);
    drwav_uninit(&wav);
  } else if (strcmp(ext, "mp3") == 0) {
    printf("Processing MP3 file: %s\n", srcPath);
    drmp3 mp3;
    if (!drmp3_init_file(&mp3, srcPath, NULL)) {
      printf("Failed to initialize MP3 file.\n");
      return NULL;
    }
    size_t frameCount = (size_t)drmp3_get_pcm_frame_count(&mp3);
    channelBuffer = malloc(frameCount * sizeof(float));
    float *frameBuffer = malloc(WINDOW_SIZE * mp3.channels * sizeof(float));
    if (!channelBuffer || !frameBuffer) {
      free(channelBuffer);
      free(frameBuffer);
      drmp3_uninit(&mp3);
      return NULL;
    }
    while (channelBufferIndex < frameCount) {
      size_t framesToRead = frameCount - channelBufferIndex;
      if (framesToRead > WINDOW_SIZE)
        framesToRead = WINDOW_SIZE;
      drmp3_uint64 framesRead =
          drmp3_read_pcm_frames_f32(&mp3, framesToRead, frameBuffer);
      if (framesRead == 0)
        break;
      for (size_t i = 0; i < (size_t)framesRead; i++)
        channelBuffer[channelBufferIndex++] = frameBuffer[i * mp3.channels];
    }
    free(frameBuffer);
    drmp3_uninit(&mp3);
  } else if (strcmp(ext, "flac") == 0) {
    printf("Processing FLAC file: %s\n", srcPath);
    drflac *flac = drflac_open_file(srcPath, NULL);
    if (!flac) {
      printf("Failed to initialize FLAC file.\n");
      return NULL;
    }
    size_t frameCount = (size_t)flac->totalPCMFrameCount;
    channelBuffer = malloc(frameCount * sizeof(float));
    float *frameBuffer = malloc(WINDOW_SIZE * flac->channels * sizeof(float));
    if (!channelBuffer || !frameBuffer) {
      free(channelBuffer);
      free(frameBuffer);
      drflac_close(flac);
      return NULL;
    }
    while (channelBufferIndex < frameCount) {
      size_t framesToRead = frameCount - channelBufferIndex;
      if (framesToRead > WINDOW_SIZE)
        framesToRead = WINDOW_SIZE;
      drflac_uint64 framesRead =
          drflac_read_pcm_frames_f32(flac, framesToRead, frameBuffer);
      if (framesRead == 0)
        break;
      for (size_t i = 0; i < (size_t)framesRead; i++)
        channelBuffer[channelBufferIndex++] = frameBuffer[i * flac->channels];
    }
    free(frameBuffer);
    drflac_close(flac);
  } else {
    printf("Unsupported file extension: %s\n", fileExtension);
    return NULL;
  }
  float *imageData = buildSpectrogram(channelBuffer, channelBufferIndex,
                                      dstImageWidth, dstImageHeight);
  free(channelBuffer);
  return imageData;
}
#else
float *processAudio(char *srcPath, int *dstImageWidth, int *dstImageHeight) {
  SF_INFO fileInfo;
  memset(&fileInfo, 0, sizeof(fileInfo));

  SNDFILE *file = sf_open(srcPath, SFM_READ, &fileInfo);
  if (!file) {
    printf("Error opening file: %s\n", sf_strerror(NULL));
    return NULL;
  }

  printf("Processing the audio file: %s\n", srcPath);

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

  float *imageData = buildSpectrogram(channelBuffer, channelBufferIndex,
                                      dstImageWidth, dstImageHeight);
  free(channelBuffer);

  return imageData;
}
#endif