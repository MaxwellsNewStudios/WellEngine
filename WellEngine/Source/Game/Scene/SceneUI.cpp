#pragma region Includes, Usings & Defines
#include "stdafx.h"
#include "Scene.h"
#include "../Game.h"
#include "../Behaviours/Debug/B_DebugManager.h"
#include "../Behaviours/Rendering/Mesh/B_Mesh.h"
#include "Engine/Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion

#ifdef USE_IMGUI
using namespace ImGui;

bool Scene::RenderEntityHierarchyUI(Entity *root, UINT depth, bool skipCulling, const std::string &search)
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

	static ImVec4 dropSeparatorColor = (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.8f, 0.3f);

	float frameHeight = GetFrameHeight();
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

	BeginGroup();

	float lastHeight = 0.0f;
	if (root->IsVisibleInHierarchy(lastHeight) || skipCulling)
	{
		float indentedXPos = GetCursorPosX() + indenting;
		SetCursorPosX(indentedXPos);
		PushID(("Ent:" + std::to_string(entID)).c_str());

		bool hasVisibleChild = false;
		if (root->GetChildCount() > 0)
		{
			for (const Entity *const &child : *root->GetChildren())
			{
				if (!child->GetShowInHierarchy())
					continue;

				hasVisibleChild = true;
				break;
			}
		}

		// Collapse arrow
		{
			ImVec2 originalCursorPos = GetCursorPos();
			Dummy({ arrowSize.x, frameHeight });
			SameLine(0.0f, 2.0f);

			if (hasVisibleChild)
			{
				ImVec2 arrowCursorPos = originalCursorPos;
				arrowCursorPos.y += 0.5f * (frameHeight - arrowSize.y);

				ImVec2 afterCursorPos = GetCursorPos();
				SetCursorPos(arrowCursorPos);
				if (isCollapsed)
				{
					if (ArrowButtonEx("Expand", ImGuiDir_Right, arrowSize, ImGuiButtonFlags_None))
					{
						isCollapsed = false;
						_collapsedEntities.erase(_collapsedEntities.begin() + collapseIndex);
					}
				}
				else
				{
					if (ArrowButtonEx("Collapse", ImGuiDir_Down, arrowSize, ImGuiButtonFlags_None))
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
				SetCursorPos(afterCursorPos);

				if (!_isHoveringHierarchyItem)
					_isHoveringHierarchyItem = IsItemHovered();
			}
		}

		float entityButtonPosX = 0.0f;

		// Entity selection button
		{
			bool isSelected = false;
			if (_debugManager.IsValid())
				isSelected = _debugManager.Get()->IsSelected(root, nullptr);

			if (isSelected)
			{
				PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.1f, 0.55f, 0.5f));
				PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.1f, 0.65f, 0.6f));
				PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.1f, 0.75f, 0.7f));
			}

			SameLine(0.0f, 3.0f);
			entityButtonPosX = GetCursorPosX();
			if (Button(std::format("{}", entName).c_str()))
			{
				// Additive select if holding shift
				// Deselect if already selected or holding ctrl
				if (_debugManager.IsValid())
				{
					bool shiftHeld = GetIO().KeyShift;
					bool ctrlHeld = GetIO().KeyCtrl;

					if (!isSelected && !ctrlHeld)
						_debugManager.Get()->Select(root, shiftHeld);
					else if (isSelected && !shiftHeld)
						_debugManager.Get()->Deselect(root);
				}
			}

			if (isSelected)
			{
				PopStyleColor(3);
			}

			if (!_isHoveringHierarchyItem)
				_isHoveringHierarchyItem = IsItemHovered();
		}

		// Entity Drag & Drop field
		{
			const char *dragDropTag = PayloadTags.at(PayloadType::ENTITY);

			if (BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				EntityPayload payload = { entID, GetUID() };
				SetDragDropPayload(dragDropTag, &payload, sizeof(EntityPayload));

				Text(std::format("{}", entName).c_str());

				// Display preview (could be anything, e.g. when dragging an image we could decide to display
				// the filename and a small preview of the image, etc.)
				EndDragDropSource();
			}

			if (BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = AcceptDragDropPayload(dragDropTag))
				{
					IM_ASSERT(payload->DataSize == sizeof(EntityPayload));
					EntityPayload entPayload = *(const EntityPayload *)payload->Data;

					Entity *payloadEnt = nullptr;

					if (entPayload.sceneUID != GetUID())
					{
						// Dragging from another scene
						Scene *payloadScene = _game->GetSceneByUID(entPayload.sceneUID);

						if (!payloadScene)
						{
							ErrMsg("Failed to get scene from payload!");
							EndDragDropTarget();
							PopID();
							EndGroup();
							return false;
						}

						Entity *payloadEnt = payloadScene->_sceneHolder.GetEntityByID(entPayload.entID);

						json::Document doc;
						json::Value entObj = json::Value(json::kObjectType);
						
						if (!payloadEnt->Serialize(doc.GetAllocator(), entObj, true))
						{
							ErrMsg("Failed to serialize entity from payload!");
							EndDragDropTarget();
							PopID();
							return false;
						}

						if (!payloadScene->_sceneHolder.RemoveEntityImmediate(payloadEnt))
						{
							ErrMsg("Failed to remove entity from payload scene!");
							EndDragDropTarget();
							PopID();
							return false;
						}

						payloadEnt = nullptr;
						if (!DeserializeEntity(entObj, &payloadEnt))
						{
							ErrMsg("Failed to deserialize entity from payload!");
							EndDragDropTarget();
							PopID();
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
				EndDragDropTarget();
			}
		}

		// Right-click Context Menu
		{
			std::string ctxID = std::format("EntCtxMenu:{}:{}", entID, GetUID());

			if (IsMouseReleased(ImGuiMouseButton_Right) && IsItemHovered())
				OpenPopup(ctxID.c_str());

			if (BeginPopupContextItem(ctxID.c_str()))
			{
				if (!root->UIContextMenu())
				{
					EndPopup();
					PopID();
					EndGroup();
					ErrMsg("Entity context menu failed!");
					return false;
				}

				EndPopup();
			}			
		}

		if (root->IsPrefab())
		{
			SameLine();
			SetCursorPosY(GetCursorPosY() - 2.0f);
			ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_FAR, 14.0f);
			Text(ICON_FA_FILE_POWERPOINT);
			ImGuiUtils::EndFont();

			if (IsItemHovered())
			{
				SetTooltip("'%s' Prefab Instance", root->GetPrefabName().c_str());
				_isHoveringHierarchyItem = true;
			}
		}

		if (!root->GetShowInHierarchy(true))
		{
			SameLine();
			ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_FAR, 12.0f);
			Text(ICON_FA_EYE_SLASH);
			ImGuiUtils::EndFont();

			if (IsItemHovered())
			{
				SetTooltip("Hidden");
				_isHoveringHierarchyItem = true;
			}
		}

		if (!root->IsSerializable())
		{
			SameLine();
			ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_LC, 14.0f);
			Text(ICON_LC_PEN_OFF);
			ImGuiUtils::EndFont();

			if (IsItemHovered())
			{
				SetTooltip("Non-Serialized");
				_isHoveringHierarchyItem = true;
			}
		}

		float rightEdgeX = GetContentRegionAvail().x - 6.0f;

		static ImVec2 buttonSize = ImVec2(20, 20);
		const ImVec2 buttonTextPadding = GetStyle().FramePadding - ImVec2(1.0f, 0.5f);

		// Dock/Undock button
		{
			SameLine(rightEdgeX - 10.0f - frameHeight - (buttonSize.x * 2));

			PushID(("Dock:" + std::to_string(entID)).c_str());
			const std::string windowID = std::format("Ent#{}:{}", entID, GetUID());

			// Check if entity is undocked
			if (ImGuiUtils::Utils::GetWindow(windowID, nullptr))
			{
				PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.05f, 0.55f, 0.5f));
				PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.05f, 0.65f, 0.6f));
				PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.05f, 0.75f, 0.7f));

				PushStyleVar(ImGuiStyleVar_FramePadding, buttonTextPadding);

				// If undocked, show dock button
				ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_LC, 14.0f);
				if (Button(ICON_LC_SQUARE_ARROW_OUT_DOWN_LEFT, buttonSize))
				{
					if (!ImGuiUtils::Utils::CloseWindow(windowID))
					{
						ImGuiUtils::EndFont();
						PopStyleVar();
						PopStyleColor(3);
						PopID();
						EndGroup();
						EndGroup();
						ErrMsg("Failed to dock entity window!");
						return false;
					}
				}
				ImGuiUtils::EndFont();

				PopStyleVar();

				if (IsItemHovered())
				{
					SetTooltip("Dock Entity Window");
					_isHoveringHierarchyItem = true;
				}
			}
			else
			{
				PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.6f, 0.55f, 0.5f));
				PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.6f, 0.65f, 0.6f));
				PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.6f, 0.75f, 0.7f));

				PushStyleVar(ImGuiStyleVar_FramePadding, buttonTextPadding);

				// If docked, show undock button
				ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_LC, 14.0f);
				if (Button(ICON_LC_SQUARE_ARROW_OUT_UP_RIGHT, buttonSize))
				{
					SameLine();
					ImRect rect = {
						GetCursorScreenPos(), 
						GetWindowSize()
					};
					NewLine();

					rect.Max.x = MAX(100.0f, rect.Max.x * 0.75f - 50.0f);
					rect.Max.y = MAX(150.0f, rect.Max.y * 0.75f - 50.0f);

					rect.Min.x -= rect.Max.x;

					const std::string windowName = std::format("Entity '{}'", root->GetName());
					if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, std::bind(&Entity::InitialRenderUI, root), rect))
					{
						ImGuiUtils::EndFont();
						PopStyleColor(3);
						PopID();
						ErrMsg("Failed to undock entity window!");
						return false;
					}
				}
				ImGuiUtils::EndFont();

				PopStyleVar();

				if (IsItemHovered())
				{
					SetTooltip("Undock Entity Window");
					_isHoveringHierarchyItem = true;
				}
			}

			PopStyleColor(3);
			PopID();
		}

		// Enabled checkbox
		{
			SameLine(rightEdgeX - 5.0f - frameHeight - buttonSize.x);

			bool isEnabled = root->IsEnabledSelf();
			if (Checkbox("##Enabled", &isEnabled))
				root->SetEnabledSelf(isEnabled);

			buttonSize = GetItemRectSize();

			if (!_isHoveringHierarchyItem)
				_isHoveringHierarchyItem = IsItemHovered();
		}

		// Remove button
		{
			SameLine(rightEdgeX - buttonSize.x);

			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_LC, 14.0f);
			PushStyleVar(ImGuiStyleVar_FramePadding, buttonTextPadding);
			if (Button(ICON_LC_X "##RemoveEnt", buttonSize))
				root->Destroy();
			PopStyleVar();
			ImGuiUtils::EndFont();
			ImGuiUtils::EndButtonStyle();

			if (!_isHoveringHierarchyItem)
				_isHoveringHierarchyItem = IsItemHovered();
		}

		// Recurse Children
		if (hasVisibleChild && !isCollapsed)
		{
			const std::vector<Entity *> *currChildren = root->GetChildren();
			std::vector<Ref<Entity>> tempChildrenVec;
			tempChildrenVec.reserve(currChildren->size());

			for (Entity *child : *currChildren)
				tempChildrenVec.emplace_back(*child);

			// Child drop field
			if (!currChildren->empty())
			{
				float innerIndenting = (depth + 1) * frameHeight * arrowScale;
				float addedIndenting = innerIndenting - indenting;

				// Set vertical spacing
				float dummyDropTargetPosY = GetCursorPosY() - 3.0f;
				SetCursorPos({ entityButtonPosX + addedIndenting, dummyDropTargetPosY });
				Dummy({ GetContentRegionAvail().x, 5.0f });
				float dummyDropTargetHeight = GetItemRectSize().y;
				ImVec2 nextCursorPos = GetCursorPos();

				if (BeginDragDropTarget())
				{
					if (const ImGuiPayload *payload = AcceptDragDropPayload(PayloadTags.at(PayloadType::ENTITY)))
					{
						IM_ASSERT(payload->DataSize == sizeof(EntityPayload));
						EntityPayload entPayload = *(const EntityPayload *)payload->Data;

						Entity *payloadEnt = nullptr;

						if (entPayload.sceneUID != GetUID())
						{
							// Dragging from another scene
							Scene *payloadScene = _game->GetSceneByUID(entPayload.sceneUID);

							if (!payloadScene)
							{
								ErrMsg("Failed to get scene from payload!");
								EndDragDropTarget();
								PopID();
								return false;
							}

							Entity *payloadEnt = payloadScene->_sceneHolder.GetEntityByID(entPayload.entID);

							json::Document doc;
							json::Value entObj = json::Value(json::kObjectType);
							
							if (!payloadEnt->Serialize(doc.GetAllocator(), entObj, true))
							{
								ErrMsg("Failed to serialize entity from payload!");
								EndDragDropTarget();
								PopID();
								return false;
							}

							if (!payloadScene->_sceneHolder.RemoveEntityImmediate(payloadEnt))
							{
								ErrMsg("Failed to remove entity from payload scene!");
								EndDragDropTarget();
								PopID();
								return false;
							}

							payloadEnt = nullptr;
							if (!DeserializeEntity(entObj, &payloadEnt))
							{
								ErrMsg("Failed to deserialize entity from payload!");
								EndDragDropTarget();
								PopID();
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
							if (!root->IsChildOf(payloadEnt))
							{
								payloadEnt->SetParent(root, true);
							}

							root->ReorderChild(payloadEnt, 0u);
							_sceneHolder.ReorderEntity(payloadEnt, root);
						}
					}
					EndDragDropTarget();
				}

				SameLine(entityButtonPosX + addedIndenting, 0.0f);
				SetCursorPosY(dummyDropTargetPosY + (0.5f * dummyDropTargetHeight));
				PushStyleColor(ImGuiCol_Separator, dropSeparatorColor);
				Separator();
				PopStyleColor();

				SetCursorPos(nextCursorPos);
				Dummy({ 0, 0 });
				SameLine(0.0f, 0.0f);
			}

			for (Ref<Entity> &childRef : tempChildrenVec)
			{
				Entity *child = nullptr;
				if (!childRef.TryGet(child))
					continue;

				if (!child->IsChildOf(root, true))
					continue; // Skip if no longer a child of this parent

				if (!RenderEntityHierarchyUI(child, depth + 1, skipCulling))
				{
					ErrMsg("Failed to render entity hierarchy UI!");
					return false;
				}
			}
		}

		PopID();

		// Sibling drop field
		{
			// Set vertical spacing
			float dummyDropTargetPosY = GetCursorPosY() - 3.0f;
			SetCursorPos({ entityButtonPosX, dummyDropTargetPosY });
			Dummy({ GetContentRegionAvail().x, 5.0f });
			float dummyDropTargetHeight = GetItemRectSize().y;
			ImVec2 nextCursorPos = GetCursorPos();
			nextCursorPos.y -= 5.0f;

			if (BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = AcceptDragDropPayload(PayloadTags.at(PayloadType::ENTITY)))
				{
					IM_ASSERT(payload->DataSize == sizeof(EntityPayload));
					EntityPayload entPayload = *(const EntityPayload *)payload->Data;

					Entity *payloadEnt = nullptr;

					if (entPayload.sceneUID != GetUID())
					{
						// Dragging from another scene
						Scene *payloadScene = _game->GetSceneByUID(entPayload.sceneUID);

						if (!payloadScene)
						{
							ErrMsg("Failed to get scene from payload!");
							EndDragDropTarget();
							PopID();
							return false;
						}

						Entity *payloadEnt = payloadScene->_sceneHolder.GetEntityByID(entPayload.entID);

						json::Document doc;
						json::Value entObj = json::Value(json::kObjectType);
						
						if (!payloadEnt->Serialize(doc.GetAllocator(), entObj, true))
						{
							ErrMsg("Failed to serialize entity from payload!");
							EndDragDropTarget();
							PopID();
							return false;
						}

						if (!payloadScene->_sceneHolder.RemoveEntityImmediate(payloadEnt))
						{
							ErrMsg("Failed to remove entity from payload scene!");
							EndDragDropTarget();
							PopID();
							return false;
						}

						payloadEnt = nullptr;
						if (!DeserializeEntity(entObj, &payloadEnt))
						{
							ErrMsg("Failed to deserialize entity from payload!");
							EndDragDropTarget();
							PopID();
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
						{
							skipParenting = rootParent->IsChildOf(payloadEnt);
						}

						if (!skipParenting)
						{
							payloadEnt->SetParent(rootParent, true);
						}

						if (rootParent)
						{
							rootParent->ReorderChild(payloadEnt, root);
						}

						_sceneHolder.ReorderEntity(payloadEnt, root);
					}
				}
				EndDragDropTarget();
			}

			SameLine(entityButtonPosX, 0.0f);
			SetCursorPosY(dummyDropTargetPosY + (0.5f * dummyDropTargetHeight));
			PushStyleColor(ImGuiCol_Separator, dropSeparatorColor);
			Separator();
			PopStyleColor();

			SetCursorPos(nextCursorPos);
			Dummy({ 0, 0 });
			SameLine(0.0f, 0.0f);
		}
	}
	else
	{
		// If not visible, just add a dummy to keep the same spacing
		Dummy({ GetContentRegionAvail().x, MAX(lastHeight, frameHeight)});
	}

	EndGroup();

	if (!skipCulling)
		root->SetVisibleInHierarchy(IsItemVisible(), GetItemRectSize().y);

	return true;
}

