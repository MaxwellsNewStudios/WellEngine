#pragma region Includes, Usings & Defines
#include "stdafx.h"
#include "Scene.h"
#include "Game.h"
#include "Debug/DebugData.h"
#include "GraphManager.h"
#include "Behaviours/BreadcrumbPileBehaviour.h"
#include "Behaviours/MonsterHintBehaviour.h"
#include "Behaviours/DebugPlayerBehaviour.h"
#include "Behaviours/GraphNodeBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion

#ifdef USE_IMGUI
bool Scene::IsUndocked() const
{
	return ImGuiUtils::Utils::GetWindow(_sceneName, nullptr);
}

bool Scene::RenderEntityCreatorUI()
{
	Entity *ent = nullptr;
	static bool positionWithCursor = false;
	ImGui::Checkbox("Position Entity With Cursor", &positionWithCursor);

	ImVec2 buttonSize = { ImGui::GetContentRegionAvail().x, 35.0f };

	// Empty entity creator
	{
		if (ImGui::Button("Empty Entity", buttonSize))
			ImGui::OpenPopup("Empty Entity Creator");

		if (ImGui::BeginPopupModal("Empty Entity Creator", NULL))
		{
			static std::string entityName = "Empty Entity";

			// Set Entity Name
			ImGui::Text("Entity Name:"); ImGui::SameLine();
			ImGui::InputText("##EntityName", &entityName, ImGuiInputTextFlags_AutoSelectAll);

			if (ImGui::Button("Create", ImVec2(120, 0)))
			{
				// Create entity with given parameters.

				const dx::BoundingOrientedBox bounds = { {0,0,0}, {0.25f, 0.25f, 0.25f}, {0,0,0,1} };

				if (!CreateEntity(&ent, entityName, bounds, true))
					Warn("Failed to create empty entity!");

				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus(); 
			
			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(120, 0)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	// Mesh entity creator
	{
		if (ImGui::Button("Mesh Entity", buttonSize))
			ImGui::OpenPopup("Mesh Entity Creator");

		if (ImGui::BeginPopupModal("Mesh Entity Creator", NULL))
		{
			static std::string entityName = "";

			// Set Entity Name
			ImGui::BeginGroup();
			ImGui::Text("Entity Name:"); ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::InputText("##EntityName", &entityName, ImGuiInputTextFlags_AutoSelectAll);
			ImGui::EndGroup();
			ImGui::SetItemTooltip("Leave empty to use mesh name");

			static UINT meshID = 0;

			// Select Mesh
			{
				std::vector<std::string> meshNames;
				_content->GetMeshNames(&meshNames);

				ImGuiComboFlags comboFlags = ImGuiComboFlags_None;
				comboFlags |= ImGuiComboFlags_HeightLarge;

				ImGui::Text("Mesh:"); ImGui::SameLine();
				if (ImGui::BeginCombo("##MeshCombo", _content->GetMeshName(meshID).c_str(), comboFlags))
				{
					static std::string filter = "";
					ImGui::InputText("##MeshFilter", &filter, ImGuiInputTextFlags_AutoSelectAll);
					if (!ImGui::IsItemActive() && filter.empty())
					{
						ImGui::SameLine(8.0f);
						ImGui::TextDisabled("Search");
					}

					if (!filter.empty())
						std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

					ImGui::Separator();

					for (int i = 0; i < meshNames.size(); i++)
					{
						if (!filter.empty())
						{
							std::string meshLower = meshNames[i];
							std::transform(meshLower.begin(), meshLower.end(), meshLower.begin(), ::tolower);

							if (meshLower.find(filter) == std::string::npos)
								continue;
						}

						const bool isSelected = meshID == _content->GetMeshID(meshNames[i]);
						if (ImGui::Selectable(meshNames[i].c_str(), isSelected))
							meshID = _content->GetMeshID(meshNames[i]);

						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			if (ImGui::Button("Create", ImVec2(120, 0)))
			{
				Material mat{};
				mat.textureID = _content->GetTextureID("Fallback");

				// Create entity with given parameters.
				if (!CreateMeshEntity(&ent, entityName.empty() ? _content->GetMeshName(meshID) : entityName, meshID, mat))
					Warn("Failed to create mesh entity!");

				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus(); 

			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(120, 0)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	// Prefab creator
	{
		if (ImGui::Button("Create Prefab", buttonSize))
			ImGui::OpenPopup("Prefab Spawner");

		if (ImGui::BeginPopupModal("Prefab Spawner", NULL))
		{
			std::vector<std::string> prefabs;
			GetPrefabNames(prefabs);

			static std::string selectedPrefab = "";

			ImGui::Text("Selected: '%s'", selectedPrefab.c_str());
			ImGui::Separator();

			// Search filter
			{
				static std::string search = "";
				if (ImGui::Button("Clear"))
					search.clear();
				ImGui::SameLine();

				if (ImGui::InputText("##PrefabSearch", &search))
					std::transform(search.begin(), search.end(), search.begin(), ::tolower);

				for (int i = 0; i < prefabs.size(); i++)
				{
					std::string prefabLower = prefabs[i];
					std::transform(prefabLower.begin(), prefabLower.end(), prefabLower.begin(), ::tolower);

					if (prefabLower.find(search) == std::string::npos)
					{
						prefabs.erase(prefabs.begin() + i);
						i--;
					}
				}
			}

			ImGuiChildFlags prefabChildFlags = ImGuiChildFlags_None;
			prefabChildFlags |= ImGuiChildFlags_Borders;

			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
			windowFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;

			static float prefabListHeight = 300.0f;

			if (ImGui::BeginChild("Prefab List", ImVec2(ImGui::GetContentRegionAvail().x, prefabListHeight), prefabChildFlags, windowFlags))
			{
				for (int i = 0; i < prefabs.size(); i++)
				{
					std::string &prefab = prefabs[i];

					if (ImGui::Selectable(prefab.c_str(), selectedPrefab == prefab))
						selectedPrefab = std::move(prefab);
				}
			}
			ImGui::EndChild();
			prefabListHeight = ImGui::GetItemRectSize().y;

			ImGui::Separator();

			if (ImGui::Button("Spawn") && !selectedPrefab.empty())
			{
				ent = SpawnPrefab(selectedPrefab);

				if (!ent)
					WarnF("Failed to spawn prefab '{}'", selectedPrefab);

				ImGui::CloseCurrentPopup();
			}

			static float cancelButtonWidth = 30.0f;
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - cancelButtonWidth);

			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			cancelButtonWidth = ImGui::GetItemRectSize().x;

			ImGui::EndPopup();
		}
	}

	// Takes a file during runtime and creates a new mesh and entity from it.
	// HACK: Temporarily disabled 
	/*
	if (ImGui::Button("Mesh from file", buttonSize))
	{
		const char *filterPatterns[] = { "*.obj", "*.png", "*.dds" };
		const char *selectedFiles = tinyfd_openFileDialog(
			"Select Mesh & Textures",
			"",
			3,
			filterPatterns,
			"Supported Files",
			1
		);

		if (selectedFiles)
		{
			std::string fileString = selectedFiles;

			std::vector<std::string> filePaths;
			std::stringstream ss(fileString);
			std::string filePath;
			while (std::getline(ss, filePath, '|'))
			{
				filePaths.emplace_back(filePath);
			}

			std::string meshFolder = std::filesystem::current_path().string() + "\\" + MESHES_PATH;
			std::string textureFolder = std::filesystem::current_path().string() + "\\" + TEXTURES_PATH;

			std::filesystem::create_directories(meshFolder);
			std::filesystem::create_directories(textureFolder);

			UINT meshID = CONTENT_NULL;
			Material mat;
			mat.textureID = _content->GetTextureID("White");
			mat.ambientID = _content->GetTextureID("Ambient");

			std::string meshName;

			for (const auto &sourcePath : filePaths)
			{
				std::string fileName = std::filesystem::path(sourcePath).filename().string();

				size_t dot = fileName.find('.');
				std::string name = fileName.substr(0, dot);
				std::string ext = fileName.substr(dot + 1);
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				if (ext == "obj")
				{
					UINT id = _content->GetMeshID(name);
					if (id == _content->GetMeshID("Error"))
					{
						std::ofstream file(PATH_FILE(MESHES_PATH, MESHES_FILE), std::ios::app);
						std::string destPath = meshFolder + '\\' + fileName;
						std::filesystem::copy_file(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing);

						file << '\n' + name;
						meshName = name;
						if (!_content->AddMesh(_device, std::format("{}", name), PATH_FILE_EXT(MESHES_PATH, name, "obj").c_str()))
						{
							ErrMsgF("Failed to add Mesh {}", name);
							return false;
						}
						meshID = _content->GetMeshID("" + name);
						file.close();
					}
					else
					{
						meshID = id;
						meshName = std::move(name);
					}
				}

				else if (ext == "png" || ext == "dds")
				{
					std::string destPath = textureFolder + "\\" + fileName;

					std::filesystem::copy_file(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing);

					UINT *destID = nullptr;

					if (name.ends_with("_Ambient"))
					{
						destID = &mat.ambientID;
					}
					else if (name.ends_with("_Normal"))
					{
						destID = &mat.normalID;
					}
					else if (name.ends_with("_Specular"))
					{
						destID = &mat.specularID;
					}
					else if (name.ends_with("_Glossiness"))
					{
						destID = &mat.glossinessID;
					}
					else if (name.ends_with("_Occlusion"))
					{
						destID = &mat.occlusionID;
					}
					else
					{
						destID = &mat.textureID;
					}

					if (!_content->HasTexture(name))
					{
						if (_content->AddTexture(
							_device, _context, name,
							PATH_FILE_EXT(TEXTURES_PATH, name, ext).c_str(),
							DXGI_FORMAT_R8G8B8A8_UNORM, false, true) == CONTENT_NULL)
						{
							ErrMsgF("Failed to add Tex {}!", name);
							return false;
						}

						std::ofstream file(PATH_FILE(TEXTURES_PATH, TEXTURES_FILE), std::ios::app);
						file << "\n" + name;
					}

					*destID = _content->GetTextureID(name);
				}
			}

			if (!CreateMeshEntity(&ent, meshName, meshID, mat, false, true, false))
			{
				ErrMsg("Failed to create object!");
				return false;
			}
		}
	}
	*/

	/*
	// Sound Creator
	{
		if (ImGui::Button("Sound", ImVec2(200, 50)))
			ImGui::OpenPopup("Sound Creator");

		if (ImGui::BeginPopupModal("Sound Creator", NULL))
		{
			// Set entity parameters
			static char entityName[64] = "Sound Entity";
			{
				ImGui::Text("Entity Name:");
				ImGui::SameLine();
				ImGui::InputText("##EntityName", entityName, sizeof(entityName));
			}
			static std::string soundName = "";
			static float volume = 1.0f;
			static float distanceScaler = 50.0f;
			static float reverbScaler = 1.0f;
			static bool loop = false;
			static bool foundFile = false;
			static int amountOfSounds = 1;

			{
				ImGui::Text("Sound Name:");
				ImGui::SameLine();
				if (ImGui::InputText("##SoundName", &soundName))
				{
					std::string fileName = PATH_FILE_EXT(SOUNDS_PATH, soundName, "wav");
					struct stat buffer;
					foundFile = stat(fileName.c_str(), &buffer) == 0;
				}
				ImGui::SetItemTooltip(std::format("Name of the sound file you want to use, located in {}.", SOUNDS_PATH).c_str());

				if (ImGui::Button("Browse"))
				{
					const char *filterPatterns[] = { "*.wav" };
					const char *selectedFiles = tinyfd_openFileDialog(
						"Open File",
						PATH_FILE(SOUNDS_PATH, "").c_str(),
						1,
						filterPatterns,
						"Supported Files",
						0
					);

					if (selectedFiles)
					{
						std::string fileString = selectedFiles;

						std::vector<std::string> filePaths;
						std::stringstream ss(fileString);
						std::string filePath;
						while (std::getline(ss, filePath, '|'))
						{
							filePaths.emplace_back(filePath);
						}

						if (filePaths.size() > 0)
						{
							soundName = filePaths[0].substr(filePaths[0].find_last_of('\\') + 1);
							soundName = soundName.substr(0, soundName.find_last_of('.'));

							std::string fileName = PATH_FILE_EXT(SOUNDS_PATH, soundName, "wav");
							struct stat buffer;
							foundFile = stat(fileName.c_str(), &buffer) == 0;
						}
					}
				}
			}

			ImGui::DragFloat("Volume", &volume, 0.05f, 0.0f, 1.0f);
			ImGui::DragFloat("Distance", &distanceScaler, 0.1f);
			ImGui::DragFloat("Reverb", &reverbScaler, 0.1f);
			ImGui::DragInt("Amount of Sounds", &amountOfSounds, 1, 1, 100);
			ImGui::Checkbox("Loop", &loop);

			if (!foundFile)
			{
				ImGui::BeginDisabled(true);
			}
			if (ImGui::Button("Create", ImVec2(120, 0)))
			{
				std::string foo(entityName);
				for (int i = 0; i < amountOfSounds; i++)
				{
					std::string temp = foo;
					if (amountOfSounds > 1)
						temp += std::format("{}", i + 1);
					strncpy_s(entityName, sizeof(entityName), temp.c_str(), sizeof(entityName));
					entityName[sizeof(entityName) - 1] = '\0';
					// Create entity with given parameters.
					if (!CreateSoundEmitterEntity(&ent, entityName, soundName, loop, volume, distanceScaler, reverbScaler))
					{
						ErrMsg("Failed to create sound entity!");
						ImGui::CloseCurrentPopup();
						ImGui::EndPopup();
						ImGui::EndChild();
						ImGui::TreePop();
						return false;
					}
				}
				entityName[0] = '\0';
				ImGui::CloseCurrentPopup();
			}
			if (!foundFile)
			{
				ImGui::EndDisabled();
				ImGui::SetItemTooltip("File could not be found.");
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(120, 0)))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}
	*/

	if (ent)
	{
		if (positionWithCursor)
		{
			_debugPlayer.Get()->ClearSelection();
			_debugPlayer.Get()->PositionWithCursor(ent);
		}
		else
		{
			SetSelection(ent, ImGui::GetIO().KeyShift);
		}
	}

	return true;
}
bool Scene::RenderSceneHierarchyUI()
{
	static std::string search = "";
	ImGui::InputText("Search", &search);
	if (ImGui::SmallButton("Clear Search"))
		search.clear();

	std::string searchLower = search;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

	ImGuiChildFlags childFlags = 0;
	childFlags |= ImGuiChildFlags_Border;
	childFlags |= ImGuiChildFlags_ResizeY;

	ImGui::BeginChild("Scene Hierarchy", ImVec2(0, 300), childFlags);
	SceneContents::SceneIterator entIter = _sceneHolder.GetEntities();

	while (Entity *entity = entIter.Step())
	{
		if (entity->GetParent() != nullptr) // Skip non-root entities
			continue;

		if (!RenderEntityHierarchyUI(entity, 0, searchLower))
		{
			ImGui::EndChild();
			return false;
		}
	}
	ImGui::EndChild();

	return true;
}
bool Scene::RenderSelectionHierarchyUI()
{
	static std::string search = "";
	ImGui::InputText("Search", &search);
	if (ImGui::SmallButton("Clear Search"))
		search.clear();

	std::string searchLower = search;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

	ImGuiChildFlags childFlags = 0;
	childFlags |= ImGuiChildFlags_Border;
	childFlags |= ImGuiChildFlags_ResizeY;

	ImGui::BeginChild("Selection Hierarchy", ImVec2(0, 300), childFlags);

	auto &selection = _debugPlayer.Get()->GetSelection();

	for (int i = 0; i < selection.size(); i++)
	{
		if (Entity *ent = selection[i].Get())
		{
			if (!RenderEntityHierarchyUI(ent, 0, searchLower))
			{
				ImGui::EndChild();
				return false;
			}
		}
	}

	ImGui::EndChild();
	return true;
}
bool Scene::RenderEntityHierarchyUI(Entity *root, UINT depth, const std::string &search)
{
	if (!root)
		return true;

	if (!root->GetShowInHierarchy())
		return true;

	std::string entName = root->GetName();
	UINT entIndex = _sceneHolder.GetEntityIndex(root);
	UINT entID = root->GetID();

	std::string entNameLower = entName;
	std::transform(entNameLower.begin(), entNameLower.end(), entNameLower.begin(), ::tolower);
	if (!search.empty() && entNameLower.find(search) == std::string::npos)
		return true;

	float frameHeight = ImGui::GetFrameHeight();
	float arrowScale = 0.7f;
	ImVec2 arrowSize = { frameHeight * arrowScale, frameHeight * arrowScale };
	float indenting = depth * frameHeight * arrowScale;

	bool isCollapsed = false;
	int collapseIndex = 0;
	for (int i = 0; i < _collapsedEntities.size(); i++)
	{
		if (_collapsedEntities[i].Get() == root)
		{
			isCollapsed = true;
			collapseIndex = i;
			break;
		}
	}

	ImGui::PushID(("Ent:" + std::to_string(entID)).c_str());
	float indentedXPos = ImGui::GetCursorPosX() + indenting;
	ImGui::SetCursorPosX(indentedXPos);

	// Collapse arrow
	{
		ImVec2 originalCursorPos = ImGui::GetCursorPos();
		ImGui::Dummy({ arrowSize.x, frameHeight });
		ImGui::SameLine(0.0f, 2.0f);

		if (root->GetChildCount() > 0)
		{
			ImVec2 arrowCursorPos = originalCursorPos;
			arrowCursorPos.y += 0.5f * (frameHeight - arrowSize.y);

			ImVec2 afterCursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(arrowCursorPos);
			if (isCollapsed)
			{
				if (ImGui::ArrowButtonEx("Expand", ImGuiDir_Right, arrowSize, ImGuiButtonFlags_None))
				{
					isCollapsed = false;
					_collapsedEntities.erase(_collapsedEntities.begin() + collapseIndex);
				}
			}
			else
			{
				if (ImGui::ArrowButtonEx("Collapse", ImGuiDir_Down, arrowSize, ImGuiButtonFlags_None))
				{
					Ref<Entity> &entRef = _collapsedEntities.emplace_back(*root);
					entRef.AddDestructCallback([this](Ref<Entity> *entRef) {
						for (int i = 0; i < _collapsedEntities.size(); i++)
						{
							if (&_collapsedEntities[i] != entRef)
								continue;
							
							_collapsedEntities.erase(_collapsedEntities.begin() + i);
							break;
						}
					});
				}
			}
			ImGui::SetCursorPos(afterCursorPos);
		}
	}

	float entityButtonPosX;

	// Entity selection button
	{
		bool isSelected = _debugPlayer.Get()->IsSelected(root, nullptr);

		if (isSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.1f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.1f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.1f, 0.75f, 0.7f));
		}

		ImGui::SameLine(0.0f, 3.0f);
		entityButtonPosX = ImGui::GetCursorPosX();
		if (ImGui::Button(std::format("{}", entName).c_str()))
		{
			// Additive select if holding shift
			_debugPlayer.Get()->Select(root, ImGui::GetIO().KeyShift);
		}

		if (isSelected)
		{
			ImGui::PopStyleColor(3);
		}
	}

	// Entity Drag & Drop field
	{
		const char *dragDropTag = ImGui::PayloadTags.at(ImGui::PayloadType::ENTITY);

		// Our buttons are both drag sources and drag targets
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::EntityPayload payload = { entID, GetUID()};
			ImGui::SetDragDropPayload(dragDropTag, &payload, sizeof(ImGui::EntityPayload));

			ImGui::Text(std::format("{}", entName).c_str());

			// Display preview (could be anything, e.g. when dragging an image we could decide to display
			// the filename and a small preview of the image, etc.)
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(dragDropTag))
			{
				IM_ASSERT(payload->DataSize == sizeof(ImGui::EntityPayload));
				ImGui::EntityPayload entPayload = *(const ImGui::EntityPayload *)payload->Data;

				Entity *payloadEnt = nullptr;

				if (entPayload.sceneUID != GetUID())
				{
					// Dragging from another scene
					Scene *payloadScene = _game->GetSceneByUID(entPayload.sceneUID);

					if (!payloadScene)
					{
						ErrMsg("Failed to get scene from payload!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					Entity *payloadEnt = payloadScene->_sceneHolder.GetEntityByID(entPayload.entID);

					json::Document doc;
					json::Value entObj = json::Value(json::kObjectType);

					if (!payloadScene->SerializeEntity(doc.GetAllocator(), entObj, payloadEnt, true))
					{
						ErrMsg("Failed to serialize entity from payload!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					if (!payloadScene->_sceneHolder.RemoveEntityImmediate(payloadEnt))
					{
						ErrMsg("Failed to remove entity from payload scene!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					payloadEnt = nullptr;
					if (!DeserializeEntity(entObj, &payloadEnt))
					{
						ErrMsg("Failed to deserialize entity from payload!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					RunPostDeserializeCallbacks();

					payloadEnt->SetParent(root, true);
				}
				else if (entPayload.entID != entID)
				{
					// Dragging from the same scene
					payloadEnt = _sceneHolder.GetEntityByID(entPayload.entID);

					if (payloadEnt)
					{
						if (!root->IsChildOf(payloadEnt))
							payloadEnt->SetParent(root, true);
					}
				}

			}
			ImGui::EndDragDropTarget();
		}
	}

	float rightEdgeX = ImGui::GetContentRegionAvail().x;

	// Dock/Undock button
	{
		ImGui::PushID(("Dock:" + std::to_string(entID)).c_str());

		const ImVec2 dockButtonRect = { 30, 20 };

		ImGui::SameLine(rightEdgeX - 8.0f - frameHeight - dockButtonRect.x);
		const std::string windowID = std::format("Ent#{}", entID);

		// Check if entity is undocked
		if (ImGuiUtils::Utils::GetWindow(windowID, nullptr))
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.05f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.05f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.05f, 0.75f, 0.7f));

			// If undocked, show dock button
			if (ImGui::Button("[x]", dockButtonRect))
			{
				if (!ImGuiUtils::Utils::CloseWindow(windowID))
				{
					ImGui::PopStyleColor(3);
					ImGui::PopID();
					ErrMsg("Failed to dock entity window!");
					return false;
				}
			}
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.6f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.6f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.6f, 0.75f, 0.7f));

			// If docked, show undock button
			if (ImGui::Button("[ ]", dockButtonRect))
			{
				const std::string windowName = std::format("Entity '{}'", root->GetName());
				if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, std::bind(&Entity::InitialRenderUI, root)))
				{
					ImGui::PopStyleColor(3);
					ImGui::PopID();
					ErrMsg("Failed to undock entity window!");
					return false;
				}
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopID();
	}

	// Enabled checkbox
	{
		// Align checkbox to the right
		ImGui::SameLine(rightEdgeX - frameHeight);

		bool isEnabled = root->IsEnabledSelf();
		if (ImGui::Checkbox("##Enabled", &isEnabled))
			root->SetEnabledSelf(isEnabled);
	}

	if (!isCollapsed)
	{
		const std::vector<Entity *> *children = root->GetChildren();
		for (auto &child : *children)
		{
			if (!child)
				continue;

			if (!RenderEntityHierarchyUI(child, depth + 1))
			{
				ErrMsg("Failed to render entity hierarchy UI!");
				return false;
			}
		}
	}

	ImGui::PopID();

	// Sibling drop field
	{
		// Set vertical spacing
		float dummyDropTargetPosY = ImGui::GetCursorPosY() - 4.0f;
		ImGui::SetCursorPos({ entityButtonPosX, dummyDropTargetPosY });
		ImGui::Dummy({ ImGui::GetContentRegionAvail().x, 6.0f});
		float dummyDropTargetHeight = ImGui::GetItemRectSize().y;
		ImVec2 nextCursorPos = ImGui::GetCursorPos();
		nextCursorPos.y -= 3.0f;

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::ENTITY)))
			{
				IM_ASSERT(payload->DataSize == sizeof(ImGui::EntityPayload));
				ImGui::EntityPayload entPayload = *(const ImGui::EntityPayload *)payload->Data;

				Entity *payloadEnt = nullptr;

				if (entPayload.sceneUID != GetUID())
				{
					// Dragging from another scene
					Scene *payloadScene = _game->GetSceneByUID(entPayload.sceneUID);

					if (!payloadScene)
					{
						ErrMsg("Failed to get scene from payload!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					Entity *payloadEnt = payloadScene->_sceneHolder.GetEntityByID(entPayload.entID);

					json::Document doc;
					json::Value entObj = json::Value(json::kObjectType);

					if (!payloadScene->SerializeEntity(doc.GetAllocator(), entObj, payloadEnt, true))
					{
						ErrMsg("Failed to serialize entity from payload!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					if (!payloadScene->_sceneHolder.RemoveEntityImmediate(payloadEnt))
					{
						ErrMsg("Failed to remove entity from payload scene!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					payloadEnt = nullptr;
					if (!DeserializeEntity(entObj, &payloadEnt))
					{
						ErrMsg("Failed to deserialize entity from payload!");
						ImGui::EndDragDropTarget();
						ImGui::PopID();
						return false;
					}

					RunPostDeserializeCallbacks();
				}
				else if (entPayload.entID != entID)
				{
					// Dragging from the same scene
					payloadEnt = _sceneHolder.GetEntityByID(entPayload.entID);

				}

				if (payloadEnt)
				{
					bool skipParenting = false;
					Entity *rootParent = root->GetParent();

					if (rootParent)
						if (rootParent->IsChildOf(payloadEnt))
							skipParenting = true;

					if (!skipParenting)
						payloadEnt->SetParent(rootParent, true);

					_sceneHolder.ReorderEntity(payloadEnt, root);
				}
			}
			ImGui::EndDragDropTarget();
		}
		
		ImGui::SameLine(entityButtonPosX, 0.0f);
		ImGui::SetCursorPosY(dummyDropTargetPosY + (0.5f * dummyDropTargetHeight) - 1.0f);
		ImGui::Separator();
		ImGui::SetCursorPos(nextCursorPos);
	}

	return true;
}

bool Scene::RenderUI()
{
	using namespace dx;
	ZoneScopedC(RandomUniqueColor());

	if (IsUndocked())
	{
		if (ImGui::Button("Dock Scene"))
		{
			if (!ImGuiUtils::Utils::CloseWindow(GetName()))
			{
				ErrMsg("Failed to close scene window!");
				return false;
			}
		}
	}

	ImGui::SeparatorText("Entities");

	if (ImGui::CollapsingHeader("Hierarchy"))
	{
		ImGui::Text("Objects in Scene: %d", _sceneHolder.GetEntityCount());

		if (ImGui::BeginTabBar("HierarchyTab"))
		{
			if (ImGui::BeginTabItem("Scene Hierarchy"))
			{
				ImGui::PushID("Scene Hierarchy");

				const std::string windowID = std::format("Scene'{}'", GetName());

				// Check if scene hierarchy is undocked
				if (!ImGuiUtils::Utils::GetWindow(windowID, nullptr))
				{
					if (ImGui::Button("Undock"))
					{
						const std::string windowName = std::format("'{}' Scene Hierarchy", GetName());

						if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, std::bind(&Scene::RenderSceneHierarchyUI, this)))
						{
							ImGui::PopID();
							ImGui::EndTabItem();
							ErrMsg("Failed to undock scene hierarchy window!");
							return false;
						}
					}
				}

				if (!RenderSceneHierarchyUI())
				{
					ImGui::PopID();
					ImGui::EndTabItem();
					ErrMsg("Failed to render scene hierarchy UI!");
					return false;
				}

				ImGui::PopID();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Selection Hierarchy"))
			{
				ImGui::PushID("Selection Hierarchy");

				const std::string windowID = std::format("Selection'{}'", GetName());

				// Check if scene hierarchy is undocked
				if (!ImGuiUtils::Utils::GetWindow(windowID, nullptr))
				{
					if (ImGui::Button("Undock"))
					{
						const std::string windowName = std::format("'{}' Selection Hierarchy", GetName());

						if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, std::bind(&Scene::RenderSelectionHierarchyUI, this)))
						{
							ImGui::PopID();
							ImGui::EndTabItem();
							ErrMsg("Failed to undock selection hierarchy window!");
							return false;
						}
					}
				}

				if (!RenderSelectionHierarchyUI())
				{
					ImGui::PopID();
					ImGui::EndTabItem();
					ErrMsg("Failed to render selection hierarchy UI!");
					return false;
				}

				ImGui::PopID();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::Separator();
	}

	if (ImGui::CollapsingHeader("Selected Entity"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Border;
		childFlags |= ImGuiChildFlags_ResizeY;

		ImGui::BeginChild("Selection UI", ImVec2(0, 300), childFlags);

		int selectionSize = (int)_debugPlayer.Get()->GetSelectionSize();
		if (selectionSize > 0)
		{
			static int selectionIndex = 1;

			if (selectionSize > 1)
			{
				float numWidth = ImGui::CalcTextSize(std::to_string(selectionIndex).c_str()).x;

				ImGui::SetNextItemWidth(numWidth + 10);
				ImGui::DragInt("##selectionIndex", &selectionIndex, 0.1f);
				ImGui::SameLine();
				ImGui::Text("/ %d", selectionSize);

				ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
				{
					ImGui::SameLine(0.0f, 6.0f);
					if (ImGui::ArrowButton("##left", ImGuiDir_Left))
						selectionIndex--;
					ImGui::SameLine();
					if (ImGui::ArrowButton("##right", ImGuiDir_Right))
						selectionIndex++;
				}
				ImGui::PopItemFlag();

				ImGui::Separator();
			}

			selectionIndex = Wrap(selectionIndex, 1, selectionSize);
			Entity *ent = _debugPlayer.Get()->GetSelection()[(size_t)selectionIndex - 1].Get();

			const UINT entID = ent->GetID();
			const std::string windowID = std::format("Ent#{}", entID);

			// Check if entity is undocked
			if (ImGuiUtils::Utils::GetWindow(windowID, nullptr))
			{
				ImGui::Text("Selected entity is undocked!");

				if (ImGui::Button("Dock"))
				{
					if (!ImGuiUtils::Utils::CloseWindow(windowID))
					{
						ImGui::EndChild();
						ErrMsg("Failed to close undocked entity window!");
						return false;
					}
				}
			}
			else
			{
				if (!ent->InitialRenderUI())
				{
					ImGui::EndChild();
					ErrMsg("Failed to render selected entity UI!");
					return false;
				}
			}
		}
		else
		{
			ImGui::Text("No entity selected.");
		}

		ImGui::EndChild();
	}

	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::SeparatorText("Tools");

	if (ImGui::CollapsingHeader("Creation"))
	{
		if (!RenderEntityCreatorUI())
		{
			ErrMsg("Failed to render entity creator UI!");
			return false;
		}
	}

	if (ImGui::CollapsingHeader("Settings"))
	{
		if (ImGui::TreeNode("Shadows"))
		{
			ImGui::Text("Pointlight Resolution:"); ImGui::SameLine();
			static int pointlightRes = (int)_pointlights->GetShadowResolution();
			if (ImGui::DragInt("##PointlightRes", &pointlightRes))
			{
				pointlightRes = max(1, pointlightRes);
				_pointlights->SetShadowResolution((UINT)pointlightRes);
			}

			ImGui::Text("Spotlight Resolution:"); ImGui::SameLine();
			static int spotlightRes = (int)_spotlights->GetShadowResolution();
			if (ImGui::DragInt("##SpotlightRes", &spotlightRes))
			{
				spotlightRes = max(1, spotlightRes);
				_spotlights->SetShadowResolution((UINT)spotlightRes);
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Scene Holder"))
		{
			if (!_sceneHolder.RenderUI())
			{
				ImGui::TreePop();
				ErrMsg("Failed to render scene holder UI!");
				return false;
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Audio"))
		{
			dx::AudioEngine *audioEngine = _soundEngine.GetAudioEngine();
			IXAudio2 *audioInterface = audioEngine->GetInterface();

			UINT outputChannels = audioEngine->GetOutputChannels();
			ImGui::Text("Output Channels: %d", outputChannels);

			// Set Reverb Zone
			{
				std::vector<std::string> reverbNames = {
					"Off", "Default", "Generic", "Forest", "Padded Cell",
					"Room", "Bathroom", "Living Room", "Stone Room", "Auditorium",
					"Concert Hall", "Cave", "Arena", "Hangar", "Carpeted Hallway",
					"Hallway", "Stone Corridor", "Alley", "City", "Mountains",
					"Quarry", "Plain", "Parking Lot", "Sewer Pipe", "Underwater",
					"Small Room", "Medium Room", "Large Room", "Medium Hall",
					"Large Hall", "Plate"
				};

				static dx::AUDIO_ENGINE_REVERB newReverb = Reverb_Cave;
				if (ImGui::BeginCombo("Reverb Zone", reverbNames[(int)newReverb].c_str()))
				{
					for (int i = 0; i < reverbNames.size(); i++)
					{
						const bool isSelected = ((int)newReverb == i);
						if (ImGui::Selectable(reverbNames[i].c_str(), isSelected))
						{
							newReverb = (dx::AUDIO_ENGINE_REVERB)i;
							audioEngine->SetReverb(newReverb);
						}

						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Graph Manager"))
		{
			ImGui::Text(std::format("Nodes in Scene: {}", _graphManager.GetNodeCount()).c_str());

			if (_monster)
			{
				ImGui::Separator();
				static bool showMonsterPath = true;
				ImGui::Checkbox("Show Monster Path", &showMonsterPath);
				if (showMonsterPath)
				{
					static bool overlayMonsterPath = false;
					ImGui::Checkbox("Overlay##OverlayMonsterPath", &overlayMonsterPath);

					std::vector<dx::XMFLOAT3> *points;
					if (_monster.GetAs<MonsterBehaviour>()->GetPath(points))
					{
						DebugDrawer &drawer = DebugDrawer::Instance();
						drawer.DrawLineStrip(
							points->data(),
							static_cast<UINT>(points->size()),
							0.275f, { 0,1,0.5f,1 },
							!overlayMonsterPath
						);
					}
				}
				ImGui::Separator();
			}

			Entity *ent = _debugPlayer.Get()->GetPrimarySelection();
			dx::XMFLOAT3 posA = ent ? To3(ent->GetTransform()->GetPosition(World)) : dx::XMFLOAT3(0, 0, 0);

			dx::XMFLOAT3 fromPos = posA;
			static dx::XMFLOAT3 toPos;
			ImGui::InputFloat3("Path To", &toPos.x);

			if (!_graphManager.RenderUI(fromPos, toPos))
			{
				ImGui::TreePop();
				return false;
			}

			static Ref<Entity> firstNode = nullptr;
			static enum class GraphEditType {
				None, Connect, Disconnect, Split,
			} graphEditType = GraphEditType::None;

			if (firstNode)
			{
				ImGui::Text("Select Target Node...");

				if (ImGui::Button("[Num1] Cancel Connection") || _input->GetKey(KeyCode::NumPad1) == KeyState::Pressed)
				{
					_debugPlayer.Get()->Select(firstNode.Get());
					firstNode = nullptr;
				}
			}

			if (ent)
			{
				GraphNodeBehaviour *node;
				if (ent->GetBehaviourByType<GraphNodeBehaviour>(node))
				{
					if (ImGui::TreeNode("Node Properties"))
					{
						ImGui::PushID("NodeProperties");
						if (!node->InitialRenderUI())
						{
							ImGui::PopID();
							ImGui::TreePop();
							ErrMsg("Failed to render selected node properties!");
							return false;
						}
						ImGui::PopID();

						ImGui::TreePop();
					}

					if (!firstNode)
					{
						if (ImGui::Button("[Num0] Connect Selected") || _input->GetKey(KeyCode::NumPad0) == KeyState::Pressed)
						{
							graphEditType = GraphEditType::Connect;
							firstNode = ent;
							_debugPlayer.Get()->ClearSelection();
						}

						if (ImGui::Button("[Num2] Disconnect Selected") || _input->GetKey(KeyCode::NumPad2) == KeyState::Pressed)
						{
							graphEditType = GraphEditType::Disconnect;
							firstNode = ent;
							_debugPlayer.Get()->ClearSelection();
						}

						ImGui::Text("[Num3] New Connected Node");

						if (ImGui::Button("[Num4] Split Selected") || _input->GetKey(KeyCode::NumPad4) == KeyState::Pressed)
						{
							graphEditType = GraphEditType::Split;
							firstNode = ent;
							_debugPlayer.Get()->ClearSelection();
						}

						if (ImGui::Button("[Num5] New Connected Node At Camera") || _input->GetKey(KeyCode::NumPad5) == KeyState::Pressed)
						{
							Transform *cameraTransform = _viewCamera.Get()->GetTransform();

							Entity *copyEnt = nullptr;
							GraphNodeBehaviour *copyNode = nullptr;

							if (!CreateGraphNodeEntity(&copyEnt, &copyNode, cameraTransform->GetPosition()))
							{
								ErrMsg("Failed to copy node entity!");
								return false;
							}

							copyNode->AddConnection(node);
							_debugPlayer.Get()->Select(copyEnt);
						}

						if (ImGui::Button("[Num6] Mark As Mine") || _input->GetKey(KeyCode::NumPad6) == KeyState::Pressed)
						{
							node->SetCost(0.0f);
						}
					}
					else
					{
						Entity *otherEnt = firstNode.Get();
						GraphNodeBehaviour *otherNode;
						if (otherEnt->GetBehaviourByType<GraphNodeBehaviour>(otherNode))
						{
							DebugDrawer::Instance().DrawLine(
								posA, otherEnt->GetTransform()->GetPosition(World),
								0.4f, { 1,0,1,0.3f }, false
							);

							if (ImGui::Button("[Enter] Apply") || _input->GetKey(KeyCode::Enter) == KeyState::Pressed)
							{
								switch (graphEditType)
								{
								case GraphEditType::Connect:
									otherNode->AddConnection(node);
									_debugPlayer.Get()->Select(firstNode.Get());
									break;

								case GraphEditType::Disconnect:
									otherNode->RemoveConnection(node);
									_debugPlayer.Get()->Select(firstNode.Get());
									break;

								case GraphEditType::Split:
									// Remove existing connection
									otherNode->RemoveConnection(node);

									// Create new node between selected nodes
									dx::XMFLOAT3 newPos;
									Store(newPos, (Load(posA) + Load(otherEnt->GetTransform()->GetPosition(World))) * 0.5f);

									Entity *newEnt;
									GraphNodeBehaviour *newNode;
									if (!CreateGraphNodeEntity(&newEnt, &newNode, newPos))
									{
										ErrMsg("Failed to create new node!");
										return false;
									}

									// Connect new node to both selected nodes
									newNode->AddConnection(node);
									newNode->AddConnection(otherNode);

									// Select new node
									_debugPlayer.Get()->Select(newEnt);
									break;
								}

								firstNode = nullptr;
							}
						}
					}
				}
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Timeline Manager"))
		{
			if (!_timelineManager.RenderUI(_viewCamera.Get()->GetTransform()))
			{
				ImGui::TreePop();
				return false;
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		ImGui::Dummy(ImVec2(0.0f, 2.0f));

		if (ImGui::Button("Clear All Duplicate Binds"))
		{
			_debugPlayer.Get()->ClearDuplicateBinds();
		}
	}

	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::SeparatorText("Misc");

	if (ImGui::CollapsingHeader("Other"))
	{
		if (ImGui::TreeNode("Raycast Tests"))
		{
			static Shape::Ray ray({ 0,0,0 }, { 0,0,1 }, 1);
			Shape::RayHit hit;
			bool didHit = false;
			Entity *hitEntity = nullptr;
			static Ref<Entity> originEntity = nullptr;

			ImGui::SeparatorText("Ray");
			{
				if (ImGui::Button(std::format("Track Entity: {}", originEntity.IsValid() ? originEntity.Get()->GetName() : "None").c_str()))
					_debugPlayer.Get()->Select(originEntity.Get(), false);
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::ENTITY)))
					{
						IM_ASSERT(payload->DataSize == sizeof(ImGui::EntityPayload));
						ImGui::EntityPayload entPayload = *(const ImGui::EntityPayload *)payload->Data;

						Entity *payloadEnt = nullptr;

						if (entPayload.sceneUID != GetUID())
						{
							// Dragging from another scene
						}
						else
						{
							// Dragging from the same scene
							payloadEnt = _sceneHolder.GetEntityByID(entPayload.entID);
							if (payloadEnt)
								originEntity.Set(payloadEnt);
						}

					}
					ImGui::EndDragDropTarget();
				}

				if (originEntity.IsValid())
				{
					Transform *originEntT = originEntity.Get()->GetTransform();
					ray.origin = originEntT->GetPosition(World);
					ray.direction = originEntT->GetForward(World);

					ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
					ImGui::SameLine();
					if (ImGui::Button("X##RaycastTests"))
					{
						originEntity = nullptr;
					}
					ImGuiUtils::EndButtonStyle();
				}
				else
				{
					ImGui::DragFloat3("Origin##RaycastTests", &ray.origin.x, 0.05f);
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::DragFloat3("Direction##RaycastTests", &ray.direction.x, 0.01f))
						Store(ray.direction, dx::XMVector3Normalize(Load(ray.direction)));
					ImGuiUtils::LockMouseOnActive();

					if (_viewCamera && ImGui::Button("Move Ray to View##RaycastTests"))
					{
						Transform *camT = _viewCamera.Get()->GetTransform();
						ray.origin = camT->GetPosition(World);
						ray.direction = camT->GetForward(World);
					}
				}

				ImGui::DragFloat("Length##RaycastTests", &ray.length, 0.01f);
				ImGuiUtils::LockMouseOnActive();

				didHit = _sceneHolder.RaycastScene(ray, hit, hitEntity);
			}

			ImGui::SeparatorText("Drawing");
			{
				static bool drawRay = true;
				ImGui::Checkbox("Draw Ray##RaycastTests", &drawRay);
				if (drawRay)
				{
					static bool overlayRay = false;
					ImGui::Checkbox("Overlay Ray##RaycastTests", &overlayRay);

					dx::XMFLOAT3 scaledDir;
					Store(scaledDir, Load(ray.direction) * ray.length);
					DebugDrawer::Instance().DrawRay(ray.origin, scaledDir, 0.1f, { 1, 1, 0, 0.5f }, !overlayRay);
				}

				static bool drawHit = true;
				ImGui::Checkbox("Draw Hit##RaycastTests", &drawHit);
				if (drawHit)
				{
					static bool overlayHit = false;
					ImGui::Checkbox("Overlay Hit##RaycastTests", &overlayHit);

					if (didHit)
					{
						DebugDrawer::Instance().DrawLine(ray.origin, hit.point, 0.2f, { 1, 0, 0, 0.5f }, !overlayHit);
						DebugDrawer::Instance().DrawRay(hit.point, hit.normal, 0.075f, { 0, 0, 1, 0.5f }, !overlayHit);
					}
				}
			}

			ImGui::SeparatorText("Data");
			{
				ImGui::Text(std::format("Ray Origin:    ({}, {}, {})", ray.origin.x, ray.origin.y, ray.origin.z).c_str());
				ImGui::Text(std::format("Ray Direction: ({}, {}, {})", ray.direction.x, ray.direction.y, ray.direction.z).c_str());
				ImGui::Text(std::format("Ray Length:	{}", ray.length).c_str());

				if (didHit)
				{
					ImGui::Text(std::format("Hit Point:		({}, {}, {})", hit.point.x, hit.point.y, hit.point.z).c_str());
					ImGui::Text(std::format("Hit Normal:	({}, {}, {})", hit.normal.x, hit.normal.y, hit.normal.z).c_str());
					ImGui::Text(std::format("Hit Length:	{}", hit.length).c_str());

					if (hitEntity)
					{
						const std::string &entName = hitEntity->GetName();
						UINT entID = hitEntity->GetID();

						ImGui::Text("Hit Entity:	");
						ImGui::SameLine();
						if (ImGui::Button(std::format("{}##RayHitEntID{}", entName, entID).c_str()))
							_debugPlayer.Get()->Select(hitEntity);
					}
					else
						ImGui::Text("Hit Entity:	None");
				}
				else
					ImGui::Text("No Hit.");
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Debug Tests"))
		{
			if (ImGui::TreeNode("Tri Tests"))
			{
				static bool overlayTri = false;
				static bool doubleTri = true;
				static bool screenSpace = false;

				ImGui::Checkbox("Overlay##OverlayTri", &overlayTri);
				ImGui::Checkbox("Double Sided##DoubleSidedTri", &doubleTri);
				ImGui::Checkbox("Screen-Space Draws##DrawScreenSpaceDebugTest", &screenSpace);
				ImGui::Dummy(ImVec2(0.0f, 8.0f));


				ImGui::SeparatorText("Single Triangle");
				{
					static DebugDraw::Tri tri{
						{ {0, 0, 0}, {1, 0, 0, 1} },
						{ {0, 1, 0}, {0, 1, 0, 1} },
						{ {0, 0, 1}, {0, 0, 1, 1} }
					};

					ImGui::Text("V0:");
					ImGui::DragFloat3("Pos##v0", &tri.v0.position.x, 0.05f, 0.0f, 0.0f, "%.3f");
					ImGui::DragFloat4("Col##v0", &tri.v0.color.x, 0.01f, 0.0f, 0.0f, "%.2f");
					ImGui::Dummy(ImVec2(0.0f, 4.0f));

					ImGui::Text("V1:");
					ImGui::DragFloat3("Pos##v1", &tri.v1.position.x, 0.05f, 0.0f, 0.0f, "%.3f");
					ImGui::DragFloat4("Col##v1", &tri.v1.color.x, 0.01f, 0.0f, 0.0f, "%.2f");
					ImGui::Dummy(ImVec2(0.0f, 4.0f));

					ImGui::Text("V2:");
					ImGui::DragFloat3("Pos##v2", &tri.v2.position.x, 0.05f, 0.0f, 0.0f, "%.3f");
					ImGui::DragFloat4("Col##v2", &tri.v2.color.x, 0.01f, 0.0f, 0.0f, "%.2f");

					DebugDrawer::Instance().DrawTri(tri, !overlayTri, doubleTri);
				}
				ImGui::Dummy(ImVec2(0.0f, 8.0f));

				ImGui::SeparatorText("Random Triangles");
				{
					static std::vector<DebugDraw::Tri> tris;
					static dx::XMFLOAT3 triSpawnBounds = { 10.0f, 10.0f, 10.0f };
					static int triCount = 10;

					ImGui::DragInt("Amount##RandTris", &triCount);
					ImGui::DragFloat3("Bounds##TriSpawnBounds", &triSpawnBounds.x, 0.2f, 0.0f, 0.0f, "%.3f");

					if (ImGui::Button("Generate Random Tris"))
					{
						tris.clear();
						tris.reserve(triCount);

						for (int i = 0; i < triCount; i++)
						{
							DebugDraw::Tri tri{};

							tri.v0.position = {
								RandomFloat(-triSpawnBounds.x, triSpawnBounds.x),
								RandomFloat(-triSpawnBounds.y, triSpawnBounds.y),
								RandomFloat(-triSpawnBounds.z, triSpawnBounds.z),
								1.0f
							};
							tri.v0.color = {
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f)
							};

							tri.v1.position = {
								RandomFloat(-triSpawnBounds.x, triSpawnBounds.x),
								RandomFloat(-triSpawnBounds.y, triSpawnBounds.y),
								RandomFloat(-triSpawnBounds.z, triSpawnBounds.z),
								1.0f
							};
							tri.v1.color = {
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f)
							};

							tri.v2.position = {
								RandomFloat(-triSpawnBounds.x, triSpawnBounds.x),
								RandomFloat(-triSpawnBounds.y, triSpawnBounds.y),
								RandomFloat(-triSpawnBounds.z, triSpawnBounds.z),
								1.0f
							};
							tri.v2.color = {
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f),
								RandomFloat(0.0f, 1.0f)
							};

							tris.emplace_back(tri);
						}
					}

					DebugDrawer::Instance().DrawTris(tris.data(), tris.size(), !overlayTri, doubleTri);
				}
				ImGui::Dummy(ImVec2(0.0f, 8.0f));

				ImGui::SeparatorText("Selected OBB");
				{
					static dx::XMFLOAT4 selectColor = { 0.0f, 1.0f, 0.0f, 0.25f };
					ImGui::DragFloat4("Selection Color##SelectColor", &selectColor.x, 0.01f, 0.0f, 0.0f, "%.2f");

					Entity *selectedEnt = _debugPlayer.Get()->GetPrimarySelection();
					if (selectedEnt)
					{
						dx::BoundingOrientedBox obb;
						selectedEnt->StoreEntityBounds(obb, World);

						DebugDrawer::Instance().DrawBoxOBB(
							obb, selectColor, !overlayTri, doubleTri
						);
					}
				}

				if (screenSpace)
				{
					DebugDrawer::Instance().DrawMinMaxRect(
						{ 20, 20 },
						{ 80, 200 },
						{ 0.0f, 0.0f, 1.0f, 0.5f },
						0.5f
					);


					MouseState mState = Input::Instance().GetMouse();
					DebugDrawer::Instance().DrawExtentRect(
						{ mState.pos.x, mState.pos.y },
						{ 6, 6 },
						{ 1.0f, 1.0f, 1.0f, 0.75f },
						1.0f
					);


					DebugDrawer::Instance().DrawLineSS(
						{ 600, 1000 },
						{ 800, 1000 },
						{ 1.0f, 1.0f, 0.0f, 0.5f },
						10.0f,
						0.5f
					);


					static dx::XMFLOAT2 lastClickPos = { 0, 0 };
					if (Input::Instance().GetKey(KeyCode::M1) == KeyState::Pressed)
						lastClickPos = { mState.pos.x, mState.pos.y };

					DebugDrawer::Instance().DrawLineSS(
						lastClickPos,
						{ mState.pos.x, mState.pos.y },
						{ 1.0f, 0.0f, 0.0f, 0.5f },
						10.0f,
						0.5f
					);
				}


				ImGui::TreePop();
				ImGui::Separator();
			}

			if (ImGui::TreeNode("Sprite Tests"))
			{
				ImGui::PushID("SpriteTests");

				static DebugDraw::Sprite sprite;
				static bool overlaySprite = false;
				static bool screenSpaceSprite = false;
				static UINT texID = _content->GetTextureID("Maxwell");

				if (ImGui::BeginCombo("##SelectSpriteTextureCombo", _content->GetTextureName((UINT)texID).c_str()))
				{
					std::vector<std::string> textureNames;
					_content->GetTextureNames(&textureNames);

					for (UINT i = 0; i < textureNames.size(); i++)
					{
						bool isSelected = (texID == i);
						if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
							texID = i;

						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				if (screenSpaceSprite)
				{
					ImGui::DragFloat2("Position", &sprite.position.x, 0.25f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Depth", &sprite.position.z, 0.001f);
					ImGuiUtils::LockMouseOnActive();
				}
				else
				{
					ImGui::DragFloat3("Position", &sprite.position.x, 0.1f);
					ImGuiUtils::LockMouseOnActive();
				}

				ImGui::DragFloat2("Size", &sprite.size.x, screenSpaceSprite ? 0.25f : 0.1f);
				ImGuiUtils::LockMouseOnActive();

				ImGui::DragFloat4("Rect", &sprite.uv0.x, 0.01f);
				ImGuiUtils::LockMouseOnActive();

				ImGui::ColorEdit4("Color", &sprite.color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

				ImGui::Checkbox("Overlay##OverlaySprite", &overlaySprite);

				ImGui::Checkbox("Screen-Space##DrawScreenSpaceSpriteDebugTest", &screenSpaceSprite);

				if (screenSpaceSprite)
					DebugDrawer::Instance().DrawSpriteSS(texID, sprite);
				else
					DebugDrawer::Instance().DrawSprite(texID, sprite, !overlaySprite);

				ImGui::PopID();
				ImGui::TreePop();
				ImGui::Separator();
			}

			if (ImGui::TreeNode("Ref Tests"))
			{
				ImGui::PushID("RefTests");

				static Ref<Behaviour> meshRef = nullptr;
				static std::vector<std::pair<std::string, Ref<Entity>>> entityRefs;

				if (Entity *ent = _debugPlayer.Get()->GetPrimarySelection())
				{
					const std::string &name = ent->GetName();
					ImGui::Text("Selected Entity: %s", name.c_str());
					ImGui::Text("Active References %d", ent->GetRefs().size());

					if (ImGui::Button("Add Reference to Selected"))
						entityRefs.emplace_back(std::make_pair(name, ent->AsRef()));

					MeshBehaviour *mesh = nullptr;
					if (ent->GetBehaviourByType<MeshBehaviour>(mesh))
					{
						if (ImGui::Button("Add Reference to Mesh"))
							meshRef = mesh;
					}
				}

				ImGui::BeginChild("Reference Testing", ImVec2(0, 200), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);
				for (int i = 0; i < entityRefs.size(); i++)
				{
					ImGui::PushID(i);

					auto &[name, ref] = entityRefs[i];
					Entity *ent = nullptr;

					ImGui::Text("Reference %d: %s", i, name.c_str());
					ImGui::Text("State: %d", ref.TryGet(ent));
					ImGui::Text("Pointer: %d", ent);

					if (ent)
					{
						ImGui::Text("Name: %s", ent->GetName().c_str());
						if (ImGui::Button("Reselect"))
							_debugPlayer.Get()->Select(ent);
					}

					if (ImGui::Button("Remove Reference"))
					{
						entityRefs.erase(entityRefs.begin() + i);
						i--;
					}

					ImGui::PopID();
					ImGui::Separator();
				}
				ImGui::EndChild();

				ImGui::SeparatorText("Mesh Reference");
				if (meshRef)
				{
					MeshBehaviour *mesh = nullptr;
					ImGui::Text("Mesh Reference State: %d", meshRef.TryGetAs<MeshBehaviour>(mesh));
					if (mesh)
					{
						ImGui::Text("Mesh Name: %s", mesh->GetName().c_str());
						ImGui::Text("Entity Name: %s", mesh->GetEntity()->GetName().c_str());

						if (ImGui::Button("Reselect"))
							_debugPlayer.Get()->Select(mesh->GetEntity());
					}
					else
					{
						ImGui::Text("Mesh Reference is invalid.");
					}

					if (ImGui::Button("Free Reference"))
						meshRef = nullptr;
				}
				else
				{
					ImGui::Text("No mesh reference set.");
				}
				ImGui::Separator();

				ImGui::PopID();
				ImGui::TreePop();
				ImGui::Separator();
			}

			ImGui::Separator();
			ImGui::TreePop();
		}

		static bool displayCullingRects = false;
		ImGui::Checkbox("Show Culling Rects", &displayCullingRects);

		if (displayCullingRects)
		{
			static bool useDepthForRects = true;
			ImGui::Checkbox("Use Depth##UseDepthForRects", &useDepthForRects);

			static bool doubleSided = false;
			ImGui::Checkbox("Double-Sided##doubleSidedRects", &doubleSided);

			static bool fullTree = false;
			ImGui::Checkbox("Show Full Tree##FullTree", &fullTree);

			static bool cullingBounds = false;
			ImGui::Checkbox("Show Culling Bounds##CullingBounds", &cullingBounds);

			static float opacity = 0.3f;
			ImGui::SliderFloat("Opacity", &opacity, 0.0000001f, 1.0f);

			static float thickness = 0.95f;
			ImGui::DragFloat("Thickness", &thickness, 0.01f);


			static UINT boxCount = 16u;
			static int recalculateCounter = 0;
			static std::vector<dx::BoundingBox> boxes;

			if (recalculateCounter <= 0)
			{
				boxes.clear();
				boxes.reserve(boxCount);
				_sceneHolder.DebugGetTreeStructure(boxes, fullTree, cullingBounds);
				boxCount = boxes.size();
				recalculateCounter += 32;

				//if (!cullingBounds)
				{
					for (int i = 0; i < boxCount; i++)
					{
						dx::XMFLOAT3 &extents = boxes[i].Extents;
						extents.x *= thickness;
						extents.y *= thickness;
						extents.z *= thickness;
					}
				}
			}
			recalculateCounter--;

			int nextSeed = std::rand();
			for (UINT i = 0; i < boxCount; i++)
			{
				std::srand(69 + 420 + i);
				float hue = (float)(std::rand() % 1080) / 3.0f;
				dx::XMFLOAT3 col = HSVtoRGB({ hue, 0.8f, 0.95f });

				dx::XMFLOAT4 colA = To4(col);
				colA.w = opacity;

				DebugDrawer::Instance().DrawBoxAABB(
					boxes[i], colA, useDepthForRects, doubleSided
				);
			}
			std::srand(nextSeed);
		}

		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 4.0f));

		static bool currState = false;
		static float fadeDuration = 1.0f;
		ImGui::SliderFloat("Fade Duration", &fadeDuration, 0.0f, 3.0f);

		if (ImGui::Button("Begin Fade"))
		{
			_graphics->BeginScreenFade(fadeDuration * (currState ? -1.0f : 1.0f));
			currState = !currState;
		}

		ImGui::Dummy(ImVec2(0.0f, 4.0f));

		dx::XMFLOAT3A camPos = _viewCamera.Get()->GetTransform()->GetPosition();
		char camXCoord[32]{}, camYCoord[32]{}, camZCoord[32]{};
		snprintf(camXCoord, sizeof(camXCoord), "%.2f", camPos.x);
		snprintf(camYCoord, sizeof(camYCoord), "%.2f", camPos.y);
		snprintf(camZCoord, sizeof(camZCoord), "%.2f", camPos.z);
		ImGui::Text(std::format("Camera Pos: ({}, {}, {})", camXCoord, camYCoord, camZCoord).c_str());

		ImGui::Separator();
		
		char nearPlane[16]{}, farPlane[16]{};
		for (UINT i = 0; i < _spotlights->GetNrOfLights(); i++)
		{
			const ProjectionInfo projInfo = _spotlights->GetLightBehaviour(i)->GetShadowCamera()->GetCurrProjectionInfo();
			snprintf(nearPlane, sizeof(nearPlane), "%.2f", projInfo.planes.nearZ);
			snprintf(farPlane, sizeof(farPlane), "%.1f", projInfo.planes.farZ);
			ImGui::Text(std::format("({}:{}) Planes Spotlight #{}", nearPlane, farPlane, i).c_str());
		}
	}
	
	return true;
}
#endif

#ifdef USE_IMGUIZMO
bool Scene::RenderGizmoUI()
{
	using namespace dx;
	ZoneScopedX;

	if (!_viewCamera)
		return true;

	Input &input = Input::Instance();

	// ImGuizmo setup
	auto windowPos = input.GetWindowPos();
	auto scenePos = input.GetSceneViewPos();
	auto sceneSize = input.GetSceneViewSize();
	float scenePosX = windowPos.x + scenePos.x;
	float scenePosY = windowPos.y + scenePos.y;
	float sceneWidth = sceneSize.x;
	float sceneHeight = sceneSize.y;

	ImGuizmo::SetOrthographic(_viewCamera.Get()->GetOrtho());

	// Selection transform gizmo
	if (auto dbgPlayer = GetDebugPlayer())
	{
		Entity *primaryEnt = dbgPlayer->GetPrimarySelection();

		if (primaryEnt)
		{
			XMFLOAT4X4A camView = _viewCamera.Get()->GetViewMatrix();
			const XMFLOAT4X4A camProj = _viewCamera.Get()->GetProjectionMatrix();

			Transform *primaryTrans = primaryEnt->GetTransform();

			const std::vector<Ref<Entity>> &selectedEnts = dbgPlayer->GetSelection();

			std::vector<Ref<Entity>> transformingEnts;
			dbgPlayer->GetParentSelection(transformingEnts);

			TransformationType transType = dbgPlayer->GetEditType();
			ReferenceSpace space = dbgPlayer->GetEditSpace();
			TransformOriginMode originMode = dbgPlayer->GetEditOriginMode();

			static XMFLOAT3A cachedPivotPos, cachedPivotScale;
			static bool hasCachedPivot = false;

			XMFLOAT3A pivotPos, pivotScale;
			XMFLOAT4A pivotRot;

			switch (originMode)
			{
				case TransformOriginMode::Primary:
				case TransformOriginMode::Separate: {
					pivotPos = primaryTrans->GetPosition(World);
					pivotRot = primaryTrans->GetRotation(World);
					pivotScale = primaryTrans->GetScale(World);
					break;
				}

				case TransformOriginMode::Center: {
					pivotRot = primaryTrans->GetRotation(World);

					if (hasCachedPivot)
					{
						pivotPos = cachedPivotPos;
						pivotScale = cachedPivotScale;
					}
					else
					{
						std::vector<XMFLOAT3> points;
						points.resize(selectedEnts.size() * 8);

						size_t i = 0;
						for (const auto &entRef : selectedEnts)
						{
							if (Entity *ent; entRef.TryGet(ent))
							{
								BoundingOrientedBox entBounds;
								ent->StoreEntityBounds(entBounds, World);

								entBounds.GetCorners(&points[i * 8]);
								i++;
							}
						}

						BoundingBox mergedBounds;
						BoundingBox::CreateFromPoints(mergedBounds, i * 8, points.data(), sizeof(XMFLOAT3));

						pivotPos = To3(mergedBounds.Center);
						pivotScale = To3(mergedBounds.Extents);
					}
					break;
				}

				case TransformOriginMode::Average: {
					XMVECTOR sumPos = XMVectorZero();

					size_t i = 0;
					for (const auto &entRef : transformingEnts)
					{
						if (Entity *ent; entRef.TryGet(ent))
						{
							sumPos += Load(ent->GetTransform()->GetPosition(World));
							i++;
						}
					}

					sumPos /= (float)i;

					Store(pivotPos, sumPos);
					pivotRot = primaryTrans->GetRotation(World);
					pivotScale = primaryTrans->GetScale(World);
					break;
				}

				case TransformOriginMode::None:
				default: {
					pivotRot = primaryTrans->GetRotation(World);
					if (hasCachedPivot)
					{
						pivotPos = cachedPivotPos;
						pivotScale = cachedPivotScale;
					}
					else
					{
						pivotPos = { 0, 0, 0 };
						pivotScale = { 1, 1, 1 };
					}
					break;
				}
			}

			/*if (hasCachedPivot)
			{
				pivotPos = cachedPivotPos;
				pivotScale = cachedPivotScale;
			}*/

			ImGuizmo::MODE mode = (space == World) ? ImGuizmo::MODE::WORLD : ImGuizmo::MODE::LOCAL;
			ImGuizmo::OPERATION operation = (ImGuizmo::OPERATION)0u;
			float *boundsPtr = nullptr;
			float bounds[6]{};

			XMFLOAT3 snap;
			XMFLOAT3 snapBounds;
			float *snapFloats = nullptr;
			float *snapBoundsFloats = nullptr;
			if (input.IsPressedOrHeld(KeyCode::LeftControl)) // Enable snap with Control
			{
				float snapFloat = DebugData::Get().transformSnap;

				snap = { snapFloat, snapFloat, snapFloat };
				snapBounds = { snapFloat, snapFloat, snapFloat };

				snapFloats = &snap.x;
				snapBoundsFloats = &snapBounds.x;
			}

			switch (transType)
			{
			case Translate:
				operation = ImGuizmo::OPERATION::TRANSLATE;
				break;

			case Rotate:
				operation = ImGuizmo::OPERATION::ROTATE;
				break;

			case Scale:
				operation = ImGuizmo::OPERATION::SCALE;
				break;

			case Universal:
				operation = ImGuizmo::OPERATION::UNIVERSAL;
				break;

			case Bounds:
			{
				operation = ImGuizmo::OPERATION::BOUNDS;

				XMFLOAT3A minBounds, maxBounds;

				if (originMode == TransformOriginMode::Center)
				{
					minBounds = { -1.0f, -1.0f, -1.0f };
					maxBounds = {  1.0f,  1.0f,  1.0f };
					pivotRot = { 0, 0, 0, 1 };
				}
				else
				{
					BoundingOrientedBox box{};
					primaryEnt->StoreEntityBounds(box, Local);

					Store(minBounds, Load(box.Center) - Load(box.Extents));
					Store(maxBounds, Load(box.Center) + Load(box.Extents));
				}

				std::memcpy(&(bounds[0]), &minBounds, sizeof(XMFLOAT3));
				std::memcpy(&(bounds[3]), &maxBounds, sizeof(XMFLOAT3));

				boundsPtr = bounds;
				break;
			}
			}

			XMVECTOR posVec = Load(pivotPos);
			XMVECTOR rotQuat = XMQuaternionNormalize(Load(pivotRot));
			XMVECTOR scaleVec = Load(pivotScale);

			XMMATRIX pivotMatrix = XMMatrixAffineTransformation(scaleVec, XMVectorZero(), rotQuat, posVec);
			XMFLOAT4X4A pivotMat;
			Store(pivotMat, pivotMatrix);

			XMFLOAT4X4A deltaMat{};

			ImGuizmo::SetRect(scenePosX, scenePosY, sceneWidth, sceneHeight);
			ImGuizmo::Manipulate(
				&(camView.m[0][0]), &(camProj.m[0][0]),
				operation, mode,
				(float *)(&pivotMat), (float *)(&deltaMat),
				snapFloats, boundsPtr, snapBoundsFloats
			);

			if (ImGuizmo::IsUsingAny())
			{
				XMMATRIX deltaMatrix = Load(deltaMat);

				if (operation == ImGuizmo::OPERATION::BOUNDS)
				{
					// ImGuizmo is kinda weird ngl and does not output a delta matrix for bounds specifically,
					// so we have to calculate it ourselves based on pivotMat

					XMMATRIX prevPivotMatrix = pivotMatrix;
					XMMATRIX newPivotMatrix = Load(pivotMat);

					// Calculate difference
					deltaMatrix = XMMatrixMultiply(XMMatrixInverse(nullptr, prevPivotMatrix), newPivotMatrix);
				}

				if (originMode == TransformOriginMode::Separate)
				{
					XMMATRIX conversionMatrix{};
					XMFLOAT3A posChange{}, scaleChange{};
					XMFLOAT4A rotChange{};

					{
						XMMATRIX entMatrix = pivotMatrix;
						XMMATRIX transformedMatrix = Load(pivotMat);

						// Calculate difference
						conversionMatrix = XMMatrixMultiply(XMMatrixInverse(nullptr, entMatrix), transformedMatrix);

						// Decompose
						XMVECTOR posChangeVec, rotChangeVec, scaleChangeVec;
						dx::XMMatrixDecompose(&scaleChangeVec, &rotChangeVec, &posChangeVec, conversionMatrix);

						// Ensure normalized quaternion
						rotChangeVec = XMQuaternionNormalize(rotChangeVec);

						Store(posChange, posChangeVec);
						Store(scaleChange, scaleChangeVec);
						Store(rotChange, rotChangeVec);
					}

					for (const auto &entRef : transformingEnts)
					{
						if (Entity *ent; entRef.TryGet(ent))
						{
							Transform *entTrans = ent->GetTransform();

							// Get current entity transform
							XMFLOAT3A entPos = entTrans->GetPosition(World);
							XMFLOAT4A entRot = entTrans->GetRotation(World);
							XMFLOAT3A entScale = entTrans->GetScale(World);

							XMVECTOR entPosVec = Load(entPos);
							XMVECTOR entRotQuat = XMQuaternionNormalize(Load(entRot));
							XMVECTOR entScaleVec = Load(entScale);

							// Create entity matrix
							XMMATRIX entMatrix = XMMatrixAffineTransformation(entScaleVec, XMVectorZero(), entRotQuat, entPosVec);

							// Apply delta transformation
							XMMATRIX transformedMatrix = XMMatrixMultiply(entMatrix, conversionMatrix);

							// Decompose and apply to entity
							XMVECTOR finalScale, finalRotQuat, finalTrans;
							dx::XMMatrixDecompose(&finalScale, &finalRotQuat, &finalTrans, transformedMatrix);

							XMFLOAT3A finalPos, finalScaleFloat;
							XMFLOAT4A finalRotFloat;

							Store(finalPos, finalTrans);
							Store(finalScaleFloat, finalScale);
							Store(finalRotFloat, finalRotQuat);

							entTrans->SetPosition(finalPos, World);
							entTrans->SetScale(finalScaleFloat, World);
							entTrans->SetRotation(finalRotFloat, World);
						}
					}
				}
				else
				{
					// For other modes, apply delta matrix to each entity
					for (const auto &entRef : transformingEnts)
					{
						if (Entity *ent; entRef.TryGet(ent))
						{
							Transform *entTrans = ent->GetTransform();
							
							// Get current entity transform
							XMFLOAT3A entPos = entTrans->GetPosition(World);
							XMFLOAT4A entRot = entTrans->GetRotation(World);
							XMFLOAT3A entScale = entTrans->GetScale(World);
							
							XMVECTOR entPosVec = Load(entPos);
							XMVECTOR entRotQuat = XMQuaternionNormalize(Load(entRot));
							XMVECTOR entScaleVec = Load(entScale);
							
							// Create entity matrix
							XMMATRIX entMatrix = XMMatrixAffineTransformation(entScaleVec, XMVectorZero(), entRotQuat, entPosVec);
							
							// Apply delta transformation
							XMMATRIX transformedMatrix = XMMatrixMultiply(entMatrix, deltaMatrix);
							
							// Decompose and apply to entity
							XMVECTOR finalScale, finalRotQuat, finalTrans;
							dx::XMMatrixDecompose(&finalScale, &finalRotQuat, &finalTrans, transformedMatrix);
							
							XMFLOAT3A finalPos, finalScaleFloat;
							XMFLOAT4A finalRotFloat;
							
							Store(finalPos, finalTrans);
							Store(finalScaleFloat, finalScale);
							Store(finalRotFloat, finalRotQuat);
							
							entTrans->SetPosition(finalPos, World);
							entTrans->SetScale(finalScaleFloat, World);
							entTrans->SetRotation(finalRotFloat, World);
						}
					}
				}

				// Update cached position for modes with moving pivots
				XMMATRIX newMat = Load(pivotMat);
				XMVECTOR newScale, newRotQuat, newTrans;
				dx::XMMatrixDecompose(&newScale, &newRotQuat, &newTrans, newMat);

				Store(cachedPivotPos, newTrans);
				Store(cachedPivotScale, newScale);
				hasCachedPivot = true;
			}
			else
			{
				hasCachedPivot = false;
			}
		}
	}

	// Camera orientation gizmo
	if (DebugData::Get().showViewManipGizmo && _viewCamera)
	{
		Transform *camTransform = _viewCamera.Get()->GetTransform();

		XMFLOAT3A
			camPos = camTransform->GetPosition(World),
			camDir = camTransform->GetForward(World);

		XMFLOAT3A r, u, f;
		camTransform->GetAxes(&r, &u, &f, World);

		XMFLOAT4X4A camView;
		{
			XMVECTOR
				posVec = Load(camTransform->GetPosition(World)),
				up = Load(u),
				forward = -Load(f);

			XMMATRIX viewMatrix = XMMatrixLookAtLH(
				posVec,
				posVec + forward,
				up
			);

			Store(camView, viewMatrix);
		}

		// The pivot distance is only needed when interacting with the gizmo, and is only calculated when needed.
		// It is calculated one frame after the gizmo is first interacted with, and is sustained until the interaction stops.
		static float pivotDist = FLT_MAX;
		static bool hasPivotDist = false;
		static bool needsPivotDist = false;

		if (needsPivotDist && !hasPivotDist)
		{
			// Get distance to pivot
			pivotDist = FLT_MAX;
			bool foundDist = false;

			RaycastOut out;
			if (_sceneHolder.RaycastScene(camPos, camDir, out, false))
			{
				pivotDist = out.distance;
				foundDist = true;
			}

			if (!foundDist)
				pivotDist = 10.0f;

			hasPivotDist = true;
		}

		ImGuizmo::ViewManipulate(&(camView.m[0][0]), pivotDist, ImVec2(sceneWidth + scenePosX - 16 - 96, scenePosY + 16), ImVec2(96, 96), 0x10101010);

		bool isUsingViewManip = ImGuizmo::IsUsingViewManipulate();
		if (isUsingViewManip)
		{
			if (hasPivotDist)
			{
				// Draw dot at pivot point
				{
					XMFLOAT3A pivot;
					Store(pivot, Load(camPos) + (Load(camDir) * pivotDist));
					DebugDrawer::Instance().DrawRay(pivot, { 0, 0.1f, 0 }, 0.15f, { 1,1,1,1 });
				}

				XMMATRIX view = Load(camView);
				XMMATRIX viewInv = XMMatrixInverse(nullptr, view);

				// Invert forward vector
				XMVECTOR
					right = viewInv.r[0],
					up = viewInv.r[1],
					forward = viewInv.r[2];

				viewInv.r[0] = -right;
				viewInv.r[1] = up;
				viewInv.r[2] = -forward;

				XMVECTOR camPos, camOrient, camScale;
				dx::XMMatrixDecompose(&camScale, &camOrient, &camPos, viewInv);

				XMFLOAT3A pos;
				Store(pos, camPos);

				XMFLOAT4A orient;
				Store(orient, camOrient);

				camTransform->SetPosition(pos, World);
				camTransform->SetRotation(orient, World);

				// Zoom based on mouse scroll
				MouseState mState = input.GetMouse();
				float scroll = mState.scroll.y * -0.1f;

				if (scroll != 0.0f)
				{
					float prevDist = pivotDist;
					pivotDist *= 1.0f + scroll;

					float distChange = -(pivotDist - prevDist);

					camTransform->MoveRelative({ 0, 0, distChange }, World);
				}

				needsPivotDist = false;
			}
			else
			{
				needsPivotDist = true;
			}
		}
		else
		{
			hasPivotDist = false;
			needsPivotDist = false;
		}

		if (!input.GetMouseAbsorbed())
		{
			if (isUsingViewManip || ImGuizmo::IsViewManipulateHovered() ||
				ImGuizmo::IsOver() || ImGuizmo::IsUsingAny())
			{
				input.SetMouseAbsorbed(true);
			}
		}
	}

	return true;
}
#endif