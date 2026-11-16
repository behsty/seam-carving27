# Seam Carving

C++17 and OpenCV implementation of **seam carving**, a content-aware image resizing method. The program removes the least important connected paths of pixels (seams) one at a time, so that the main objects in an image keep their shape while the image gets smaller.

Seams are found with three methods, which are compared on the same images and target sizes:

- **Dynamic programming**: optimal seam, the reference method
- **Greedy**: picks the cheapest pixel row by row, without looking ahead
- **Graph cut**: optimal seam found as a minimum s–t cut (max-flow)

<!-- Demo GIF: add e.g. docs/images/dp_steps.gif here -->

## Features

- Energy map from Sobel gradients
- Vertical and horizontal seam removal, resizing to any smaller target size
- Dynamic programming plus two variants for the analysis:
  - **compact memory**: keeps only two cost rows instead of the full table
  - **parallel rows**: each row is computed across several threads (OpenMP)
- Greedy and graph-cut seam finders
- Comparison table of runtime and seam cost for every method
- **Step-by-step visualization**: every seam drawn in red as it is removed, optionally saved as a video
- **Interactive mode**: width, height and method sliders; carve on Enter
- Correctness tests, including DP against brute force and graph cut against DP

OpenCV is used only for image input and output, gradient (Sobel) computation and display. The seam finding and seam removal are implemented in this project; no built-in content-aware resizing function is used.

## Setup (Windows)

### 1. Install the Visual Studio C++ tools

Open **Visual Studio Installer**, choose **Modify** for your Visual Studio installation, and install:

- the **Desktop development with C++** workload
- the MSVC x64/x86 compiler tools and a Windows SDK
- **C++ CMake tools for Windows**, which include CMake and Ninja

These instructions are for the full **Visual Studio** application. Visual Studio Code needs a different setup. CMake 3.21 or newer is required.

### 2. Clone the repository

```powershell
git clone https://github.com/behsty/seam-carving27.git
```

### 3. Install vcpkg

vcpkg downloads and builds OpenCV for you; you do not need to install OpenCV yourself. If you already have vcpkg, skip to step 4.

```powershell
git clone https://github.com/microsoft/vcpkg.git C:/tools/vcpkg
& C:/tools/vcpkg/bootstrap-vcpkg.bat
```

You can use a different folder, but keep vcpkg **outside** this repository.

### 4. Tell CMake where vcpkg is

