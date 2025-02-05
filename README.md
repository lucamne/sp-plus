# sp-plus
Audio production program designed to mimic the workflow of the Roland SP-404 MKII sampler while being lightweight, fast, and portable.

## Goals
- Simple UI
- Keyboard controls
- More flexible sequencing and mixing than the SP-404
- Minimal dependecies
- Low resource usage

## Design
The program has a platform-dependent layer which requests services (audio and graphics) from the platform-independent code. This design should allow the program to be ported to a different operating system by writing a new platform layer that implements the API defined in `sp_plus.h`.

## Usage
- Trigger samples (Q, W, E, R). NOTE: Sample paths are hardcoded in sp-plus.c, sample loading from the UI is not yet implemented.
- Kill all playing samples (X)
- Waveform viewer zoom in/out (= / -)
- Active sample playback options
  - Gate trigger mode (G)
  - Reverse playback (V)
  - Iterate through loop modes (L)
  - Increment/Decrement start point (U / SHIFT + U)
  - Increment/Decrement end point (I / SHIFT + I)
  - Increase/Decrease attack time (J / SHIFT + J)
  - Increase/Decrease release time (I / SHIFT + I)
  - Increase/Decrease playback speed (O / SHIFT + O)

NOTE: All these features are implemented but some are not viewable given rendering is not yet re-implemented. \
For a small demo try loading the program, pressing Q to trigger the test sample and then mess around with playback speed and reverse.

## Build
### sp-plus dependencies
Requires gcc, make, ALSA dev library, and X11 dev library \
Ubuntu: `sudo apt install build-essential libasound2-dev libX11-dev`
### Build smarc static library
1. Navigate to sp-plus/libsrc/smarc
2. Run `make lib`
### Build sp-plus
1. Navigate to sp-plus/src
2. Run `make`
