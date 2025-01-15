# LIBRARY DESIGN

# What is this library trying to achieve?
This library is designed to be a powerful and flexible cross-platform graphics library. Much like any other cross-platform library, the platform code is separated from the library API so that the client is not aware of what API calls are running under the hood. Cross-platform in this context implies both multiple platforms (Windows, MacOS and Linux) and multiple graphics APIs (Vulkan, DirectX and OpenGL). Since the library includes older and modern graphics APIs, the design must find a careful balance between the two. 

The goal is to provide more flexibility than the older APIs, but less verbosity and control than what modern APIs provide. Older APIs like OpenGL are known as state-machine APIs because they tend to expose state-altering functions that the client can call to set up whatever state they want before dispatching a draw call. There is only a single internal state, so the state must be changed very linearly and cannot be parallelized. Modern APIs like Vulkan, on the other hand, force the client to allocate their own reasources and to be extremely explicit about the state before it's dispatched. This means that the client is able to multi-thread Vulkan API calls, and is also heavily encouraged to do so in order to take advantage of the hardware as much as possible. Another consequence of the explicit nature of modern APIs is the fact that every resource access must also be explicitly synchronized, since the driver no longer manages this under the hood. Taking these differences into account, this library's design must be fine-grained enough to be parallelizable, and therefore must *not* contain a single internal state much like the older APIs. It must also hide all explicit synchronization calls and instead manage them all under the hood in order to be compatible with both kinds of graphics APIs. In order to achieve this, the library must leverage a render graph system that will automatically find any resource dependencies between passes and insert the proper barriers when using APIs that require it. 

# Important API ojects
**IRenderDevice**
- Provides a way to interface with the physical and logical render device. This is where the client can request information about the render device, such as it's name, available memory, and other stats. 
- Any allocations performed by the client must be done through the render device since this is the object that owns all the virtual memory. For example, if the client wishes to allocate a buffer in VRAM it must get the handle to the render device and call a function to allocate the buffer's memory. Any allocation call must return a valid ID in case the allocation was successful, or a reserved invalid ID otherwise.
- (?) Must handle calls between different threads when using graphics APIs that allow multi-threaded allocations.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IDeviceContext**
- Provides access to all the commands that can be issued to the GPU for things like setting vertex/index buffers, bindings descriptor sets, binding shaders, binding pipeline states, issuing graphics draw calls, issuing compute dispatch calls, binding render targets, and so on. This is the main way that the client will interact with the library, except for any allocation calls since those must be done directly through the render device object.
- Must handle calls between different threads when using graphics APIs that allow multi-threaded allocations.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IWindow**
- Provides a way to interface with the main window. Through the interface, the client may request information such as the window dimensions and position.
- This library will only support a single window initially, but it should be written in a way that allows multiple windows in the future if absolutely necessary. 
**ITexture**
- Contains functionality for getting/setting data for a given texture object, such as it's size (width, height, depth), type (1D, 2D, 3D, cube, array), format (R8G8B8A8_UINT, R32_SFLOAT, etc), mipCount, name (debug only?). It must also provide a way for the client to create image views on whatever section of the texture they want, whether it's on a range of mips, all mips, the entire image, or only for a section of the image.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**ITextureView**
- Provides a way to represent a specific portion (or view) of a texture object. This is the object that is bound to the pipeline, rather than the textures themselves. The reason behind this is to allow different operations to happen on different parts of the texture at the same time. For example, reading from mip level 0, blurring and writing the output into mip level 1 of the same texture object.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**ICommandBuffer**
- Holds a collection of all recorded commands. The interface provides a way to record a variety of new commands, but does not expose functionality for removing commands which have already been recorded. Command buffers can later be submitted to the GPU, once the client is done recording commands.
- The interface is written in a thread-safe manner, so that commands can be written into the buffer by different threads.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IBuffer**
- Holds information about a generic buffer, such as it's size and format. This generic buffer object can be used for any buffer that lives on the GPU, such as a vertex buffer, index buffer, constant buffer, etc.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IPipeline**
- Contains the entire state of the pipeline that may later be submitted to the GPU, and offers a way to alter the state of the pipeline. This holds information such as the rasterizer state, the blend state, cull mode, primitive topology, render target and depth formats, etc.
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IFrameBuffer**
- Provides a way to add or remove texture objects into a collection called a framebuffer. This can then be bound to a render pass and used to output the data of the render pass
- Holds references (non-owning) to a collection of framebuffer attachments
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IShader**
- Contains basic information about a shader object, such as the shader's bytecode, it's type (pixel, geometry, vertex, compute) and other pieces of metadata (e.g. compile status). It does *not* directly provide a way to compile shaders from file, but rather represents the shader object itself and owns the pointer to it's shader bytecode. 
- Must issue the correct graphics API calls once a graphics API is selected by the client
**IMaterial**
- Materials combine a shader and other pieces of data, used to represent how a surface should be shaded. It will likely hold a collection of textures, buffers and other shader variables. 
- It's important to note that, while a material can potentially reference lots of data, it does *not* own any of it. 
- Must issue the correct graphics API calls once a graphics API is selected by the client
**ISwapChain**
- Provides an interface to the swap chain used to present images to the screen. The Present() function is exposed through here, which is used at the very end of the render loop to switch back buffers and show the rendered image. The client can also get or set other information from the swap chain, such as the refresh rate, v-sync, presentation mode, number of frames in flight, etc.
- Must issue the correct graphics API calls once a graphics API is selected by the client

