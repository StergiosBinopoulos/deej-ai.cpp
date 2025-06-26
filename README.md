# deej‑ai.cpp

A C++ implementation for inference of **Deej‑AI** models using **ONNX Runtime**.


##  Overview

**deej‑ai.cpp** is designed to be portable so the onnx runtime is bundled in the build. Running CMake will download Eigen and the prebuild ONNX Runtime. The only external requirement is the ffmpeg libraries. The playlist generation is much faster than the python version, the scan is also quite faster.


## Prerequisites

- **C++20**
- **CMake** ≥ 3.14
- **Ninja**
- **ffmpeg libraries**

## Quick Start
### Build
```bash
git clone https://github.com/StergiosBinopoulos/deej-ai.cpp
cd deej-ai.cpp

# Download the required libraries using your package manager
# if you are using windows manually download the libraries and add them to $PATH
sudo apt update
sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev
# or if you don't need the headers
# sudo apt install libavcodec libavformat libavutil libswresample

cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
ninja -C build deej-ai
```
To get a portable package build the **package** target instead:
```bash
ninja -C build package
```
The bundle is exported in the `package` folder. Use target **package_zip** to also zip the output.
### Download a model
You can download a ready to go ONNX deej-ai model or use the scipts in the [Deej-AI](https://github.com/teticio/Deej-AI) repository to covert your existing model to ONNX.
```bash
curl -L https://huggingface.co/StergiosBinopoulos/deej-ai.onnx/resolve/main/deej-ai.onnx?download=true --output deej-ai.onnx
```
## Usage

### Scan your Libary
Scan your music folder(s). Replace <music_folder> with the folder you like to scan. Adjust the name of the vectors directory accordingly.
```bash
build/bin/deej-ai --model deej-ai.onnx --scan <music_folder_1> --scan <music_folder_2> --vec-dir test_folder
```

### Generate a Playlist. 

Example 1: Append 15 songs at the end of the input. (This will print the output)
```bash
  build/bin/deej-ai --generate append --input <path_of_song_1> --input <path_of_song_2> ... --nsongs 15 --vec-dir test_folder
```
To save the output in m3u file use *--m3u-out* or *-o*:
```bash
  build/bin/deej-ai --generate append --input <path_of_song_1> --input <path_of_song_2> ... --nsongs 15 --vec-dir test_folder --m3u-out playlist.m3u
```

Example 2: Connect your input songs with 6 songs inbetween them:
```bash
  build/bin/deej-ai --generate connect --input <path_of_song_1> --input <path_of_song_2> --nsongs 6 --vec-dir test_folder
```
Example 3: Append 20 songs. Only determine the playlist from the original cluster of input songs.
```bash
  build/bin/deej-ai --generate cluster --input <path_of_song_1> --input <path_of_song_2> --nsongs 20 --vec-dir test_folder
```

Use -h to view all options:
```bash
build/bin/deej-ai -h
```
