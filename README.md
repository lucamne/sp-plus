# sp-plus
Audio production program designed to mimic the workflow of the Roland SP-404 MKII sampler while being lightweight, fast, and portable.

## Goals
- Simple UI
- Keyboard controls
- More flexible sequencing and mixing than the SP-404
- Minimal dependecies
- Low resource usage

## Branches
The main branch is currently more featureful and uses [raylib](https://github.com/raysan5/raylib) for the UI.

The dev branch is currently in a state of feature regression, but has been redesigned to allow simpler porting by creating distinct platform-dependent and platform-independent layers. This branch only relies on the [smarc resampling library](https://audio-smarc.sourceforge.net/) as audio, graphics, and user input interface directly with native linux APIs (ALSA, XLib).

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
 

## Build
NOTE: Build system will be improved or replaced once dev is at feature parity
### sp-plus dependencies
Requires gcc, make, and ALSA dev library \
Ubuntu: `sudo apt install build-essential libasound2-dev`
### raylib dependencies
Requires ALSA, MESA, X11 dev libraries \
Ubuntu: `sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev`.
### Build Static Libraries
1. Navigate to sp-plus/libsrc/smarc
2. Run `make lib`
3. Navigate to sp-plus/libsrc/raylib/src
4. Run `make`
5. Move `sp-plus/libsrc/raylib/src/libraylib.a` to `sp-plus/lib/libraylib.a`
### Build sp-plus
1. Navigate to sp-plus/src
2. Run `make`
