import librosa
import librosa.display
import matplotlib.pyplot as plt
import numpy as np
import sys

if len(sys.argv) != 2:
    print("Usage: python test.py <audio_file>")
    sys.exit(1)

file_path = sys.argv[1]

y, sr = librosa.load(file_path, sr=None, mono=True)

S = np.abs(librosa.stft(y, n_fft=1024, hop_length=512))
S_db = librosa.amplitude_to_db(S, ref=np.max)

plt.figure(figsize=(10, 4))
librosa.display.specshow(
    S_db,
    sr=sr,
    hop_length=512,
    x_axis="time",
    y_axis="hz",
    cmap="gray"
)
plt.colorbar(format="%+2.0f dB")
plt.title("Spectrogram")
plt.tight_layout()
plt.show()