#include "stdafx.h"
#include "Behaviours/ColliderBehaviour.h"
#include "Scenes/Scene.h"
#include <format>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool ColliderBehaviour::Start()
{
	if (_name.empty())
		_name = "ColliderBehaviour"; // For categorization in ImGui.

	_isDirty = true;

	QueueUpdate();

    return true;
}

bool ColliderBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (!_baseCollider || !_transformedCollider)
		return true;

	if (_isDirty)
	{
		dx::XMFLOAT4X4A worldMatrix = GetTransform()->GetWorldMatrix();
		_baseCollider->Transform(worldMatrix, _transformedCollider);

#ifdef DEBUG_BUILD
		if (!_transformedCollider->TransformDebug(GetScene()->GetContext(), time, input))
			Warn("Failed to transform collider debug!");
#endif

		_isDirty = false;
	}

	return true;
}

bool ColliderBehaviour::Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo)
{
#ifdef DEBUG_BUILD
	if (_transformedCollider)
		if (!_transformedCollider->RenderDebug(GetScene(), queuer, rendererInfo))
			Warn("Failed to render collider!");
#endif
	
	return true;
}

#ifdef USE_IMGUI
bool ColliderBehaviour::RenderUI()
{
	if (ImGui::TreeNode("Collider Type"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Border;

		ImGui::BeginChild("Scene Hierarchy", ImVec2(0, 70), childFlags);

		const int nColliders = 6;
		const char *colliderNames[nColliders] = {
			"None", "Ray", "Sphere", "Capsule", "AABB", "OBB"
		};

		// TODO: Fix Ray Collider
		const char *preview = colliderNames[_selectedColliderType];

		bool applySelection = false;
		if (ImGui::BeginCombo("Colliders", preview))
		{
			for (int i = 0; i < nColliders; i++)
			{
				const bool isSelected = (_selectedColliderType == i);
				if (ImGui::Selectable(colliderNames[i], isSelected))
				{
					_selectedColliderType = i;
					applySelection = true;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (applySelection)
		{
			Collisions::ColliderTypes type = _selectedColliderType == 0 ? Collisions::NULL_COLLIDER : (Collisions::ColliderTypes)(_selectedColliderType - 1);

			Entity *entity = GetEntity();
			dx::BoundingOrientedBox entBounds;
			entity->StoreEntityBounds(entBounds, Local);

			switch (type)
			{
			case Collisions::RAY_COLLIDER:
			{
				auto col = new Collisions::Ray(entBounds.Center, { 0, 0, 1 }, 1, Collisions::MAP_COLLIDER_TAGS);
				SetCollider(col);
				return true;
			}

			case Collisions::SPHERE_COLLIDER:
			{
				auto col = new Collisions::Sphere(entBounds.Center, std::min<float>(std::min<float>(entBounds.Extents.x, entBounds.Extents.z), entBounds.Extents.y), Collisions::MAP_COLLIDER_TAGS);
				SetCollider(col);
				break;
			}

			case Collisions::CAPSULE_COLLIDER:
			{
				auto col = new Collisions::Capsule(entBounds.Center, { 0, 1, 0 }, std::min<float>(entBounds.Extents.x, entBounds.Extents.z), entBounds.Extents.y, Collisions::MAP_COLLIDER_TAGS);
				SetCollider(col);
				break;
			}

			case Collisions::AABB_COLLIDER:
			{
				auto col = new Collisions::AABB(entBounds, Collisions::MAP_COLLIDER_TAGS);
				SetCollider(col);
				break;
			}

			case Collisions::OBB_COLLIDER:
			{
				auto col = new Collisions::OBB(entBounds, Collisions::MAP_COLLIDER_TAGS);
				SetCollider(col);
				break;
			}

			case Collisions::TERRAIN_COLLIDER:
			{
				break;
			}

			case Collisions::NULL_COLLIDER:
			default:
				SetCollider(nullptr);
				break;
			}

			GetScene()->GetSceneHolder()->SetRecalculateColliders();
		}

		ImGui::Dummy(ImVec2(0, 8));

		ImGui::EndChild();

		ImGui::TreePop();
	}

	if (!_baseCollider || !_transformedCollider)
		return true; // Collider type is not set, skip type-specific UI.

	if (ImGui::Checkbox("Render Collider Wireframe", &_transformedCollider->debug))
	{
		if (_transformedCollider->debug && !_transformedCollider->haveDebugEnt)
		{
			if (!_transformedCollider->SetDebugEnt(GetScene(), GetScene()->GetContent()))
				Warn("Failed to create collider debug entity!");
		}

		_isDirty = true;
		_toCheck = true;

		if (_transformedCollider->debug)
		{
			if (!GetScene()->GetSceneHolder()->IncludeEntityInTree(GetEntity()))
			{
				ErrMsg("Failed to include collider in scene tree!");
				return false;
			}
		}
		else
		{
			if (!GetScene()->GetSceneHolder()->ExcludeEntityFromTree(GetEntity()))
			{
				ErrMsg("Failed to exclude collider from scene tree!");
				return false;
			}
		}
	}

	bool status = _transformedCollider->GetActive();
	if (ImGui::Checkbox("Collider Actie", &status))
		_transformedCollider->SetActive(status);

	if (!_baseCollider->RenderUI())
		Warn("Failed to render collider UI!");

	if (_baseCollider->GetDirty())
	{
		_isDirty = true;
		_toCheck = true;
	}
	return true;
}
#endif

void ColliderBehaviour::OnDirty()
{
	_isDirty = true;
	_toCheck = true;
}

ColliderBehaviour::~ColliderBehaviour()
{
	delete _baseCollider;
	delete _transformedCollider;
}

void ColliderBehaviour::SetCollider(Collisions::Collider *collider)
{
	delete _baseCollider;
	_baseCollider = nullptr;

	delete _transformedCollider;
	_transformedCollider = nullptr;

	if (!collider)
		return;

	_baseCollider = collider;

	switch (_baseCollider->colliderType)
	{
	case Collisions::RAY_COLLIDER:
		_transformedCollider = new Collisions::Ray(*static_cast<Collisions::Ray *>(collider));
		break;
	case Collisions::SPHERE_COLLIDER:
		_transformedCollider = new Collisions::Sphere(*static_cast<Collisions::Sphere *>(collider));
		break;
	case Collisions::CAPSULE_COLLIDER:
		_transformedCollider = new Collisions::Capsule(*static_cast<Collisions::Capsule *>(collider));
		break;
	case Collisions::AABB_COLLIDER:
		_transformedCollider = new Collisions::AABB(*static_cast<Collisions::AABB *>(collider));
		break;
	case Collisions::OBB_COLLIDER:
		_transformedCollider = new Collisions::OBB(*static_cast<Collisions::OBB *>(collider));
		break;
	case Collisions::TERRAIN_COLLIDER:
		_transformedCollider = new Collisions::Terrain(*static_cast<Collisions::Terrain *>(collider));
		break;
	default:
		break;
	}
	_isDirty = true;
	_toCheck = true;
}

const Collisions::Collider *ColliderBehaviour::GetCollider() const
{
    return _transformedCollider;
}

void ColliderBehaviour::AddOnIntersection(std::function<void(const Collisions::CollisionData &)> callback)
{
	if (_transformedCollider)
		_transformedCollider->AddIntersectionCallback(callback);
}

void ColliderBehaviour::AddOnCollisionEnter(std::function<void(const Collisions::CollisionData &)> callback)
{
	if (_transformedCollider)
		_transformedCollider->AddOnCollisionEnterCallback(callback);
}

void ColliderBehaviour::AddOnCollisionExit(std::function<void(const Collisions::CollisionData &)> callback)
{
	if (_transformedCollider)
		_transformedCollider->AddOnCollisionExitCallback(callback);
}

#ifdef DEBUG_BUILD
bool ColliderBehaviour::SetDebugCollider(Scene *scene, const Content *content)
{
	if (!_transformedCollider->SetDebugEnt(scene, content))
		Warn("Failed to create collider debug entity!");

	return true;
}
#endif

void ColliderBehaviour::SetIntersecting(bool value)
{
	_isIntersecting = value;
}

bool ColliderBehaviour::GetIntersecting()
{
	return _isIntersecting;
}

void ColliderBehaviour::CheckDone()
{
	_toCheck = false;
}

bool ColliderBehaviour::GetToCheck()
{
	return IsEnabled() && _toCheck && _transformedCollider->GetActive();
}

bool ColliderBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	using namespace Collisions;

	ColliderTypes type = _baseCollider->colliderType;
	obj.AddMember("Type", (int)type, docAlloc);

	if (type == ColliderTypes::RAY_COLLIDER)
	{
		{
			Ray *base = dynamic_cast<Ray *>(_baseCollider);

			obj.AddMember("Tag", base->GetTag(), docAlloc);

			json::Value baseObj(json::kObjectType);

			json::Value originArr(json::kArrayType);
			originArr.PushBack(base->origin.x, docAlloc);
			originArr.PushBack(base->origin.y, docAlloc);
			originArr.PushBack(base->origin.z, docAlloc);
			baseObj.AddMember("Origin", originArr, docAlloc);

			json::Value dirArr(json::kArrayType);
			dirArr.PushBack(base->dir.x, docAlloc);
			dirArr.PushBack(base->dir.y, docAlloc);
			dirArr.PushBack(base->dir.z, docAlloc);
			baseObj.AddMember("Direction", dirArr, docAlloc); // TODO: Shorten name

			baseObj.AddMember("Length", base->length, docAlloc);

			obj.AddMember("Base", baseObj, docAlloc);
		}

		{
			Ray *transformed = dynamic_cast<Ray *>(_transformedCollider);

			json::Value transObj(json::kObjectType);

			json::Value originArr(json::kArrayType);
			originArr.PushBack(transformed->origin.x, docAlloc);
			originArr.PushBack(transformed->origin.y, docAlloc);
			originArr.PushBack(transformed->origin.z, docAlloc);
			transObj.AddMember("Origin", originArr, docAlloc);

			json::Value dirArr(json::kArrayType);
			dirArr.PushBack(transformed->dir.x, docAlloc);
			dirArr.PushBack(transformed->dir.y, docAlloc);
			dirArr.PushBack(transformed->dir.z, docAlloc);
			transObj.AddMember("Direction", dirArr, docAlloc);

			transObj.AddMember("Length", transformed->length, docAlloc);

			obj.AddMember("Transformed", transObj, docAlloc);
		}
	}
	else if (type == ColliderTypes::SPHERE_COLLIDER)
	{
		{
			Sphere *base = dynamic_cast<Sphere *>(_baseCollider);

			obj.AddMember("Tag", base->GetTag(), docAlloc);

			json::Value baseObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(base->center.x, docAlloc);
			centerArr.PushBack(base->center.y, docAlloc);
			centerArr.PushBack(base->center.z, docAlloc);
			baseObj.AddMember("Center", centerArr, docAlloc);

			baseObj.AddMember("Radius", base->radius, docAlloc);

			obj.AddMember("Base", baseObj, docAlloc);
		}

		{
			Sphere *transformed = dynamic_cast<Sphere *>(_transformedCollider);

			json::Value transObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(transformed->center.x, docAlloc);
			centerArr.PushBack(transformed->center.y, docAlloc);
			centerArr.PushBack(transformed->center.z, docAlloc);
			transObj.AddMember("Center", centerArr, docAlloc);

			transObj.AddMember("Radius", transformed->radius, docAlloc);

			obj.AddMember("Transformed", transObj, docAlloc);
		}
	}
	else if (type == ColliderTypes::CAPSULE_COLLIDER)
	{
		{
			Capsule *base = dynamic_cast<Capsule *>(_baseCollider);

			obj.AddMember("Tag", base->GetTag(), docAlloc);

			json::Value baseObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(base->center.x, docAlloc);
			centerArr.PushBack(base->center.y, docAlloc);
			centerArr.PushBack(base->center.z, docAlloc);
			baseObj.AddMember("Center", centerArr, docAlloc);

			json::Value upArr(json::kArrayType);
			upArr.PushBack(base->upDir.x, docAlloc);
			upArr.PushBack(base->upDir.y, docAlloc);
			upArr.PushBack(base->upDir.z, docAlloc);
			baseObj.AddMember("Up", upArr, docAlloc);

			baseObj.AddMember("Radius", base->radius, docAlloc);
			baseObj.AddMember("Height", base->height, docAlloc);

			obj.AddMember("Base", baseObj, docAlloc);
		}

		{
			Capsule *transformed = dynamic_cast<Capsule *>(_transformedCollider);

			json::Value transObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(transformed->center.x, docAlloc);
			centerArr.PushBack(transformed->center.y, docAlloc);
			centerArr.PushBack(transformed->center.z, docAlloc);
			transObj.AddMember("Center", centerArr, docAlloc);

			json::Value upArr(json::kArrayType);
			upArr.PushBack(transformed->upDir.x, docAlloc);
			upArr.PushBack(transformed->upDir.y, docAlloc);
			upArr.PushBack(transformed->upDir.z, docAlloc);
			transObj.AddMember("Up", upArr, docAlloc);

			transObj.AddMember("Radius", transformed->radius, docAlloc);
			transObj.AddMember("Height", transformed->height, docAlloc);

			obj.AddMember("Transformed", transObj, docAlloc);
		}
	}
	else if (type == ColliderTypes::AABB_COLLIDER)
	{
		{
			AABB *base = dynamic_cast<AABB *>(_baseCollider);

			obj.AddMember("Tag", base->GetTag(), docAlloc);

			json::Value baseObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(base->center.x, docAlloc);
			centerArr.PushBack(base->center.y, docAlloc);
			centerArr.PushBack(base->center.z, docAlloc);
			baseObj.AddMember("Center", centerArr, docAlloc);

			json::Value halfLengthArr(json::kArrayType);
			halfLengthArr.PushBack(base->halfLength.x, docAlloc);
			halfLengthArr.PushBack(base->halfLength.y, docAlloc);
			halfLengthArr.PushBack(base->halfLength.z, docAlloc);
			baseObj.AddMember("Half-Length", halfLengthArr, docAlloc);

			obj.AddMember("Base", baseObj, docAlloc);
		}

		{
			AABB *transformed = dynamic_cast<AABB *>(_transformedCollider);

			json::Value transObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(transformed->center.x, docAlloc);
			centerArr.PushBack(transformed->center.y, docAlloc);
			centerArr.PushBack(transformed->center.z, docAlloc);
			transObj.AddMember("Center", centerArr, docAlloc);

			json::Value halfLengthArr(json::kArrayType);
			halfLengthArr.PushBack(transformed->halfLength.x, docAlloc);
			halfLengthArr.PushBack(transformed->halfLength.y, docAlloc);
			halfLengthArr.PushBack(transformed->halfLength.z, docAlloc);
			transObj.AddMember("Half-Length", halfLengthArr, docAlloc);

			obj.AddMember("Transformed", transObj, docAlloc);
		}
	}
	else if (type == ColliderTypes::OBB_COLLIDER)
	{
		{
			OBB *base = dynamic_cast<OBB *>(_baseCollider);
			obj.AddMember("Tag", base->GetTag(), docAlloc);

			json::Value baseObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(base->center.x, docAlloc);
			centerArr.PushBack(base->center.y, docAlloc);
			centerArr.PushBack(base->center.z, docAlloc);
			baseObj.AddMember("Center", centerArr, docAlloc);

			json::Value halfLengthArr(json::kArrayType);
			halfLengthArr.PushBack(base->halfLength.x, docAlloc);
			halfLengthArr.PushBack(base->halfLength.y, docAlloc);
			halfLengthArr.PushBack(base->halfLength.z, docAlloc);
			baseObj.AddMember("Half-Length", halfLengthArr, docAlloc);

			json::Value axesArr(json::kArrayType);
			for (const auto &axis : base->axes)
			{
				json::Value axisArr(json::kArrayType);
				axisArr.PushBack(axis.x, docAlloc);
				axisArr.PushBack(axis.y, docAlloc);
				axisArr.PushBack(axis.z, docAlloc);
				axesArr.PushBack(axisArr, docAlloc);
			}
			baseObj.AddMember("Axes", axesArr, docAlloc);

			obj.AddMember("Base", baseObj, docAlloc);
		}

		{
			OBB *transformed = dynamic_cast<OBB *>(_transformedCollider);

			json::Value transObj(json::kObjectType);

			json::Value centerArr(json::kArrayType);
			centerArr.PushBack(transformed->center.x, docAlloc);
			centerArr.PushBack(transformed->center.y, docAlloc);
			centerArr.PushBack(transformed->center.z, docAlloc);
			transObj.AddMember("Center", centerArr, docAlloc);

			json::Value halfLengthArr(json::kArrayType);
			halfLengthArr.PushBack(transformed->halfLength.x, docAlloc);
			halfLengthArr.PushBack(transformed->halfLength.y, docAlloc);
			halfLengthArr.PushBack(transformed->halfLength.z, docAlloc);
			transObj.AddMember("Half-Length", halfLengthArr, docAlloc);

			json::Value axesArr(json::kArrayType);
			for (const auto &axis : transformed->axes)
			{
				json::Value axisArr(json::kArrayType);
				axisArr.PushBack(axis.x, docAlloc);
				axisArr.PushBack(axis.y, docAlloc);
				axisArr.PushBack(axis.z, docAlloc);
				axesArr.PushBack(axisArr, docAlloc);
			}
			transObj.AddMember("Axes", axesArr, docAlloc);

			obj.AddMember("Transformed", transObj, docAlloc);
		}
	}
	else if (type == ColliderTypes::TERRAIN_COLLIDER)
	{
		{
			Terrain *base = dynamic_cast<Terrain *>(_baseCollider);

			obj.AddMember("Tag", base->GetTag(), docAlloc);

			json::Value baseObj(json::kObjectType);
			base->Serialize(docAlloc, baseObj);
			obj.AddMember("Base", baseObj, docAlloc);
		}

		{
			Terrain *transformed = dynamic_cast<Terrain *>(_transformedCollider);

			json::Value transObj(json::kObjectType);
			transformed->Serialize(docAlloc, transObj);
			obj.AddMember("Transformed", transObj, docAlloc);
		}
	}

	return true;
}

bool ColliderBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	using namespace Collisions;

	ColliderTypes type = (ColliderTypes)obj["Type"].GetInt();
	ColliderTags tag = (ColliderTags)obj["Tag"].GetUint();

	if (type == ColliderTypes::RAY_COLLIDER)
	{
		dx::XMFLOAT3 origin, dir;
		float length;

		const json::Value &baseObj = obj["Base"];
		SerializerUtils::DeserializeVec(origin, baseObj["Origin"]);
		SerializerUtils::DeserializeVec(dir, baseObj["Direction"]);
		length = baseObj["Length"].GetFloat();
		_baseCollider = new Ray(origin, dir, length, tag);

		const json::Value &transObj = obj["Transformed"];
		SerializerUtils::DeserializeVec(origin, transObj["Origin"]);
		SerializerUtils::DeserializeVec(dir, transObj["Direction"]);
		length = transObj["Length"].GetFloat();
		_transformedCollider = new Ray(origin, dir, length, tag);
	}
	else if (type == ColliderTypes::SPHERE_COLLIDER)
	{
		dx::XMFLOAT3 center;
		float radius;

		const json::Value &baseObj = obj["Base"];
		SerializerUtils::DeserializeVec(center, baseObj["Center"]);
		radius = baseObj["Radius"].GetFloat();
		_baseCollider = new Sphere(center, radius, tag);

		const json::Value &transObj = obj["Transformed"];
		SerializerUtils::DeserializeVec(center, transObj["Center"]);
		radius = transObj["Radius"].GetFloat();
		_transformedCollider = new Sphere(center, radius, tag);
	}
	else if (type == ColliderTypes::CAPSULE_COLLIDER)
	{
		dx::XMFLOAT3 center, upDir;
		float radius, height;

		const json::Value &baseObj = obj["Base"];
		SerializerUtils::DeserializeVec(center, baseObj["Center"]);
		SerializerUtils::DeserializeVec(upDir, baseObj["Up"]);
		radius = baseObj["Radius"].GetFloat();
		height = baseObj["Height"].GetFloat();
		_baseCollider = new Capsule(center, upDir, radius, height, tag);

		const json::Value &transObj = obj["Transformed"];
		SerializerUtils::DeserializeVec(center, transObj["Center"]);
		SerializerUtils::DeserializeVec(upDir, transObj["Up"]);
		radius = transObj["Radius"].GetFloat();
		height = transObj["Height"].GetFloat();
		_transformedCollider = new Capsule(center, upDir, radius, height, tag);
	}
	else if (type == ColliderTypes::AABB_COLLIDER)
	{
		dx::XMFLOAT3 center, halfLength;

		const json::Value &baseObj = obj["Base"];
		SerializerUtils::DeserializeVec(center, baseObj["Center"]);
		SerializerUtils::DeserializeVec(halfLength, baseObj["Half-Length"]);
		_baseCollider = new AABB(center, halfLength, tag);

		const json::Value &transObj = obj["Transformed"];
		SerializerUtils::DeserializeVec(center, transObj["Center"]);
		SerializerUtils::DeserializeVec(halfLength, transObj["Half-Length"]);
		_transformedCollider = new AABB(center, halfLength, tag);
	}
	else if (type == ColliderTypes::OBB_COLLIDER)
	{
		dx::XMFLOAT3 center, halfLength;
		dx::XMFLOAT3 axes[3]{};

		const json::Value &baseObj = obj["Base"];
		const json::Value &baseAxesArr = baseObj["Axes"];
		SerializerUtils::DeserializeVec(center, baseObj["Center"]);
		SerializerUtils::DeserializeVec(halfLength, baseObj["Half-Length"]);
		for (int i = 0; i < 3; i++)
			SerializerUtils::DeserializeVec(axes[i], baseAxesArr[i]);
		_baseCollider = new OBB(center, halfLength, axes, tag);

		const json::Value &transObj = obj["Transformed"];
		const json::Value &transAxesArr = transObj["Axes"];
		SerializerUtils::DeserializeVec(center, transObj["Center"]);
		SerializerUtils::DeserializeVec(halfLength, transObj["Half-Length"]);
		for (int i = 0; i < 3; i++)
			SerializerUtils::DeserializeVec(axes[i], transAxesArr[i]);
		_transformedCollider = new OBB(center, halfLength, axes, tag);
	}
	else if (type == ColliderTypes::TERRAIN_COLLIDER)
	{
		Content *content = scene->GetContent();

		dx::XMFLOAT3 center, halfLength;
		dx::XMINT2 tileSize;
		HeightMap *heightMap;

		const json::Value &baseObj = obj["Base"];
		SerializerUtils::DeserializeVec(center, baseObj["Center"]);
		SerializerUtils::DeserializeVec(halfLength, baseObj["Half-Length"]);
		SerializerUtils::DeserializeVec(tileSize, baseObj["Tile Size"]);
		heightMap = content->GetHeightMap(baseObj["Map"].GetString());
		_baseCollider = new Terrain(center, halfLength, heightMap, tag);

		const json::Value &transObj = obj["Transformed"];
		SerializerUtils::DeserializeVec(center, transObj["Center"]);
		SerializerUtils::DeserializeVec(halfLength, transObj["Half-Length"]);
		SerializerUtils::DeserializeVec(tileSize, transObj["Tile Size"]);
		heightMap = content->GetHeightMap(transObj["Map"].GetString());
		_transformedCollider = new Terrain(center, halfLength, heightMap, tag);
	}

	return true;
}
