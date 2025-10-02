# Well Engine

A D3D11-based 3D game + engine originally developed as a group project by six students for Lurks Below. 

![Editor](Docs/Images/PrefabEditing.png)

![Sponza](Docs/Images/Sponza.png)

![Lighting](Docs/Images/VolumetricLighting.gif)

![Performance Tools](Docs/Images/Overdraw.png)

## Functionality

### Entities & Behaviours

- Similar to Unity's GameObjects & Components 

### Scenes

- Quad-tree based frustum culling
- Serialization
- Prefabs

### Dev Tools

- Transformation controls
- Entity creation
- Simple debug shape drawing
- Full [Tracy](https://github.com/wolfpld/tracy) support
- Runtime resource loading & shader compilation

### Multi-pass Rendering

- Shadowmapping
- Metallic reflections
- Environment maps
- Tiled forward rendering
	- Transparency pass
- Post-processing
	- Volumetric fog
	- Bloom
 	- Depth of Field

## Libraries

- [Dear Imgui](https://github.com/ocornut/imgui) for UI
- [ImPlot](https://github.com/epezent/implot) for prettier plotting UI
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) for transformation tools
- [SDL3](https://github.com/libsdl-org/sdlwiki/tree/main/SDL3) for window handling
- [Tracy](https://github.com/wolfpld/tracy) for frame capture
- [DirectXTex](https://github.com/microsoft/DirectXTex) for texture loading, manipulation & block compression
- [Magick++](https://github.com/ImageMagick/ImageMagick/tree/main/Magick%2B%2B) for texture creation & manipulation
- [stb](https://github.com/nothings/stb) for texture loading
