
I recorded with OBS, then got this trancription using `whisper.cpp`.

```
ffmpeg -i 2024-06-13_09-09-01.mkv -ac 1 -ar 16000 2024-06-13-09-09-01.wav
```

- `-ac 1`: extract audio channel 1
- `-ar 16000`: convert to 16kHz audio; this is what whisper.cpp expects.

```
/home/cceckman/r/github.com/ggerganov/whisper.cpp/main -of transcript.txt -m /home/cceckman/r/github.com/ggerganov/whisper.cpp/models/ggml-large-v3-q5_0.bin -f *.wav
```

Initially I tried this; it seemed to work, but was going slowly; switched to
more threads and a smaller model:

```
/home/cceckman/r/github.com/ggerganov/whisper.cpp/main \
    -t 7 \
    -m /home/cceckman/r/github.com/ggerganov/whisper.cpp/models/ggml-small.en.bin \
    -f *.wav \
    | tee transcript.txt
```

That ran a lot faster; maybe close to real-time? I didn't time it.


