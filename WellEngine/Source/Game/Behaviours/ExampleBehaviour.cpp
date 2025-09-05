#include "stdafx.h"
#include "Behaviours/ExampleBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool ExampleBehaviour::Start()
{
	if (_name.empty())
		_name = "ExampleBehaviour"; // For categorization in ImGui.

	_firstFrame = true;

	QueueUpdate();

    return true;
}

bool ExampleBehaviour::Update(TimeUtils &time, const Input &input)
{
	// Rotate this behaviour's entity around the Y axis.
    Transform *t = GetTransform();
	t->RotateYaw(_spinSpeed * time.GetDeltaTime());

	if (!_firstFrame && !_hasCreatedChild && input.GetKey(KeyCode::Y) == KeyState::Pressed)
	{
		Scene *scene = GetScene();
		SceneHolder *sceneHolder = scene->GetSceneHolder();
		ID3D11Device *device = scene->GetDevice();
		Content *content = scene->GetContent();

		// Duplicate example behaviour
		{
			UINT meshID = content->GetMeshID("Maxwell");
			Material mat;
			mat.textureID = content->GetTextureID("Maxwell");
			mat.specularID = content->GetTextureID("Default_Specular");
			mat.ambientID = content->GetTextureID("AmbientBright");

			Entity *maxwell = nullptr;
			if (!scene->CreateMeshEntity(&maxwell, "Maxwell", meshID, mat))
			{
				ErrMsg("Failed to create object!");
				return false;
			}

			maxwell->SetParent(GetEntity());
			maxwell->GetTransform()->SetPosition({0, 0.7f, 0}, Local);

			meshID = content->GetMeshID("Whiskers");
			mat.textureID = content->GetTextureID("Whiskers");
			mat.specularID = content->GetTextureID("Black_Specular");

			Entity *whiskers = nullptr;
			if (!scene->CreateMeshEntity(&whiskers, "Whiskers", meshID, mat, true))
			{
				ErrMsg("Failed to create object!");
				return false;
			}

			whiskers->SetParent(maxwell);

			ExampleBehaviour *behaviour = new ExampleBehaviour();
			if (!behaviour->Initialize(maxwell))
			{
				ErrMsg("Failed to initialize example behaviour!");
				return false;
			}
		}

		_hasCreatedChild = true;
	}

	if (_debugDraw)
	{
		DebugDrawer &debugDraw = DebugDrawer::Instance();

		XMFLOAT3A pos = t->GetPosition(World);
		XMFLOAT3A right = t->GetRight(World);
		XMFLOAT3A up = t->GetUp(World);

		int precision = 24;

		// Draw a mouth as a line strip
		float mouthHeight = 3.5f;

		std::vector<XMFLOAT3> mouthPoints;
		mouthPoints.reserve(precision);

		for (int i = 0; i < precision; i++)
		{
			float t = (i * XM_PI) / (precision - 2.0f);

			float
				x = cosf(t) * 2.0f,
				y = -sinf(t) * 2.0f + mouthHeight;

			mouthPoints.emplace_back(
				pos.x + (x * right.x) + (y * up.x),
				pos.y + (x * right.y) + (y * up.y),
				pos.z + (x * right.z) + (y * up.z)
			);
		}

		debugDraw.DrawLineStrip(mouthPoints, 0.3f, { 1, 0.3f, 0.325f, 1 }, !_overlay);
		mouthPoints.clear();

		for (int i = 0; i < precision; i++)
		{
			float t = (i * XM_PI) / (precision - 2.0f);

			float
				x = -cosf(t) * 2.0f,
				y = -sinf(t) * 0.5f + mouthHeight;

			mouthPoints.emplace_back(
				pos.x + (x * right.x) + (y * up.x),
				pos.y + (x * right.y) + (y * up.y),
				pos.z + (x * right.z) + (y * up.z)
			);
		}

		// Draw the teeth
		debugDraw.DrawLineStrip(mouthPoints, 0.3f, { 1, 0.3f, 0.325f, 1 }, !_overlay);
		
		std::vector<XMFLOAT3> teethPoints;
		teethPoints.reserve(precision);

		for (int i = 0; i < precision; i++)
		{
			float t = (i * XM_PI) / (precision - 2.0f);

			float
				x = cosf(t) * 1.8f,
				y = -sinf(t) * 1.55f + mouthHeight - 0.25f;

			teethPoints.emplace_back(
				pos.x + (x * right.x) + (y * up.x),
				pos.y + (x * right.y) + (y * up.y),
				pos.z + (x * right.z) + (y * up.z)
			);
		}

		debugDraw.DrawLineStrip(teethPoints, 0.25f, { 1, 1, 1, 1 }, !_overlay);
		teethPoints.clear();

		for (int i = 0; i < precision; i++)
		{
			float t = (i * XM_PI) / (precision - 2.0f);

			float
				x = -cosf(t) * 1.8f,
				y = -sinf(t) * 0.5f + mouthHeight - 0.25f;

			teethPoints.emplace_back(
				pos.x + (x * right.x) + (y * up.x),
				pos.y + (x * right.y) + (y * up.y),
				pos.z + (x * right.z) + (y * up.z)
			);
		}

		debugDraw.DrawLineStrip(teethPoints, 0.25f, { 1, 1, 1, 1 }, !_overlay);


		// Draw the eyes
		float eyesHeight = 5.0f;
		float eyesSeparation = 1.1f;

		for (int i = -1; i <= 1; i += 2)
		{
			std::vector<DebugDraw::LineSection> eyePoints;

			float
				eyeX = i * eyesSeparation,
				eyeY = eyesHeight;

			DebugDraw::LineSection section;

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY + 0.75f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY + 0.75f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY + 0.75f) * up.z)
			};
			section.size = 0.01f;
			section.color = { 1, 1, 1, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY + 0.3f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY + 0.3f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY + 0.3f) * up.z)
			};
			section.size = 0.55f;
			section.color = { 1, 1, 1, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY + 0.25f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY + 0.25f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY + 0.25f) * up.z)
			};
			section.size = 0.575f;
			section.color = { 0, 0, 0, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY + 0.1f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY + 0.1f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY + 0.1f) * up.z)
			};
			section.size = 0.6f;
			section.color = { 0, 0, 0, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY + 0.075f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY + 0.075f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY + 0.075f) * up.z)
			};
			section.size = 0.6f;
			section.color = { 0.25f, 0.35f, 1, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY - 0.075f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY - 0.075f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY - 0.075f) * up.z)
			};
			section.size = 0.6f;
			section.color = { 0.25f, 0.35f, 1, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY - 0.1f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY - 0.1f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY - 0.1f) * up.z)
			};
			section.size = 0.6f;
			section.color = { 0, 0, 0, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY - 0.25f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY - 0.25f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY - 0.25f) * up.z)
			};
			section.size = 0.575f;
			section.color = { 0, 0, 0, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY - 0.3f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY - 0.3f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY - 0.3f) * up.z)
			};
			section.size = 0.55f;
			section.color = { 1, 1, 1, 1 };
			eyePoints.emplace_back(section);

			section.position = {
				pos.x + (eyeX * right.x) + ((eyeY - 0.75f) * up.x),
				pos.y + (eyeX * right.y) + ((eyeY - 0.75f) * up.y),
				pos.z + (eyeX * right.z) + ((eyeY - 0.75f) * up.z)
			};
			section.size = 0.01f;
			section.color = { 1, 1, 1, 1 };
			eyePoints.emplace_back(section);

			debugDraw.DrawLineStrip(eyePoints, !_overlay);
		}
	}

	_firstFrame = false;
    return true;
}

#ifdef USE_IMGUI
bool ExampleBehaviour::RenderUI()
{
	// Set spin speed.
	ImGui::SliderFloat("Spin Speed", &_spinSpeed, -15.0f, 15.0f);

	ImGui::Checkbox("Debug Draw", &_debugDraw);
	if (_debugDraw)
		ImGui::Checkbox("Overlay", &_overlay);

    return true;
}
#endif

bool ExampleBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Created Child", _hasCreatedChild, docAlloc);
	obj.AddMember("Spin Speed", _spinSpeed, docAlloc);

	return true;
}
bool ExampleBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_hasCreatedChild = obj["Created Child"].GetBool();
	_spinSpeed = obj["Spin Speed"].GetFloat();

	return true;
}
