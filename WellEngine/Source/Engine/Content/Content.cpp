#include "stdafx.h"
#include "Content.h"
#include "ContentLoader.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

Content::Content()
{
	_materialVec.reserve(512);
}
Content::~Content()
{
	Shutdown();
}

void Content::Shutdown()
{
	if (_hasShutDown)
		return;

	for (auto *item : _meshes)
		delete item;

	for (auto *item : _shaders)
		delete item;

	for (auto *item : _textures)
		delete item;

	for (auto *item : _cubemaps)
		delete item;

	for (auto *item : _heightTextures)
		delete item;

	for (auto *item : _samplers)
		delete item;
	
	for (auto *item : _blendStates)
		delete item;

	for (auto *item : _inputLayouts)
		delete item;

	for (auto *item : _materialVec)
		delete item;

	_hasShutDown = true;
}

#ifdef USE_IMGUI
bool Content::RenderUI(ID3D11Device *device)
{
	ZoneScopedXC(RandomUniqueColor());

	if (ImGui::TreeNode("Textures"))
	{
		const float scrollBarWidth = ImGui::GetStyle().ScrollbarSize + 16.0f;
		const float texPadding = 4.0f;
		const ImVec4 texBorder = {1.0f,1.0f,1.0f,0.3f};

		static float previewHeight = 128.0f;
		if (ImGui::DragFloat("Preview Scale", &previewHeight))
			previewHeight = max(4.0f, previewHeight);
		ImGuiUtils::LockMouseOnActive();

		static float imageHeight = 512.0f;
		if (ImGui::DragFloat("Image Scale", &imageHeight))
			imageHeight = max(128.0f, imageHeight);
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::BeginTabBar("HierarchyTab"))
		{
			ImGuiChildFlags texViewChildFlags = 0;
			texViewChildFlags |= ImGuiChildFlags_Borders;
			texViewChildFlags |= ImGuiChildFlags_ResizeY;

			ImGuiWindowFlags texViewWindowFlags = 0;
			texViewWindowFlags |= ImGuiWindowFlags_NoSavedSettings;

			ImGuiTableFlags texViewTableFlags = 0; 
			texViewTableFlags |= ImGuiTableFlags_NoPadOuterX;
			texViewTableFlags |= ImGuiTableFlags_NoClip;

			if (ImGui::BeginTabItem("Textures"))
			{
				bool refreshTextures = ImGui::Button("Refresh");
				static bool resortTextures = true;

				enum ImGuiSortMode {
					ImGuiSortMode_ID = 0,
					ImGuiSortMode_Name,
					ImGuiSortMode_Size,
					ImGuiSortMode_Format
				};
				static ImGuiSortMode sortMode = ImGuiSortMode_ID;
				static ImGuiSortDirection sortDirection = ImGuiSortDirection_Ascending;

				// Filtering
				static std::string filterSearch = "";
				static bool filterID = false;
				static dx::XMINT2 filterIDRange = {0, (int)_textures.size() - 1};
				static DXGI_FORMAT filterFormat = DXGI_FORMAT_UNKNOWN;
				static int filterTransparent = -1; // <0: not filtered, 1: filter non-matching, 2: filter matching

				// Filter options
				{
					static bool showFilterOptions = false;
					ImGui::SameLine();
					if (ImGui::Button(showFilterOptions ? "Hide Filter" : "Show Filter" ))
					{
						showFilterOptions = !showFilterOptions;
					}

					ImGui::SameLine();
					if (ImGui::Button("Clear Filters"))
					{
						filterSearch.clear();
						filterID = false;
						filterFormat = DXGI_FORMAT_UNKNOWN;
						filterTransparent = -1;

						refreshTextures = true;
					}

					if (showFilterOptions)
					{
						ImGui::SeparatorText("Filters");

						// Search
						if (ImGui::InputText("##SearchFilter", &filterSearch))
						{
							refreshTextures = true;
						}

						// ID
						if (ImGui::Checkbox("ID Range", &filterID))
						{
							if (filterID)
								filterIDRange = { 0, (int)_textures.size() - 1 };

							refreshTextures = true;
						}

						dx::XMINT2 prevFilterIDRange = filterIDRange;
						ImGui::BeginDisabled(!filterID); ImGui::SameLine();
						if (ImGui::DragInt2("##IDRange", (int *)&filterIDRange))
						{
							if (filterIDRange.x < 0)
								filterIDRange.x = 0;

							if (filterIDRange.y >= (int)_textures.size())
								filterIDRange.y = (int)_textures.size() - 1;

							if (filterIDRange.x != prevFilterIDRange.x && filterIDRange.x > filterIDRange.y)
								filterIDRange.x = filterIDRange.y;

							if (filterIDRange.y != prevFilterIDRange.y && filterIDRange.y < filterIDRange.x)
								filterIDRange.y = filterIDRange.x;

							refreshTextures = true;
						}
						ImGuiUtils::LockMouseOnActive();
						ImGui::EndDisabled();

						// Transparent
						bool filterTransparentState = (filterTransparent > 0);
						if (ImGui::Checkbox("##TransparentFilter", &filterTransparentState))
						{
							filterTransparent *= -1;

							refreshTextures = true;
						}

						ImGui::BeginDisabled(!filterTransparentState); ImGui::SameLine();
						if (ImGui::Button(std::abs(filterTransparent) == 1 ? "Transparent" : "Opaque"))
						{
							if (filterTransparent == 1)
								filterTransparent = 2;
							else
								filterTransparent = 1;

							refreshTextures = true;
						}
						ImGui::EndDisabled();

						// Format
						if (ImGui::BeginCombo("Format", filterFormat == DXGI_FORMAT_UNKNOWN ? "Any" : D3D11FormatData::GetName(filterFormat).c_str()))
						{
							if (ImGui::Selectable("Any"))
							{
								filterFormat = DXGI_FORMAT_UNKNOWN;
								refreshTextures = true;
							}

							int formatCount = 0;
							const DXGI_FORMAT *formatList = D3D11FormatData::GetLinearList(&formatCount);

							for (int i = 1; i < formatCount; i++)
							{
								const std::string &formatName = D3D11FormatData::GetName(formatList[i]);
								if (ImGui::Selectable(formatName.c_str(), filterFormat == formatList[i]))
								{
									filterFormat = formatList[i];
									refreshTextures = true;
								}
							}
							ImGui::EndCombo();
						}

						ImGui::Separator();
					}
				}

				// Sort options
				{
					static const char *sortModeNames = "ID\0Name\0Size\0Format\0\0";
					ImGui::Text("Sort:"); ImGui::SameLine(); ImGui::SetNextItemWidth(120);
					if (ImGui::Combo("##SortMode", (int *)(&sortMode), sortModeNames))
					{
						resortTextures = true;
					}

					ImGui::SameLine();
					if (ImGui::ArrowButton("SortDir", sortDirection == ImGuiSortDirection_Ascending ? ImGuiDir_Up : ImGuiDir_Down))
					{
						sortDirection = (sortDirection == ImGuiSortDirection_Ascending) ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending;
						resortTextures = true;
					}
				}

				static std::vector<const Texture *> sortedTextures(_textures.begin(), _textures.end());
				// Apply filters & sorting
				{
					if (refreshTextures)
					{
						sortedTextures.clear();
						sortedTextures.insert(sortedTextures.end(), _textures.begin(), _textures.end());

						std::string searchLower = filterSearch;
						if (!searchLower.empty())
							std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

						// Apply filters
						int count = sortedTextures.size();
						for (int i = 0; i < count; i++)
						{
							const Texture *texture = sortedTextures[i];
							bool cull = false;

							// Search
							if (!searchLower.empty())
							{
								std::string nameLower = texture->name;
								std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
								cull = nameLower.find(searchLower) == std::string::npos;
							}

							// ID Range
							if (filterID && !cull)
							{
								cull = texture->id < filterIDRange.x || texture->id > filterIDRange.y;
							}

							// Format
							if (filterFormat != DXGI_FORMAT_UNKNOWN && !cull)
							{
								cull = texture->data.GetFormat() != filterFormat;
							}

							if (cull)
							{
								sortedTextures.erase(sortedTextures.begin() + i);
								i--;
								count--;
							}
						}

						resortTextures = true;
					}

					if (resortTextures)
					{
						std::sort(sortedTextures.begin(), sortedTextures.end(), [](const Texture *lhs, const Texture *rhs) -> bool {
							bool sort = false;
							switch (sortMode)
							{
							case ImGuiSortMode_ID:
								sort = lhs->id <= rhs->id;
								break;

							case ImGuiSortMode_Name:
								sort = lhs->name.compare(rhs->name) <= 0;
								break;

							case ImGuiSortMode_Size: {
								auto &lhsSize = lhs->data.GetSize();
								auto &rhsSize = rhs->data.GetSize();

								if (lhsSize.x != rhsSize.x)
									sort = lhsSize.x <= rhsSize.x;
								else
									sort = lhsSize.y <= rhsSize.y;
								break;
							}

							case ImGuiSortMode_Format:
								sort = lhs->data.GetFormat() <= rhs->data.GetFormat();
								break;

							default:
								break;
							}
							return (sortDirection == ImGuiSortDirection_Ascending) ? sort : !sort;
						});

						resortTextures = false;
					}
				}

				int count = sortedTextures.size();
				const float availableWidth = ImGui::GetContentRegionAvail().x - scrollBarWidth;

				const int tableItemSize = previewHeight + texPadding;
				const int columnsCount = max(1, (int)(availableWidth / tableItemSize));
				const int rowsCount = (count + columnsCount - 1) / columnsCount;

				const float widthPerColumn = (float)(columnsCount * tableItemSize);
				const float tableWidth = widthPerColumn;
				const float tableHeight = (float)(rowsCount * tableItemSize);

				ImGui::BeginChild("##TextureChild", { tableWidth + scrollBarWidth, (float)(tableItemSize) * 3 }, texViewChildFlags, texViewWindowFlags);
				
				if (count > 0 && ImGui::BeginTable("TexturesGrid", columnsCount, texViewTableFlags, { tableWidth, tableHeight }))
				{
					for (int i = 0; i < count; i++)
					{
						if (i % columnsCount == 0)
							ImGui::TableNextRow();
						ImGui::TableNextColumn();
						
						const Texture *texture = sortedTextures[i];
						const auto &texData = texture->data;
						auto &size = texData.GetSize();
						float aspectRatio = (float)size.x / (float)size.y;
						float thumbWidth = previewHeight * aspectRatio;
						float thumbHeight = previewHeight;
						
						// Keep aspect ratio within bounds
						if (thumbWidth > previewHeight)
						{
							thumbWidth = previewHeight;
							thumbHeight = previewHeight / aspectRatio;
						}

						// Center the thumbnail
						ImVec2 centeredCursorPos = {
							(tableItemSize - thumbWidth) * 0.5f,
							(tableItemSize - texPadding - thumbHeight) * 0.5f
						};
						ImVec2 imageCursorPos = ImGui::GetCursorPos() + centeredCursorPos;

						ImGui::Dummy({ (float)tableItemSize, (float)tableItemSize - texPadding });
						ImGui::SetCursorPos(imageCursorPos);
						ImGui::Image(
							(ImTextureID)texData.GetSRV(),
							ImVec2(thumbWidth, thumbHeight),
							ImVec2(1, 1), ImVec2(0, 0), {1,1,1,1}, texBorder
						);

						if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNone))
						{
							if (ImGui::BeginTooltip())
							{
								ImGui::Image(
									(ImTextureID)texData.GetSRV(),
									ImVec2(imageHeight * aspectRatio, imageHeight),
									ImVec2(1, 1), ImVec2(0, 0)
								);

								ImGui::Text("ID: %d", texture->id);
								ImGui::Text(texture->name.c_str());
								ImGui::Text(texture->path.c_str());
								ImGui::Text("%d x %d", size.x, size.y);
								ImGui::Text(D3D11FormatData::GetName(texData.GetFormat()).c_str());
								ImGui::Text("Mipmapped: %s", (texture->mipmapped ? "True" : "False"));
								ImGui::EndTooltip();
							}
						}

						if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
						{
							ImGui::ContentPayload payload = { texture->id };
							ImGui::SetDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE), &payload, sizeof(ImGui::ContentPayload));

							ImVec2 cursorPos = ImGui::GetCursorPos();

							const int texSize = 32;
							const int texWidth = texSize * aspectRatio;
							ImGui::Image(
								(ImTextureID)texData.GetSRV(),
								ImVec2(texWidth, texSize),
								ImVec2(1, 1), ImVec2(0, 0), { 1,1,1,1 }, texBorder
							);

							ImGui::SetCursorPos(cursorPos + ImVec2(texWidth + 8, 2));
							ImGui::Text("%s", texture->name.c_str());

							ImGui::SetCursorPosX(cursorPos.x + texWidth + 8);
							ImGui::Text("ID: %d", texture->id);

							ImGui::EndDragDropSource();
						}
					}
					ImGui::EndTable();
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Cubemaps"))
			{
				bool refreshTextures = ImGui::Button("Refresh");
				static bool resortTextures = true;

				enum ImGuiSortMode {
					ImGuiSortMode_ID = 0,
					ImGuiSortMode_Name,
					ImGuiSortMode_Size,
					ImGuiSortMode_Format
				};
				static ImGuiSortMode sortMode = ImGuiSortMode_ID;
				static ImGuiSortDirection sortDirection = ImGuiSortDirection_Ascending;

				// Filtering
				static std::string filterSearch = "";
				static bool filterID = false;
				static dx::XMINT2 filterIDRange = { 0, (int)_cubemaps.size() - 1 };
				static DXGI_FORMAT filterFormat = DXGI_FORMAT_UNKNOWN;

				// Filter options
				{
					static bool showFilterOptions = false;
					ImGui::SameLine();
					if (ImGui::Button(showFilterOptions ? "Hide Filter" : "Show Filter"))
					{
						showFilterOptions = !showFilterOptions;
					}

					ImGui::SameLine();
					if (ImGui::Button("Clear Filters"))
					{
						filterSearch.clear();
						filterID = false;
						filterFormat = DXGI_FORMAT_UNKNOWN;

						refreshTextures = true;
					}

					if (showFilterOptions)
					{
						ImGui::SeparatorText("Filters");

						// Search
						if (ImGui::InputText("##SearchFilter", &filterSearch))
						{
							refreshTextures = true;
						}

						// ID
						if (ImGui::Checkbox("ID Range", &filterID))
						{
							if (filterID)
								filterIDRange = { 0, (int)_cubemaps.size() - 1 };

							refreshTextures = true;
						}

						dx::XMINT2 prevFilterIDRange = filterIDRange;
						ImGui::BeginDisabled(!filterID); ImGui::SameLine();
						if (ImGui::DragInt2("##IDRange", (int *)&filterIDRange))
						{
							if (filterIDRange.x < 0)
								filterIDRange.x = 0;

							if (filterIDRange.y >= (int)_cubemaps.size())
								filterIDRange.y = (int)_cubemaps.size() - 1;

							if (filterIDRange.x != prevFilterIDRange.x && filterIDRange.x > filterIDRange.y)
								filterIDRange.x = filterIDRange.y;

							if (filterIDRange.y != prevFilterIDRange.y && filterIDRange.y < filterIDRange.x)
								filterIDRange.y = filterIDRange.x;

							refreshTextures = true;
						}
						ImGuiUtils::LockMouseOnActive();
						ImGui::EndDisabled();

						// Format
						if (ImGui::BeginCombo("Format", filterFormat == DXGI_FORMAT_UNKNOWN ? "Any" : D3D11FormatData::GetName(filterFormat).c_str()))
						{
							if (ImGui::Selectable("Any"))
							{
								filterFormat = DXGI_FORMAT_UNKNOWN;
								refreshTextures = true;
							}

							int formatCount = 0;
							const DXGI_FORMAT *formatList = D3D11FormatData::GetLinearList(&formatCount);

							for (int i = 1; i < formatCount; i++)
							{
								const std::string &formatName = D3D11FormatData::GetName(formatList[i]);
								if (ImGui::Selectable(formatName.c_str(), filterFormat == formatList[i]))
								{
									filterFormat = formatList[i];
									refreshTextures = true;
								}
							}
							ImGui::EndCombo();
						}

						ImGui::Separator();
					}
				}

				// Sort options
				{
					static const char *sortModeNames = "ID\0Name\0Size\0Format\0\0";
					ImGui::Text("Sort:"); ImGui::SameLine(); ImGui::SetNextItemWidth(120);
					if (ImGui::Combo("##SortMode", (int *)(&sortMode), sortModeNames))
					{
						resortTextures = true;
					}

					ImGui::SameLine();
					if (ImGui::ArrowButton("SortDir", sortDirection == ImGuiSortDirection_Ascending ? ImGuiDir_Up : ImGuiDir_Down))
					{
						sortDirection = (sortDirection == ImGuiSortDirection_Ascending) ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending;
						resortTextures = true;
					}
				}

				static std::vector<const Cubemap *> sortedTextures(_cubemaps.begin(), _cubemaps.end());
				// Apply filters & sorting
				{
					if (refreshTextures)
					{
						sortedTextures.clear();
						sortedTextures.insert(sortedTextures.end(), _cubemaps.begin(), _cubemaps.end());

						std::string searchLower = filterSearch;
						if (!searchLower.empty())
							std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

						// Apply filters
						int count = sortedTextures.size();
						for (int i = 0; i < count; i++)
						{
							const Cubemap *texture = sortedTextures[i];
							bool cull = false;

							// Search
							if (!searchLower.empty())
							{
								std::string nameLower = texture->name;
								std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
								cull = nameLower.find(searchLower) == std::string::npos;
							}

							// ID Range
							if (filterID && !cull)
							{
								cull = texture->id < filterIDRange.x || texture->id > filterIDRange.y;
							}

							// Format
							if (filterFormat != DXGI_FORMAT_UNKNOWN && !cull)
							{
								cull = texture->data.GetFormat() != filterFormat;
							}

							if (cull)
							{
								sortedTextures.erase(sortedTextures.begin() + i);
								i--;
								count--;
							}
						}

						resortTextures = true;
					}

					if (resortTextures)
					{
						std::sort(sortedTextures.begin(), sortedTextures.end(), [this](const Cubemap *lhs, const Cubemap *rhs) -> bool {
							bool sort = false;
							switch (sortMode)
							{
							case ImGuiSortMode_ID:
								sort = lhs->id <= rhs->id;
								break;

							case ImGuiSortMode_Name:
								sort = lhs->name.compare(rhs->name) <= 0;
								break;

							case ImGuiSortMode_Size:
								sort = lhs->data.GetSize().x <= rhs->data.GetSize().x;
								break;

							case ImGuiSortMode_Format:
								sort = lhs->data.GetFormat() <= rhs->data.GetFormat();
								break;

							default:
								break;
							}
							return (sortDirection == ImGuiSortDirection_Ascending) ? sort : !sort;
						});

						resortTextures = false;
					}
				}

				int count = sortedTextures.size();
				const float availableWidth = ImGui::GetContentRegionAvail().x - scrollBarWidth;

				const int tableItemSize = previewHeight + texPadding;
				const int columnsCount = max(1, (int)(availableWidth / tableItemSize));
				const int rowsCount = (count + columnsCount - 1) / columnsCount;

				const float widthPerColumn = (float)(columnsCount * tableItemSize);
				const float tableWidth = widthPerColumn;
				const float tableHeight = (float)(rowsCount * tableItemSize);

				ImGui::BeginChild("##TextureChild", { tableWidth + scrollBarWidth, (float)(tableItemSize) * 3 }, texViewChildFlags, texViewWindowFlags);

				if (count > 0 && ImGui::BeginTable("CubemapsGrid", columnsCount, texViewTableFlags, { tableWidth, tableHeight }))
				{
					for (int i = 0; i < count; i++)
					{
						if (i % columnsCount == 0)
							ImGui::TableNextRow();
						ImGui::TableNextColumn();
						
						const Cubemap *texture = sortedTextures[i];
						const auto &texData = texture->data;
						auto &size = texData.GetSize();

						// Display placeholder or cubemap representation (square)
						ImVec2 placeholderSize(tableItemSize, tableItemSize);
						ImVec2 pos = ImGui::GetCursorScreenPos();
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						drawList->AddRectFilled(pos, ImVec2(pos.x + placeholderSize.x, pos.y + placeholderSize.y), IM_COL32(50, 50, 50, 255));
						drawList->AddRect(pos, ImVec2(pos.x + placeholderSize.x, pos.y + placeholderSize.y), IM_COL32(100, 100, 100, 255));
						
						// Add text overlay to indicate it's a cubemap
						std::string trimmedName = texture->name;
						ImVec2 textSize = ImGui::CalcTextSize(trimmedName.c_str());
						if (textSize.x > placeholderSize.x - 4)
						{
							do
							{
								trimmedName.pop_back();
								textSize = ImGui::CalcTextSize((trimmedName + "..").c_str());
							} while (textSize.x > placeholderSize.x - 2);

							trimmedName += "..";
						}
						ImVec2 textPos = ImVec2(pos.x + (placeholderSize.x - textSize.x) * 0.5f, pos.y + (placeholderSize.y - textSize.y) * 0.5f);
						drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), trimmedName.c_str());
						
						ImGui::Dummy(placeholderSize);

						if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNone))
						{
							if (ImGui::BeginTooltip())
							{
								ImGui::Text("ID: %d", texture->id);
								ImGui::Text(texture->name.c_str());
								ImGui::Text(texture->path.c_str());
								ImGui::Text("%d x %d", size.x, size.y);
								ImGui::Text(D3D11FormatData::GetName(texData.GetFormat()).c_str());
								ImGui::Text("Mipmapped: %s", (texData.IsMipmapped() ? "True" : "False"));
								ImGui::EndTooltip();
							}
						}

						if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
						{
							ImGui::ContentPayload payload = { texture->id };
							ImGui::SetDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::CUBEMAP), &payload, sizeof(ImGui::ContentPayload));

							ImGui::Text("%s", texture->name.c_str());
							ImGui::Text("ID: %d", texture->id);

							ImGui::EndDragDropSource();
						}
					}
					ImGui::EndTable();
				}
					
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Shaders"))
	{
		static bool enableAutoRecompile = false;
		ImGui::Checkbox("Auto Recompile Exposed Shaders", &enableAutoRecompile);

		bool recompile = false;
		if (enableAutoRecompile)
		{
			static float delay = 1.0f;
			ImGui::DragFloat("Recompile Delay", &delay, 0.01f, 0.1f, 10.0f);
			ImGuiUtils::LockMouseOnActive();

			static float timeUntilReload = delay;
			timeUntilReload -= TimeUtils::GetInstance().GetDeltaTime();

			if (timeUntilReload <= 0.0f)
			{
				timeUntilReload = delay;
				recompile = true;
			}
		}

		ImGui::BeginChild("ShaderList", { 0, 300 }, ImGuiChildFlags_ResizeY);

		std::vector<std::string> shaderNames;
		GetShaderNames(&shaderNames);

		for (int i = 0; i < shaderNames.size(); i++)
		{
			ImGui::PushID(i);
			if (ImGui::TreeNode(shaderNames[i].c_str()))
			{
				ImGui::Text("ID: %d", i);
				ImGui::Text("Name: %s", shaderNames[i].c_str());

				if (recompile || ImGui::Button("Recompile"))
					bool _ = RecompileShader(device, shaderNames[i]);

				if (ImGui::TreeNode("Shader Info"))
				{
					// Expose reflection data
					ShaderD3D11 *shader = GetShader(shaderNames[i]);
					auto reflector = shader->GetReflector();

					D3D11_SHADER_DESC shaderDesc;
					if (FAILED(reflector->GetDesc(&shaderDesc)))
					{
						DbgMsg("Failed to get shader description!");
						ImGui::TreePop();
						ImGui::PopID();
						continue;
					}

					if (ImGui::TreeNode("Shader Description"))
					{
						float offset = 250.0f;

						ImGui::SeparatorText("General");
						ImGui::Text("Version:");				ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.Version).c_str());
						ImGui::Text("Creator:");				ImGui::SameLine(offset); ImGui::Text(shaderDesc.Creator);
						ImGui::Text("Flags:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.Flags).c_str());

						ImGui::SeparatorText("Resources");
						ImGui::Text("Constant Buffers:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.ConstantBuffers).c_str());
						ImGui::Text("Bound Resources:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.BoundResources).c_str());
						ImGui::Text("Input Parameters:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.InputParameters).c_str());
						ImGui::Text("Output Parameters:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.OutputParameters).c_str());
						ImGui::Text("Temp Registers:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TempRegisterCount).c_str());
						ImGui::Text("Temp Arrays:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TempArrayCount).c_str());

						ImGui::SeparatorText("Definitions");
						ImGui::Text("Constant Defines:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.DefCount).c_str());
						ImGui::Text("Declarations:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.DclCount).c_str());

						ImGui::SeparatorText("Instructions");
						ImGui::Text("Total:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.InstructionCount).c_str());
						ImGui::Text("Float:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.FloatInstructionCount).c_str());
						ImGui::Text("Int:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.IntInstructionCount).c_str());
						ImGui::Text("Uint:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.UintInstructionCount).c_str());
						ImGui::Text("Array:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.ArrayInstructionCount).c_str());
						ImGui::Text("Macro:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.MacroInstructionCount).c_str());
						ImGui::Text("Cut:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.CutInstructionCount).c_str());
						ImGui::Text("Emit:");					ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.EmitInstructionCount).c_str());
						ImGui::Text("Dynamic Flow Control:");	ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.DynamicFlowControlCount).c_str());
						ImGui::Text("Static Flow Control:");	ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.StaticFlowControlCount).c_str());
						ImGui::Text("Texture Reads:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TextureLoadInstructions).c_str());
						ImGui::Text("Texture Writes:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.cTextureStoreInstructions).c_str());
						ImGui::Text("Texture Normal:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TextureNormalInstructions).c_str());
						ImGui::Text("Texture Comparison:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TextureCompInstructions).c_str());
						ImGui::Text("Texture Bias:");			ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TextureBiasInstructions).c_str());
						ImGui::Text("Texture Gradient:");		ImGui::SameLine(offset); ImGui::Text(std::to_string(shaderDesc.TextureGradientInstructions).c_str());

						ImGui::Separator();
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Parameters"))
					{
						ImGui::PushID("ParameterDescs");
						for (size_t j = 0; j < shaderDesc.InputParameters; j++)
						{
							D3D11_SIGNATURE_PARAMETER_DESC desc;
							if (FAILED(reflector->GetInputParameterDesc(j, &desc)))
							{
								DbgMsg("Failed to get input parameter description!");
								continue;
							}

							ImGui::PushID(j);
							if (ImGui::TreeNode(desc.SemanticName))
							{
								ImGui::Text("Semantic Index: %d",		desc.SemanticIndex);
								ImGui::Text("System Value Type: %d",	desc.SystemValueType);
								ImGui::Text("Component Type: %d",		desc.ComponentType);

								ImGui::TreePop();
							}
							ImGui::PopID();

						}
						ImGui::PopID();

						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Resource Bindings"))
					{
						ImGui::PushID("ResourceBindingDescs");
						for (size_t j = 0; j < shaderDesc.BoundResources; j++)
						{
							D3D11_SHADER_INPUT_BIND_DESC desc;
							if (FAILED(reflector->GetResourceBindingDesc(j, &desc)))
							{
								DbgMsg("Failed to get resource binding description!");
								continue;
							}

							ImGui::PushID(j);
							if (ImGui::TreeNode(desc.Name))
							{
								ImGui::Text("Bind Point: %d",	desc.BindPoint);
								ImGui::Text("Bind Count: %d",	desc.BindCount);
								ImGui::Text("Dimension: %d",	desc.Dimension);
								ImGui::Text("Type: %d",			desc.Type);
								ImGui::Text("Return Type: %d",	desc.ReturnType);
								ImGui::Text("uFlags: %d",		desc.uFlags);

								ImGui::TreePop();
							}
							ImGui::PopID();
						}
						ImGui::PopID();

						ImGui::TreePop();
					}

					ImGui::Separator();
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Materials"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Borders;
		childFlags |= ImGuiChildFlags_ResizeX;
		childFlags |= ImGuiChildFlags_ResizeY;

		ImGui::BeginChild("##MaterialChild", ImVec2(300, 0), childFlags);
		for (int i = 0; i < _materialVec.size(); i++)
		{
			const Material *material = _materialVec[i];
			const std::string name = std::format("Material #{}", i);

			ImGui::PushID(name.c_str());
			if (ImGui::TreeNode(name.c_str()))
			{
				if (material->textureID != CONTENT_NULL)
					ImGui::Text(std::format("Texture: [{}] {}", material->textureID, GetTextureName(material->textureID)).c_str());

				if (material->normalID != CONTENT_NULL)
					ImGui::Text(std::format("Normal Map: [{}] {}", material->normalID, GetTextureName(material->normalID)).c_str());

				if (material->specularID != CONTENT_NULL)
					ImGui::Text(std::format("Specular Map: [{}] {}", material->specularID, GetTextureName(material->specularID)).c_str());

				if (material->glossinessID != CONTENT_NULL)
					ImGui::Text(std::format("Glossiness Map: [{}] {}", material->glossinessID, GetTextureName(material->glossinessID)).c_str());

				if (material->ambientID != CONTENT_NULL)
					ImGui::Text(std::format("Ambient Map: [{}] {}", material->ambientID, GetTextureName(material->ambientID)).c_str());

				if (material->reflectiveID != CONTENT_NULL)
					ImGui::Text(std::format("Reflection Map: [{}] {}", material->reflectiveID, GetTextureName(material->reflectiveID)).c_str());

				if (material->occlusionID != CONTENT_NULL)
					ImGui::Text(std::format("Occlusion: [{}] {}", material->occlusionID, GetTextureName(material->occlusionID)).c_str());

				if (material->samplerID != CONTENT_NULL)
					ImGui::Text(std::format("Sampler: [{}] {}", material->samplerID, GetSamplerName(material->samplerID)).c_str());

				if (material->vsID != CONTENT_NULL)
					ImGui::Text(std::format("Vertex Shader: [{}] {}", material->vsID, GetShaderName(material->vsID)).c_str());

				if (material->psID != CONTENT_NULL)
					ImGui::Text(std::format("Pixel Shader: [{}] {}", material->psID, GetShaderName(material->psID)).c_str());

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Info"))
	{
		ImGui::Text("Materials: %d/%d", _materialVec.size(), _materialVec.capacity());
		ImGui::Dummy({ 0, 8 });
		ImGui::Text("Meshes: %d", _meshes.size());
		ImGui::Text("Shaders: %d", _shaders.size());
		ImGui::Text("Textures: %d", _textures.size());
		ImGui::Text("Cubemaps: %d", _cubemaps.size());
		ImGui::Text("Samplers: %d", _samplers.size());
		ImGui::Text("Blend States: %d", _blendStates.size());
		ImGui::Text("Input Layouts: %d", _inputLayouts.size());
		ImGui::Text("Sounds: %d", _sounds.size());
		ImGui::Text("Height Maps: %d", _heightTextures.size());

		ImGui::TreePop();
	}

	return true;
}
#endif


CompiledData Content::GetMeshData(const char *path) const
{
	CompiledData data{};

	MeshData *meshData = new MeshData();

	if (!LoadMeshFromFile(path, meshData))
	{
		ErrMsg("Failed to load mesh from file!");
		return data;
	}

	std::vector<char> dataChars;
	meshData->Compile(dataChars);

	data.size = dataChars.size();
	data.data = new char[data.size];
	std::memcpy(data.data, dataChars.data(), data.size);
	return data;
}
CompiledData Content::GetShaderData(const std::string &name, const char *path, ShaderType shaderType) const
{
	CompiledData data{};

	std::string shaderFileData;
	std::ifstream reader;

	reader.open(path, std::ios::binary | std::ios::ate);
	if (!reader.is_open())
	{
		ErrMsg("Failed to open shader file!");
		return data;
	}

	reader.seekg(0, std::ios::end);
	shaderFileData.reserve(static_cast<unsigned int>(reader.tellg()));
	reader.seekg(0, std::ios::beg);

	shaderFileData.assign(
		std::istreambuf_iterator<char>(reader),
		std::istreambuf_iterator<char>()
	);

	shaderFileData.clear();
	reader.close();

	data.size = shaderFileData.length();
	data.data = new char[data.size];
	std::memcpy(data.data, shaderFileData.c_str(), data.size);
	return data;
}


UINT Content::AddMesh(ID3D11Device *device, const std::string &name, MeshData **meshData)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma warning(disable: 6993)
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_meshes.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_meshes[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			break;
		}

		if (!duplicateName)
		{
			Mesh *addedMesh = new Mesh(name, id);
			if (!addedMesh->data.Initialize(device, meshData))
			{
				delete (*meshData);
				delete addedMesh;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added mesh!");
			}
			else
			{
				_meshes.emplace_back(addedMesh);
			}
		}
	}
#pragma warning(default: 6993)

	meshData = nullptr;
	return id;
}
UINT Content::AddMesh(ID3D11Device *device, const std::string &name, const char *path)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	MeshData *meshData = new MeshData();
	if (!LoadMeshFromFile(path, meshData))
	{
		delete meshData;
		ErrMsg("Failed to load mesh from file!");
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma warning(disable: 6993)
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_meshes.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_meshes[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			delete meshData;
			break;
		}

		if (!duplicateName)
		{
			Mesh *addedMesh = new Mesh(name, id);
			if (!addedMesh->data.Initialize(device, &meshData))
			{
				delete meshData;
				delete addedMesh;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added mesh!");
			}
			else
			{
				_meshes.emplace_back(addedMesh);
			}
		}
	}
#pragma warning(default: 6993)

	meshData = nullptr;
	return id;
}

UINT Content::AddShader(ID3D11Device *device, const std::string &name, ShaderType shaderType, ID3DBlob *&shaderBlob)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma warning(disable: 6993)
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_shaders.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_shaders[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			break;
		}

		if (!duplicateName)
		{
			Shader *addedShader = new Shader(name, id);
			if (!addedShader->data.Initialize(device, shaderType, shaderBlob))
			{
				delete addedShader;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added shader!");
			}
			else
			{
				_shaders.emplace_back(addedShader);
			}
		}
	}
#pragma warning(default: 6993)

	shaderBlob = nullptr;
	return id;
}
UINT Content::AddShader(ID3D11Device *device, const std::string &name, const ShaderType shaderType, const void *dataPtr, const size_t dataSize)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma warning(disable: 6993)
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_shaders.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_shaders[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			break;
		}

		if (!duplicateName)
		{
			Shader *addedShader = new Shader(name, id);
			if (!addedShader->data.Initialize(device, shaderType, dataPtr, dataSize))
			{
				delete addedShader;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added shader!");
			}
			else
			{
				_shaders.emplace_back(addedShader);
			}
		}
	}
#pragma warning(default: 6993)

	return id;
}
UINT Content::AddShader(ID3D11Device *device, const std::string &name, ShaderType shaderType, const std::string &path)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma warning(disable: 6993)
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_shaders.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_shaders[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			break;
		}

		if (!duplicateName)
		{
			Shader *addedShader = new Shader(name, id);
			if (!addedShader->data.Initialize(device, shaderType, path.c_str()))
			{
				delete addedShader;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added shader!");
			}
			else
			{
				_shaders.emplace_back(addedShader);
			}
		}
	}
#pragma warning(default: 6993)

	return id;
}

ID3DBlob *Content::CompileShader(ID3D11Device *device, const std::string &path, ShaderType shaderType) const
{
	ZoneScopedC(RandomUniqueColor());

	// Prefer higher shader profile when possible as 5.0 provides better performance on 11-class hardware.
	bool useV5 = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;

	std::string shaderVersion;
	switch (shaderType)
	{
	case ShaderType::VERTEX_SHADER:
		shaderVersion = useV5 ? "vs_5_0" : "vs_4_0";
		break;
	case ShaderType::HULL_SHADER:
		shaderVersion = useV5 ? "hs_5_0" : "hs_4_0";
		break;
	case ShaderType::DOMAIN_SHADER:
		shaderVersion = useV5 ? "ds_5_0" : "ds_4_0";
		break;
	case ShaderType::GEOMETRY_SHADER:
		shaderVersion = useV5 ? "gs_5_0" : "gs_4_0";
		break;
	case ShaderType::PIXEL_SHADER:
		shaderVersion = useV5 ? "ps_5_0" : "ps_4_0";
		break;
	case ShaderType::COMPUTE_SHADER:
		shaderVersion = useV5 ? "cs_5_0" : "cs_4_0";
		break;
	default:
		break;
	}

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	flags |= D3DCOMPILE_DEBUG;
#endif

	LPCSTR profile = shaderVersion.c_str();

	const D3D_SHADER_MACRO defines[] =
	{
		"__HLSL", "1",
		"RECOMPILE", "1",
		NULL, NULL
	};

	ID3DBlob *shaderBlob = nullptr;
	ID3DBlob *errorBlob = nullptr;

	HRESULT hr = D3DCompileFromFile(
		std::wstring(path.begin(), path.end()).c_str(),
		defines,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", profile, flags, 0,
		&shaderBlob, &errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			DbgMsgF("Shader compilation failed: {}!", static_cast<const char *>(errorBlob->GetBufferPointer()));
			errorBlob->Release();
		}
		else
		{
			DbgMsg("Shader compilation failed with unknown error!");
		}
		
		return nullptr;
	}

	if (errorBlob)
		errorBlob->Release();

	return shaderBlob;
}
bool Content::RecompileShader(ID3D11Device *device, const std::string &name) const
{
	ZoneScopedC(RandomUniqueColor());

	ShaderD3D11 *shader = GetShader(name);
	if (!shader)
	{
		ErrMsgF("Shader '{}' not found!", name);
		return false;
	}

	ShaderType shaderType = shader->GetShaderType();
	std::string path = PATH_FILE_EXT(ENGINE_PATH_SHADERS, name, "hlsl");

	auto shaderBlob = CompileShader(device, path, shaderType);

	if (!shaderBlob)
	{
		DbgMsgF("Failed to recompile shader '{}'", name);
		return false;
	}

	if (!shader->Initialize(device, shaderType, shaderBlob))
	{
		ErrMsg("Failed to reinitialize shader after recompilation!");
		shaderBlob->Release();
		return false;
	}

	return true;
}

UINT Content::AddTexture(ID3D11Device *device, ID3D11DeviceContext *context, const std::string &name, const std::string &path, DXGI_FORMAT format, bool useMipmaps, int downsample)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;
#pragma warning(disable: 6993)
#pragma omp critical
	{
		if (!IsNameDuplicate(name, _textures, &id))
		{
			ComPtr<ID3D11Texture2D> texture;
			ComPtr<ID3D11ShaderResourceView> srv;

			bool failed = false;
			if (!LoadTextureFromFile(device, context, path, *texture.GetAddressOf(), *srv.GetAddressOf(), format, useMipmaps, downsample))
			{
				Warn("Failed to load texture from file!");
				failed = true;
				id = CONTENT_NULL;
			}

			if (!failed)
			{
				Texture *addedTexture = new Texture(name, std::string(path), id, useMipmaps);
				if (!addedTexture->data.Initialize(std::move(texture), std::move(srv)))
				{
					delete addedTexture;
					id = CONTENT_NULL;
					Warn("Failed to initialize added texture!");
				}
				else
				{
					_textures.emplace_back(addedTexture);
				}
			}
		}
	}
#pragma warning(default: 6993)

	return id;
}
UINT Content::AddCubemap(ID3D11Device *device, ID3D11DeviceContext *context, const std::string &name, const std::string &path, DXGI_FORMAT format, bool useMipmaps, int downsample)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT width, height, inChannels, inBitsPerChannel;
	std::vector<unsigned char> texData;

	if (!LoadTextureFromFile(path, texData, width, height, inChannels, inBitsPerChannel))
	{
		ErrMsg("Failed to load cubemap texture from file!");
		return CONTENT_NULL;
	}

	UINT inBitsPerPixel = inChannels * inBitsPerChannel;
	UINT inBytesPerChannel = inBitsPerChannel / 8;
	UINT inBytesPerPixel = inChannels * inBytesPerChannel;

	downsample += MIPS_DISCARDED;
	if (downsample > 0)
	{
		UINT newWidth = width >> downsample;
		UINT newHeight = height >> downsample;

		if (newWidth > 0 && newHeight > 0)
		{
			if (DownsampleTexture(texData, width, height, newWidth, newHeight))
			{
				width = newWidth;
				height = newHeight;
			}
			else
				Warn("Failed to downsample texture!");
		}
	}

	dx::XMINT4 outBitLayout = D3D11FormatData::GetBitsPerChannel(format);
	if (outBitLayout.x == 0)
	{
		ErrMsg("Texture formats with variable or undefined bit layouts are not currently supported!");
		return CONTENT_NULL;
	}

	UINT outChannels = D3D11FormatData::GetChannelCount(format);
	UINT outBitsPerPixel = D3D11FormatData::GetBitsPerPixel(format);
	UINT outBitsPerChannel = outBitsPerPixel / outChannels;
	UINT outBytesPerPixel = outBitsPerPixel / 8;
	UINT outBytesPerChannel = outBitsPerChannel / 8;

	if (outBitsPerChannel < 8)
	{
		ErrMsg("Texture formats with sub-byte channels are not currently supported!");
		return CONTENT_NULL;
	}

	int bitDiff = (int)outBitsPerChannel - (int)inBitsPerChannel;
	double precisionChange = std::pow(2.0, bitDiff);

	std::vector<uint8_t> formattedTexData;
	formattedTexData.resize((size_t)width * height * outBytesPerPixel);

	uint8_t *inRawData = texData.data();
	uint8_t *outRawData = formattedTexData.data();

	for (size_t pixel = 0; pixel < (size_t)width * height; pixel++)
	{
		uint8_t *inPixelPtr = inRawData + (pixel * inBytesPerPixel);
		uint8_t *outPixelPtr = outRawData + (pixel * outBytesPerPixel);

		for (size_t channel = 0; channel < outChannels; channel++)
		{
			uint8_t *inChannelPtr = inPixelPtr + (channel * inBytesPerChannel);
			uint8_t *outChannelPtr = outPixelPtr + (channel * outBytesPerChannel);

			if (channel < inChannels)
			{
				if (outBytesPerChannel == 1 && inBytesPerChannel == 1)
				{
					(*outChannelPtr) = (uint8_t)((*inChannelPtr) * precisionChange);
				}
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 1)
				{
					(*(uint16_t *)outChannelPtr) = (uint16_t)((*inChannelPtr) * precisionChange);
				}
				else if (outBytesPerChannel == 1 && inBytesPerChannel == 2)
				{
					(*outChannelPtr) = (uint8_t)((*(uint16_t *)inChannelPtr) * precisionChange);
				}
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 2)
				{
					(*(uint16_t *)outChannelPtr) = (uint16_t)((*(uint16_t *)inChannelPtr) * precisionChange);
				}
			}
			else if (channel == 3) // Set alpha channel to maximum value by default
			{
				if (outBytesPerChannel == 1 && inBytesPerChannel == 1)
				{
					*outChannelPtr = (uint8_t)NumericLimit(*outChannelPtr);
				}
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 1)
				{
					*(uint16_t *)outChannelPtr = (uint16_t)NumericLimit(*(uint16_t *)outChannelPtr);
				}
				else if (outBytesPerChannel == 1 && inBytesPerChannel == 2)
				{
					*outChannelPtr = (uint8_t)NumericLimit(*outChannelPtr);
				}
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 2)
				{
					*(uint16_t *)outChannelPtr = (uint16_t)NumericLimit(*(uint16_t *)outChannelPtr);
				}
			}
		}
	}

	UINT id = CONTENT_NULL;

#pragma warning(disable: 6993)
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_cubemaps.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_cubemaps[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			break;
		}

		if (!duplicateName)
		{
			Cubemap *addedCubemap = new Cubemap(name, std::string(path), id);
			if (!addedCubemap->data.Initialize(device, context, width, height, formattedTexData.data(), format, useMipmaps, true))
			{
				delete addedCubemap;
				id = CONTENT_NULL;
				Warn("Failed to initialize added cubemap!");
			}
			else
			{
				_cubemaps.emplace_back(addedCubemap);
			}
		}
	}
#pragma warning(default: 6993)

	return id;
}