bool Scene::RenderSceneHierarchyUI(bool skipCulling)
{
	static std::string search = "";

	float padding = GetStyle().WindowPadding.x;
	float inputBoxPosX = GetCursorPosX();

	SetNextItemWidth(GetContentRegionAvail().x);
	InputText("##HierarchySearch", &search, ImGuiInputTextFlags_AutoSelectAll);
	if (!IsItemActive() && search.empty())
	{
		SameLine(inputBoxPosX + padding);
		TextDisabled("Name Search");
	}

	std::string searchLower = search;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

	SetNextWindowSize(GetContentRegionAvail(), ImGuiCond_Always);
	BeginChild("Scene Hierarchy");
	{
		if (!_isHoveringHierarchy)
			_isHoveringHierarchy = IsWindowHovered();

		SceneContents::SceneIterator entIter = _sceneHolder.GetEntities();

		Dummy({ 0, 0 });

		while (Entity *entity = entIter.Step())
		{
			if (entity->GetParent() != nullptr) // Skip non-root entities
				continue;

			if (!RenderEntityHierarchyUI(entity, 0, skipCulling, searchLower))
			{
				EndChild();
				return false;
			}
		}
	}
	EndChild();

	return true;
}

bool Scene::RenderSelectionHierarchyUI(bool skipCulling)
{
	static std::string search = "";

	float padding = GetStyle().WindowPadding.x;
	float inputBoxPosX = GetCursorPosX();

	SetNextItemWidth(GetContentRegionAvail().x);
	InputText("##SelectionSearch", &search, ImGuiInputTextFlags_AutoSelectAll);
	if (!IsItemActive() && search.empty())
	{
		SameLine(inputBoxPosX + padding);
		TextDisabled("Name Search");
	}

	std::string searchLower = search;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

	SetNextWindowSize(GetContentRegionAvail(), ImGuiCond_Always);
	BeginChild("Selection Hierarchy");
	{
		if (!_isHoveringHierarchy)
			_isHoveringHierarchy = IsWindowHovered();

		auto &selection = _debugManager.Get()->GetSelection();

		Dummy({ 0, 0 });

		for (int i = 0; i < selection.size(); i++)
		{
			if (Entity *ent = selection[i].Get())
			{
				if (!RenderEntityHierarchyUI(ent, 0, skipCulling, searchLower))
				{
					EndChild();
					return false;
				}
			}
		}
	}
	EndChild();

	return true;
}

