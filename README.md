# Vulkan Scene Renderer

The project for the Rendering portion of COMP5411. Heavily inspired by 
[Sascha Willems's Vulkan examples](https://github.com/SaschaWillems/Vulkan).

## Changes from Vulkan Examples

The initial code which this project is based on is the 
[glTF Scene Renderering](https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfscenerendering) example.

This project adds and changes the following things:

- Modify the entire project to use [Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp) for automatic resource 
  management.
- Add GLFW for window and surface creation.
- Implement Blinn-Phong Illumination Model
- Port the following features from other Vulkan examples, where each works in conjunction with another:
    - [Multi sampling](https://github.com/SaschaWillems/Vulkan/blob/master/examples/multisampling)
    - [Capturing screenshots](https://github.com/SaschaWillems/Vulkan/blob/master/examples/screenshot)
    - [Pipeline statistics](https://github.com/SaschaWillems/Vulkan/blob/master/examples/pipelinestatistics)
    - [Normal debugging](https://github.com/SaschaWillems/Vulkan/blob/master/examples/geometryshader)
    - [Model tessellation](https://github.com/SaschaWillems/Vulkan/blob/master/examples/tessellation)
- Implement multiple lights (referenced from [LearnOpenGL](https://learnopengl.com/Lighting/Multiple-lights))

## Setup

### Prerequisites

- C++17-compliant compiler
- [CMake 3.11+](https://cmake.org/)
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)

### Steps to Run

1. Clone this repository.
2. Run `download_assets.py` in the root directory to download the necessary assets to run the application.
3. (Optional) Run `data/shaders/compileshaders.py`
4. Create a `build` directory and change into the directory.
5. Run CMake's configuration (`cmake ..`)
6. Build the application (`cmake --build .`)
7. Run the application in `src/VulkanSceneRenderer`.

### CMake Options

- `CMAKE_BUILD_TYPE`: Build type of the application. `RelWithDebInfo` or `Release` is recommended.

### Application Options

- `-vsync`: Enables VSync
- `-validation`: Enables validation layers (if available in the system)
- `--fullscreen`: Runs the application in fullscreen