UINT Content::AddSampler(ID3D11Device *device, const std::string &name, 
	D3D11_TEXTURE_ADDRESS_MODE adressMode, D3D11_FILTER filter, D3D11_COMPARISON_FUNC comparisonFunc)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	const UINT id = static_cast<UINT>(_samplers.size());
	for (UINT i = 0; i < id; i++)
	{
		if (_samplers[i]->name == name)
			return i;
	}

	Sampler* addedSampler = new Sampler(name, id);
	if (!addedSampler->data.Initialize(device, adressMode, filter, comparisonFunc))
	{
		delete addedSampler;
		ErrMsgF("Failed to initialize added sampler '{}'!", name);
		return CONTENT_NULL;
	}
	_samplers.emplace_back(addedSampler);

	return id;
}
UINT Content::AddSampler(ID3D11Device *device, const std::string &name, const D3D11_SAMPLER_DESC &samplerDesc)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	const UINT id = static_cast<UINT>(_samplers.size());
	for (UINT i = 0; i < id; i++)
	{
		if (_samplers[i]->name == name)
			return i;
	}

	Sampler* addedSampler = new Sampler(name, id);
	if (!addedSampler->data.Initialize(device, samplerDesc))
	{
		delete addedSampler;
		ErrMsgF("Failed to initialize added sampler '{}'!", name);
		return CONTENT_NULL;
	}
	_samplers.emplace_back(addedSampler);

	return id;
}