bool Scene::RenderHierarchyMenuBarUI()
{
	if (ImGui::BeginMenu("Settings"))
	{
		ImGui::Text("Objects: %d", _sceneHolder.GetEntityCount());

		ImGui::Checkbox("Show Hidden", &DebugData::Get().hierarchyShowHidden);

		if (!ImGuiUtils::Utils::GetWindow(_sceneName + "Hierarchy", nullptr))
		{
			if (ImGui::Button("Undock Hierarchy"))
			{
				std::function<bool()> renderFunc = [this]() -> bool {
					return RenderHierarchyUI(true);
				};

				if (!ImGuiUtils::Utils::OpenWindow(std::format("'{}' Hierarchy", _sceneName), _sceneName + "Hierarchy", renderFunc))
				{
					ErrMsg("Failed to open scene window!");
					return false;
				}
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Keep this hierarchy open after\nswitching to another scene.");
			}
		}

		ImGui::EndMenu();
	}

	return true;
}

bool Scene::RenderHierarchyContextMenuUI()
{
	if (ImGui::MenuItem("New Entity"))
	{
		const dx::BoundingOrientedBox bounds = { {0,0,0}, {0.25f, 0.25f, 0.25f}, {0,0,0,1} };

		Entity* ent = nullptr;
		if (!CreateEntity(&ent, "New Entity", bounds, true))
			Warn("Failed to create new entity!");

		// If created successfully, select the new entity
		if (ent)
		{
			if (_debugManager.IsValid())
				_debugManager.Get()->Select(ent, false);
		}
	}

	// Pasting
	{
		constexpr const char *entDataPrefix = "[[ENTITY_JSON]] ";
		std::string clipboardData = ImGui::GetClipboardText();

		// Check if clipboard data starts with behaviour data prefix
		if (clipboardData.rfind(entDataPrefix, 0) == 0)
		{
			if (ImGui::MenuItem("Paste##PasteEnt"))
			{
				clipboardData = clipboardData.substr(strlen(entDataPrefix)); // Remove prefix

				json::Document doc;
				doc.Parse(clipboardData.c_str());
				if (doc.HasParseError())
				{
					ErrMsg("Failed to parse entity JSON data!");
					return false;
				}

#pragma push_macro("GetObject")
#undef GetObject
				Entity *childEntity = nullptr;
				if (!DeserializeEntity(doc.GetObject(), &childEntity))
				{
					ErrMsg("Failed to deserialize entity!");
					return false;
				}
#pragma pop_macro("GetObject")

				RunPostDeserializeCallbacks();
			}
		}
		else
		{
			// No valid clipboard data for pasting
			ImGui::MenuItem("Paste##PasteNULL", nullptr, false, false);
		}
	}

	return true;
}

bool Scene::RenderHierarchyUI(bool skipCulling)
{
	if (!BeginTabBar("HierarchyTab"))
		return true;

	_isHoveringHierarchyItem = false;
	_isHoveringHierarchy = false;

	if (BeginTabItem(GetName().c_str()))
	{
		PushID("Scene Hierarchy");

		const std::string windowID = std::format("Scene'{}'", GetName());

		if (!RenderSceneHierarchyUI(skipCulling))
		{
			PopID();
			EndTabItem();
			ErrMsg("Failed to render scene hierarchy UI!");
			return false;
		}

		PopID();
		EndTabItem();
	}
	
	if (BeginTabItem("Selection"))
	{
		PushID("Selection Hierarchy");

		const std::string windowID = std::format("Selection'{}'", GetName());

		if (!RenderSelectionHierarchyUI(true))
		{
			PopID();
			EndTabItem();
			ErrMsg("Failed to render selection hierarchy UI!");
			return false;
		}

		PopID();
		EndTabItem();
	}

	EndTabBar();

	if (_debugManager.IsValid())
	{
		// Clear selection if user pressed and released left mouse button while hovering empty area of hierarchy.
		// Holding shift will prevent this, to prevent accidental deselection when trying to multi-select.
		static bool wasPressed = false;

		if (_isHoveringHierarchy && !_isHoveringHierarchyItem && !GetIO().KeyShift)
		{
			if (wasPressed)
			{
				if (IsMouseReleased(ImGuiMouseButton_Left))
				{
					wasPressed = false;
					_debugManager.Get()->ClearSelection();
				}
			}
			else
			{
				wasPressed = IsMouseClicked(ImGuiMouseButton_Left);
			}
		}

		if (wasPressed)
		{
			if (!IsMouseDown(ImGuiMouseButton_Left))
				wasPressed = false;
		}
	}

	// If right-clicking empty area of hierarchy, open general context menu
	if (_isHoveringHierarchy && !_isHoveringHierarchyItem)
	{
		if (IsMouseReleased(ImGuiMouseButton_Right))
			OpenPopup("GeneralCtxMenu");
	}

	if (BeginPopupContextItem("GeneralCtxMenu"))
	{
		if (!RenderHierarchyContextMenuUI())
		{
			EndPopup();
			ErrMsg("General context menu failed!");
			return false;
		}

		EndPopup();
	}

	return true;
}

bool Scene::RenderSelectionUI()
{
	PushID((_sceneName + "Selection").c_str());

	int selectionSize = 0;
	if (_debugManager.IsValid())
		selectionSize = (int)_debugManager.Get()->GetSelectionSize();

	if (selectionSize > 0)
	{
		static int selectionIndex = 1;

		if (selectionSize > 1)
		{
			PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
			{
				if (ArrowButton("##left", ImGuiDir_Left))
					selectionIndex--;
				SameLine(0.0f, 4.0f);
				if (ArrowButton("##right", ImGuiDir_Right))
					selectionIndex++;
				SameLine(0.0f, 8.0f);
			}
			PopItemFlag();

			float numWidth = CalcTextSize(std::to_string(selectionSize).c_str()).x;

			SetNextItemWidth(numWidth + 10.0f);
			DragInt("##selectionIndex", &selectionIndex, 0.1f);
			SameLine();
			Text("/ %d", selectionSize);

			Separator();
		}

		selectionIndex = Wrap(selectionIndex, 1, selectionSize);
		Entity *ent = _debugManager.Get()->GetSelection()[(size_t)selectionIndex - 1].Get();

		if (ent)
		{
			if (!ent->InitialRenderUI())
			{
				PopID();
				ErrMsg("Failed to render selected entity UI!");
				return false;
			}
		}
	}
	else
	{
		Text("No selection.");
	}

	PopID();

	return true;
}

