import librosa
import matplotlib.pyplot as plt

import sys

if len(sys.argv) != 2:
    print("Usage: python test.py <audio_file>")
    sys.exit(1)

file_path = sys.argv[1]

y, sr = librosa.load(file_path, sr=None, mono=True)

plt.figure(figsize=(10, 4))
plt.specgram(y, NFFT=1024, Fs=sr, noverlap=0, cmap="gray")
plt.xlabel("Time")
plt.ylabel("Frequency")
plt.title("Spectrogram")
plt.colorbar()
plt.tight_layout()
plt.show()