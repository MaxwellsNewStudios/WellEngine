#pragma region Includes, Usings & Defines
#include "stdafx.h"
#include "Scenes/Scene.h"
#include "Game.h"
#include "GraphManager.h"
#include "Utils/SerializerUtils.h"
#include "BehaviourFactory.h"
#ifdef DEBUG_BUILD
#include "Behaviours/DebugPlayerBehaviour.h"
#endif
#include "Behaviours/GraphNodeBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion


bool Scene::Serialize(bool asSaveFile)
{
	ZoneScopedC(RandomUniqueColor());

	const std::string ext = asSaveFile ? ASSET_EXT_SAVE : ASSET_EXT_SCENE;
	const std::string path = asSaveFile ? ASSET_PATH_SAVES : ASSET_PATH_SCENES;
	const std::string fullPath = PATH_FILE_EXT(path, _sceneName, ext);

#ifdef EDIT_MODE
	if (!asSaveFile)
	{
		// Copy the previous file to a backup folder to prevent accidentally overwriting important work
		std::ifstream prevFile(fullPath);

		if (prevFile)
		{
			time_t t = std::time(nullptr);
			tm tm;
			localtime_s(&tm, &t);

			std::ostringstream oss;
			oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
			std::string timestamp = oss.str();
			
			std::ofstream backupFile(std::format("{}\\Backups\\{} ({}).{}", ASSET_PATH_SCENES, _sceneName, timestamp, ASSET_EXT_SCENE));
			backupFile << prevFile.rdbuf();
			prevFile.close();
			backupFile.close();
		}
	}
#endif

#ifndef EDIT_MODE
	// Temporarily reconvert baked graph nodes to entities
	auto &bakedNodes = _graphManager.GetBakedNodes();

	std::vector<GraphNodeBehaviour *> nodeBehaviours;
	nodeBehaviours.resize(bakedNodes.size());

	for (int i = 0; i < bakedNodes.size(); i++)
	{
		const Pathfinding::GraphNode &bakedNode = bakedNodes[i];

		dx::XMFLOAT4 nodePos = bakedNode.point;

		Entity *nodeEnt = nullptr;
		if (!CreateGraphNodeEntity(&nodeEnt, &(nodeBehaviours[i]), To3(nodePos)))
		{
			ErrMsg("Failed to create temporary graph node entity!");
			return false;
		}
	}

	for (int i = 0; i < bakedNodes.size(); i++)
	{
		const Pathfinding::GraphNode &bakedNode = bakedNodes[i];

		if (nodeBehaviours[i])
		{
			nodeBehaviours[i]->SetEnabled(true);
			nodeBehaviours[i]->SetCost(bakedNode.point.w);

			for (int j = 0; j < bakedNode.connections.size(); j++)
				nodeBehaviours[i]->AddConnection(nodeBehaviours[bakedNode.connections[j]]);
		}
	}
#endif

	{
		// Create JSON document
		json::Document doc;
		json::Document::AllocatorType &docAlloc = doc.GetAllocator();
		json::Value &sceneObj = doc.SetObject();

		json::Value sceneSettingsObj(json::kObjectType);
		{
			sceneSettingsObj.AddMember("Transitional", _transitionScene, docAlloc);

			json::Value sceneBoundsObj(json::kObjectType);
			{
				const dx::BoundingBox &sceneBounds = _sceneHolder.GetBounds();
				sceneBoundsObj.AddMember("Center", SerializerUtils::SerializeVec(sceneBounds.Center, docAlloc), docAlloc);
				sceneBoundsObj.AddMember("Extents", SerializerUtils::SerializeVec(sceneBounds.Extents, docAlloc), docAlloc);
			}
			sceneSettingsObj.AddMember("Bounds", sceneBoundsObj, docAlloc);

			json::Value sceneGraphicsObj(json::kObjectType);
			{
				sceneGraphicsObj.AddMember("Spotlight Resolution", _spotlights->GetShadowResolution(), docAlloc);
				sceneGraphicsObj.AddMember("Pointlight Resolution", _pointlights->GetShadowResolution(), docAlloc);

				UINT envCubemapID = _graphics->GetEnvironmentCubemapID();
				std::string envCubemapName = _content->GetCubemapName(envCubemapID);
				sceneGraphicsObj.AddMember("Cubemap", SerializerUtils::SerializeString(envCubemapName, docAlloc), docAlloc);

				UINT skyboxShaderID = _graphics->GetSkyboxShaderID();
				std::string skyboxShaderName = _content->GetShaderName(skyboxShaderID);
				sceneGraphicsObj.AddMember("Skybox", SerializerUtils::SerializeString(skyboxShaderName, docAlloc), docAlloc);

				sceneGraphicsObj.AddMember("Ambient", SerializerUtils::SerializeVec(_graphics->GetAmbientColor(), docAlloc), docAlloc);

				json::Value sceneFogObj(json::kObjectType);
				{
					FogSettingsBuffer fogSettings = _graphics->GetFogSettings();
					sceneFogObj.AddMember("Thickness", fogSettings.thickness, docAlloc);
					sceneFogObj.AddMember("Step Size", fogSettings.stepSize, docAlloc);
					sceneFogObj.AddMember("Min Steps", fogSettings.minSteps, docAlloc);
					sceneFogObj.AddMember("Max Steps", fogSettings.maxSteps, docAlloc);
				}
				sceneGraphicsObj.AddMember("Fog", sceneFogObj, docAlloc);

				json::Value sceneEmissionObj(json::kObjectType);
				{
					EmissionSettingsBuffer emissionSettings = _graphics->GetEmissionSettings();
					sceneEmissionObj.AddMember("Strength", emissionSettings.strength, docAlloc);
					sceneEmissionObj.AddMember("Exponent", emissionSettings.exponent, docAlloc);
					sceneEmissionObj.AddMember("Threshold", emissionSettings.threshold, docAlloc);
				}
				sceneGraphicsObj.AddMember("Emission", sceneEmissionObj, docAlloc);

			}
			sceneSettingsObj.AddMember("Graphics", sceneGraphicsObj, docAlloc);

			// TODO: Audio parameters
		}
		sceneObj.AddMember("Scene", sceneSettingsObj, docAlloc);

		json::Value entArr(json::kArrayType);

		SceneContents::SceneIterator entIter = _sceneHolder.GetEntities();
		while (Entity *entity = entIter.Step())
		{
			if (entity->GetParent())
				continue; // Skip non-root entities

			json::Value entObj(json::kObjectType);
			if (!SerializeEntity(docAlloc, entObj, entity))
			{
				ErrMsgF("Failed to serialize entity '{}'!", entity->GetName());
				return false;
			}

			// Add entity to the array if it isn't empty
			if (entObj.MemberCount() > 0)
				entArr.PushBack(entObj, docAlloc);
		}
		sceneObj.AddMember("Hierarchy", entArr, docAlloc);

		// Write doc to file
		std::ofstream file(fullPath, std::ios::out);
		if (!file)
		{
			ErrMsg("Could not save game!");
			return false;
		}

		json::StringBuffer buffer;
		json::PrettyWriter<json::StringBuffer> writer(buffer);
		doc.Accept(writer);

		file << buffer.GetString();
		file.close();
	}


	// TODO: Move to a separate function
	{
		// Serialize the timeline manager
		std::string code;

		std::ofstream file = std::ofstream(ASSET_FILE_SEQUENCES, std::ios::out);
		if (!file)
		{
			ErrMsg("Could not open sequences file.");
			return false;
		}

		if (!_timelineManager.Serialize(&code))
		{
			ErrMsg("Failed to serialize timeline manager!");
			return false;
		}

		file << code;
		file.close();
	}

#ifndef EDIT_MODE
	// Remove the temporary graph node entities
	for (int i = 0; i < nodeBehaviours.size(); i++)
	{
		if (!_sceneHolder.RemoveEntity(nodeBehaviours[i]->GetEntity()))
		{
			ErrMsg("Failed to remove temporary graph node entity!");
			return false;
		}
	}
#endif

	return true;
}
bool Scene::SerializeEntity(json::Document::AllocatorType &docAlloc, json::Value &obj, Entity *entity, bool forceSerialize)
{
	ZoneScopedXC(RandomUniqueColor());

	if (!entity->IsSerializable() && !forceSerialize)
		return true; // Skip non-serializable entities

	Entity *parentEntity = entity->GetParent();
	Transform *entTransform = entity->GetTransform();
	dx::XMFLOAT3A pos = entTransform->GetPosition();
	dx::XMFLOAT3A euler = entTransform->GetEuler();
	dx::XMFLOAT3A scale = entTransform->GetScale();

	json::Value nameStr(json::kStringType);
	nameStr.SetString(entity->GetName().c_str(), docAlloc);
	obj.AddMember("Name", nameStr, docAlloc);
	obj.AddMember("ID", entity->GetID(), docAlloc);
	obj.AddMember("Enabled", entity->IsEnabledSelf(), docAlloc);

	json::Value posArr(json::kArrayType);
	posArr.PushBack(pos.x, docAlloc);
	posArr.PushBack(pos.y, docAlloc);
	posArr.PushBack(pos.z, docAlloc);
	obj.AddMember("Pos", posArr, docAlloc);

	json::Value rotArr(json::kArrayType);
	rotArr.PushBack(euler.x, docAlloc);
	rotArr.PushBack(euler.y, docAlloc);
	rotArr.PushBack(euler.z, docAlloc);
	obj.AddMember("Rot", rotArr, docAlloc);

	json::Value scaleArr(json::kArrayType);
	scaleArr.PushBack(scale.x, docAlloc);
	scaleArr.PushBack(scale.y, docAlloc);
	scaleArr.PushBack(scale.z, docAlloc);
	obj.AddMember("Scale", scaleArr, docAlloc);

	if (entity->IsPrefab())
	{
		json::Value prefabStr(json::kStringType);
		prefabStr.SetString(entity->GetPrefabName().c_str(), docAlloc);
		obj.AddMember("Prefab", prefabStr, docAlloc);
	}
	else
	{
		obj.AddMember("Static", entity->IsStatic(), docAlloc);
		obj.AddMember("Select", entity->IsDebugSelectable(), docAlloc);
		obj.AddMember("InTree", _sceneHolder.IsEntityIncludedInTree(entity), docAlloc);

		json::Value behArr(json::kArrayType);
		UINT count = entity->GetBehaviourCount();

		for (int i = 0; i < count; i++)
		{
			Behaviour *beh = entity->GetBehaviour(i);
			json::Value behObj(json::kObjectType);

			if (!beh->InitialSerialize(docAlloc, behObj))
			{
				ErrMsgF("Failed to serialize behaviour '{}'!", beh->GetName());
				return false;
			}

			// Add behaviour to the array if it isn't empty
			if (behObj.MemberCount() > 0)
				behArr.PushBack(behObj, docAlloc);
		}
		obj.AddMember("Beh", behArr, docAlloc);


		json::Value childArr(json::kArrayType);
		const std::vector<Entity *> *children = entity->GetChildren();

		for (auto &child : *children)
		{
			if (!child)
				continue;

			// HACK: Does forceSerialize not being recursive cause problems?
			json::Value childObj(json::kObjectType);
			if (!SerializeEntity(docAlloc, childObj, child))
			{
				ErrMsgF("Failed to serialize child entity '{}'!", child->GetName());
				return false;
			}

			// Add entity to the array if it isn't empty
			if (childObj.MemberCount() > 0)
				childArr.PushBack(childObj, docAlloc);
		}

		// Add the child arrat unless it is empty
		if (!childArr.Empty())
			obj.AddMember("Child", childArr, docAlloc);
	}

	return true;
}