bool Scene::RenderEntityCreatorUI()
{
	Entity *ent = nullptr;
	static bool positionWithCursor = false;
	Checkbox("Position Entity With Cursor", &positionWithCursor);

	ImVec2 buttonSize = { GetContentRegionAvail().x, 35.0f };

	// Empty entity creator
	{
		if (Button("Empty Entity", buttonSize))
			OpenPopup("Empty Entity Creator");

		if (BeginPopupModal("Empty Entity Creator", NULL))
		{
			static std::string entityName = "Empty Entity";

			// Set Entity Name
			Text("Entity Name:"); SameLine();
			InputText("##EntityName", &entityName, ImGuiInputTextFlags_AutoSelectAll);

			if (Button("Create", ImVec2(120, 0)))
			{
				// Create entity with given parameters.

				const dx::BoundingOrientedBox bounds = { {0,0,0}, {0.25f, 0.25f, 0.25f}, {0,0,0,1} };

				if (!CreateEntity(&ent, entityName, bounds, true))
					Warn("Failed to create empty entity!");

				CloseCurrentPopup();
			}
			SetItemDefaultFocus(); 
			
			SameLine();
			if (Button("Close", ImVec2(120, 0)))
				CloseCurrentPopup();

			EndPopup();
		}
	}

	// Mesh entity creator
	{
		if (Button("Mesh Entity", buttonSize))
			OpenPopup("Mesh Entity Creator");

		if (BeginPopupModal("Mesh Entity Creator", NULL))
		{
			static std::string entityName = "";

			// Set Entity Name
			BeginGroup();
			Text("Entity Name:"); SameLine();
			SetNextItemWidth(GetContentRegionAvail().x);
			InputText("##EntityName", &entityName, ImGuiInputTextFlags_AutoSelectAll);
			EndGroup();
			SetItemTooltip("Leave empty to use mesh name");

			static UINT meshID = 0;

			// Select Mesh
			{
				std::vector<std::string> meshNames;
				_content->GetMeshNames(&meshNames);

				ImGuiComboFlags comboFlags = ImGuiComboFlags_None;
				comboFlags |= ImGuiComboFlags_HeightLarge;

				Text("Mesh:"); SameLine();
				if (BeginCombo("##MeshCombo", _content->GetMeshName(meshID).c_str(), comboFlags))
				{
					static std::string filter = "";
					InputText("##MeshFilter", &filter, ImGuiInputTextFlags_AutoSelectAll);
					if (!IsItemActive() && filter.empty())
					{
						SameLine(8.0f);
						TextDisabled("Search");
					}

					if (!filter.empty())
						std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

					Separator();

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
						if (Selectable(meshNames[i].c_str(), isSelected))
							meshID = _content->GetMeshID(meshNames[i]);

						if (isSelected)
							SetItemDefaultFocus();
					}
					EndCombo();
				}
			}

			if (Button("Create", ImVec2(120, 0)))
			{
				Material mat{};
				mat.textureID = _content->GetTextureID("Fallback");

				// Create entity with given parameters.
				if (!CreateMeshEntity(&ent, entityName.empty() ? _content->GetMeshName(meshID) : entityName, meshID, mat))
					Warn("Failed to create mesh entity!");

				CloseCurrentPopup();
			}
			SetItemDefaultFocus(); 

			SameLine();
			if (Button("Close", ImVec2(120, 0)))
				CloseCurrentPopup();

			EndPopup();
		}
	}

	// Prefab creator
	{
		if (Button("Create Prefab", buttonSize))
			OpenPopup("Prefab Spawner");

		if (BeginPopupModal("Prefab Spawner", NULL))
		{
			std::vector<std::string> prefabs;
			GetPrefabNames(prefabs);

			static std::string selectedPrefab = "";

			Text("Selected: '%s'", selectedPrefab.c_str());
			Separator();

			// Search filter
			{
				static std::string search = "";
				if (Button("Clear"))
					search.clear();
				SameLine();

				if (InputText("##PrefabSearch", &search))
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

			if (BeginChild("Prefab List", ImVec2(GetContentRegionAvail().x, prefabListHeight), prefabChildFlags, windowFlags))
			{
				for (int i = 0; i < prefabs.size(); i++)
				{
					std::string &prefab = prefabs[i];

					if (Selectable(prefab.c_str(), selectedPrefab == prefab))
						selectedPrefab = std::move(prefab);
				}
			}
			EndChild();
			prefabListHeight = GetItemRectSize().y;

			Separator();

			if (Button("Spawn") && !selectedPrefab.empty())
			{
				ent = SpawnPrefab(selectedPrefab);

				if (!ent)
					WarnF("Failed to spawn prefab '{}'", selectedPrefab);

				CloseCurrentPopup();
			}

			static float cancelButtonWidth = 30.0f;
			SameLine(GetWindowContentRegionMax().x - cancelButtonWidth);

			if (Button("Cancel"))
				CloseCurrentPopup();
			cancelButtonWidth = GetItemRectSize().x;

			EndPopup();
		}
	}

	// Takes a file during runtime and creates a new mesh and entity from it.
	// HACK: Temporarily disabled 
	/*
	if (Button("Mesh from file", buttonSize))
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
						std::ofstream file(WE_DF(MESHES_PATH, MESHES_FILE), std::ios::app);
						std::string destPath = meshFolder + '\\' + fileName;
						std::filesystem::copy_file(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing);

						file << '\n' + name;
						meshName = name;
						if (!_content->AddMesh(_device, std::format("{}", name), WE_DFE(MESHES_PATH, name, "obj").c_str()))
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
							WE_DFE(TEXTURES_PATH, name, ext).c_str(),
							DXGI_FORMAT_R8G8B8A8_UNORM, false, true) == CONTENT_NULL)
						{
							ErrMsgF("Failed to add Tex {}!", name);
							return false;
						}

						std::ofstream file(WE_DF(TEXTURES_PATH, TEXTURES_FILE), std::ios::app);
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
		if (Button("Sound", ImVec2(200, 50)))
			OpenPopup("Sound Creator");

		if (BeginPopupModal("Sound Creator", NULL))
		{
			// Set entity parameters
			static char entityName[64] = "Sound Entity";
			{
				Text("Entity Name:");
				SameLine();
				InputText("##EntityName", entityName, sizeof(entityName));
			}
			static std::string soundName = "";
			static float volume = 1.0f;
			static float distanceScaler = 50.0f;
			static float reverbScaler = 1.0f;
			static bool loop = false;
			static bool foundFile = false;
			static int amountOfSounds = 1;

			{
				Text("Sound Name:");
				SameLine();
				if (InputText("##SoundName", &soundName))
				{
					std::string fileName = WE_DFE(SOUNDS_PATH, soundName, "wav");
					struct stat buffer;
					foundFile = stat(fileName.c_str(), &buffer) == 0;
				}
				SetItemTooltip(std::format("Name of the sound file you want to use, located in {}.", SOUNDS_PATH).c_str());

				if (Button("Browse"))
				{
					const char *filterPatterns[] = { "*.wav" };
					const char *selectedFiles = tinyfd_openFileDialog(
						"Open File",
						WE_DF(SOUNDS_PATH, "").c_str(),
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

							std::string fileName = WE_DFE(SOUNDS_PATH, soundName, "wav");
							struct stat buffer;
							foundFile = stat(fileName.c_str(), &buffer) == 0;
						}
					}
				}
			}

			DragFloat("Volume", &volume, 0.05f, 0.0f, 1.0f);
			DragFloat("Distance", &distanceScaler, 0.1f);
			DragFloat("Reverb", &reverbScaler, 0.1f);
			DragInt("Amount of Sounds", &amountOfSounds, 1, 1, 100);
			Checkbox("Loop", &loop);

			if (!foundFile)
			{
				BeginDisabled(true);
			}
			if (Button("Create", ImVec2(120, 0)))
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
						CloseCurrentPopup();
						EndPopup();
						EndChild();
						TreePop();
						return false;
					}
				}
				entityName[0] = '\0';
				CloseCurrentPopup();
			}
			if (!foundFile)
			{
				EndDisabled();
				SetItemTooltip("File could not be found.");
			}
			SetItemDefaultFocus();
			SameLine();
			if (Button("Close", ImVec2(120, 0)))
				CloseCurrentPopup();
			EndPopup();
		}
	}
	*/

	if (ent)
	{
		if (positionWithCursor)
		{
			_debugManager.Get()->ClearSelection();
			_debugManager.Get()->PositionWithCursor(ent);
		}
		else
		{
			SetSelection(ent, GetIO().KeyShift);
		}
	}

	return true;
}

