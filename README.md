<p align="center">
  <img src="media/logo.png" alt="PHOENIX Logo" />
</p>

<h2 align="center">PHOENIX - A cross-platform graphics library</h3>

<p align="center">
  <a href="../../actions/workflows/ci.yml"><img src="https://github.com/BenjaminSetton/PHOENIX/actions/workflows/ci.yml/badge.svg?branch=main" alt="Build" /></a>
</p>

## Overview

PHOENIX is a cross-platform and multi-backend graphics library designed to provide a flexible, fine-grained rendering API that bridges the gap between older state-machine APIs (e.g. OpenGL) and modern explicit APIs (e.g. Vulkan, DirectX 12). The library hides explicit resource synchronization and pipeline state management behind a render graph system, allowing clients to focus on rendering logic rather than low-level barrier management.

### Design Goals

- **Flexibility without verbosity** — more control than older state-machine APIs, but less boilerplate than modern explicit APIs
- **No single internal state** — the API is designed to be parallelizable, unlike older APIs that rely on a single global state machine
- **Automatic synchronization** — all resource barriers and layout transitions are automatically managed by the render graph
- **Render graph-driven** — the render graph has global knowledge of all passes per frame, enabling optimizations such as pass trimming, memory aliasing, and parallel execution

## Building

PHOENIX uses [Premake5](https://premake.github.io/) as its build system. A bundled premake5 executable is included under `PHOENIX/vendor/premake/`.

### Prerequisites

- **Vulkan SDK** — must be installed and the `VULKAN_SDK` environment variable must be set
- **CMake** — required to build third-party dependencies (GLFW, glslang, assimp, glm)

### Configuration

The generator and output directories can be configured in `utils/config.bat`:

```bat
set GENERATOR="vs2022"   :: Options: "vs2019", "vs2022", etc
```

### First-time setup

All the project utility scripts can be found in the "utils" folder. Scripts can be run individually depending on the desired use-case. If you want to run the library standalone (without samples), simply run:

```bat
cd utils
build_lib_dependencies.bat
build_project.bat
```

Run the first-time setup script, which builds all dependencies (both samples and lib) and generates the desired solution:

```bat
cd utils
first_time_setup.bat
```

### Build Configurations

The workspace defines two configurations:

- **Debug** — includes debug symbols (`PHX_DEBUG` define) and links debug versions of all dependencies.
- **Release** — optimized build (`PHX_RELEASE` define).

## Current Backends

- **Graphics API:** Vulkan
- **Platform:** Windows (win64)

The architecture is designed to support additional backends and platforms (DirectX, OpenGL, macOS, Linux, etc) in the future.

## Samples

The project includes several sample applications that demonstrate different features of the library. All samples link against the PHOENIX static library and share common code under `samples/common/`.

| Sample | Description |
|---|---|
| **HelloTriangle** | Renders a single colored triangle. Demonstrates the minimal setup: window creation, render device, swap chain, vertex buffer upload via a transfer pass, and a graphics pass with a render graph. |
| **BasicModel** | Loads and renders a 3D model (Suzanne) from a `.fbx` file using assimp. Demonstrates vertex/index buffers, depth testing, uniform buffers for transform data, and camera projection. |
| **TexturedModel** | Extends BasicModel with texture mapping. Demonstrates texture loading, sampling, and binding textures to the pipeline. |
| **ComputeParticles** | GPU-based particle system using compute shaders. Demonstrates compute passes, storage buffers, and compute-to-graphics handoff within the render graph. |
| **ImGui** | Integrates [Dear ImGui](https://github.com/ocornut/imgui) with PHOENIX. Demonstrates UI rendering, input handling, and overlay rendering on top of the scene. |

## Dependencies

### PHOENIX library

| Dependency | Purpose | Source |
|---|---|---|
| [Vulkan SDK](https://vulkan.lunarg.com/) | Graphics API and validation layers | System install (`VULKAN_SDK` env var) |
| [GLFW](https://www.glfw.org/) | Windowing and input | Git submodule (`PHOENIX/vendor/glfw`) |
| [VMA](https://github.com/GPUOpen-LibrariesAndSDK/VulkanMemoryAllocator) | Vulkan memory allocation | Vendored (`PHOENIX/vendor/vma`) |
| [glslang](https://github.com/KhronosGroup/glslang) | Shader compilation (GLSL to SPIR-V) | Git submodule (`PHOENIX/vendor/glslang`) |
| [Premake5](https://premake.github.io/) | Build system / project generation | Vendored (`PHOENIX/vendor/premake`) |

### Samples

| Dependency | Purpose | Source |
|---|---|---|
| [assimp](https://github.com/assimp/assimp) | 3D model loading (FBX, OBJ, etc.) | Git submodule (`samples/common/vendor/assimp`) |
| [glm](https://github.com/g-truc/glm) | Math library (vectors, matrices, transforms) | Git submodule (`samples/common/vendor/glm`) |
| [stb_image](https://github.com/nothings/stb) | Image loading (PNG, JPEG, etc.) | Vendored (`samples/common/vendor/stb_image`) |
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate-mode GUI | Git submodule (`samples/common/vendor/imgui`) |

## License

PHOENIX is licensed under the MIT License. See [LICENSE.txt](LICENSE.txt) for details.