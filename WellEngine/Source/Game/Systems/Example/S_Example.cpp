#include "stdafx.h"
#include "S_Example.h"
#include "Game/Game.h"
#include "Game/Scene/Scene.h"

bool S_Example::Update(TimeUtils &time, const Input &input)
{
	return true;
}

#ifdef USE_IMGUI
bool S_Example::RenderUI()
{
	// Query for rendering camera and select it in the currently open scene.
	// To show how systems can be hooked into the game loop.

	if (ImGui::Button("Select View Camera"))
	{
		do
		{
			Scene *scene = GetGame()->GetActiveScene();
			if (!scene)
				break;

			B_Camera *camera = scene->GetMainCamera();
			if (!camera)
				break;

			scene->SetSelection(camera->GetEntity());
		} while (false);
	}

	return true;
}
#endif // USE_IMGUI
