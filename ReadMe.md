# GPUWorks_Sandbox - Unreal Engine CLWorks Test Project
GPUWorks_Sandbox is a sample Unreal Engine project showcasing how to use the [GPUWorks Plugin](https://github.com/JSzajek/GPUWorks) for GPU compute and texture operations. It serves as a functional sandbox to demonstrate and validate plugin features.

## Demonstrates
- Creating and compiling GPU programs at runtime.
- Uploading data from UE to GPU buffers.
- Executing GPU kernels and reading result back.
- Writing to GPU images and conversion to UTextures.
- Real-time updates to materials or UI from GPU results.

## Examples
### Boids
<img src="/Resources/01_Boids.gif" alt="Boids Gif" width="512"/>

### Terrain
<img src="/Resources/02_Terrain.gif" alt="Infinite Terrain" width="512"/>

### Cables
<img src="/Resources/03_Rope.gif" alt="Cables" width="512"/>

## Requirements
 - Unreal Engine 5.0+.
 - GPUWorks Plugin.
 - OpenCL 1.2 or later (avaliable via CPU or GPU drivers).
 - C++20 compatible compiler.

## License
Licenses Under the **Apache 2.0** License.