bool Scene::RenderSceneUI()
{
	ZoneScopedC(RandomUniqueColor());
	using namespace dx;

	if (CollapsingHeader("Creation"))
	{
		if (!RenderEntityCreatorUI())
		{
			ErrMsg("Failed to render entity creator UI!");
			return false;
		}
	}

	if (CollapsingHeader("Settings"))
	{
		if (TreeNode("Shadows"))
		{
			Text("Pointlight Resolution:"); SameLine();
			static int pointlightRes = (int)_pointlights->GetShadowResolution();
			if (DragInt("##PointlightRes", &pointlightRes))
			{
				pointlightRes = MAX(1, pointlightRes);
				_pointlights->SetShadowResolution((UINT)pointlightRes);
			}

			Text("Spotlight Resolution:"); SameLine();
			static int spotlightRes = (int)_spotlights->GetShadowResolution();
			if (DragInt("##SpotlightRes", &spotlightRes))
			{
				spotlightRes = MAX(1, spotlightRes);
				_spotlights->SetShadowResolution((UINT)spotlightRes);
			}

			Separator();
			TreePop();
		}

		if (TreeNode("Scene Holder"))
		{
			if (!_sceneHolder.RenderUI(this))
			{
				TreePop();
				ErrMsg("Failed to render scene holder UI!");
				return false;
			}

			Separator();
			TreePop();
		}

		if (TreeNode("Audio"))
		{
			dx::AudioEngine *audioEngine = _soundEngine.GetAudioEngine();
			IXAudio2 *audioInterface = audioEngine->GetInterface();

			UINT outputChannels = audioEngine->GetOutputChannels();
			Text("Output Channels: %d", outputChannels);

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
				if (BeginCombo("Reverb Zone", reverbNames[(int)newReverb].c_str()))
				{
					for (int i = 0; i < reverbNames.size(); i++)
					{
						const bool isSelected = ((int)newReverb == i);
						if (Selectable(reverbNames[i].c_str(), isSelected))
						{
							newReverb = (dx::AUDIO_ENGINE_REVERB)i;
							audioEngine->SetReverb(newReverb);
						}

						if (isSelected)
							SetItemDefaultFocus();
					}
					EndCombo();
				}
			}

			Separator();
			TreePop();
		}
	}

	if (CollapsingHeader("Other"))
	{
		if (TreeNode("Fog Testing"))
		{
			const static auto calcStepSize = [](float currDist, float maxDist, int samplesLeft, int maxSamples, float sampleBias) -> float {
				float bias = sampleBias;
				float u = currDist / maxDist;
				float s = pow(u, 1.0f / bias);
				float remaining_s = 1.0f - s;
				float delta_s = remaining_s / (float)samplesLeft;
				float s_next = s + delta_s;
				float u_next = pow(s_next, bias);
				return maxDist * (u_next - u);
			};

			static Shape::Ray ray({ 0,0,0 }, { 0,0,1 }, 1);
			static Shape::RayHit hit;
			static bool doRecalc = false;

			static std::vector<dx::XMFLOAT3> samplePoints;

			if (DragFloat("Length##FogTests", &ray.length, 0.01f))
				doRecalc = true;
			ImGuiUtils::LockMouseOnActive();

			static int maxSamples = 0;
			if (DragInt("Max Samples##FogTests", &maxSamples, 1, 0, 1024))
				doRecalc = true;

			static float sampleBias = 1.0f;
			if (DragFloat("Sample Bias##FogTests", &sampleBias, 0.01f, 0.1f, 10.0f))
				doRecalc = true;
			ImGuiUtils::LockMouseOnActive();

			static float randomOffset = 0.0f;
			if (DragFloat("Random Offset##FogTests", &randomOffset, 0.01f, 0.0f, 1.0f))
				doRecalc = true;
			ImGuiUtils::LockMouseOnActive();

			static bool alwaysRecalc = false;
			Checkbox("Always Recalculate##FogTests", &alwaysRecalc);

			SeparatorText("Ray");
			{
				static bool followingCamera = true;
				Checkbox("Follow Camera##FogTests", &followingCamera);

				if (_mainCamera && followingCamera)
				{
					Transform *camT = _mainCamera.Get()->GetTransform();
					ray.origin = camT->GetPosition(World);
					ray.direction = camT->GetForward(World);
				}
				else
				{
					DragFloat3("Origin##FogTests", &ray.origin.x, 0.05f);
					ImGuiUtils::LockMouseOnActive();

					if (DragFloat3("Direction##FogTests", &ray.direction.x, 0.01f))
						Store(ray.direction, dx::XMVector3Normalize(Load(ray.direction)));
					ImGuiUtils::LockMouseOnActive();
				}
			}

			if (Button("Recalculate##FogTests") || doRecalc || alwaysRecalc)
			{
				doRecalc = false;
				Entity *_ = nullptr;
				hit = {};
				bool didHit = _sceneHolder.RaycastScene(ray, hit, _);

				samplePoints.clear();
				float currDist = 0.0f;
				for (int i = 0; i < maxSamples; i++)
				{
					float stepSize = calcStepSize(currDist, didHit ? hit.length : ray.length, maxSamples - i, maxSamples, sampleBias);

					float randomFactor = 1.0f;
					if (randomOffset > 0.0f)
						randomFactor = RandomFloat(1.0f - randomOffset, 1.0f);

					dx::XMFLOAT3 samplePoint;
					Store(samplePoint, Load(ray.origin) + Load(ray.direction) * (currDist + stepSize * randomFactor));
					samplePoints.push_back(samplePoint);

					currDist += stepSize;
				}
			}

			SeparatorText("Drawing");
			{
				DebugDrawer &drawer = DebugDrawer::Instance();

				static bool drawRay = true;
				Checkbox("Draw Ray##FogTests", &drawRay);
				if (drawRay)
				{
					dx::XMFLOAT3 scaledDir{};
					Store(scaledDir, Load(ray.direction) * hit.length);
					drawer.DrawRay(ray.origin, scaledDir, 0.01f, { 1, 1, 1, 0.25f });
				}

				static bool drawSamples = true;
				Checkbox("Draw Samples##FogTests", &drawSamples);
				if (drawSamples)
				{
					for (const auto &point : samplePoints)
					{
						drawer.DrawSphere(point, 0.025f, 1, { 0, 1, 0, 0.5f });
					}
				}
			}

			Separator();
			TreePop();
		}
		
		if (TreeNode("Raycast Tests"))
		{
			static Shape::Ray ray({ 0,0,0 }, { 0,0,1 }, 1);
			Shape::RayHit hit;
			bool didHit = false;
			Entity *hitEntity = nullptr;
			static Ref<Entity> originEntity = nullptr;

			SeparatorText("Ray");
			{
				if (Button(std::format("Track Entity: {}", originEntity.IsValid() ? originEntity.Get()->GetName() : "None").c_str()))
					_debugManager.Get()->Select(originEntity.Get(), false);
				if (BeginDragDropTarget())
				{
					if (const ImGuiPayload *payload = AcceptDragDropPayload(PayloadTags.at(PayloadType::ENTITY)))
					{
						IM_ASSERT(payload->DataSize == sizeof(EntityPayload));
						EntityPayload entPayload = *(const EntityPayload *)payload->Data;

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
					EndDragDropTarget();
				}

				if (originEntity.IsValid())
				{
					Transform *originEntT = originEntity.Get()->GetTransform();
					ray.origin = originEntT->GetPosition(World);
					ray.direction = originEntT->GetForward(World);

					ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
					SameLine();
					if (Button("X##RaycastTests"))
					{
						originEntity = nullptr;
					}
					ImGuiUtils::EndButtonStyle();
				}
				else
				{
					DragFloat3("Origin##RaycastTests", &ray.origin.x, 0.05f);
					ImGuiUtils::LockMouseOnActive();

					if (DragFloat3("Direction##RaycastTests", &ray.direction.x, 0.01f))
						Store(ray.direction, dx::XMVector3Normalize(Load(ray.direction)));
					ImGuiUtils::LockMouseOnActive();

					if (_mainCamera && Button("Move Ray to View##RaycastTests"))
					{
						Transform *camT = _mainCamera.Get()->GetTransform();
						ray.origin = camT->GetPosition(World);
						ray.direction = camT->GetForward(World);
					}
				}

				DragFloat("Length##RaycastTests", &ray.length, 0.01f);
				ImGuiUtils::LockMouseOnActive();

				didHit = _sceneHolder.RaycastScene(ray, hit, hitEntity);
			}

			SeparatorText("Drawing");
			{
				static bool drawRay = true;
				Checkbox("Draw Ray##RaycastTests", &drawRay);
				if (drawRay)
				{
					static bool overlayRay = false;
					Checkbox("Overlay Ray##RaycastTests", &overlayRay);

					dx::XMFLOAT3 scaledDir;
					Store(scaledDir, Load(ray.direction) * ray.length);
					DebugDrawer::Instance().DrawRay(ray.origin, scaledDir, 0.1f, { 1, 1, 0, 0.5f }, !overlayRay);
				}

				static bool drawHit = true;
				Checkbox("Draw Hit##RaycastTests", &drawHit);
				if (drawHit)
				{
					static bool overlayHit = false;
					Checkbox("Overlay Hit##RaycastTests", &overlayHit);

					if (didHit)
					{
						DebugDrawer::Instance().DrawLine(ray.origin, hit.point, 0.2f, { 1, 0, 0, 0.5f }, !overlayHit);
						DebugDrawer::Instance().DrawRay(hit.point, hit.normal, 0.075f, { 0, 0, 1, 0.5f }, !overlayHit);
					}
				}
			}

			SeparatorText("Data");
			{
				Text(std::format("Ray Origin:    ({}, {}, {})", ray.origin.x, ray.origin.y, ray.origin.z).c_str());
				Text(std::format("Ray Direction: ({}, {}, {})", ray.direction.x, ray.direction.y, ray.direction.z).c_str());
				Text(std::format("Ray Length:	{}", ray.length).c_str());

				if (didHit)
				{
					Text(std::format("Hit Point:		({}, {}, {})", hit.point.x, hit.point.y, hit.point.z).c_str());
					Text(std::format("Hit Normal:	({}, {}, {})", hit.normal.x, hit.normal.y, hit.normal.z).c_str());
					Text(std::format("Hit Length:	{}", hit.length).c_str());

					if (hitEntity)
					{
						const std::string &entName = hitEntity->GetName();
						UINT entID = hitEntity->GetID();

						Text("Hit Entity:	");
						SameLine();
						if (Button(std::format("{}##RayHitEntID{}", entName, entID).c_str()))
							_debugManager.Get()->Select(hitEntity);
					}
					else
						Text("Hit Entity:	None");
				}
				else
					Text("No Hit.");
			}

			Separator();
			TreePop();
		}

		if (TreeNode("Ref Tests"))
		{
			PushID("RefTests");

			static Ref<Behaviour> meshRef = nullptr;
			static std::vector<std::pair<std::string, Ref<Entity>>> entityRefs;

			if (Entity *ent = _debugManager.Get()->GetPrimarySelection())
			{
				const std::string &name = ent->GetName();
				Text("Selected Entity: %s", name.c_str());
				Text("Active References %d", ent->GetRefs().size());

				if (Button("Add Reference to Selected"))
					entityRefs.emplace_back(std::make_pair(name, ent->AsRef()));

				B_Mesh *mesh = nullptr;
				if (ent->GetBehaviourByType<B_Mesh>(mesh))
				{
					if (Button("Add Reference to Mesh"))
						meshRef = mesh;
				}
			}

			BeginChild("Reference Testing", ImVec2(0, 200), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);
			for (int i = 0; i < entityRefs.size(); i++)
			{
				PushID(i);

				auto &[name, ref] = entityRefs[i];
				Entity *ent = nullptr;

				Text("Reference %d: %s", i, name.c_str());
				Text("State: %d", ref.TryGet(ent));
				Text("Pointer: %d", ent);

				if (ent)
				{
					Text("Name: %s", ent->GetName().c_str());
					if (Button("Reselect"))
						_debugManager.Get()->Select(ent);
				}

				if (Button("Remove Reference"))
				{
					entityRefs.erase(entityRefs.begin() + i);
					i--;
				}

				PopID();
				Separator();
			}
			EndChild();

			SeparatorText("Mesh Reference");
			if (meshRef)
			{
				B_Mesh *mesh = nullptr;
				Text("Mesh Reference State: %d", meshRef.TryGetAs<B_Mesh>(mesh));
				if (mesh)
				{
					Text("Mesh Name: %s", mesh->GetName().data());
					Text("Entity Name: %s", mesh->GetEntity()->GetName().data());

					if (Button("Reselect"))
						_debugManager.Get()->Select(mesh->GetEntity());
				}
				else
				{
					Text("Mesh Reference is invalid.");
				}

				if (Button("Free Reference"))
					meshRef = nullptr;
			}
			else
			{
				Text("No mesh reference set.");
			}
			Separator();

			PopID();
			TreePop();
			Separator();
		}

		if (TreeNode("Debug Draw Tests"))
		{
			if (TreeNode("Tri Tests"))
			{
				static bool overlayTri = false;
				static bool doubleTri = true;
				static bool screenSpace = false;

				Checkbox("Overlay##OverlayTri", &overlayTri);
				Checkbox("Double Sided##DoubleSidedTri", &doubleTri);
				Checkbox("Screen-Space Draws##DrawScreenSpaceDebugTest", &screenSpace);
				Dummy(ImVec2(0.0f, 8.0f));


				SeparatorText("Single Triangle");
				{
					static DebugDraw::Tri tri{
						{ {0, 0, 0}, {1, 0, 0, 1} },
						{ {0, 1, 0}, {0, 1, 0, 1} },
						{ {0, 0, 1}, {0, 0, 1, 1} }
					};

					Text("V0:");
					DragFloat3("Pos##v0", &tri.v0.position.x, 0.05f, 0.0f, 0.0f, "%.3f");
					DragFloat4("Col##v0", &tri.v0.color.x, 0.01f, 0.0f, 0.0f, "%.2f");
					Dummy(ImVec2(0.0f, 4.0f));

					Text("V1:");
					DragFloat3("Pos##v1", &tri.v1.position.x, 0.05f, 0.0f, 0.0f, "%.3f");
					DragFloat4("Col##v1", &tri.v1.color.x, 0.01f, 0.0f, 0.0f, "%.2f");
					Dummy(ImVec2(0.0f, 4.0f));

					Text("V2:");
					DragFloat3("Pos##v2", &tri.v2.position.x, 0.05f, 0.0f, 0.0f, "%.3f");
					DragFloat4("Col##v2", &tri.v2.color.x, 0.01f, 0.0f, 0.0f, "%.2f");

					DebugDrawer::Instance().DrawTri(tri, !overlayTri, doubleTri);
				}
				Dummy(ImVec2(0.0f, 8.0f));

				SeparatorText("Random Triangles");
				{
					static std::vector<DebugDraw::Tri> tris;
					static dx::XMFLOAT3 triSpawnBounds = { 10.0f, 10.0f, 10.0f };
					static int triCount = 10;

					DragInt("Amount##RandTris", &triCount);
					DragFloat3("Bounds##TriSpawnBounds", &triSpawnBounds.x, 0.2f, 0.0f, 0.0f, "%.3f");

					if (Button("Generate Random Tris"))
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
				Dummy(ImVec2(0.0f, 8.0f));

				SeparatorText("Selected OBB");
				{
					static dx::XMFLOAT4 selectColor = { 0.0f, 1.0f, 0.0f, 0.25f };
					DragFloat4("Selection Color##SelectColor", &selectColor.x, 0.01f, 0.0f, 0.0f, "%.2f");

					Entity *selectedEnt = _debugManager.Get()->GetPrimarySelection();
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


				TreePop();
				Separator();
			}

			if (TreeNode("Sprite Tests"))
			{
				PushID("SpriteTests");

				static DebugDraw::Sprite sprite;
				static bool overlaySprite = false;
				static bool screenSpaceSprite = false;
				static UINT texID = _content->GetTextureID("Maxwell");

				if (BeginCombo("##SelectSpriteTextureCombo", _content->GetTextureName((UINT)texID).c_str()))
				{
					std::vector<std::string> textureNames;
					_content->GetTextureNames(&textureNames);

					for (UINT i = 0; i < textureNames.size(); i++)
					{
						bool isSelected = (texID == i);
						if (Selectable(textureNames[i].c_str(), isSelected))
							texID = i;

						if (isSelected)
							SetItemDefaultFocus();
					}
					EndCombo();
				}

				if (screenSpaceSprite)
				{
					DragFloat2("Position", &sprite.position.x, 0.25f);
					ImGuiUtils::LockMouseOnActive();

					DragFloat("Depth", &sprite.position.z, 0.001f);
					ImGuiUtils::LockMouseOnActive();
				}
				else
				{
					DragFloat3("Position", &sprite.position.x, 0.1f);
					ImGuiUtils::LockMouseOnActive();
				}

				DragFloat2("Size", &sprite.size.x, screenSpaceSprite ? 0.25f : 0.1f);
				ImGuiUtils::LockMouseOnActive();

				DragFloat4("Rect", &sprite.uv0.x, 0.01f);
				ImGuiUtils::LockMouseOnActive();

				ColorEdit4("Color", &sprite.color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

				Checkbox("Overlay##OverlaySprite", &overlaySprite);

				Checkbox("Screen-Space##DrawScreenSpaceSpriteDebugTest", &screenSpaceSprite);

				if (screenSpaceSprite)
					DebugDrawer::Instance().DrawSpriteSS(texID, sprite);
				else
					DebugDrawer::Instance().DrawSprite(texID, sprite, !overlaySprite);

				PopID();
				TreePop();
				Separator();
			}

			if (TreeNode("Mesh Tests"))
			{
				PushID("MeshTests");

				{
					static dx::XMFLOAT3 pos = {0,0,0};
					static float rad = 1;
					static dx::XMFLOAT4 col = {1,1,1,1};
					static int subdivs = 0;
					static bool depth = true;

					Text("Center:"); SameLine();
					DragFloat3("##Pos", &pos.x, 0.05f);
					ImGuiUtils::LockMouseOnActive();

					Text("Radius:"); SameLine();
					DragFloat("##Rad", &rad, 0.01f, 0.001f);
					ImGuiUtils::LockMouseOnActive();

					Text("Subdivisions:"); SameLine();
					DragInt("##Divs", &subdivs, 0.1f, 0, 4, "%d", ImGuiSliderFlags_AlwaysClamp);
					ImGuiUtils::LockMouseOnActive();

					ColorEdit4("Color", &col.x);

					Text("Depth:"); SameLine();
					Checkbox("##Depth", &depth);

					DebugDrawer::Instance().DrawSphere(pos, rad, subdivs, col, depth);
				}

				PopID();
				TreePop();
			}

			if (TreeNode("Strip Tests"))
			{
				PushID("StripTests");

				static DebugDraw::LineStrip strip;
				static bool depth = true;
				static bool screenSpace = false;

				int pointCount = strip.points.size();
				if (DragInt("Point Count##StripTests", &pointCount, 0.1f, 0, 128))
				{
					pointCount = MAX(pointCount, 0);
					strip.points.resize(pointCount);
				}
				ImGuiUtils::LockMouseOnActive();

				if (TreeNode("Points"))
				{
					BeginChild("PointsChild", ImVec2(0, 200), true);
					for (int i = 0; i < strip.points.size(); i++)
					{
						DragFloat3(std::format("Point {}", i).c_str(), &strip.points[i].x, 0.05f);
						ImGuiUtils::LockMouseOnActive();
					}
					EndChild();
					TreePop();
				}

				ColorEdit4("Color", &strip.color.x);

				Text("Depth:"); SameLine();
				Checkbox("##Depth", &depth);

				Text("Screen-Space:"); SameLine();
				Checkbox("##ScreenSpace", &screenSpace);

				if (screenSpace)
				{
					DebugDrawer::Instance().DrawStripSS(strip);
				}
				else
				{
					DebugDrawer::Instance().DrawStrip(strip, depth);
				}

				PopID();
				TreePop();
			}

			if (TreeNode("Bezier Tests"))
			{
				PushID("BezierTests");

				static DebugDraw::LineBezier bezier;
				static bool depth = true;
				static bool screenSpace = false;

				DragFloat3("P0", &bezier.p0.x, 0.05f);
				ImGuiUtils::LockMouseOnActive();

				DragFloat3("P1", &bezier.p1.x, 0.05f);
				ImGuiUtils::LockMouseOnActive();

				DragFloat3("P2", &bezier.p2.x, 0.05f);
				ImGuiUtils::LockMouseOnActive();

				DragFloat3("P3", &bezier.p3.x, 0.05f);
				ImGuiUtils::LockMouseOnActive();

				ColorEdit4("Color0", &bezier.color0.x);

				ColorEdit4("Color1", &bezier.color1.x);

				DragFloat("Tess Factor", &bezier.tessFactor, 0.1f, 0.1f, 64.0f);

				Text("Depth:"); SameLine();
				Checkbox("##Depth", &depth);

				Text("Screen-Space:"); SameLine();
				Checkbox("##ScreenSpace", &screenSpace);

				if (screenSpace)
				{
					DebugDrawer::Instance().DrawBezierSS(bezier);
				}
				else
				{
					DebugDrawer &drawer = DebugDrawer::Instance();

					drawer.DrawSphere(bezier.p0, 0.05f, 0, { 1,0,0,0.5f }, depth);
					drawer.DrawSphere(bezier.p1, 0.03f, 0, { 1,1,0,0.5f }, depth);
					drawer.DrawSphere(bezier.p2, 0.03f, 0, { 1,1,0,0.5f }, depth);
					drawer.DrawSphere(bezier.p3, 0.05f, 0, { 1,0,0,0.5f }, depth);
					drawer.DrawBezier(bezier, depth);
				}

				PopID();
				TreePop();
			}

			Separator();
			TreePop();
		}

		if (TreeNode("Curve Editor Tests"))
		{
			// Use custom curve editor
			static std::vector<BezierPoint> points = {
				{ { 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.5f, 0.0f } },
				{ { 1.0f, 1.0f }, { 0.5f, 1.0f }, { 0.5f, 1.0f } }
			};

			static ImGuiCurveEditFlags flags = ImGuiCurveEditFlags_None;

			static ImVec2 curveSize(350, 350);
			static ImRect padding(48, 24, 20, 24);
			static ImRect curveRect(0, 0, 1, 1);
			static ImVec2i gridLines(4, 4);
			static float thickness = 3.0f;

			if (TreeNode("Flags"))
			{
				std::vector<std::string> flagNames = {
					"Linear", "Quadratic", "Jointed", "Force Span Width", "Force Injective",
					"Read Only", "No Labels", "No Points", "Clamp X", "Clamp Y"
				};

				// Flag checkboxes
				for (int i = 0; i < flagNames.size(); i++)
				{
					bool flagSet = (flags & (1 << i)) != 0;
					if (Checkbox(flagNames[i].c_str(), &flagSet))
					{
						if (flagSet)
							flags |= (1 << i);
						else
							flags &= ~(1 << i);
					}
				}

				TreePop();
			}

			SliderFloat("Thickness##CurveEditor", &thickness, 0.1f, 10.0f);
			DragFloat2("Size##CurveEditor", &curveSize.x, 1.0f, 1.0f, 0.0f, "%.0f");
			ImGuiUtils::LockMouseOnActive();
			if (DragInt2("Grid Lines##CurveEditor", &gridLines.x, 0.05f))
			{
				gridLines.x = MAX(gridLines.x, 0);
				gridLines.y = MAX(gridLines.y, 0);
			}
			ImGuiUtils::LockMouseOnActive();
			DragFloat4("Padding##CurveEditor", &padding.Min.x, 0.01f, 0.0f, 0.0f, "%.2f");
			ImGuiUtils::LockMouseOnActive();
			DragFloat4("Bounds##CurveEditor", &curveRect.Min.x, 0.01f, 0.0f, 0.0f, "%.2f");
			ImGuiUtils::LockMouseOnActive();

			Separator();
			CurveEdit("##CurveEditor", &points, curveSize, curveRect, thickness, padding, gridLines, flags);

			// Test sampling using Curves.h
			if (TreeNode("Sampling"))
			{
				using namespace Curves;

				Curve curve;
				curve.type = (flags & ImGuiCurveEditFlags_Linear) ? CurveType::Linear : ((flags & ImGuiCurveEditFlags_Quadratic) ? CurveType::BezierQuadratic : CurveType::BezierCubic);

				curve.points.resize(points.size());
				std::memcpy(curve.points.data(), points.data(), points.size() * sizeof(BezierPoint));

				static bool injective = false;
				static float sampleT = 0.5f;

				BeginChild("Curve Sampling", curveSize, true);
				{
					dx::XMFLOAT2 sampleValue;

					if (injective)
					{
						float sampleY = curve.SampleInjective(sampleT);
						sampleValue = { sampleT, sampleY };
					}
					else
					{
						sampleValue = curve.SamplePoint(sampleT);
					}

					ImVec2 windowPos = ImGui::GetWindowPos();
					ImDrawList* drawList = ImGui::GetWindowDrawList();

					ImVec2 samplePos = { sampleValue.x, 1.0f - sampleValue.y };
					samplePos *= curveSize;

					drawList->AddCircleFilled(windowPos + samplePos, thickness * 1.6f, IM_COL32(255, 255, 255, 255));
				}
				EndChild();

				// Sample slider, same width as child window
				SetNextItemWidth(curveSize.x);
				SliderFloat("##Sample T", &sampleT, 0.0f, 1.0f);
				Checkbox("Injective Sampling##CurveSampling", &injective);

				TreePop();
			}

			Separator();
			TreePop();
		}

		if (TreeNode("Node Graph Tests"))
		{
			using namespace ImGui::NodeGraph;

			static GraphContext gCtx;
			static GraphInstance gInstance(gCtx);

			static bool firstTime = true;
			if (firstTime)
			{
				// Create context, then add some nodes to instance

				PinPreset pinFloatPreset = {};
				pinFloatPreset.name = "Float";
				pinFloatPreset.color = ImColor(115, 240, 115);
				pinFloatPreset.type = ImGui::NodeGraph::PinType::Float;

				PinPreset pinVec3Preset = {};
				pinVec3Preset.name = "Vec3";
				pinVec3Preset.color = ImColor(240, 115, 115);
				pinVec3Preset.type = ImGui::NodeGraph::PinType::Vec3;

				PinPreset pinBoolPreset = {};
				pinBoolPreset.name = "Bool";
				pinBoolPreset.color = ImColor(115, 115, 240);
				pinBoolPreset.type = ImGui::NodeGraph::PinType::Bool;

				PinPreset pinBitPreset = {};
				pinBitPreset.name = "Bit";
				pinBitPreset.color = ImColor(240, 115, 240);
				pinBitPreset.type = ImGui::NodeGraph::PinType::Bool;

				PinPreset pinCustomEntPreset = {};
				pinCustomEntPreset.name = "Entity";
				pinCustomEntPreset.color = ImColor(240, 240, 115);
				pinCustomEntPreset.type = ImGui::NodeGraph::PinType::Custom;
				pinCustomEntPreset.customTypeName = "Entity";

				PinPreset pinCustomBehPreset = {};
				pinCustomBehPreset.name = "Behaviour";
				pinCustomBehPreset.color = ImColor(115, 240, 240);
				pinCustomBehPreset.type = ImGui::NodeGraph::PinType::Custom;
				pinCustomBehPreset.customTypeName = "Behaviour";

				PinPreset pinFlowPreset = {};
				pinFlowPreset.name = "";
				pinFlowPreset.color = ImColor(255, 255, 255);
				pinFlowPreset.type = ImGui::NodeGraph::PinType::Flow;


				NodePreset nodePreset = {};
				nodePreset.name = "Test Node A";
				nodePreset.headerColor = ImColor(96, 164, 96);
				nodePreset.inputs.push_back(pinFloatPreset);
				nodePreset.inputs.push_back(pinVec3Preset);
				nodePreset.inputs.push_back(pinBoolPreset);
				nodePreset.outputs.push_back(pinVec3Preset);
				nodePreset.outputs.push_back(pinBoolPreset);
				nodePreset.outputs.push_back(pinBoolPreset);
				nodePreset.outputs.push_back(pinFloatPreset);
				nodePreset.CalcSize();
				gCtx.AddNodePreset(nodePreset);

				nodePreset = {};
				nodePreset.name = "Test Node B";
				nodePreset.headerColor = ImColor(164, 96, 164);
				nodePreset.inputs.push_back(pinBoolPreset);
				nodePreset.inputs.push_back(pinCustomEntPreset);
				nodePreset.inputs.push_back(pinCustomBehPreset);
				nodePreset.outputs.push_back(pinCustomEntPreset);
				nodePreset.CalcSize();
				gCtx.AddNodePreset(nodePreset);

				nodePreset = {};
				nodePreset.name = "Test Node C";
				nodePreset.headerColor = ImColor(128, 128, 64);
				nodePreset.outputs.push_back(pinCustomEntPreset);
				nodePreset.outputs.push_back(pinCustomBehPreset);
				nodePreset.CalcSize();
				gCtx.AddNodePreset(nodePreset);

				nodePreset = {};
				nodePreset.name = "Bit";
				nodePreset.headerColor = ImColor(96, 96, 164);
				nodePreset.bodySize = ImVec2(35, 35);
				nodePreset.outputs.push_back(pinBitPreset);
				nodePreset.drawBodyFunc = [](GraphInstance &instance, Node &node, ImVec2 bodySize) {
					// HACK: Store bool in ImGui storage, this would instead be some storage location in node
					ImGuiStorage *storage = ImGui::GetStateStorage();
					ImGuiID bitID = ImGui::GetID("##Bit");
					bool bit = storage->GetBool(bitID, false);
					if (ImGui::Checkbox("##Checkbox", &bit))
						storage->SetBool(bitID, bit);
				};
				nodePreset.CalcSize();
				gCtx.AddNodePreset(nodePreset);

				nodePreset = {};
				nodePreset.name = "Flow";
				nodePreset.inputs.push_back(pinFlowPreset);
				nodePreset.outputs.push_back(pinFlowPreset);
				nodePreset.CalcSize();
				gCtx.AddNodePreset(nodePreset);


				NodeId nodeId1 = gInstance.AddNode(0, ImVec2(40, 30));
				NodeId nodeId2 = gInstance.AddNode(0, ImVec2(350, 90));

				gInstance.AddLink(
					gInstance.GetNodePin(nodeId1, 0, PinGender::Output), 
					gInstance.GetNodePin(nodeId2, 1, PinGender::Input)
				);

				gInstance.AddLink(
					gInstance.GetNodePin(nodeId1, 3, PinGender::Output), 
					gInstance.GetNodePin(nodeId2, 0, PinGender::Input)
				);

				gInstance.AddNode(1, ImVec2(60, 200));
				gInstance.AddNode(2, ImVec2(400, 220));
				gInstance.AddNode(0, ImVec2(100, 350));
				gInstance.AddNode(3, ImVec2(400, 400));

				for (int i = 0; i < 5; i++)
					gInstance.AddNode(4, ImVec2(0, -50));


				firstTime = false;
			}

			OpenNodeGraph("Graph", gInstance, ImVec2(0, 0), ImGuiNodeGraphFlags_EnableGrid);

			Separator();
			TreePop();
		}

		static bool displayCullingRects = false;
		Checkbox("Show Culling Rects", &displayCullingRects);

		if (displayCullingRects)
		{
			static bool useDepthForRects = true;
			Checkbox("Use Depth##UseDepthForRects", &useDepthForRects);

			static bool doubleSided = false;
			Checkbox("Double-Sided##doubleSidedRects", &doubleSided);

			static bool fullTree = false;
			Checkbox("Show Full Tree##FullTree", &fullTree);

			static bool cullingBounds = false;
			Checkbox("Show Culling Bounds##CullingBounds", &cullingBounds);

			static float opacity = 0.3f;
			SliderFloat("Opacity", &opacity, 0.0000001f, 1.0f);

			static float thickness = 0.95f;
			DragFloat("Thickness", &thickness, 0.01f);


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

		Separator();
		Dummy(ImVec2(0.0f, 4.0f));

		static bool currState = false;
		static float fadeDuration = 1.0f;
		SliderFloat("Fade Duration", &fadeDuration, 0.0f, 3.0f);

		if (Button("Begin Fade"))
		{
			_graphics->BeginScreenFade(fadeDuration * (currState ? -1.0f : 1.0f));
			currState = !currState;
		}

		Dummy(ImVec2(0.0f, 4.0f));

		dx::XMFLOAT3A camPos = _mainCamera.Get()->GetTransform()->GetPosition();
		char camXCoord[32]{}, camYCoord[32]{}, camZCoord[32]{};
		snprintf(camXCoord, sizeof(camXCoord), "%.2f", camPos.x);
		snprintf(camYCoord, sizeof(camYCoord), "%.2f", camPos.y);
		snprintf(camZCoord, sizeof(camZCoord), "%.2f", camPos.z);
		Text(std::format("Camera Pos: ({}, {}, {})", camXCoord, camYCoord, camZCoord).c_str());

		Separator();
		
		char nearPlane[16]{}, farPlane[16]{};
		for (UINT i = 0; i < _spotlights->GetNrOfLights(); i++)
		{
			const ProjectionInfo projInfo = _spotlights->GetLightBehaviour(i)->GetShadowCamera()->GetCurrProjectionInfo();
			snprintf(nearPlane, sizeof(nearPlane), "%.2f", projInfo.planes.nearZ);
			snprintf(farPlane, sizeof(farPlane), "%.1f", projInfo.planes.farZ);
			Text(std::format("({}:{}) Planes Spotlight #{}", nearPlane, farPlane, i).c_str());
		}
	}
	
	return true;
}
#endif

#ifdef USE_IMGUIZMO
bool Scene::RenderGizmoUI()
{
	ZoneScopedXC(RandomUniqueColor());
	using namespace dx;

	if (!_mainCamera)
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

	ImGuizmo::SetOrthographic(_mainCamera.Get()->GetOrtho());

	// Selection transform gizmo
	if (auto dbgPlayer = GetDebugManager())
	{
		Entity *primaryEnt = dbgPlayer->GetPrimarySelection();

		if (primaryEnt)
		{
			XMFLOAT4X4A camView = _mainCamera.Get()->GetViewMatrix();
			const XMFLOAT4X4A camProj = _mainCamera.Get()->GetProjectionMatrix();

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
							ent->SignalTransformEdited();
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
							ent->SignalTransformEdited();
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
	if (DebugData::Get().showViewManipGizmo)
	{
		constexpr float viewManipGizmoSize = 96.0f;

		Transform *camTransform = _mainCamera.Get()->GetTransform();

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
		static bool skipNextDelta = false;

		if (needsPivotDist && !hasPivotDist)
		{
			skipNextDelta = false;

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

		auto mState = input.GetMouse();
		ImGuizmo::SetViewManipulatorDelta(skipNextDelta ? ImVec2(0, 0) : ImVec2(mState.delta * input.GetMouseSensitivity()));

		//float camUp[3] = { 0.0f, u.y > 0.0f ? 1.0f : -1.0f, 0.0f };
		//float camUp[3] = { u.x, u.y, u.z };
		//ImGuizmo::SetViewManipulatorUp(camUp);

		ImGuizmo::ViewManipulate(
			&(camView.m[0][0]), 
			pivotDist, 
			ImVec2(sceneWidth + scenePosX - 16 - viewManipGizmoSize, scenePosY + 16),
			ImVec2(viewManipGizmoSize, viewManipGizmoSize),
			0x10101010
		);

		if (ImGuizmo::IsUsingViewManipulate())
		{
			if (hasPivotDist)
			{
				skipNextDelta = input.TryWrapMouse(true);

				// Draw dot at pivot point
				{
					XMFLOAT3A pivot;
					Store(pivot, Load(camPos) + (Load(camDir) * pivotDist));
					DebugDrawer::Instance().DrawSphere(pivot, 0.1f, 3, { 1,1,1,1 });
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
			if (ImGuizmo::IsUsingViewManipulate() || ImGuizmo::IsViewManipulateHovered() ||
				ImGuizmo::IsOver() || ImGuizmo::IsUsingAny())
			{
				input.SetMouseAbsorbed(true);
			}
		}
	}

	return true;
}
#endif
