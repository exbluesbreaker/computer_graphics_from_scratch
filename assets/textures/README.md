# Textures

The pictures that the demo scenes are painted with. The build compiles the place of
this folder into the demo library (`CGFS_TEXTURE_DIR`), and `cgfs::demo::CrateTexture()`
reads `wooden_crate.ppm` from it.

## wooden_crate

The side of a wooden crate, 512 x 512: a frame of four boards around upright planks,
with one plank nailed across them from the top-left corner to the bottom-right one. The
texture of the `textures` demo, on the crate and on every tile of the floor.

- `wooden_crate_texture_by_dementiarunner_d79rssy-375w-2x.jpg` is the picture as it
  was put into the repository, under the name it came with, which names its author.
  **Its licence is not recorded here.** It is somebody else's work: before the
  repository is passed on with it, find out on what terms it may be, and write them
  down in this file.
- `wooden_crate.ppm` is the same picture as a binary PPM (P6, 8 bits a channel, no
  comments in the header), the one format the renderer reads (`ReadPpm`). It was made
  once, outside the build, with Pillow 10.2:

  ```python
  from PIL import Image
  Image.open("wooden_crate_texture_by_dementiarunner_d79rssy-375w-2x.jpg").convert("RGB").save("wooden_crate.ppm")
  ```

The PPM is what is drawn and what the tests pin (`tests/demo/textures_test.cpp` checks
seven of its texels, and the two `textures` golden images depend on all of them).
Converting the JPEG again with another decoder can give bytes that differ by a level
here and there: do it only on purpose, and regenerate the two references with it.