UINT Content::AddBlendState(ID3D11Device *device, const std::string &name, const D3D11_BLEND_DESC &blendDesc)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		WarnF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	const UINT id = static_cast<UINT>(_blendStates.size());
	for (UINT i = 0; i < id; i++)
	{
		if (_blendStates[i]->name == name)
			return i;
	}

	BlendState *addedBlendState = new BlendState(name, id);
	if (FAILED(device->CreateBlendState(&blendDesc, addedBlendState->data.ReleaseAndGetAddressOf())))
	{
		delete addedBlendState;
		ErrMsgF("Failed to initialize added blend state '{}'!", name);
		return CONTENT_NULL;
	}
	_blendStates.emplace_back(addedBlendState);

	return id;
}

UINT Content::AddInputLayout(ID3D11Device *device, const std::string &name, const std::vector<Semantic> &semantics,
	const void *vsByteData, const size_t vsByteSize)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	const UINT id = static_cast<UINT>(_inputLayouts.size());
	for (UINT i = 0; i < id; i++)
	{
		if (_inputLayouts[i]->name == name)
			return i;
	}

	InputLayout* addedInputLayout = new InputLayout(name, id);
	for (const Semantic& semantic : semantics)
	{
		if (!addedInputLayout->data.AddInputElement(semantic))
		{
			delete addedInputLayout;
			ErrMsgF("Failed to add element \"{}\" to input layout!", semantic.name);
			return CONTENT_NULL;
		}
	}

	if (!addedInputLayout->data.FinalizeInputLayout(device, vsByteData, vsByteSize))
	{
		delete addedInputLayout;
		ErrMsgF("Failed to finalize added input layout '{}'!", name);
		return CONTENT_NULL;
	}
	_inputLayouts.emplace_back(addedInputLayout);

	return id;
}
UINT Content::AddInputLayout(ID3D11Device *device, const std::string &name, const std::vector<Semantic> &semantics, const UINT vShaderID)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	if (vShaderID == CONTENT_NULL)
	{
		ErrMsg("Failed to get vertex shader byte code, shader ID was CONTENT_NULL!");
		return CONTENT_NULL;
	}

	const ShaderD3D11* vShader = GetShader(vShaderID);
	if (vShader == nullptr)
	{
		ErrMsg("Failed to get vertex shader byte code, shader ID returned nullptr!");
		return CONTENT_NULL;
	}

	if (vShader->GetShaderType() != ShaderType::VERTEX_SHADER)
	{
		ErrMsgF("Failed to get vertex shader byte code, shader ID returned invalid type ({})!", (UINT)vShader->GetShaderType());
		return CONTENT_NULL;
	}

	return AddInputLayout(device, name, semantics, vShader->GetShaderByteData(), vShader->GetShaderByteSize());
}

