# Seam Carving — Assignment 2

C++17 and OpenCV project for content-aware image resizing.

**For teammates:** complete the one-time Windows setup below. After that, click Visual Studio's green Run button to build and run. OpenCV is installed automatically through vcpkg during CMake configuration.

## Current status

The build setup, OpenCV image loading, and Sobel energy-map calculation are implemented. The program loads `assets/test.png`, saves an energy-map preview as `energy.png` in its working directory, and prints the image dimensions in a console. Seam-carving algorithms are still to be implemented. There is no image window or resizing interface yet.

## First-time setup on Windows

### 1. Install the Visual Studio C++ tools

Open **Visual Studio Installer**, choose **Modify** for your Visual Studio installation, and install:

- **Desktop development with C++** workload.
- The MSVC x64/x86 compiler tools and a Windows SDK.
- **C++ CMake tools for Windows**, including CMake and Ninja.

Use the full **Visual Studio** application for these instructions. Visual Studio Code has a different setup. The preset files require CMake 3.21 or newer.

### 2. Clone the repository

Clone this repository using GitHub Desktop or Git. If you already have it, pull the latest changes before continuing.

Each teammate needs their own local build. You do not need to copy another teammate's `out` folder.

### 3. Install vcpkg

If you already have a working vcpkg installation, skip to step 4.

