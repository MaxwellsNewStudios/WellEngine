#include "stdafx.h"
#include "Scenes/Scene.h"
#include "Behaviours/InventoryBehaviour.h"
#include "Behaviours/BreadcrumbBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool InventoryBehaviour::Start()
{
	if (_name.empty())
		_name = "InventoryBehaviour"; // For categorization in ImGui.

	Entity* cam = GetEntity();

	// Compass
	{
		Entity* compass = nullptr;
		if (!GetScene()->CreateEntity(&compass, "Compass", { {0,0,0},{0.1f,0.1f,0.1f},{0,0,0,1} }, false))
		{
			ErrMsg("Failed to initialize compass holder!");
			return false;
		}
		compass->GetTransform()->SetPosition({ -0.5f, -0.35f, 0.6f });
		compass->GetTransform()->SetEuler({ XM_PI / 4, XM_PI, XM_PI / 6 });

		_compass = new CompassBehaviour();
		if (!_compass->Initialize(compass))
		{
			ErrMsg("Failed to initialize compass behaviour!");
			return false;
		}
		compass->SetSerialization(false);
		compass->Disable();
	}

	// Held breadcrumb
	{
		Content* content = GetScene()->GetContent();
		UINT meshID = content->GetMeshID("Breadcrumb");
		Material mat{};
		mat.textureID = content->GetTextureID("Breadcrumb_Red");
		mat.normalID = content->GetTextureID("Breadcrumb_Normal");
		mat.specularID = content->GetTextureID("Breadcrumb_Red_Specular");
		mat.glossinessID = content->GetTextureID("Breadcrumb_Glossiness");
		mat.ambientID = content->GetTextureID("Breadcrumb_Red_Light");
		mat.occlusionID = content->GetTextureID("Breadcrumb_Occlusion");
		mat.vsID = content->GetShaderID("VS_Geometry");

		Entity* breadcrumb = nullptr;
		if (!GetScene()->CreateMeshEntity(&breadcrumb, "HeldBreadcrumb", meshID, mat, false, false, true))
		{
			ErrMsg("Failed to initialize breadcrumb holder!");
			return false;
		}
		breadcrumb->SetParent(cam);
		breadcrumb->GetTransform()->SetPosition({ -0.3f, -0.325f, 0.5f });
		breadcrumb->Disable();

		breadcrumb->GetBehaviourByType<MeshBehaviour>(_heldBreadcrumb);
#ifdef DEBUG_BUILD
		breadcrumb->SetSerialization(false);
#endif

	}

	QueueUpdate();

	return true;
}

bool InventoryBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (BindingCollection::IsTriggered(InputBindings::InputAction::PlaceBreadcrumb))
	{
		if (_heldBreadcrumb->GetEntity()->IsEnabledSelf() && _breadcrumbCount > 0)
		{
			CameraBehaviour *playerView = GetScene()->GetPlayerCamera();

			XMFLOAT3A position = GetScene()->GetSceneHolder()->GetEntityByName("Player Entity")->GetTransform()->GetPosition(World);

			const BoundingOrientedBox bounds = { {0,0,0}, {0.2f, 0.2f, 0.2f}, {0,0,0,1} };

			Entity *crumb = nullptr;
			if (!GetScene()->CreateEntity(&crumb, "Breadcrumb", bounds, true))
			{
				ErrMsg("Failed to create object!");
				return false;
			}

			crumb->GetTransform()->SetPosition(position, World);
			crumb->GetTransform()->Move({0.0f, 0.05f, 0.0f}, World);

			BreadcrumbBehaviour *behaviour = new BreadcrumbBehaviour(_breadcrumbColor);
			if (!behaviour->Initialize(crumb))
			{
				ErrMsg("Failed to initialize breadcrumb behaviour!");
				return false;
			}

			_breadcrumbCount--;
			if (_breadcrumbCount <= 0)
				_heldBreadcrumb->GetEntity()->Disable();
		}
	}

	if (input.IsCursorLocked())
	{
		if (input.GetKey(KeyCode::R) == KeyState::Pressed)
		{
			if(_heldItem)
				_heldItem->GetEntity()->Disable();

			if (_handState == HandState::PicturePiece)
				_interactor->HidePicture();


			_handState = (HandState)(((int)_handState + 1) % (int)HandState::Count);

			switch (_handState)
			{
			case HandState::Empty:
				_heldItem = nullptr;
				break;

			case HandState::Breadcrumbs:
				_heldItem = _heldBreadcrumb;
				break;

			case HandState::Compass:
				_heldItem = _compass;
				break;

			case HandState::PicturePiece:
				GetEntity()->GetBehaviourByType<InteractorBehaviour>(_interactor);
				if (_interactor->GetCollectedPieces() == 0)
					_handState = (HandState)(((int)_handState + 1) % (int)HandState::Count);

				_interactor->ShowPicture();
				_heldItem = nullptr;
				break;

			case HandState::Count:
			default:
				break;
			}

			if (_heldItem)
				_heldItem->GetEntity()->Enable();
		}

		if (_handState == HandState::Breadcrumbs)
		{
			float scroll = input.GetMouse().scroll.y;
			if (scroll != 0.0f)
			{
				int scrollSign = scroll > 0.0f ? 1 : -1;
				SetBreadcrumbColor((BreadcrumbColor)(((int)_breadcrumbColor + (scrollSign + 3)) % 3));
			}
		}
	}

	return true;
}