static bool BlurHeightMap(const HeightMap &hm, const char* path)
{
	/*
	std::vector<float> originalData(hm.GetHeightValues());  // Read-only in each iteration
	std::vector<float> temp(originalData); // Buffer for updates

	const UINT w = hm.GetWidth(), h = hm.GetHeight();
	const float threshold = 0.97f;

	for (UINT y = 0; y < h; y++)
	{
		for (UINT x = 0; x < w; x++)
		{
			if (originalData[y * w + x] >= threshold)
			{
				temp[y * w + x] = 0;
			}
		}
	}

	// Write final blurred heightmap
	if (!WriteTextureToFile(path, w, h, temp, 1, false))
	{
		ErrMsg("Failed to blur height map");
		return false;
	}
	*/

	/*
	std::vector<float> originalData(hm.GetHeightValues());  // Read-only in each iteration
	std::vector<float> temp(originalData); // Buffer for updates

	const UINT w = hm.GetWidth(), h = hm.GetHeight();
	const float threshold = 0.6f;

	for (UINT i = 0; i < 20; i++) 
	{
		for (UINT y = 0; y < h; y++)
		{
			for (UINT x = 0; x < w; x++)
			{
				if (originalData[y * w + x] >= threshold)
				{
					float avg = INFINITY;
					int avgCount = 0;

					for (int dy = -1; dy <= 1; dy++)
						for (int dx = -1; dx <= 1; dx++)
						{
							int nx = x + dx, 
								ny = y + dy;
							if (dy != dx && nx >= 0 && nx < (int)w && ny >= 0 && ny < (int)h)
								if (originalData[ny * w + nx] < threshold) 	
								{
									avg = std::min<float>(avg, originalData[ny * w + nx]);
									avgCount = 1;
								}
						}

					if (avgCount > 0)
						temp[y * w + x] = avg / avgCount;
				}
			}
		}

		for (int i = 0; i < w * h; i++)
			originalData[i] = temp[i];

		DbgMsg(std::format("Iteration: {}/20"), i + 1);
	}

	// Write final blurred heightmap
	if (!WriteTextureToFile(path, w, h, originalData, 1, false))
	{
		ErrMsg("Failed to blur height map");
		return false;
	}
	*/
	return true;
}
UINT Content::AddHeightMap(const std::string &name, const std::string &path)
{
	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT width, height;
	std::vector<float> texData;

	if (!LoadTextureFromFile(path, width, height, texData, 1, true))
	{
		ErrMsgF("Failed to load heightmap from file '{}'!", path);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;
#pragma omp critical
	{
		bool duplicateName = false;
		id = (UINT)_heightTextures.size();
		for (UINT i = 0; i < id; i++)
		{
			if (_heightTextures[i]->name != name)
				continue;

			duplicateName = true;
			id = i;
			break;
		}

		if (!duplicateName)
		{
			HeightTexture *ht = new HeightTexture(name, id);
			ht->data.Initialize(texData, width, height, name);

			_heightTextures.emplace_back(ht);
		}
	}

#if(0)
	if (name == "CaveHeightmap")
	{
		if (!BlurHeightMap(ht->data, path))
		{
			ErrMsg("Failed to add height map");
			return false;
		}
	}
#endif

	return id;
}

UINT Content::GetMeshCount() const
{
	return static_cast<UINT>(_meshes.size());
}
UINT Content::GetTextureCount() const
{
	return static_cast<UINT>(_textures.size());
}
UINT Content::GetCubemapCount() const
{
	return static_cast<UINT>(_cubemaps.size());
}
UINT Content::GetSamplerCount() const
{
	return static_cast<UINT>(_samplers.size());
}
UINT Content::GetBlendStateCount() const
{
	return static_cast<UINT>(_blendStates.size());
}

void Content::GetMeshNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_meshes.size());
	for (const Mesh *mesh : _meshes)
		names->emplace_back(mesh->name);
}
void Content::GetShaderNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_shaders.size());
	for (const Shader *shader : _shaders)
		names->emplace_back(shader->name);
}
void Content::GetSamplerNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_samplers.size());
	for (const Sampler *sampler : _samplers)
		names->emplace_back(sampler->name);
}
void Content::GetBlendStateNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_blendStates.size());
	for (const BlendState *blendState : _blendStates)
		names->emplace_back(blendState->name);
}
void Content::GetTextureNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_textures.size());
	for (const Texture *texture : _textures)
		names->emplace_back(texture->name);
}
void Content::GetCubemapNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_cubemaps.size());
	for (const Cubemap *cubemap : _cubemaps)
		names->emplace_back(cubemap->name);
}