Open PowerShell and run the following example commands. Git must be installed and available in the terminal:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:/tools/vcpkg
& C:/tools/vcpkg/bootstrap-vcpkg.bat
```

You can choose a different folder if needed; use that same folder in the next step. Keep vcpkg outside this repository.

**You do not need to install OpenCV manually or set up its library paths.** The project's `vcpkg.json` declares OpenCV as a dependency.

### 4. Set your own vcpkg path

In the project root, open `CMakeUserPresets.json`. If it is missing, create it using this content:

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

Change **only the `VCPKG_ROOT` value** to your vcpkg installation folder. For example, if you installed it at `D:\tools\vcpkg`, enter `D:/tools/vcpkg` in the JSON.

The existing file may contain `C:/Users/alyy/tools/vcpkg`. That is Alyssa's local path; replace it with yours. Keep the preset name `windows-debug` unchanged.

Save the file. The folder you specify must contain `scripts/buildsystems/vcpkg.cmake`.

### 5. Open the project and let CMake configure

1. In Visual Studio, choose **File > Open > Folder**.
2. Select the repository root folder containing `CMakeLists.txt`.
3. Select **windows-debug** in the configuration dropdown at the top.
4. Wait for CMake configuration to finish. If it does not start automatically, use **Project > Configure Cache**.

An internet connection is needed for the first dependency installation. vcpkg downloads and builds OpenCV and its dependencies, so the first configuration can take a while. Check the CMake output for completion or errors before trying to run.

### 6. Select the startup target and click Run

At the top of Visual Studio, use these selections:

| Toolbar setting | Select |
| --- | --- |
| Run location | Local Machine |
| Build/configure preset | windows-debug |
| Startup target beside the green Run button | seam_carving.exe |

**`CMakeLists.txt` is the build-instructions file. `seam_carving.exe` is the program you run.**

Click the green **Run** button. Visual Studio builds the program if needed and launches it.

Expected output with the supplied image:

```text
OpenCV setup successful!
Image width: 162
Image height: 198
```

This confirms that the program can load OpenCV and read the test image. A console is expected; there is no graphical window yet. If the console closes too quickly, use **Debug > Start Without Debugging** (`Ctrl+F5`).

## Every time after setup

1. Pull the latest project changes.
2. Open the project folder in Visual Studio.
3. Check that **windows-debug** and **seam_carving.exe** are selected.
4. Click the green **Run** button.

There is no need to reinstall vcpkg or manually type build commands each time. Dependency changes may trigger another installation during configuration.

## If something goes wrong

| Problem | Fix |
| --- | --- |
| `windows-debug` is missing | Check that `CMakeUserPresets.json` exists in the project root, contains the example above, and has valid JSON. Reopen the folder if needed. |
| vcpkg toolchain file cannot be found | Check your `VCPKG_ROOT` path and make sure vcpkg was cloned and bootstrapped. |
| Compiler or Ninja cannot be found | Install the C++ workload and C++ CMake tools through Visual Studio Installer, then restart Visual Studio. |
| OpenCV installation or configuration fails | Read the first error in the CMake/vcpkg output. Check your internet connection and installed C++ tools. |
| `seam_carving.exe` is missing from the startup dropdown | Let CMake finish configuring successfully first. Check the CMake output for errors. |
| `Failed to load image!` | The working directory must contain `assets/test.png`. Run from the project root or `out/build/windows-debug` after building. |
| DLL missing when running a copied executable | Run from the original build folder with its generated DLLs. Copying only the `.exe` is insufficient. |
| You changed the vcpkg path but CMake still uses the old one | After saving the correct path, use **Project > Delete Cache and Reconfigure** for the selected preset. This regenerates the local build cache. |

The image path is relative to the program's working directory. CMake copies `assets` beside the executable after linking. If you change only an image and the build reports no work, the build-folder copy can be stale; running from the project root uses the source image directly.

## Files and responsibilities

| File/folder | Purpose |
| --- | --- |
| `src/main.cpp` | Current OpenCV image-loading test and program entry point. |
| `include/` | Folder for project headers. |
| `assets/test.png` | Test input image. |
| `CMakeLists.txt` | Defines C++17, source files, OpenCV linking, and the asset-copy step. |
| `CMakePresets.json` | Shared Ninja, Debug, and Windows build settings. |
| `CMakeUserPresets.json` | Defines `windows-debug` and your local vcpkg path. |
| `vcpkg.json` | Declares the `opencv4` dependency. |
| `out/build/windows-debug/` | Generated executable, DLLs, dependencies, and build files. |

CMake prepares the build instructions, Ninja runs them, the compiler builds the C++ code, and OpenCV provides image-processing functions. vcpkg supplies OpenCV to CMake through its toolchain file.

## Team development notes

- Keep your own vcpkg path locally. The current `.gitignore` does **not** exclude `CMakeUserPresets.json`, so do not commit a personal path change over a teammate's configuration.
- Commit source code, headers, required assets, and shared build/dependency changes. Build output and Visual Studio caches are ignored by Git.
- Put new `.cpp` files under `src/` (subfolders are supported). CMake automatically detects them on the next build; no source-list edit is needed. Keep only one `main()` under `src/`, and place separate test programs outside that folder. Put headers under `include/`.
- Agree on function inputs and outputs before implementing separate algorithms. OpenCV's `cv::Mat` can represent images and energy maps; agree on the energy map's numeric type too.
- The current preset targets **64-bit Windows**. macOS needs a separate toolchain/preset setup; the included `windows-debug` preset is not a Mac configuration.

## Planned work

- Calculate the energy map, for example using Sobel gradients.
- Implement vertical and horizontal seam removal using dynamic programming.
- Implement greedy and graph-cut alternatives.
- Compare visual quality, seams, runtime, and complexity.

The assignment allows OpenCV for image input/output and gradient calculations. The team must implement the seam-carving logic rather than use an existing content-aware resizing function.

## Running the tests

`tests/test_seams.cpp` builds into a second program, `seam_tests.exe`. It checks the DP seam finder against brute force, checks that graph cut finds seams of the same (optimal) cost, checks that greedy is never cheaper than DP, and covers edge cases plus an end-to-end `seamCarve()` run. It prints `N checks, 0 failed` and exits with 0 when everything passes.

In Visual Studio, pick `seam_tests.exe` as the startup target and click Run. From a terminal:

```powershell
cmake --build out/build/windows-debug
ctest --test-dir out/build/windows-debug --output-on-failure
```

## Optional terminal workflow

The Visual Studio Run-button workflow above is sufficient. For terminal use, open **Developer PowerShell for Visual Studio** in the repository root after completing the same vcpkg/preset setup:

```powershell
cmake --preset windows-debug
cmake --build out/build/windows-debug
.\out\build\windows-debug\seam_carving.exe
```