bool InventoryBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Breadcrumbs", _breadcrumbCount, docAlloc);

	return true;
}
bool InventoryBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_breadcrumbCount = obj["Breadcrumbs"].GetUint();

	return true;
}

void InventoryBehaviour::SetBreadcrumbColor(BreadcrumbColor color)
{
	_breadcrumbColor = color;

	std::string crumbColorName = "";
	switch (color)
	{
	case BreadcrumbColor::Red:
		crumbColorName = "Red";
		break;

	case BreadcrumbColor::Green:
		crumbColorName = "Green";
		break;

	case BreadcrumbColor::Blue:
		crumbColorName = "Blue";
		break;
	}

	// Update held breadcrumb material
	Content *content = GetScene()->GetContent();

	Material mat = Material(*_heldBreadcrumb->GetMaterial());
	mat.textureID = content->GetTextureID(std::format("Breadcrumb_{}", crumbColorName));
	mat.specularID = content->GetTextureID(std::format("Breadcrumb_{}_Specular", crumbColorName));
	mat.ambientID = content->GetTextureID(std::format("Breadcrumb_{}_Light", crumbColorName));

	if (!_heldBreadcrumb->SetMaterial(&mat))
	{
		ErrMsg("Failed to set material to held breadcrumb!");
	}

	// Randomize the held breadcrumb rotation
	XMFLOAT3A rotation{0, 0, 0};
	rotation.y = (float)(rand() % 360) * DEG_TO_RAD;

	_heldBreadcrumb->GetEntity()->GetTransform()->SetEuler(rotation, Local);
}

bool InventoryBehaviour::SetInventoryItemParents(Entity* parent)
{
	if (parent == nullptr)
	{
		ErrMsg("Parent entity is null!");
		return false;
	}

	if (_heldBreadcrumb)
		_heldBreadcrumb->GetEntity()->SetParent(parent);

	if (_compass)
		_compass->GetEntity()->SetParent(parent);
	//Picture pieces

	return true;
}

void InventoryBehaviour::SetHeldItem(bool cycle, HandState handState)
{
	if (_heldItem)
		_heldItem->GetEntity()->Disable();

	if (_handState == HandState::PicturePiece)
		_interactor->HidePicture();

	if (cycle)
		_handState = (HandState)(((int)_handState + 1) % (int)HandState::Count);
	else
		_handState = handState;

	switch (_handState)
	{
	case HandState::Empty:
		_heldItem = nullptr;
		break;

	case HandState::Breadcrumbs:
		_heldItem = _heldBreadcrumb;
		break;

	case HandState::Compass:
		_heldItem = _compass;
		break;

	case HandState::PicturePiece:
		GetEntity()->GetBehaviourByType<InteractorBehaviour>(_interactor);
		_interactor->ShowPicture();
		_heldItem = nullptr;
		break;

	case HandState::Count:
	default:
		break;
	}

	if (_heldItem)
		_heldItem->GetEntity()->Enable();
}

void InventoryBehaviour::SetHeldEnabled(bool state)
{
	if (_handState == HandState::PicturePiece)
	{
		if (state)
			_interactor->ShowPicture();
		else
			_interactor->HidePicture();
	}

	else if (_heldItem)
	{
		if (state)
			_heldItem->GetEntity()->Enable();
		else
			_heldItem->GetEntity()->Disable();
	}
}
