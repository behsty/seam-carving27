# Seam Carving

C++ project for Assignment 2: Automatic Zoom. The goal is content-aware image resizing by removing low-energy seams, with comparisons between dynamic programming, greedy, and graph-cut approaches.

## Current progress

- CMake build setup and OpenCV integration are working.
- The program loads `assets/test.png` and prints its dimensions.
- Energy-map calculation and seam-carving algorithms are not implemented yet.

## Requirements

The included build preset targets **64-bit Windows** with a Debug build.

- Visual Studio 2022 with the **Desktop development with C++** workload, a Windows SDK, and **C++ CMake tools for Windows** (including CMake and Ninja).
- CMake 3.21 or newer (needed for the version 3 preset files; use the bundled version if it meets this requirement).
- Git and vcpkg.

After the one-time setup below, use Visual Studio's green Run button to build and run the program. You do not need to type build commands each time.

## Setup

### 1. Get the project

Clone this repository. In Visual Studio, use **File > Open > Folder** and select the project folder containing `CMakeLists.txt`.

### 2. Install vcpkg if needed

Choose a location outside the project for vcpkg. For example:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:/tools/vcpkg
& C:/tools/vcpkg/bootstrap-vcpkg.bat
```

If you already have vcpkg installed, use that installation instead.

### 3. Set your local vcpkg path

Create or update `CMakeUserPresets.json` in the project root:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-debug",
      "inherits": "vcpkg-base",
      "environment": {
        "VCPKG_ROOT": "C:/tools/vcpkg"
      }
    }
  ]
}
```

Replace `C:/tools/vcpkg` with your own installation path. The existing local preset may contain another teammate's path. Use forward slashes in the JSON path, or escape backslashes as `\\`.

### 4. Select the preset and run in Visual Studio

1. Save `CMakeUserPresets.json` and select **windows-debug** in Visual Studio's configuration dropdown.
2. Wait for CMake configuration and dependency installation to finish. If configuration does not start automatically, use **Project > Configure Cache**.
3. Select **seam_carving.exe** as the startup target beside the green Run button.
4. Click the green **Run** button. Visual Studio builds the program if needed and launches it.

During configuration, vcpkg installs the `opencv4` dependency declared in `vcpkg.json`. The first configuration can take a while because dependencies may need to be downloaded and compiled.

**For future runs:** open the project and click the green Run button. The preset and startup target normally remain selected.

With the supplied test image, the expected output is:

```text
OpenCV setup successful!
Image width: 162
Image height: 198
```

The current program uses a fixed image path and does not accept command-line arguments.

The program currently prints to a console; it does not open an image window. If the console closes too quickly, try **Debug > Start Without Debugging** (`Ctrl+F5`). Its working directory must contain `assets/test.png`; either the project root or `out/build/windows-debug` works after a successful build.

## Optional: build and run from a terminal

The Visual Studio workflow above is sufficient. If you prefer the terminal, open **Developer PowerShell for Visual Studio** in the project root and run:

```powershell
cmake --preset windows-debug
cmake --build out/build/windows-debug
.\out\build\windows-debug\seam_carving.exe
```

## How the build works

| Component | Role |
| --- | --- |
| CMake | Reads the build instructions and generates Ninja build files. |
| Ninja | Runs the compilation and linking steps. |
| C++ compiler | Compiles the source using C++17. |
| vcpkg | Installs OpenCV and makes it available to CMake through its toolchain file. |
| OpenCV | Provides image loading and image-processing functions. |

In `CMakeLists.txt`, `find_package(OpenCV REQUIRED)` locates OpenCV, and `target_link_libraries` links its libraries to the executable. A post-build command copies the `assets` folder beside the executable.

In `src/main.cpp`, `cv::imread()` reads the image into a `cv::Mat`, which stores pixel data. `image.empty()` checks for a loading failure, while `image.cols` and `image.rows` give the width and height. Colour images loaded with the default settings use BGR channel order.

## Project structure

```text
seam-carving27/
├── assets/
│   └── test.png             # Sample input image
├── include/                 # Folder for project headers
├── src/
│   └── main.cpp             # Current OpenCV loading test
├── CMakeLists.txt           # Build instructions
├── CMakePresets.json        # Shared build settings
├── CMakeUserPresets.json    # Local vcpkg path and runnable preset
├── vcpkg.json               # Dependency manifest
└── out/                     # Generated build files (ignored by Git)
```

## Troubleshooting

| Problem | What to check |
| --- | --- |
| CMake cannot find the compiler | Use Developer PowerShell for Visual Studio and confirm the C++ workload is installed. |
| Ninja cannot be found | Install Ninja or Visual Studio's C++ CMake tools and ensure Ninja is available in the terminal environment. |
| vcpkg toolchain file cannot be found | Check `VCPKG_ROOT` in your local preset. It must point to the folder containing `scripts/buildsystems/vcpkg.cmake`. |
| OpenCV configuration fails | Check the earlier vcpkg installation errors in the configure output and confirm dependency installation completed. |
| `Failed to load image!` | Run from the project root or the build folder and check that `assets/test.png` exists there. |
| A required DLL is missing | Run the executable from its build folder with the generated DLLs beside it; rebuild if needed. |

Image paths are resolved relative to the working directory, not automatically relative to the executable. If you replace the input image, its printed dimensions will change. If you run from the build folder, rebuild after changing assets so the post-build copy can run; if the build reports no work, run from the project root to use the source assets directly.

## Working as a team

- Each teammate should configure and build locally using their own vcpkg path.
- Share source files, assets, `CMakeLists.txt`, `CMakePresets.json`, and `vcpkg.json`.
- Keep machine-specific paths in `CMakeUserPresets.json`; review changes before committing because the current `.gitignore` does not exclude this file.
- Generated build files, executables, DLLs, and Visual Studio cache files are ignored by Git.
- When adding a new `.cpp` file, add it to `add_executable(...)` in `CMakeLists.txt`.

## Planned assignment work

- Calculate an energy map from image gradients, for example using Sobel filters.
- Find and remove vertical and horizontal seams using dynamic programming.
- Implement greedy and graph-cut alternatives.
- Compare seams, visual quality, runtime, and complexity.

The assignment allows OpenCV for image input/output and gradient calculations. Seam-carving logic must be implemented by the team rather than calling an existing content-aware resizing function.