In the project root, create `CMakeUserPresets.json` with your vcpkg folder:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-debug",
      "inherits": "vcpkg-base",
      "environment": { "VCPKG_ROOT": "C:/tools/vcpkg" }
    },
    {
      "name": "windows-release",
      "inherits": [ "release-build", "windows-debug" ]
    }
  ]
}
```

- Use forward slashes and a full path, e.g. `C:/tools/vcpkg` (not `C:tools/vcpkg`).
- The folder must contain `scripts/buildsystems/vcpkg.cmake`.
- This file is personal to your computer. **Do not commit it.**

### 5. Open the project and configure

1. In Visual Studio, choose **File > Open > Folder** and select the repository root (the folder containing `CMakeLists.txt`).
2. In the configuration dropdown at the top, choose:
   - **windows-debug** while developing, or
   - **windows-release** for any timings you report (Debug timings are much slower and not representative).
3. Wait for **CMake generation finished** in the Output window. If it does not start, use **Project > Configure Cache**.

The first configuration needs an internet connection and can take 20–40 minutes, because vcpkg builds OpenCV from source. Later configurations are fast.

### 6. Build and run

1. Build with **Build > Build All** (`Ctrl+Shift+B`).
2. Next to the green Run button, select **seam_carving.exe** as the startup item.
3. Click Run.

The program carves `assets/sky2.png` 50 columns narrower with every method and prints a comparison table:

```text
Image width: 250
Image height: 168
...
Method                    total ms   energy ms     seam ms  ms/seam find   avg seam cost
----------------------------------------------------------------------------------------
Dynamic programming         181.29       74.72       17.23          0.34         4231.51
DP (compact memory)         178.61       70.48        8.56          0.17         4231.51
DP (parallel rows)          209.97       76.32       18.36          0.37         4231.51
Greedy                      188.80       71.81        0.58          0.01        12329.32
Graph cut                189246.33       70.79   188982.80       3779.66         4230.49
```

After setup, the routine is: pull the latest changes, open the folder, check the preset and startup item, and click Run.

## Running the program

To pass options, run `seam_carving.exe` from its build folder (so it can find `assets/`) in a **Developer PowerShell** (see [Terminal workflow](#terminal-workflow)):

```powershell
cd out\build\windows-release
.\seam_carving.exe [options]
```

Clicking Visual Studio's Run button runs normal mode with no options.

### All options

| Option | What it does | Default |
| --- | --- | --- |
| `--image <file>` | Image to carve | `assets/sky2.png` |
| `--width <W>` | Target width, from 1 to the original width | 50 columns narrower |
| `--height <H>` | Target height, from 1 to the original height | original height |
| `--visualize` | Show every seam (in red) being removed, step by step | off |
| `--video` | Save each method's steps as `<method>_steps.mp4` (`.avi` if MP4 is unavailable) | off |
| `--delay <ms>` | Pause per seam in the window (with `--visualize` or `--interactive`) | 1 |
| `--interactive` | Slider mode: choose the size and method, then carve | off |

Options can be combined in any order. A target larger than the image, or equal to the original size, prints an error.

### 1. Normal mode: compare all methods

Runs every method on the same image and target size, saves the results and prints the comparison table, including each method's seam cost compared with DP.

```powershell
.\seam_carving.exe
.\seam_carving.exe --image assets/surfer.png --width 1150
.\seam_carving.exe --image assets/test.png --width 120 --height 150
```

Methods run in this order: Dynamic programming, DP (compact memory), DP (parallel rows), Greedy, Graph cut.

Files saved in the current folder (overwritten on every run):

| File | Contents |
| --- | --- |
| `energy.png` | Energy map of the input image |
| `<method>_result.png` | The resized image |
| `<method>_seams.png` | The original image with every removed seam in red |

Graph cut is much slower than the other methods (about 0.5 s per seam on a 250×168 image, and more on larger ones). On large images, use a small size change.

### 2. Step-by-step visualization

```powershell
.\seam_carving.exe --visualize --delay 30
.\seam_carving.exe --image assets/sky.png --width 450 --visualize --delay 50
.\seam_carving.exe --video                  # save videos only, no window
.\seam_carving.exe --visualize --video      # show and save
```

Each method plays in its own window.

| Key | Action |
| --- | --- |
| Esc | Skip the rest of this method's animation |
| Any key (on the final frame) | Continue to the next method |

Drawing time is excluded from the timing table, and the results are identical to a run without `--visualize`.

### 3. Interactive mode

```powershell
.\seam_carving.exe --interactive
.\seam_carving.exe --interactive --image assets/surfer.png --delay 20
.\seam_carving.exe --interactive --width 200 --height 150   # starting slider values
```

| Control | Action |
| --- | --- |
| Width / Height sliders | Set the target size. Everything outside the yellow box will be removed. Nothing is carved while dragging. |
| Method slider | 0 DP, 1 DP (compact memory), 2 DP (parallel rows), 3 Greedy, 4 Graph cut |
| Enter | Carve, with the step-by-step visualization |
| Any key (after carving) | Return to the sliders to try another size or method |
| Esc | Skip the animation while carving; quit from the slider screen |

Each carve prints its time and average seam cost, and saves `interactive_result.png` and `interactive_seams.png`.

## Running the tests

`seam_tests.exe` checks that:

- DP matches a brute-force search over every possible seam
- the compact and parallel DP variants return the same seam as full DP
- graph cut finds seams of the same (optimal) cost as DP
- greedy is never cheaper than DP
- edge cases (1 column, 1 row, uniform energy) work
- a full carving run removes the expected pixels

It prints `N checks, 0 failed` when everything passes. In Visual Studio, select `seam_tests.exe` as the startup item and click Run, or from a terminal:

```powershell
ctest --test-dir out/build/windows-release --output-on-failure
```

## Terminal workflow

Build commands must run in a **Developer PowerShell**, which knows where the compiler and its headers are. Open one with **View > Terminal** in Visual Studio, or **Developer PowerShell for VS** from the Start menu. In a normal PowerShell, first run:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
```

