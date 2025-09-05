
### To Do:
 - Switch to OpenAL Soft for audio.
 - Improve content management. Register content types in shared JSON file. Add content browser. Let user dynamically add/remove/modify & load/unload content.
 - Skeletal meshes & animations.
 - Generalize materials.
   - Store files like prefabs, contains shaders & textures by name, struct data by value.
   - Make shaders & meshes track vertex data. 
   - Autogenerate input layouts based on loaded shaders.
   - Only allow linking same-format meshes & shaders.
 - Perform light tile culling in compute shader.
 - Programmable particle systems.
 - Cascaded directional lights.
 - Room culling.
 - Give cameras their own render texture that they render to. Render the view camera's texture to the screen at the end (Less hardcoded. Allows camera views to be used as textures freely).
 - Add Decals. Similar to lights, sorted into light tiles & projected in pixel shader before lighting step (in case decal modifies surface properties).
 - Add support for a scripting language like Lua or C#.
