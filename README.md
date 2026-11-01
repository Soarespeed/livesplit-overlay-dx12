| IN GAME | DESKTOP |
|:---:|:---:|
| <img width="269" height="334" alt="in game" src="https://github.com/user-attachments/assets/384167ef-a316-45d9-9b80-c5a9fda6f2e2"> | <img width="266" height="319" alt="desktop" src="https://github.com/user-attachments/assets/936442c3-0611-4942-979e-e893c159546f"> |


# LiveSplit Overlay for ReShade (DirectX 12)

Displays the LiveSplit window inside the game, in full screen, through ReShade.
Works on DirectX 12 games (such as Resident Evil 4 Remake), which the original
addon did not support. Includes a chroma key to remove the background and show
only the text.

## Installation

## ⬇️ Download

**[Download the latest version (Releases)](https://github.com/Soarespeed/livesplit-overlay-dx12/releases/latest)** — extract the `.zip` and follow the installation steps below.

1. Have 1. Have [ReShade](https://reshade.me/#download) installed in the game, with addon support.
2. Copy `livesplit_overlay.addon64` into the game folder, next to `ReShade.ini`
   (usually alongside the game executable).
3. Launch the game and LiveSplit. Do not minimize LiveSplit: if it is minimized,
   capture stops.
4. In LiveSplit, turn off **Always on Top**. With it on, the real LiveSplit window
   may appear on top of the in-game overlay until you alt+tab.
5. Open the ReShade menu (Home key) and go to the **Add-ons** tab, section
   `livesplit_overlay`, to adjust the options.

## Options

- **Show LiveSplit** — turns the overlay on and off.

- **Vertical/Horizontal Alignment** — positions the overlay on screen. The first
  slider moves it horizontally, the second (just below, unlabeled) moves it
  vertically. 0 = left/top, 1 = right/bottom.

- **Vertical/Horizontal Offsets** — fine offset in pixels, added on top of the
  alignment. Useful to snap it to an exact spot.

- **Scale** — overlay size. 1.00 = the LiveSplit window's original size.

- **Overlay opacity** — overall overlay transparency. 1.00 = opaque, 0.00 = invisible.

- **Capture rate (Hz)** — how many times per second the addon reads the LiveSplit
  window. 30 is smooth. This runs on a separate thread and barely affects FPS.

- **Upload rate (Hz) - lower = more FPS** — how many times per second the image is
  uploaded to the GPU. **This is the control that affects FPS the most.** Lower
  values = more FPS, but the timer digits update in steps. Split transitions are
  always sent immediately, so only the running timer looks less smooth. Try 15, 10,
  or 8 until you find the right balance.

- **Chroma key (remove background)** — removes the LiveSplit background, leaving only
  the text and bars, similar to OBS chroma key.

- **Background color** — the color that becomes transparent. Set it to your LiveSplit
  layout's background color (black by default).

- **Key tolerance** — how much the color can vary and still be removed. If background
  remains, increase it. If text starts disappearing, decrease it.

- **Reset position** — resets alignment, offset, and scale to defaults.

## Performance and FPS

On **DirectX 12** games, the overlay may cost between **1 and 4 FPS**, depending on
your GPU and the game. This happens because DirectX 12 requires the overlay image to
be uploaded to the GPU synchronously on every update, and that upload has a cost that
cannot be fully eliminated in this setup.

To minimize it:

- Lower the **Upload rate (Hz)** to 10 or 8.
- If you still want steadier FPS, cap the game's FPS 2 or 3 below your target (for
  example, 117 instead of 120) with an external limiter. With that headroom, the
  upload cost won't drop the frame below your target.

## Notes for speedrunning

This addon only **reads and displays** the LiveSplit window. It does not alter the
timer, the splits, or any game or LiveSplit files. Even so, check the rules of the
site where you submit your runs before using any overlay.