bool Scene::Deserialize(bool sceneReload)
{
	ZoneScopedC(RandomUniqueColor());

	json::Document doc;
	std::string dir, ext;

#ifdef EDIT_MODE
	dir = ASSET_PATH_SCENES;
	ext = ASSET_EXT_SCENE;
#else
	if (sceneReload)
	{
		dir = ASSET_PATH_SCENES;
		ext = ASSET_EXT_SCENE;
	}
	else
	{
		// Check if a save file exists. If so, use it.
		std::ifstream saveFile(PATH_FILE_EXT(ASSET_PATH_SAVES, _sceneName, ASSET_EXT_SAVE));

		if (saveFile.is_open())
		{
			dir = ASSET_PATH_SAVES;
			ext = ASSET_EXT_SAVE;
		}
		else
		{
			dir = ASSET_PATH_SCENES;
			ext = ASSET_EXT_SCENE;
		}

		saveFile.close();
	}
#endif

	const std::string fullPath = PATH_FILE_EXT(dir, _sceneName, ext);

	// Parse the JSON file to doc
	{
		std::ifstream file(fullPath);
		if (!file.is_open())
			return true;

		std::string fileContents;
		file.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		doc.Parse(fileContents.c_str());

		if (doc.HasParseError())
		{
			ErrMsgF("Failed to parse JSON file: {}", (UINT)doc.GetParseError());
			return false;
		}
	}

	if (doc.HasMember("Scene"))
	{
		json::Value &sceneSettingsObj = doc["Scene"];

		if (sceneSettingsObj.HasMember("Transitional"))
			_transitionScene = sceneSettingsObj["Transitional"].GetBool();

		dx::BoundingBox sceneBounds{};
		if (sceneSettingsObj.HasMember("Bounds"))
		{
			json::Value &sceneBoundsObj = sceneSettingsObj["Bounds"];
			if (sceneBoundsObj.HasMember("Center"))
				SerializerUtils::DeserializeVec(sceneBounds.Center, sceneBoundsObj["Center"]);

			if (sceneBoundsObj.HasMember("Extents"))
				SerializerUtils::DeserializeVec(sceneBounds.Extents, sceneBoundsObj["Extents"]);
		}

		if (!_sceneHolder.Initialize(sceneBounds))
		{
			ErrMsg("Failed to initialize scene holder!");
			return false;
		}

		if (sceneSettingsObj.HasMember("Graphics"))
		{
			json::Value &sceneGraphicsObj = sceneSettingsObj["Graphics"];

			UINT pointlightRes = 512;
			if (sceneGraphicsObj.HasMember("Pointlight Resolution"))
				pointlightRes = sceneGraphicsObj["Pointlight Resolution"].GetUint();

			if (!_pointlights->Initialize(_device, pointlightRes))
			{
				ErrMsg("Failed to initialize pointlight collection!");
				return false;
			}

			UINT spotlightRes = 256;
			if (sceneGraphicsObj.HasMember("Spotlight Resolution"))
				pointlightRes = sceneGraphicsObj["Spotlight Resolution"].GetUint();

			if (!_spotlights->Initialize(_device, spotlightRes))
			{
				ErrMsg("Failed to initialize spotlight collection!");
				return false;
			}

			if (sceneGraphicsObj.HasMember("Cubemap"))
				_envCubemapID = _content->GetCubemapID(sceneGraphicsObj["Cubemap"].GetString());

			if (sceneGraphicsObj.HasMember("Skybox"))
				_skyboxShaderID = _content->GetShaderID(sceneGraphicsObj["Skybox"].GetString());

			if (sceneGraphicsObj.HasMember("Ambient"))
				SerializerUtils::DeserializeVec(_ambientColor, sceneGraphicsObj["Ambient"]);

			if (sceneGraphicsObj.HasMember("Fog"))
			{
				json::Value &sceneFogObj = sceneGraphicsObj["Fog"];

				_fogSettings.thickness = sceneFogObj["Thickness"].GetFloat();
				_fogSettings.stepSize = sceneFogObj["Step Size"].GetFloat();
				_fogSettings.minSteps = sceneFogObj["Min Steps"].GetInt();
				_fogSettings.maxSteps = sceneFogObj["Max Steps"].GetInt();
			}

			if (sceneGraphicsObj.HasMember("Emission"))
			{
				json::Value &sceneEmissionObj = sceneGraphicsObj["Emission"];

				_emissionSettings.strength = sceneEmissionObj["Strength"].GetFloat();
				_emissionSettings.exponent = sceneEmissionObj["Exponent"].GetFloat();
				_emissionSettings.threshold = sceneEmissionObj["Threshold"].GetFloat();
			}
		}
	}

	json::Value &hierarchy = doc["Hierarchy"];
	if (!hierarchy.IsArray())
	{
		ErrMsg("Hierarchy is not an array!");
		return false;
	}

	const auto &hierarchyArray = hierarchy.GetArray();
	for (const auto &entObj : hierarchyArray)
	{
		if (!DeserializeEntity(entObj))
		{
			ErrMsg("Failed to deserialize entity!");
			return false;
		}
	}

	PostDeserialize();

	if (!_timelineManager.Deserialize())
	{
		ErrMsg("Failed to deserialize timeline manager!");
		return false;
	}

	return true;
}
bool Scene::DeserializeEntity(const json::Value &obj, Entity **out)
{
	ZoneScopedXC(RandomUniqueColor());

	if (obj.MemberCount() == 0)
	{
		ErrMsg("Empty code provided for deserialization!");
		return false;
	}

	if (!obj.IsObject())
	{
		ErrMsg("Code provided for deserialization is not an object!");
		return false;
	}

	std::string name = obj["Name"].GetString();
	UINT deserializedID = obj["ID"].GetUint();

	bool enabled = true;
	if (obj.HasMember("Enabled"))
		enabled = obj["Enabled"].GetBool();

	dx::XMFLOAT3 pos, rot, scale;
	SerializerUtils::DeserializeVec(pos, obj["Pos"]);
	SerializerUtils::DeserializeVec(rot, obj["Rot"]);
	SerializerUtils::DeserializeVec(scale, obj["Scale"]);

	Entity *ent = nullptr;
	if (obj.HasMember("Prefab"))
	{
		const std::string prefabName = obj["Prefab"].GetString();

		ent = SpawnPrefab(prefabName);
		if (ent)
		{
			auto entTrans = ent->GetTransform();
			entTrans->SetPosition(To3(pos));
			entTrans->SetEuler(To3(rot));
			entTrans->SetScale(To3(scale));

			ent->SetName(name);
			ent->SetDeserializedID(deserializedID); // HACK: Doesnt work if prefab children reference the prefab root
			ent->SetEnabledSelf(enabled);
		}
		else
			WarnF("Failed to deserialize prefab '{}'!", prefabName);
	}
	else
	{
		bool isStatic = obj["Static"].GetBool();
		bool isSelectable = obj["Select"].GetBool();
		bool hasVolume = obj["InTree"].GetBool();

		// Create the entity
		constexpr dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .1f,.1f,.1f }, { 0,0,0,1 });
		if (!CreateEntity(&ent, name, bounds, hasVolume))
		{
			ErrMsgF("Failed to create entity '{}'!", name);
			return false;
		}

		ent->SetDeserializedID(deserializedID);
		ent->SetEnabledSelf(enabled);
		ent->SetStatic(isStatic);
		ent->SetDebugSelectable(isSelectable);

		auto entTrans = ent->GetTransform();
		entTrans->SetPosition(To3(pos));
		entTrans->SetEuler(To3(rot));
		entTrans->SetScale(To3(scale));

		if (obj.HasMember("Beh"))
		{
			const auto &behArr = obj["Beh"].GetArray();
			for (const auto &behObj : behArr)
			{
				const std::string behName = behObj["Name"].GetString();
				const json::Value &behAttributes = behObj["Attributes"];

				Behaviour *beh = BehaviourFactory::CreateBehaviour(behName);
				if (!beh)
					continue;

				if (!beh->InitialDeserialize(behAttributes, this))
				{
					ErrMsg("Failed to deserialize behaviour!");
					return false;
				}

				if (!beh->Initialize(ent))
				{
					ErrMsg("Failed to bind behaviour to entity!");
					return false;
				}
			}
		}

		if (obj.HasMember("Child"))
		{
			// Recursively deserialize child entities
			const auto &behArr = obj["Child"].GetArray();
			for (const auto &childObj : behArr)
			{
				Entity *childEntity = nullptr;
				if (!DeserializeEntity(childObj, &childEntity))
				{
					ErrMsg("Failed to deserialize child entity!");
					return false;
				}

				if (childEntity)
				{
					// Set the parent of the child entity
					childEntity->SetParent(ent, false);
				}
			}
		}
	}

	if (out)
		*out = ent;

	return true;
}