UINT Content::GetMeshID(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return GetMeshID("Error");

	std::string lookupName;

	if (name.find("", 0) != 0)
		lookupName = "" + name; // Add prefix if it is missing.
	else
		lookupName = name;

	const UINT count = static_cast<UINT>(_meshes.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_meshes[i]->name == lookupName)
			return i;
	}

	if (lookupName == "Error")
	{
		ErrMsg("No error mesh defined!");
		return CONTENT_NULL;
	}

	return GetMeshID("Error");
}
std::string Content::GetMeshName(UINT id) const
{
	if (id >= _meshes.size())
		return "Uninitialized";
	return _meshes[id]->name;
}
MeshD3D11 *Content::GetMesh(const std::string &name) const
{
	UINT id = GetMeshID(name);
	return GetMesh(id);
}
MeshD3D11 *Content::GetMesh(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		WarnF("Failed to find mesh #{}! Returning default.", id);
		return &_meshes[0]->data;
	}

	if (id >= _meshes.size())
	{
		WarnF("Failed to find mesh #{}! Returning default.", id);
		return &_meshes[0]->data;
	}

	return &_meshes[id]->data;
}

UINT Content::GetShaderID(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	const UINT count = static_cast<UINT>(_shaders.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_shaders[i]->name == name)
			return i;
	}

	return CONTENT_NULL;
}
std::string Content::GetShaderName(UINT id) const
{
	if (id >= _shaders.size())
		return "Uninitialized";
	return _shaders[id]->name;
}
ShaderD3D11 *Content::GetShader(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_shaders.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_shaders[i]->name == name)
			return &_shaders[i]->data;
	}

	WarnF("Failed to find shader '{}'! Returning default.", name);
	return &_shaders[0]->data;
}
ShaderD3D11 *Content::GetShader(const UINT id) const
{
	if (id == CONTENT_NULL || id >= _shaders.size())
	{
		WarnF("Failed to find shader #{}! Returning default.", id);
		return &_shaders[0]->data;
	}

	return &_shaders[id]->data;
}

