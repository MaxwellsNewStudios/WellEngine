
### To Do:
 - Write documentation.
 - Improve content management. 
   - Register content types in shared JSON file. 
   - Add content browser. 
   - Let user dynamically add/remove/modify & load/unload content in-editor.
 - Improve UI Layout.
   - Improve Content window so all content types are handelled similarly to textures.
   - Implement rect selection in hierarchy (supported by ImGui).
 - Generalize materials.
   - Store files like prefabs, contains shaders & textures by name, struct data by value.
   - Make shaders & meshes track vertex formats. 
   - Automatically create input layouts based on loaded shaders.
   - Only allow linking same-format meshes & shaders.

### Planned:
 - Text rendering.
 - Occlusion culling. Ex: Room culling.
 - Give cameras their own render texture that they render to.
 - Skeletal meshes & animations.
 - Cascaded directional lights.

### Ideas:
 - Physics system. Ex: Jolt or custom (ew).
 - Support for a scripting language. Ex: Lua or C#.
 - Switch to OpenAL Soft for audio.
 - Decals.
   - Sort into screen tiles, similar to lights.
   - Project in pixel shader. 
   - In case decal modifies surface properties, render before lighting.
 - Programmable particle systems.
 - Perform light tile culling in compute shader.

### Known Issues:
 - Docked & undocked hierarchy views share culling results, causing both to cull entities based on the undocked view rect.
 - Scaling gizmo in ImGuizmo fully broken. Until fixed, either use bounds gizmo for scale or scale manually using the Inspector view.
 - Orbiting with "Orbit / Pan" mouse movement mode breaks at steep vertical angles.
 