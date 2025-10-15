# debeng
Sandbox for implementing 3D graphics stuff.  
I had a similar project before, but I dislike how I wrote it and thought I'd
rather re-write it (probably less work).

# Structure
edi - editor  
eng - engine (built as a library)  

# Building
```
git clone --recursive git@github.com:KomorXD/debeng.git
cd debeng
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .
make -j`nproc`
```

edi binary in `out/bin/edi-<configuration>`  
eng library in `eng`  
eng tests in `out/bin/eng-tests`  

Mind you I only work with clang and on Linux, I can't promise it will work on other setup for now.

# The things
 - [x] Per-shader, per-material instance rendering (would be only per-shader with batched materials if RenderDoc supported bindless textures...)
 - [x] Color picking
 - [x] Tone-mapping, exposure (HDR)
 - [x] Bloom (using compute shaders) based on [Sledgehammer Games' presentation](https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/)
 - [x] My own ECS storage and views, based on [flecs's](https://github.com/SanderMertens/flecs) creator [Medium posts](https://ajmmertens.medium.com/building-an-ecs-1-where-are-my-entities-and-components-63d07c7da742)
 - [x] PBR materials (w/o height maps as of now)
 - [x] Basic Burley's diffuse model with multi-scattered GGX specular model ([paper](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf))
 - [x] Env maps + diffuse and specular IBL (utilizing compute shaders)
 - [x] Directional, cascaded shadow maps
 - [x] Point and spotlight shadow maps (utilizing geometry shaders)
 - [x] Soft shadows, utilizing random offsets technique
 - [x] Asset manager
 - [x] Deferred asset and data usage, to reduce data traffic
 - [x] More-or-less flexible OpenGL abstractions, rendering
 - [x] Dynamic light sources count (with a limit ofc) so to not waste resources
 - [x] Not-that-miserable editor
 - [x] Whatever smaller thing/thing I forgot
 - [x] 2D forward+ rendering (with that, early-Z for color pass)
 - [x] Culling shadow meshes, shadow casters
 - [x] Scene hierarchy (parent-child only)
 - [ ] Forward+ clustered rendering
 - [ ] MSAA (2xSMAA later perhaps)
 - [ ] Screen space contact shadows
 - [ ] Screen space reflections
 - [ ] Support for transparent objects
 - [ ] SSAO (HBAO/MSAO later?)
 - [ ] Volumetric fogs
 - [ ] Whatever comes to mind next

# Libraries
I used libraries for math, OpenGL and anything that's UI/not multi-platform to avoid hedaches.
 - [glad](https://github.com/Dav1dde/glad)
 - [glfw](https://github.com/glfw/glfw)
 - [glm](https://github.com/g-truc/glm)
 - [ImGui](https://github.com/ocornut/imgui)
 - [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog)
 - [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
 - [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