# Directory structure
The main features of the library's interface aim to separate the API from it's platform code. As such, the two main folders inside "src" will be an "api" folder and a "platform" folder, where the "api" folder contains generic objects which are meant to be included by the client and "platform" contains any platform-dependent code and must NOT be included by the client. Other sibling folders to the "src" folder include "tools" and "vendor". The former hosts tools specific to the library which are not necessarily meant to be used by the client, and the latter contains any third-party dependencies used by the PHOENIX library (see more on the "Third-party" section). Below is an example of what the core file structure might look like:
```
- out* (this folder is generated by the build process)
- src
  L include
    L PHX
      L interface
      L types
  L platform
  L utils
- tools
  L python
- vendor
  L vulkan
  L openGL
  L directX
  L windows
  L linux
  L macOS
```

# Third-party
The library must contain minimal dependencies to make it as easy as possible to pull and build. 
- GLFW (window library)
- Premake (build system)
- Vulkan SDK
- VMA (Vulkan Memory Allocator)
- glslc (shader compilation library)
- Python

# Coding conventions
- **NAMESPACES**: Everything inside the PHOENIX library must be inside the PHX namespace, and all namespace declarations must follow the upper-case *snake_case* convention. E.g. `namespace OBJECT_FACTORY { ... }`
- **FILE NAMES**: Every filename must follow the *snake_case* naming convention. E.g. "file_utils.h"
- **CLASS TYPENAMES**: Every class typename must follow the *PascalCase* naming convention. E.g `class IDeviceContext { ... }`
- **STRUCT TYPENAMES**: Every struct typename must follow the *PascalCase* naming convention. E.g. `struct ImageCreateInfo { ... }`
- **ENUM TYPENAMES**: All enums must be declared as *enum class*, and must also follow the upper-case *snake_case* naming convention. E.g. `enum class TEXTURE_FORMAT { ... }`
- **ENUM VALUES**: All enum values must follow the upper-case *snake_case* naming convention. E.g. `enum class TEXTURE_FORMAT { RGBA8_UINT, RGBA32_FLOAT, ... }`
- **MACROS**: All macros must follow the *MACRO_CASE* naming convention and be pre-prended by "PHX". E.g. `#define PHX_ASSERT_ALWAYS(...)`
- **FUNCTION POINTERS**: All function pointer declarations must follow the *camelCase* naming convention, and must also be pre-pended with "fp_". `E.g. void(*fp_onWindowResized)(u32 w, u32 h)`
- **CONSTANT EXPRESSION VALUES**: All constant expressions values must follow the upper-case *snake_case* naming convention. E.g. `static constexpr bool SHOW_WINDOW = true;`
- **CONSTANT EXPRESSION FUNCTIONS**: All constant expression functions must follow the *PascalCase* naming convention, much like member functions (defined below). E.g. `constexpr u64 CalculateFixedNumber(...)`
- **MEMBER FUNCTIONS**: All member functions from a class must follow the *PascalCase* naming convention. E.g. `void ITexture::SetEnable(bool val) const;`
- **MEMBER VARIABLES**: All member variables must follow the *camelCase* naming convention. They must also be pre-pended with "m_". E.g. `bool m_isEnabled;`
- **CONST CORRECTNESS**: The library must be as const-correct as possible. This means marking pointers/references as const when they must NOT be mutated, as well as marking member functions as const when they must NOT modify the object state. Loose variables by value may be marked as const for extra precaution, but this is not required.
- **INCLUDE ORDER**: The include files must be written in this order, where every every category (or block) is in alphabetical order. (1) Any included file that is *not* from the library code, must be included with angle braces (< and >), and *must* be included before any other files. (2) In cases where the "parent" header file is being included in a cpp file, it must be listed as it's own category after non-library includes. (3) Any other included header files must be added in their own category and must come last.