void Content::GetShadersOfType(std::vector<UINT> &shaders, ShaderType type)
{
	shaders.clear();
	shaders.reserve(_shaders.size() / 2);

	for (int i = 0; i < _shaders.size(); i++)
	{
		const Shader *shader = _shaders[i];
		if (shader->data.GetShaderType() == type)
			shaders.emplace_back(i);
	}
}
ShaderType Content::GetShaderTypeFromName(const std::string &name)
{
	char firstLetter = std::toupper(name[0]);

	if (firstLetter == 'V')
		return ShaderType::VERTEX_SHADER;
	else if (firstLetter == 'P')
		return ShaderType::PIXEL_SHADER;
	else if (firstLetter == 'G')
		return ShaderType::GEOMETRY_SHADER;
	else if (firstLetter == 'C')
		return ShaderType::COMPUTE_SHADER;
	else if (firstLetter == 'H')
		return ShaderType::HULL_SHADER;
	else if (firstLetter == 'D')
		return ShaderType::DOMAIN_SHADER;

	return ShaderType::VERTEX_SHADER;
}

bool Content::HasTexture(const std::string & name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return false;

	std::string lookupName;

	if (name.find("", 0) != 0)
		lookupName = "" + name; // Add prefix if it is missing.
	else
		lookupName = name;

	const UINT count = static_cast<UINT>(_textures.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_textures[i]->name == lookupName)
			return true;
	}

	return false;
}
bool Content::HasCubemap(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return false;

	const UINT count = static_cast<UINT>(_cubemaps.size());
	for (UINT i = 0; i < count; i++)
	{
		if (_cubemaps[i]->name == name)
			return true;
	}

	return false;
}