Then, from the repository root:

```powershell
cmake --preset windows-release             # configure (first time, or after adding files)
cmake --build out/build/windows-release    # build
.\out\build\windows-release\seam_carving.exe
```

Use `windows-debug` in place of `windows-release` for a Debug build.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `windows-debug` / `windows-release` is missing | Check that `CMakeUserPresets.json` exists in the project root and is valid JSON (step 4). Reopen the folder. |
| `Could not find toolchain file` | `VCPKG_ROOT` in `CMakeUserPresets.json` is wrong. Fix it, then **Project > Delete Cache and Reconfigure**. |
| `vcpkg install failed` … `requires a manifest with a specified baseline` | You are using Visual Studio's built-in vcpkg. Install your own vcpkg (step 3) and point `VCPKG_ROOT` to it. |
| CMake still uses an old vcpkg path | **Project > Delete Cache and Reconfigure**, or delete `out/build/<preset>`. |
| `Cannot open include file: 'limits'` (or `string`, `algorithm`) | The terminal is not a Developer PowerShell. See [Terminal workflow](#terminal-workflow). |
| Compiler or Ninja cannot be found | Install the C++ workload and C++ CMake tools (step 1), then restart Visual Studio. |
| `seam_carving.exe` is missing from the startup dropdown | Build first (`Ctrl+Shift+B`). If it is still missing, switch Solution Explorer to **CMake Targets View** and right-click **seam_carving > Set as Startup Item**. |
| `Failed to load image: <path>` | Image paths are relative to the folder you run from. Run from `out/build/<preset>` (where `assets/` is copied after building) or from the project root. If you changed only an image, rebuild so the copy is updated. |
| A DLL is missing when running a copied `.exe` | Run it from its build folder, which contains the DLLs it needs. |

## Project structure

| File / folder | Purpose |
| --- | --- |
| `src/main.cpp` | Entry point: options, normal mode, comparison table |
| `src/energy.cpp`, `include/energy.h` | Sobel energy map |
| `src/dp.cpp`, `include/dp.h` | Dynamic programming seam finder, plus compact-memory and parallel variants |
| `src/greedy.cpp`, `include/greedy.h` | Greedy seam finder |
| `src/graphcut.cpp`, `include/graphcut.h` | Graph-cut (min-cut / max-flow) seam finder |
| `src/seam.cpp`, `include/seam.h` | Seam removal, seam cost, the shared carving loop and `carveToSize` |
| `src/visualize.cpp`, `include/visualize.h` | Step-by-step visualization and video output |
| `src/interactive.cpp`, `include/interactive.h` | Interactive slider mode |
| `tests/test_seams.cpp` | Correctness tests (`seam_tests.exe`) |
| `assets/` | Test images (copied next to the executable when building) |
| `CMakeLists.txt` | Build definition: sources, OpenCV, OpenMP, asset copy, tests |
| `CMakePresets.json` | Shared build settings (Ninja, vcpkg, Debug/Release) |
| `vcpkg.json` | Declares the `opencv4` dependency |

Every seam finder takes an energy map and returns one seam (`std::vector<int>`, one index per row or column), so all methods share the same carving loop and are compared fairly.

New `.cpp` files under `src/` are picked up automatically after reconfiguring. Keep exactly one `main()` under `src/`, and put headers under `include/`.

The presets target 64-bit Windows; macOS and Linux need their own toolchain setup.

Demo video: *(YouTube link)*  To be updated

## References

- S. Avidan and A. Shamir, [Seam carving for content-aware image resizing](https://dl.acm.org/doi/10.1145/1275808.1276390), SIGGRAPH 2007.
- M. Rubinstein, A. Shamir and S. Avidan, [Improved seam carving for video retargeting](https://dl.acm.org/doi/pdf/10.1145/1360612.1360615), SIGGRAPH 2008.
- Surfer image credit goes to Kiril Dobrev from [Pixabay](https://pixabay.com/photos/blue-beach-surf-travel-surfer-4145659/).
