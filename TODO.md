
### To Do:
 - Write documentation.
 - Improve content management. 
   - Register content types in shared JSON file. 
   - Add content browser. 
   - Let user dynamically add/remove/modify & load/unload content.
 - Improve UI Layout.
   - Create separate windows similar to Unity's "Inspector" & "Hierarchy" instead of hiding them in the Scene window.
   - Improve Content window so all content types are handelled similarly to textures.
   - Implement rect selection in both hierarchy (supported by ImGui) & scene view (handle rect as frustum).
 - Generalize materials.
   - Store files like prefabs, contains shaders & textures by name, struct data by value.
   - Make shaders & meshes track vertex data. 
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