UINT Content::GetTextureID(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return GetTextureID("Fallback");

	std::string lookupName;

	if (name.find("", 0) != 0)
		lookupName = "" + name; // Add prefix if it is missing.
	else
		lookupName = name;

	const UINT count = static_cast<UINT>(_textures.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_textures[i]->name == lookupName)
			return i;
	}

	if (lookupName == "Fallback")
	{
		Warn("No fallback texture defined!");
		return CONTENT_NULL;
	}

	return GetTextureID("Fallback");
}
std::string Content::GetTextureName(UINT id) const
{
	if (id >= _textures.size())
		return "Uninitialized";
	return _textures[id]->name;
}
UINT Content::GetTextureIDByPath(const std::string &path) const
{
	const UINT count = static_cast<UINT>(_textures.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_textures[i]->path == path)
			return i;
	}

	return CONTENT_NULL;
}
ShaderResourceTextureD3D11 *Content::GetTexture(const UINT id) const
{
	if (id >= _textures.size())
		return GetTexture("Fallback");

	return &_textures[id]->data;
}
ShaderResourceTextureD3D11 *Content::GetTexture(const std::string &name) const
{
	UINT id = GetTextureID(name);
	return GetTexture(id);
}

UINT Content::GetCubemapID(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return false;

	const UINT count = static_cast<UINT>(_cubemaps.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_cubemaps[i]->name == name)
			return i;
	}

	return CONTENT_NULL;
}
std::string Content::GetCubemapName(UINT id) const
{
	if (id >= _cubemaps.size())
		return "Uninitialized";
	return _cubemaps[id]->name;
}
UINT Content::GetCubemapIDByPath(const std::string &path) const
{
	const UINT count = static_cast<UINT>(_cubemaps.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_cubemaps[i]->path == path)
			return i;
	}

	return CONTENT_NULL;
}
ShaderResourceTextureD3D11 *Content::GetCubemap(UINT id) const
{
	if (id >= _cubemaps.size())
		return GetCubemap("Fallback");

	return &_cubemaps[id]->data;
}
ShaderResourceTextureD3D11 *Content::GetCubemap(const std::string &name) const
{
	UINT id = GetCubemapID(name);
	return GetCubemap(id);
}

