# sp-plus
Audio production program designed to mimic the workflow of the Roland SP-404 MKII sampler while being lightweight, fast, and portable.

![alt text](https://github.com/lucamne/sp-plus/blob/main/demo_scrnsht.png "Demo Screenshot")

## Goals
- Simple UI
- Keyboard controls
- More flexible sequencing and mixing than the SP-404
- Minimal dependecies
- Low resource usage

## Design
The program has a platform-dependent layer which requests services (audio and graphics) from the platform-independent code. This design should allow the program to be ported to a different operating system by writing a new platform layer that implements the API defined in `sp_plus.h`.

## Usage
Read manual.txt

## Build
### sp-plus dependencies
Requires gcc, make, ALSA dev library, and X11 dev library \
Ubuntu: `sudo apt install build-essential libasound2-dev libX11-dev`
### Build smarc static library
1. Navigate to sp-plus/libsrc/smarc
2. Run `make lib`
### Build sp-plus
1. Navigate to sp-plus/src
2. Run build.sh. Set -r flag for release mode
