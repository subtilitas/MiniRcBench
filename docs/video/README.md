# Videos of the MiniRcBench

Two storyboards and their stills. A storyboard is a JSON file that
`tools/make_video.py` of the rcbench repository
([github.com/subtilitas/rcbench](https://github.com/subtilitas/rcbench))
renders into an MP4 video of 1920 x 1080 pixels at 30 frames per second.
The tool, its player and its reference `tools/video/README.md` are in
rcbench; nothing in this repository builds or checks a video.

| File | Shows | Length |
| --- | --- | --- |
| `minircbench-build.json` | the build of a MiniRcBench in 10 steps: the parts, the firmware, the wiring and the first screens | 505.57 s |
| `minircbench-features.json` | every screen of the MiniRcBench in 12 chapters, from the tester to the shared rules | 1000.70 s |
| `stills/mini-*.png` | 83 frames of the 240 x 320 display, one PNG a frame, drawn by the host suite | |

Both are narrated in English and play Pachelbel's Canon at half level.

## Rendering

In a checkout of rcbench, with this repository beside it:

```sh
python3 tools/make_video.py ../MiniRcBench/docs/video/minircbench-features.json --check
python3 tools/make_video.py ../MiniRcBench/docs/video/minircbench-features.json
```

- A storyboard given by its path takes its stills from `stills/` beside it.
- `--check` reads the storyboard and names every field the player would
  not draw. It needs Python only.
- A render needs what rcbench's reference lists, and the model
  `tools/video/cad/minircbench.glb`, which
  `tools/video_cad.py --parts` converts from
  `hardware/mechanical/models/parts/minircbench.stp` of this repository.
- The 3D drawing of the bench (`tools/video/player/mini3d.js`) and the
  flat wiring drawing (`tools/video/boards/minircbench-wiring.json`) are
  part of rcbench's player.

## The stills

A still is a frame of the host suite: `cd test && make` writes
`shot_*.ppm`, and each is converted to a PNG of 240 x 320 pixels named
`mini-NAME.png`. The 83 stills are frames of commit `2cdf044`.

## Current limitations

- The stills and the narration describe the firmware at commit `2cdf044`.
  Tag `v0.1.0-rc2` (commit `5a47422`) changes the header of every screen
  (BACK on every screen, LEAVE or BACK on leave pages, red badges), so
  every still differs from that firmware's display.
- Chapter 8 of `minircbench-features` says that S.BUS OUTPUT is not
  built. Firmware from `v0.1.0-rc2` on has the S.BUS OUTPUT screen.
- `minircbench-features` does not mention the servo tester's log
  `LOGS/SRVnnnnn.CSV`, which firmware from `v0.1.0-rc2` on writes.
- The lengths are those of renders made from rcbench. The render of
  `minircbench-build` at 505.57 s used 12 older stills, frames of commit
  `8c7ace3`; a render with these stills is not made.
- `minircbench-build` shows the build the owner stated on 2026-11-14
  and 2026-11-15: the boost board's input on the battery socket's pins,
  its enable pad on GP28 with 10 kΩ to ground, the 0 Ω resistor of size
  0402 between the enable pad and IN+ taken off, a male servo connector
  with the switched 5 V in each of the case's two openings, the INA219
  in the boost board's OUT+, a 470 µF capacitor of 10 V or more and a
  5.6 V zener diode of 2 W to 5 W across the boost board's output, no
  fuse, a cell with its own protection circuit. The zener's package is
  not stated; it is drawn as an axial part.
- The storyboard states on its own, with no other source: the order of
  the steps, the wire colours, the pull-down's place between EN and IN-,
  the daisy chain from the TMT connector to the ESC connector, the
  cell's lead running over the board, and screws of M2.5 x 6.
- The sockets' places and prints are read from Waveshare's photo of the
  board's rear and its pin drawing. How a contact leaves a JST-SH plug
  is described from the housing's form and is not tried on the lead the
  board comes with.
- No frame shows hardware: every still is drawn by the host suite from
  simulated values, and the currents, voltages and rpm in them are not
  measurements.