UINT Content::GetSamplerID(const std::string &name) const
{
	if (name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	std::string lookupName;

	if (name.find("", 0) != 0)
		lookupName = "" + name; // Add prefix if it is missing.
	else
		lookupName = name;

	const UINT count = static_cast<UINT>(_samplers.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_samplers[i]->name == lookupName)
			return i;
	}

	return CONTENT_NULL;
}
std::string Content::GetSamplerName(UINT id) const
{
	if (id >= _samplers.size())
		return "Uninitialized";
	return _samplers[id]->name;
}
SamplerD3D11 *Content::GetSampler(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_samplers.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_samplers[i]->name == name)
			return &_samplers[i]->data;
	}

	DbgMsg(std::format("Failed to find sampler '{}'! Returning default.", name));
	return &_samplers[0]->data;
}
SamplerD3D11 *Content::GetSampler(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsg(std::format("Failed to find sampler #{}! Returning default.", id));
		return &_samplers[0]->data;
	}

	if (id >= _samplers.size())
	{
		DbgMsg(std::format("Failed to find sampler #{}! Returning default.", id));
		return &_samplers[0]->data;
	}

	return &_samplers[id]->data;
}

UINT Content::GetBlendStateID(const std::string &name) const
{
	if (name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	std::string lookupName;

	if (name.find("", 0) != 0)
		lookupName = "" + name; // Add prefix if it is missing.
	else
		lookupName = name;

	const UINT count = static_cast<UINT>(_blendStates.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_blendStates[i]->name == lookupName)
			return i;
	}

	return CONTENT_NULL;
}
std::string Content::GetBlendStateName(UINT id) const
{
	if (id >= _blendStates.size())
		return "Uninitialized";
	return _blendStates[id]->name;
}
ID3D11BlendState *Content::GetBlendState(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_blendStates.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_blendStates[i]->name == name)
			return _blendStates[i]->data.Get();
	}

	DbgMsgF("Failed to find blend state '{}'! Returning default.", name);
	return _blendStates[0]->data.Get();
}
ID3D11BlendState *Content::GetBlendState(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return _blendStates[0]->data.Get();
	}

	if (id >= _blendStates.size())
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return _blendStates[0]->data.Get();
	}

	return _blendStates[id]->data.Get();
}
ComPtr<ID3D11BlendState> *Content::GetBlendStateAddress(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_blendStates.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_blendStates[i]->name == name)
			return &_blendStates[i]->data;
	}

	DbgMsgF("Failed to find blend state '{}'! Returning default.", name);
	return &_blendStates[0]->data;
}
ComPtr<ID3D11BlendState> *Content::GetBlendStateAddress(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return &_blendStates[0]->data;
	}

	if (id >= _blendStates.size())
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return &_blendStates[0]->data;
	}

	return &_blendStates[id]->data;
}

UINT Content::GetInputLayoutID(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_inputLayouts.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_inputLayouts[i]->name == name)
			return i;
	}

	return CONTENT_NULL;
}
InputLayoutD3D11 *Content::GetInputLayout(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_inputLayouts.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_inputLayouts[i]->name == name)
			return &_inputLayouts[i]->data;
	}

	DbgMsg(std::format("Failed to find input layout '{}'! Returning default.", name));
	return &_inputLayouts[0]->data;
}
InputLayoutD3D11 *Content::GetInputLayout(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		ErrMsgF("Failed to find input layout #{}! Returning default.", id);
		return &_inputLayouts[0]->data;
	}

	if (id >= _inputLayouts.size())
	{
		ErrMsgF("Failed to find input layout #{}! Returning default.", id);
		return &_inputLayouts[0]->data;
	}

	return &_inputLayouts[id]->data;
}

UINT Content::GetSoundID(const std::string &name) const
{
	if (name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	const UINT count = static_cast<UINT>(_sounds.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_sounds[i]->name == name)
			return i;
	}

	return CONTENT_NULL;
}
SoundSource *Content::GetSound(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_sounds.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_sounds[i]->name == name)
			return &_sounds[i]->data;
	}

	return nullptr;
}
SoundSource *Content::GetSound(UINT id) const
{
	if (id == CONTENT_NULL)
		return nullptr;
	if (id >= _sounds.size())
		return nullptr;

	return &_sounds[id]->data;
}

UINT Content::GetHeightMapID(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_heightTextures.size());

	for (UINT i = 0; i < count; i++)
	{
		if (_heightTextures[i]->name == name)
			return i;
	}

	return CONTENT_NULL;
}
HeightMap *Content::GetHeightMap(const std::string &name) const
{
	const UINT count = static_cast<UINT>(_heightTextures.size());

	for (UINT i = 0; i < count; i++)
		if (_heightTextures[i]->name == name)
			return &_heightTextures[i]->data;
	return nullptr;
}
HeightMap *Content::GetHeightMap(UINT id) const
{
	if (id == CONTENT_NULL)
		return nullptr;
	if (id >= _heightTextures.size())
		return nullptr;

	return &_heightTextures[id]->data;
}

// Looks for material with the same properties as the arguments.
// If one exists, that one is returned. Otherwise it is created and returned.
const Material *Content::GetOrAddMaterial(Material mat)
{
	auto it = _materialSet.find(&mat);
	if (it != _materialSet.end())
		return (*it);

	Material *newMat = new Material(mat);

	_materialVec.emplace_back(newMat);
	_materialSet.insert(newMat);

	return newMat;
}
const Material *Content::GetDefaultMaterial()
{
	return GetOrAddMaterial(Material(GetTextureID("Fallback")));
}
const Material *Content::GetErrorMaterial()
{
	Material mat;
	mat.textureID = GetTextureID("Error");
	mat.ambientID = GetTextureID("Red");

	return GetOrAddMaterial(mat);
}