void Scene::AddPostDeserializeCallback(Behaviour *beh)
{
	// Check if the callback already exists
	if (std::find(_postDeserializeCallbacks.begin(), _postDeserializeCallbacks.end(), beh) != _postDeserializeCallbacks.end())
		return;

	_postDeserializeCallbacks.emplace_back(beh);
}
void Scene::RunPostDeserializeCallbacks()
{
	std::vector<Ref<Entity>> deserializedEntities;
	deserializedEntities.reserve(_postDeserializeCallbacks.size());

	for (Behaviour *beh : _postDeserializeCallbacks)
	{
		if (!beh)
			continue;

		beh->InitialPostDeserialize();

		deserializedEntities.emplace_back(*beh->GetEntity());
	}

	_postDeserializeCallbacks.clear();

	// Reset all deserialized IDs
	for (Ref<Entity> &entRef : deserializedEntities)
	{
		Entity *ent;
		if (entRef.TryGet(ent))
			ent->SetDeserializedID(-1);
	}
}
void Scene::PostDeserialize()
{
	ZoneScopedXC(RandomUniqueColor());

	RunPostDeserializeCallbacks();

	_graphManager.CompleteDeserialization();
}

void Scene::GetPrefabNames(std::vector<std::string> &prefabs) const
{
	prefabs.clear();
	
	// Search for all .prefab files in PREFABS_PATH
	for (const auto &entry : std::filesystem::directory_iterator(ASSET_PATH_PREFABS))
	{
		const auto &path = entry.path();
		std::string filename = path.filename().string();
		std::string ext = filename.c_str() + filename.find_last_of('.') + 1;

		if (ext != ASSET_EXT_PREFAB)
			continue; // Skip non-prefab files

		filename = filename.substr(0, filename.find_last_of('.'));
		prefabs.emplace_back(filename);
	}
}
bool Scene::SaveAsPrefab(const std::string &name, Entity *entity)
{
	if (!entity)
	{
		WarnF("Failed to save prefab '{}' as no entity was given!", name);
		return false;
	}

	if (entity->GetPrefabName() == name)
	{
		// If overwriting a prefab with an instance of itself,
		// we must first unlink it to prevent an infinite recursion
		// when serializing the prefab.
		entity->UnlinkFromPrefab();
	}

	// Create JSON document
	json::Document doc;
	json::Document::AllocatorType &docAlloc = doc.GetAllocator();

	json::Value prefabObj(json::kObjectType);
	{
		prefabObj.AddMember("Name", SerializerUtils::SerializeString(name, docAlloc), docAlloc);

		json::Value entObj(json::kObjectType);
		if (!SerializeEntity(docAlloc, entObj, entity, true))
		{
			Warn("Failed to serialize root entity!");
			return false;
		}
		prefabObj.AddMember("Entity", entObj, docAlloc);
	}
	doc.SetObject().AddMember("Prefab", prefabObj, docAlloc);

	// Write doc to file
	std::ofstream file(PATH_FILE_EXT(ASSET_PATH_PREFABS, name, ASSET_EXT_PREFAB), std::ios::out);
	if (!file)
	{
		WarnF("Failed to save prefab '{}'!", name);
		return false;
	}

	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	doc.Accept(writer);

	file << buffer.GetString();
	file.close();

	entity->SetPrefabName(name);
	return true;
}
bool Scene::DeletePrefab(const std::string &name)
{
	const std::string fullPath = PATH_FILE_EXT(ASSET_PATH_PREFABS, name, ASSET_EXT_PREFAB);

	if (std::filesystem::exists(fullPath))
	{
		try
		{
			std::filesystem::remove(fullPath);
		}
		catch (const std::exception &e)
		{
			WarnF("Failed to delete prefab file '{}': {}", fullPath, e.what());
			return false;
		}
	}

	// Unlink all instances of this prefab in the scene
	/*SceneContents::SceneIterator entIter = _sceneHolder.GetEntities();
	while (Entity *entity = entIter.Step())
	{
		if (entity->GetPrefabName() == name)
			entity->UnlinkFromPrefab();
	}*/

	return true;
}
Entity *Scene::SpawnPrefab(const std::string &name)
{
	Entity *ent = nullptr;
	json::Document doc;

	// Parse the prefab file to doc if the file exists
	{
		const std::string fullPath = PATH_FILE_EXT(ASSET_PATH_PREFABS, name, ASSET_EXT_PREFAB);

		std::ifstream file(fullPath);
		if (!file.is_open())
			return nullptr;

		std::string fileContents;
		file.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		doc.Parse(fileContents.c_str());

		if (doc.HasParseError())
		{
			WarnF("Failed to parse JSON file: {}", (UINT)doc.GetParseError());
			return nullptr;
		}
	}

	// Deserialize the prefab file to ent
	json::Value &prefab = doc["Prefab"];
	if (!prefab.IsObject() || !prefab.HasMember("Entity"))
	{
		Warn("Prefab file does not contain a valid entity!");
		return nullptr;
	}

	json::Value &entObj = prefab["Entity"];
	if (!DeserializeEntity(entObj, &ent))
	{
		ErrMsg("Failed to deserialize prefab entity!");
		return nullptr;
	}

	if (ent)
		ent->SetPrefabName(name);

	RunPostDeserializeCallbacks();

	return ent;
}
