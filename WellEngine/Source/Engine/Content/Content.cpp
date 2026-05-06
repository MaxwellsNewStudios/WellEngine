#include "stdafx.h"
#include "Content.h"
#include "ContentLoader.h"
#include "Engine/Debug/DebugData.h"

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

	for (auto &[id, item] : _meshes)
		delete item;

	for (auto &[id, item] : _shaders)
		delete item;

	for (auto &[id, item] : _textures)
		delete item;

	for (auto &[id, item] : _cubemaps)
		delete item;

	for (auto &[id, item] : _samplers)
		delete item;
	
	for (auto &[id, item] : _blendStates)
		delete item;

	for (auto &[id, item] : _inputLayouts)
		delete item;

	for (auto *item : _materialVec)
		delete item;

	_hasShutDown = true;
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


#ifdef USE_IMGUI
bool Content::AssetDirEntry::operator<(const AssetDirEntry &other) const
{
	bool thisFolder = reinterpret_cast<const AssetDirFolder *>(this) != nullptr;
	bool otherFolder = reinterpret_cast<const AssetDirFolder *>(&other) != nullptr;

	if (thisFolder != otherFolder)
		return thisFolder; // Directories come before files

	return name < other.name; // Alphabetical order within directories and files
}
bool Content::AssetDirEntry::operator==(const AssetDirEntry &other) const
{
	bool thisFolder = reinterpret_cast<const AssetDirFolder *>(this) != nullptr;
	bool otherFolder = reinterpret_cast<const AssetDirFolder *>(&other) != nullptr;

	if (thisFolder != otherFolder)
		return false; // One is a directory and the other is a file, so they are not equal

	return name == other.name; // Names must be the same for them to be considered equal
}

std::string Content::AssetDirEntry::GetRelativePath(const AssetDirFolder &root) const
{
	std::string path = name;

	std::function<bool(const AssetDirEntry *)> searchPathAppendRec = [&](const AssetDirEntry *entry) -> bool {
		if (entry == this)
			return true;

		const AssetDirFolder *folder = reinterpret_cast<const AssetDirFolder *>(entry);

		if (folder == nullptr) // Is file, but not destination
			return false;

		// Reversed to prioritize files to reduce unnecessary recursions
		for (auto childIter = folder->children.rbegin(); childIter != folder->children.rend(); childIter++)
		{
			if (!searchPathAppendRec(childIter->entry.get()))
				continue;

			path = folder->name + "/" + path;
			return true;
		}

		return false;
	};

	searchPathAppendRec(&root);
	return path;
}


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
			previewHeight = MAX(4.0f, previewHeight);
		ImGuiUtils::LockMouseOnActive();

		static float imageHeight = 512.0f;
		if (ImGui::DragFloat("Image Scale", &imageHeight))
			imageHeight = MAX(128.0f, imageHeight);
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::BeginTabBar("HierarchyTab"))
		{
			SamplerD3D11 *pointSampler = GetSampler("Point");

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
				static bool firstFrame = true;

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

				static std::vector<const Texture *> sortedTextures;
				if (firstFrame)
				{
					sortedTextures.reserve(_textures.size());
					for (const auto &[id, tex] : _textures)
						sortedTextures.push_back(tex);
					firstFrame = false;
				}

				// Apply filters & sorting
				{
					if (refreshTextures)
					{
						sortedTextures.clear();
						for (const auto &[id, tex] : _textures)
							sortedTextures.push_back(tex);

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
				const int columnsCount = MAX(1, (int)(availableWidth / tableItemSize));
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
								if (pointSampler)
									ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ImplDX11_SetSampler, pointSampler->GetSamplerState());

								ImGui::Image(
									(ImTextureID)texData.GetSRV(),
									ImVec2(imageHeight * aspectRatio, imageHeight),
									ImVec2(1, 1), ImVec2(0, 0)
								);

								if (pointSampler)
									ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ImplDX11_SetSampler, NULL);

								std::string dimName;
								switch (texture->data.GetDim())
								{
								case TexDim::Tex2D:			dimName = "2D"; break;
								case TexDim::Tex2DArray:	dimName = "2D Array"; break;
								case TexDim::Tex3D:			dimName = "3D"; break;
								case TexDim::Tex3DArray:	dimName = "3D Array"; break;
								case TexDim::Cubemap:		dimName = "Cubemap"; break;
								case TexDim::CubemapArray:	dimName = "Cubemap Array"; break;
								default:					dimName = "Unknown"; break;
								}

								ImGui::Text("ID: %d", texture->id);
								ImGui::Text("Name: %s", texture->name.c_str());
								ImGui::Text("Asset: %s", texture->path.c_str());
								ImGui::Text("Loaded: %s", texture->actualPath.c_str());
								ImGui::Text("%d x %d", size.x, size.y);
								ImGui::Text("Type: %s", dimName.c_str());
								ImGui::Text("Format: %s", D3D11FormatData::GetName(texData.GetFormat()).c_str());
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
				static bool firstFrame = true;

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

				static std::vector<const Cubemap *> sortedTextures;
				if (firstFrame)
				{
					sortedTextures.reserve(_cubemaps.size());
					for (const auto &[id, tex] : _cubemaps)
						sortedTextures.push_back(tex);
					firstFrame = false;
				}

				// Apply filters & sorting
				{
					if (refreshTextures)
					{
						sortedTextures.clear();
						for (const auto &[id, cub] : _cubemaps)
							sortedTextures.push_back(cub);

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
				const int columnsCount = MAX(1, (int)(availableWidth / tableItemSize));
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

		bool tryRecompile = false;
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
				tryRecompile = true;
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
				const Shader *shaderContainer = GetShaderContainer(shaderNames[i]);

				ImGui::Text("ID: %d", i);
				ImGui::Text("Name: %s", shaderNames[i].c_str());
				ImGui::Text("Path: %s", shaderContainer->path.c_str());

				bool recompile = false;
				if (tryRecompile)
				{
					// Check if the shader source file has been modified after the cso was last compiled
					// If so, trigger a recompile

					std::string csoPath = WE_DFE(WE_D_COMPILED_CSO, shaderNames[i], "cso");
					std::string sourcePath = WE_DFE(WE_D_ENGINE_SHADER, shaderContainer->path, "hlsl");

					// Ensure both files exist before comparing timestamps
					if (!std::filesystem::exists(csoPath) || !std::filesystem::exists(sourcePath))
					{
						recompile = true;
					}
					else
					{
						std::filesystem::file_time_type csoTime = std::filesystem::last_write_time(csoPath);
						std::filesystem::file_time_type sourceTime = std::filesystem::last_write_time(sourcePath);

						if (sourceTime > csoTime)
							recompile = true;
					}
				}

				if (recompile || ImGui::Button("Recompile"))
				{
					if (!RecompileShader(device, shaderNames[i]))
					{
						// Recompilation failed, likely due to a syntax error in the shader.
						// Touch the cso file to prevent continuous recompilation attempts until the source file is modified again.
						std::string csoPath = WE_DFE(WE_D_COMPILED_CSO, shaderNames[i], "cso");
						std::filesystem::last_write_time(csoPath, std::filesystem::file_time_type::clock::now());
					}
				}

				if (ImGui::TreeNode("Shader Info"))
				{
					// Expose reflection data
					auto reflector = shaderContainer->data.GetReflector();

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

	if (ImGui::TreeNode("Fonts"))
	{
		if (ImGui::Button("New Font Atlas"))
			ImGui::OpenPopup("NewFontAtlasPopup");

		if (ImGui::BeginPopup("NewFontAtlasPopup"))
		{
			static std::string fontName = "New Font";
			ImGui::InputText("Name", &fontName);

			if (ImGui::Button("Create") && !fontName.empty())
			{
				if (AddFontAtlas(fontName) == CONTENT_NULL)
					ErrMsg("Failed to create new font atlas!");

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::SetNextWindowSize({ ImGui::GetContentRegionAvail().x, 0 }, ImGuiCond_Always);
		ImGui::BeginChild("FontList", { 0, 0 }, ImGuiChildFlags_AutoResizeY);

		std::vector<std::string> fontNames;
		GetFontAtlasNames(&fontNames);

		for (int i = 0; i < fontNames.size(); i++)
		{
			ImGui::PushID(i);
			if (ImGui::TreeNode(fontNames[i].c_str()))
			{
				UINT fontID = GetFontAtlasID(fontNames[i]);
				FontAtlas *fontAtlas = GetFontAtlas(fontID);
				if (!fontAtlas->RenderUI(this))
				{
					ErrMsg("Failed to render font atlas UI!");
					ImGui::TreePop();
					ImGui::PopID();
					ImGui::EndChild();
					ImGui::TreePop();
					return false;
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
		ImGui::TreePop();
	}

	return true;
}

bool Content::RenderAssetInspectorUI(ID3D11Device *device, ID3D11DeviceContext *context)
{
	using namespace ContentManager;
	using namespace ContentManager::Registry;

	/*
	static bool showFullFormatList = false;
	static bool dirty = false;

	static std::string currAlias;
	static std::string editAlias;
	static AssetType editType = AssetType::None;
	static std::string editAssetPath;
	static std::string editRegistryPath;
	static std::string editCompiledPath;
	static fs::file_time_type editCompileTime;
	static AssetPropertiesTexture editTexProps;
	static AssetPropertiesShader editShaderProps;

	if (selectedPath.empty())
	{
		ImGui::TextDisabled("Select an asset to inspect its properties.");
	}
	else
	{
		ImGui::SeparatorText("Asset Info");

		const char *typeName = "Unknown";
		switch (editType)
		{
		case AssetType::Mesh:    typeName = "Mesh";    break;
		case AssetType::Texture: typeName = "Texture"; break;
		case AssetType::Cubemap: typeName = "Cubemap"; break;
		case AssetType::Shader:  typeName = "Shader";  break;
		case AssetType::Audio:   typeName = "Audio";   break;
		case AssetType::Scene:   typeName = "Scene";   break;
		case AssetType::Prefab:  typeName = "Prefab";  break;
		case AssetType::Font:    typeName = "Font";    break;
		default:                 typeName = "Unknown"; break;
		}

		ImGui::TextDisabled("Type:");     ImGui::SameLine(); ImGui::Text("%s", typeName);
		ImGui::TextDisabled("Asset:");    ImGui::SameLine(); ImGui::TextWrapped("%s", editAssetPath.c_str());

		ImGui::Spacing();
		ImGui::SeparatorText("Editable Properties");

		// Alias is always editable
		if (ImGui::InputText("Alias##BrowserAlias", &editAlias))
			dirty = true;

		// Type-specific editable properties
		switch (editType)
		{
		case AssetType::Texture:
		case AssetType::Cubemap:
		{
			ImGui::Checkbox("Show All Formats##BrowserFormatList", &showFullFormatList);

			// Format selector
			int formatCount = 0;
			const DXGI_FORMAT *formatList = showFullFormatList ? D3D11FormatData::GetLinearList(&formatCount) : D3D11FormatData::GetCommonLinearList(&formatCount);
			const std::string currentFormatName = D3D11FormatData::GetName(editTexProps.format);

			if (ImGui::BeginCombo("Format##BrowserFormat", currentFormatName.c_str()))
			{
				for (int i = 0; i < formatCount; i++)
				{
					const DXGI_FORMAT fmt = formatList[i];
					const std::string fmtName = D3D11FormatData::GetName(fmt);

					const bool selected = (editTexProps.format == fmt);
					if (ImGui::Selectable(fmtName.c_str(), selected))
					{
						editTexProps.format = fmt;
						dirty = true;
					}

					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::Checkbox("Mipmapped##BrowserMipped", &editTexProps.mipmapped))
				dirty = true;

			if (ImGui::DragInt("Downsample##BrowserDownsample", &editTexProps.downsample, 0.05f))
			{
				editTexProps.downsample = MAX(editTexProps.downsample, 0);
				dirty = true;
			}
			ImGuiUtils::LockMouseOnActive();

			break;
		}

		case AssetType::Shader:
		{
			static const char *shaderTypeNames[] = { "Vertex", "Hull", "Domain", "Geometry", "Pixel", "Compute" };
			int shaderTypeIdx = static_cast<int>(editShaderProps.type);
			if (ImGui::Combo("Shader Type##BrowserShaderType", &shaderTypeIdx, shaderTypeNames, IM_ARRAYSIZE(shaderTypeNames)))
			{
				editShaderProps.type = static_cast<ShaderType>(shaderTypeIdx);
				dirty = true;
			}

			break;
		}

		default:
			ImGui::TextDisabled("No editable properties for this asset type.");
			break;
		}

		// Apply / discard
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!dirty);
		if (ImGui::Button("Apply##BrowserApply"))
		{
			// Build the updated registry data
			RegistryData newReg;
			newReg.header.assetType = editType;
			newReg.header.alias = editAlias;
			newReg.header.assetPath = editAssetPath;
			newReg.header.registryPath = editRegistryPath;
			newReg.header.compiledPath = editCompiledPath;
			newReg.header.compileTime = editCompileTime;

			// Serialize type-specific properties into the properties vector
			switch (editType)
			{
			case AssetType::Texture:
			case AssetType::Cubemap:
				newReg.properties.resize(sizeof(AssetPropertiesTexture));
				*reinterpret_cast<AssetPropertiesTexture *>(newReg.properties.data()) = editTexProps;
				break;
			case AssetType::Shader:
				newReg.properties.resize(sizeof(AssetPropertiesShader));
				*reinterpret_cast<AssetPropertiesShader *>(newReg.properties.data()) = editShaderProps;
				break;
			case AssetType::Mesh:
				newReg.properties.resize(sizeof(AssetPropertiesMesh));
				*reinterpret_cast<AssetPropertiesMesh *>(newReg.properties.data()) = AssetPropertiesMesh{};
				break;
			default:
				break;
			}

			// Persist the updated registry to disk
			RegisterAsset(std::string(TO_SOLUTION_PATH) + editAssetPath, newReg);

			// Apply immediately editable changes to the in-memory content
			if (!currAlias.empty())
			{
				switch (editType)
				{
				case AssetType::Texture:
					for (auto &[id, tex] : _textures)
					{
						if (tex->name == currAlias)
						{
							tex->name = editAlias;
							tex->mipmapped = editTexProps.mipmapped;
							break;
						}
					}
					break;
				case AssetType::Cubemap:
					for (auto &[id, cub] : _cubemaps)
					{
						if (cub->name == currAlias)
						{
							cub->name = editAlias;
							break;
						}
					}
					break;
				case AssetType::Shader:
					for (auto &[id, shd] : _shaders)
					{
						if (shd->name == currAlias)
						{
							shd->name = editAlias;
							break;
						}
					}
					break;
				case AssetType::Mesh:
					for (auto &[id, msh] : _meshes)
					{
						if (msh->name == currAlias)
						{
							msh->name = editAlias;
							break;
						}
					}
					break;
				default:
					break;
				}
			}

			// Reload the entry list so the panel reflects the saved state
			needsRefresh = true;
			dirty = false;
			currAlias = editAlias;

			// Hot-reload asset data from disk with the updated properties
			switch (editType)
			{
			case AssetType::Texture:
				if (!ReloadTexture(device, context, editAlias, editTexProps.format, editTexProps.mipmapped, editTexProps.downsample))
					DbgMsgF("Failed to hot-reload texture '{}'", editAlias);
				break;
			case AssetType::Cubemap:
				if (!ReloadCubemap(device, context, editAlias, editTexProps.format, editTexProps.mipmapped, editTexProps.downsample))
					DbgMsgF("Failed to hot-reload cubemap '{}'", editAlias);
				break;
			case AssetType::Shader:
				if (!RecompileShader(device, editAlias))
					DbgMsgF("Failed to recompile shader '{}'", editAlias);
				break;
			default:
				break;
			}
		}
		ImGui::EndDisabled();

		if (dirty)
		{
			ImGui::SameLine();
			if (ImGui::Button("Discard##BrowserDiscard"))
			{
				// Reset edit state by re-selecting the current asset from the cached list
				for (const auto &e : entries)
				{
					if (e.header.registryPath == selectedPath)
					{
						selectAsset(e);
						break;
					}
				}
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Unsaved changes");
		}
	}
	*/

	return true;
}

bool Content::RenderFileBrowserUI(ID3D11Device *device, ID3D11DeviceContext *context)
{
	using namespace ContentManager;
	using namespace ContentManager::Registry;

	DebugData &dbgData = DebugData::Get();
	int &displayMode = dbgData.contentBrowserDisplayMode;
	float &iconScale = dbgData.contentBrowserIconScale;

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Settings##FileBrowserMenu"))
		{
			if (ImGui::BeginMenu("Display Mode"))
			{
				if (ImGui::MenuItem("List", nullptr, displayMode == 0))
					dbgData.contentBrowserDisplayMode = 0;

				if (ImGui::MenuItem("Icons", nullptr, displayMode == 1))
					dbgData.contentBrowserDisplayMode = 1;

				ImGui::EndMenu();
			}

			if (ImGui::SliderFloat("Icon Size", &iconScale, 0.1f, 2.0f, "%.1f"))
				iconScale = std::round(iconScale * 10.0f) * 0.1f;

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}


	// Reload registry on first call or after an apply
	if (_reloadDir)
	{
		_assetDirRoot = AssetDirFolder("root");

		 std::vector<RegistryData> entries = GetAssetRegistriesInDirectory(WE_D_REGISTRY, true);

		for (int i = 0; i < entries.size(); i++)
		{
			// Insert into _assetDirRoot, creating folders as needed
			RegistryData &e = entries[i];

			fs::path p = fs::relative(e.header.registryPath, WE_D_REGISTRY);

			std::vector<std::string> parts;
			for (const auto &seg : p)
				parts.push_back(seg.string());

			std::string endPart = parts.empty() ? "" : parts.back();
			parts.pop_back();

			AssetDirFolder *currFolder = &_assetDirRoot;
			for (const std::string &part : parts)
			{
				AssetDirFolder *nextFolder = nullptr;

				for (auto childIter = currFolder->children.begin(); childIter != currFolder->children.end(); ++childIter)
				{
					AssetDirFolder *child = reinterpret_cast<AssetDirFolder *>(childIter->entry.get());

					if (child == nullptr)
						break; // Reached files without finding the folder, stop searching

					if (child->name != part)
						continue;

					nextFolder = child;
					break;
				}

				if (nextFolder == nullptr)
				{
					auto result = currFolder->children.emplace(
						std::make_unique<AssetDirFolder>(part)
					);
					nextFolder = reinterpret_cast<AssetDirFolder *>(result.first->entry.get());
				}

				currFolder = nextFolder;
			}

			// Add the asset as a leaf in the final folder
			currFolder->children.emplace(
				std::make_unique<AssetDirFile>(endPart, e)
			);
		}

		_reloadDir = false;
	}




	/*
	// Selects an asset and populates the edit state from its registry data
	auto selectAsset = [&](const RegistryData &data) {
		selectedPath = data.header.registryPath;
		currAlias = editAlias = data.header.alias;
		editType = data.header.assetType;
		editAssetPath = data.header.assetPath;
		editRegistryPath = data.header.registryPath;
		editCompiledPath = data.header.compiledPath;
		editCompileTime = data.header.compileTime;
		dirty = false;

		editTexProps = {};
		editShaderProps = {};

		if (!data.properties.empty())
		{
			switch (data.header.assetType)
			{
			case AssetType::Texture:
			case AssetType::Cubemap:
				if (data.properties.size() >= sizeof(AssetPropertiesTexture))
					editTexProps = *reinterpret_cast<const AssetPropertiesTexture *>(data.properties.data());
				break;
			case AssetType::Shader:
				if (data.properties.size() >= sizeof(AssetPropertiesShader))
					editShaderProps = *reinterpret_cast<const AssetPropertiesShader *>(data.properties.data());
				break;
			default:
				break;
			}
		}
	};


	// Toolbar
	if (ImGui::Button("Refresh##FileBrowser"))
	{
		entries = GetAssetRegistriesInDirectory(WE_D_REGISTRY, true);
		if (!selectedPath.empty())
		{
			for (const auto &e : entries)
			{
				if (e.header.registryPath == selectedPath)
				{
					selectAsset(e);
					break;
				}
			}
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%d registered assets", (int)entries.size());
	ImGui::Separator();

	// Build a folder tree from registry paths
	struct FolderNode
	{
		std::map<std::string, FolderNode> children;
		std::vector<RegistryData *> assets;
	};

	FolderNode root;
	for (auto &e : entries)
	{
		fs::path p(e.header.registryPath);
		FolderNode *cur = &root;

		std::vector<std::string> parts;
		for (const auto &seg : p)
			parts.push_back(seg.string());

		// All path components except the last are folder nodes; the last is the asset leaf
		for (size_t i = 0; i + 1 < parts.size(); ++i)
			cur = &cur->children[parts[i]];

		if (!parts.empty())
			cur->assets.push_back(&e);
	}

	// Two-panel layout
	const float panelHeight = ImGui::GetContentRegionAvail().y;

	ImGui::BeginChild("##FileBrowserLeft", ImVec2(250.0f, panelHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

	std::function<void(FolderNode &, const std::string &)> renderFolder;
	renderFolder = [&](FolderNode &node, const std::string &nodePath) {
		for (auto &[name, child] : node.children)
		{
			const std::string childPath = nodePath + "/" + name;
			ImGui::PushID(childPath.c_str());

			const bool open = ImGui::TreeNodeEx(
				name.c_str(),
				ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen
			);
			if (open)
			{
				renderFolder(child, childPath);
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		for (auto *e : node.assets)
		{
			ImGui::PushID(e->header.registryPath.c_str());

			// Strip the .wer extension to get the original asset filename for display
			const std::string displayName = fs::path(e->header.registryPath).stem().string();

			const bool isSelected = (selectedPath == e->header.registryPath);

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isSelected)
				flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::TreeNodeEx(displayName.c_str(), flags);

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				selectAsset(*e);

			ImGui::PopID();
		}
	};

	renderFolder(root, "");

	ImGui::EndChild();
	ImGui::SameLine();

	// Right panel: property inspector
	ImGui::BeginChild("##FileBrowserRight", ImVec2(0.0f, panelHeight), ImGuiChildFlags_Borders);

	ImGui::EndChild();
	*/

	return true;
}
#endif

CompiledData Content::GetMeshData(const char *path) const
{
	CompiledData data{};

	MeshData meshData = MeshData();

	if (!LoadMeshFromFile(path, &meshData))
	{
		ErrMsg("Failed to load mesh from file!");
		return data;
	}

	std::vector<char> dataChars;
	meshData.Compile(dataChars);

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

UINT Content::AddMesh(ID3D11Device *device, const std::string &name, bool generateCollider)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	MeshData *meshData = new MeshData();
	meshData->vertexInfo.nrOfVerticesInBuffer = 1;
	meshData->vertexInfo.sizeOfVertex = sizeof(float) * 5;
	meshData->vertexInfo.vertexData = new float[5];
	meshData->indexInfo.nrOfIndicesInBuffer = 3;
	meshData->indexInfo.indexData = new UINT[3];
	meshData->subMeshInfo.push_back(MeshData::SubMeshInfo());

	UINT id = CONTENT_NULL;

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, mesh] : _meshes)
		{
			if (mesh->name != name)
				continue;

			duplicateName = true;
			id = key;
			delete meshData;
			break;
		}

		if (!duplicateName)
		{
			id = _nextMeshID++;
			Mesh *addedMesh = new Mesh(name, id);
			if (!addedMesh->data.Initialize(device, &meshData, generateCollider))
			{
				delete meshData;
				delete addedMesh;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added mesh!");
			}
			else
			{
				_meshes[id] = addedMesh;
			}
		}
	}

	meshData = nullptr;
	return id;
}
UINT Content::AddMesh(ID3D11Device *device, const std::string &name, MeshData **meshData, bool generateCollider)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, mesh] : _meshes)
		{
			if (mesh->name != name)
				continue;

			duplicateName = true;
			id = key;
			break;
		}

		if (!duplicateName)
		{
			id = _nextMeshID++;
			Mesh *addedMesh = new Mesh(name, id);
			if (!addedMesh->data.Initialize(device, meshData, generateCollider))
			{
				delete (*meshData);
				delete addedMesh;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added mesh!");
			}
			else
			{
				_meshes[id] = addedMesh;
			}
		}
	}

	meshData = nullptr;
	return id;
}
UINT Content::AddMesh(ID3D11Device *device, const std::string &name, const char *path, bool generateCollider)
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

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, mesh] : _meshes)
		{
			if (mesh->name != name)
				continue;

			duplicateName = true;
			id = key;
			delete meshData;
			break;
		}

		if (!duplicateName)
		{
			id = _nextMeshID++;
			Mesh *addedMesh = new Mesh(name, id);
			if (!addedMesh->data.Initialize(device, &meshData, generateCollider))
			{
				delete meshData;
				delete addedMesh;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added mesh!");
			}
			else
			{
				_meshes[id] = addedMesh;
			}
		}
	}

	meshData = nullptr;
	return id;
}

UINT Content::AddShader(ID3D11Device *device, const std::string &name, const std::string &codePath, ShaderType shaderType, ID3DBlob *&shaderBlob)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, shader] : _shaders)
		{
			if (shader->name != name)
				continue;

			duplicateName = true;
			id = key;
			break;
		}

		if (!duplicateName)
		{
			id = _nextShaderID++;
			Shader *addedShader = new Shader(name, codePath, id);
			if (!addedShader->data.Initialize(device, shaderType, shaderBlob))
			{
				delete addedShader;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added shader!");
			}
			else
			{
				_shaders[id] = addedShader;
			}
		}
	}

	shaderBlob = nullptr;
	return id;
}
UINT Content::AddShader(ID3D11Device *device, const std::string &name, const std::string &codePath, ShaderType shaderType, const void *dataPtr, size_t dataSize)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, shader] : _shaders)
		{
			if (shader->name != name)
				continue;

			duplicateName = true;
			id = key;
			break;
		}

		if (!duplicateName)
		{
			id = _nextShaderID++;
			Shader *addedShader = new Shader(name, codePath, id);
			if (!addedShader->data.Initialize(device, shaderType, dataPtr, dataSize))
			{
				delete addedShader;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added shader!");
			}
			else
			{
				_shaders[id] = addedShader;
			}
		}
	}

	return id;
}
UINT Content::AddShader(ID3D11Device *device, const std::string &name, const std::string &codePath, ShaderType shaderType, const std::string &csoPath)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, shader] : _shaders)
		{
			if (shader->name != name)
				continue;

			duplicateName = true;
			id = key;
			break;
		}

		if (!duplicateName)
		{
			id = _nextShaderID++;
			Shader *addedShader = new Shader(name, codePath, id);
			if (!addedShader->data.Initialize(device, shaderType, csoPath.c_str()))
			{
				delete addedShader;
				id = CONTENT_NULL;
				ErrMsg("Failed to initialize added shader!");
			}
			else
			{
				_shaders[id] = addedShader;
			}
		}
	}

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

	// Save compiled shader to cso path for future loading
	{
		std::string shaderName = std::filesystem::path(path).stem().string();
		std::string csoPath = WE_DFE(WE_D_COMPILED_CSO, shaderName, "cso");
		std::ofstream writer(csoPath, std::ios::binary);

		if (writer.is_open())
		{
			writer.write(static_cast<const char *>(shaderBlob->GetBufferPointer()), shaderBlob->GetBufferSize());
			writer.close();
		}
		else
		{
			Warn("Failed to save compiled shader to cso path!");
		}
	}

	return shaderBlob;
}
bool Content::RecompileShader(ID3D11Device *device, const std::string &name) const
{
	ZoneScopedC(RandomUniqueColor());

	const Shader *shaderContainer = GetShaderContainer(name);
	if (!shaderContainer)
	{
		ErrMsgF("Shader '{}' not found!", name);
		return false;
	}

	ShaderType shaderType = shaderContainer->data.GetShaderType();
	std::string path = WE_DFE(WE_D_ENGINE_SHADER, shaderContainer->path, "hlsl");

	auto shaderBlob = CompileShader(device, path, shaderType);

	if (!shaderBlob)
	{
		DbgMsgF("Failed to recompile shader '{}'", name);
		return false;
	}

	ShaderD3D11 *shader = GetShader(name);
	if (!shader->Initialize(device, shaderType, shaderBlob))
	{
		shaderBlob->Release();
		ErrMsg("Failed to reinitialize shader after recompilation!");
		return false;
	}

	return true;
}

bool Content::ReloadTexture(ID3D11Device *device, ID3D11DeviceContext *context, const std::string &name, TexLoadInfo *info)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	TexLoadInfo defaultInfo;
	if (info == nullptr)
		info = &defaultInfo;

	Texture *tex = nullptr;
	for (auto &[id, t] : _textures)
	{
		if (t->name == name)
		{
			tex = t;
			break;
		}
	}

	if (!tex)
	{
		ErrMsgF("Texture '{}' not found for reload!", name);
		return false;
	}

	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> srv;

	// Force-load from the original source file, bypassing any bake cache
	if (!LoadTextureFromFile(device, context, tex->path, *texture.GetAddressOf(), *srv.GetAddressOf(), info))
	{
		ErrMsgF("Failed to reload texture '{}' from file!", name);
		return false;
	}

	if (!tex->data.Initialize(std::move(texture), std::move(srv)))
	{
		ErrMsg("Failed to reinitialize texture data after reload!");
		return false;
	}

	tex->mipmapped = info->mipmapped;
	tex->actualPath = tex->path;
	return true;
}

bool Content::ReloadCubemap(ID3D11Device *device, ID3D11DeviceContext *context, const std::string &name, TexLoadInfo *info)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneText(name.c_str(), name.size());

	TexLoadInfo defaultInfo;
	if (info == nullptr)
		info = &defaultInfo;

	Cubemap *cub = nullptr;
	for (auto &[id, c] : _cubemaps)
	{
		if (c->name == name)
		{
			cub = c;
			break;
		}
	}

	if (!cub)
	{
		ErrMsgF("Cubemap '{}' not found for reload!", name);
		return false;
	}

	UINT width, height, inChannels, inBitsPerChannel;
	std::vector<unsigned char> texData;

	if (!LoadTextureFromFile(cub->path, texData, width, height, inChannels, inBitsPerChannel))
	{
		ErrMsgF("Failed to reload cubemap '{}' from file!", name);
		return false;
	}

	const UINT inBytesPerChannel = inBitsPerChannel / 8;
	const UINT inBytesPerPixel = inChannels * inBytesPerChannel;

	// Clamp downsample to max possible value, given the image dimensions
	const int maxDownsample = MAX(0, static_cast<int>(std::floor(std::log2(std::min(width, height)))) - 2);
	const int effectiveDownsample = MIN(info->downsample + MIPS_DISCARDED, maxDownsample);

	if (effectiveDownsample > 0)
	{
		const UINT newWidth = width >> effectiveDownsample;
		const UINT newHeight = height >> effectiveDownsample;

		if (newWidth > 0 && newHeight > 0)
		{
			if (DownsampleTexture(texData, width, height, newWidth, newHeight))
			{
				width = newWidth;
				height = newHeight;
			}
			else
			{
				Warn("Failed to downsample cubemap during reload!");
			}
		}
	}

	const dx::XMINT4 outBitLayout = D3D11FormatData::GetBitsPerChannel(info->format);
	if (outBitLayout.x == 0)
	{
		ErrMsgF("Unsupported format for cubemap '{}' reload!", name);
		return false;
	}

	const UINT outChannels = D3D11FormatData::GetChannelCount(info->format);
	const UINT outBitsPerPixel = D3D11FormatData::GetBitsPerPixel(info->format);
	const UINT outBitsPerChannel = outBitsPerPixel / outChannels;
	const UINT outBytesPerPixel = outBitsPerPixel / 8;
	const UINT outBytesPerChannel = outBitsPerChannel / 8;

	if (outBitsPerChannel < 8)
	{
		ErrMsgF("Sub-byte channel formats not supported for cubemap '{}' reload!", name);
		return false;
	}

	const int bitDiff = (int)outBitsPerChannel - (int)inBitsPerChannel;
	const double precisionChange = std::pow(2.0, bitDiff);

	std::vector<uint8_t> formattedTexData;
	formattedTexData.resize((size_t)width * height * outBytesPerPixel);

	const uint8_t *inRawData = texData.data();
	uint8_t *outRawData = formattedTexData.data();

	for (size_t pixel = 0; pixel < (size_t)width * height; pixel++)
	{
		const uint8_t *inPixelPtr = inRawData + (pixel * inBytesPerPixel);
		uint8_t *outPixelPtr = outRawData + (pixel * outBytesPerPixel);

		for (size_t channel = 0; channel < outChannels; channel++)
		{
			const uint8_t *inChannelPtr = inPixelPtr + (channel * inBytesPerChannel);
			uint8_t *outChannelPtr = outPixelPtr + (channel * outBytesPerChannel);

			if (channel < inChannels)
			{
				if (outBytesPerChannel == 1 && inBytesPerChannel == 1)
					(*outChannelPtr) = (uint8_t)((*inChannelPtr) * precisionChange);
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 1)
					(*(uint16_t *)outChannelPtr) = (uint16_t)((*inChannelPtr) * precisionChange);
				else if (outBytesPerChannel == 1 && inBytesPerChannel == 2)
					(*outChannelPtr) = (uint8_t)((*(uint16_t *)inChannelPtr) * precisionChange);
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 2)
					(*(uint16_t *)outChannelPtr) = (uint16_t)((*(uint16_t *)inChannelPtr) * precisionChange);
			}
			else if (channel == 3)
			{
				if (outBytesPerChannel == 1 && inBytesPerChannel == 1)
					*outChannelPtr = (uint8_t)NumericLimit(*outChannelPtr);
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 1)
					*(uint16_t *)outChannelPtr = (uint16_t)NumericLimit(*(uint16_t *)outChannelPtr);
				else if (outBytesPerChannel == 1 && inBytesPerChannel == 2)
					*outChannelPtr = (uint8_t)NumericLimit(*outChannelPtr);
				else if (outBytesPerChannel == 2 && inBytesPerChannel == 2)
					*(uint16_t *)outChannelPtr = (uint16_t)NumericLimit(*(uint16_t *)outChannelPtr);
			}
		}
	}

	if (!cub->data.Initialize(device, context, width, height, formattedTexData.data(), info->format, info->mipmapped, true))
	{
		ErrMsgF("Failed to reinitialize cubemap '{}' after reload!", name);
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

#pragma omp critical
	{
		if (!IsNameDuplicate(name, _textures, &id))
		{
			id = _nextTextureID++;

			ComPtr<ID3D11Texture2D> texture;
			ComPtr<ID3D11ShaderResourceView> srv;

			const std::string *pathToUse = &path;
			bool doBake = true;
			bool flip = true;

#ifndef FORCE_BAKE_TEXTURES
			std::string bakePath = GetTextureBakePath(path);

			// Check if texture is prebaked
			if (std::filesystem::exists(bakePath))
			{
				if (std::filesystem::exists(path))
				{
					// Ensure that the original has not been modified since the bake
					auto originalWriteTime = std::filesystem::last_write_time(path);
					auto bakedWriteTime = std::filesystem::last_write_time(bakePath);

					if (originalWriteTime < bakedWriteTime)
					{
						// Baked is newer, use it
						pathToUse = &bakePath;

						doBake = false;
						flip = false;
						downsample = 0;
						format = DXGI_FORMAT_UNKNOWN;
					}
				}
				else
				{
					// Original doesn't exist but baked does, use baked
					pathToUse = &bakePath;

					doBake = false;
					flip = false;
					downsample = 0;
					format = DXGI_FORMAT_UNKNOWN;
				}
			}
#endif

			TexLoadInfo loadInfo{};
			loadInfo.format = format;
			loadInfo.mipmapped = useMipmaps;
			loadInfo.downsample = downsample;
			loadInfo.flipY = flip;

			bool failed = false;
			if (!LoadTextureFromFile(device, context, *pathToUse, *texture.GetAddressOf(), *srv.GetAddressOf(), &loadInfo, doBake))
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
					_textures[id] = addedTexture;
				}

				addedTexture->actualPath = *pathToUse;
			}
		}
	}

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

	// Clamp downsample to max possible value, given the image dimensions
	const int maxDownsample = MAX(0, static_cast<int>(std::floor(std::log2(std::min(width, height)))) - 2);
	const int effectiveDownsample = MIN(downsample + MIPS_DISCARDED, maxDownsample);

	if (effectiveDownsample > 0)
	{
		UINT newWidth = width >> effectiveDownsample;
		UINT newHeight = height >> effectiveDownsample;

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

#pragma omp critical
	{
		bool duplicateName = false;
		for (const auto &[key, cubemap] : _cubemaps)
		{
			if (cubemap->name != name)
				continue;

			duplicateName = true;
			id = key;
			break;
		}

		if (!duplicateName)
		{
			id = _nextCubemapID++;
			Cubemap *addedCubemap = new Cubemap(name, std::string(path), id);
			if (!addedCubemap->data.Initialize(device, context, width, height, formattedTexData.data(), format, useMipmaps, true))
			{
				delete addedCubemap;
				id = CONTENT_NULL;
				Warn("Failed to initialize added cubemap!");
			}
			else
			{
				_cubemaps[id] = addedCubemap;
			}
		}
	}

	return id;
}

UINT Content::AddFontAtlas(const std::string &name)
{
	if (name.empty() || name == "_" || name == "Uninitialized")
	{
		ErrMsgF("The name '{}' is reserved!", name);
		return CONTENT_NULL;
	}

	UINT id = CONTENT_NULL;
	bool duplicateName = false;
	for (const auto &[key, font] : _textureFonts)
	{
		if (font->name != name)
			continue;

		duplicateName = true;
		id = key;
		break;
	}

	if (!duplicateName)
	{
		id = _nextFontAtlasID++;
		TextureFont *font = new TextureFont(name, id);
		if (!font->data.Initialize(this, name))
		{
			ErrMsg("Failed to create font atlas");
			return CONTENT_NULL;
		}
		_textureFonts[id] = font;
	}

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

	for (const auto &[key, sampler] : _samplers)
	{
		if (sampler->name == name)
			return key;
	}

	const UINT id = _nextSamplerID++;
	Sampler* addedSampler = new Sampler(name, id);
	if (!addedSampler->data.Initialize(device, adressMode, filter, comparisonFunc))
	{
		delete addedSampler;
		ErrMsgF("Failed to initialize added sampler '{}'!", name);
		return CONTENT_NULL;
	}
	_samplers[id] = addedSampler;

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

	for (const auto &[key, sampler] : _samplers)
	{
		if (sampler->name == name)
			return key;
	}

	const UINT id = _nextSamplerID++;
	Sampler* addedSampler = new Sampler(name, id);
	if (!addedSampler->data.Initialize(device, samplerDesc))
	{
		delete addedSampler;
		ErrMsgF("Failed to initialize added sampler '{}'!", name);
		return CONTENT_NULL;
	}
	_samplers[id] = addedSampler;

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

	for (const auto &[key, blendState] : _blendStates)
	{
		if (blendState->name == name)
			return key;
	}

	const UINT id = _nextBlendStateID++;
	BlendState *addedBlendState = new BlendState(name, id);
	if (FAILED(device->CreateBlendState(&blendDesc, addedBlendState->data.ReleaseAndGetAddressOf())))
	{
		delete addedBlendState;
		ErrMsgF("Failed to initialize added blend state '{}'!", name);
		return CONTENT_NULL;
	}
	_blendStates[id] = addedBlendState;

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

	for (const auto &[key, inputLayout] : _inputLayouts)
	{
		if (inputLayout->name == name)
			return key;
	}

	const UINT id = _nextInputLayoutID++;
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
	_inputLayouts[id] = addedInputLayout;

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
UINT Content::GetFontAtlasCount() const
{
	return static_cast<UINT>(_textureFonts.size());
}

void Content::GetMeshNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_meshes.size());
	for (const auto &[id, mesh] : _meshes)
		names->emplace_back(mesh->name);
}
void Content::GetShaderNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_shaders.size());
	for (const auto &[id, shader] : _shaders)
		names->emplace_back(shader->name);
}
void Content::GetSamplerNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_samplers.size());
	for (const auto &[id, sampler] : _samplers)
		names->emplace_back(sampler->name);
}
void Content::GetBlendStateNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_blendStates.size());
	for (const auto &[id, blendState] : _blendStates)
		names->emplace_back(blendState->name);
}
void Content::GetTextureNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_textures.size());
	for (const auto &[id, texture] : _textures)
		names->emplace_back(texture->name);
}
void Content::GetCubemapNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_cubemaps.size());
	for (const auto &[id, cubemap] : _cubemaps)
		names->emplace_back(cubemap->name);
}
void Content::GetFontAtlasNames(std::vector<std::string> *names) const
{
	names->clear();
	names->reserve(_textureFonts.size());
	for (const auto &[id, font] : _textureFonts)
		names->emplace_back(font->name);
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

	for (const auto &[id, mesh] : _meshes)
	{
		if (mesh->name == lookupName)
			return id;
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
	auto it = _meshes.find(id);
	if (it == _meshes.end())
		return "Uninitialized";
	return it->second->name;
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
		return &_meshes.begin()->second->data;
	}

	auto it = _meshes.find(id);
	if (it == _meshes.end())
	{
		WarnF("Failed to find mesh #{}! Returning default.", id);
		return &_meshes.begin()->second->data;
	}

	return &it->second->data;
}

UINT Content::GetShaderID(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	for (const auto &[id, shader] : _shaders)
	{
		if (shader->name == name)
			return id;
	}

	return CONTENT_NULL;
}
std::string Content::GetShaderName(UINT id) const
{
	auto it = _shaders.find(id);
	if (it == _shaders.end())
		return "Uninitialized";
	return it->second->name;
}
ShaderD3D11 *Content::GetShader(const std::string &name) const
{
	for (const auto &[id, shader] : _shaders)
	{
		if (shader->name == name)
			return &shader->data;
	}

	WarnF("Failed to find shader '{}'! Returning default.", name);
	return &_shaders.begin()->second->data;
}
ShaderD3D11 *Content::GetShader(const UINT id) const
{
	if (id == CONTENT_NULL || _shaders.find(id) == _shaders.end())
	{
		WarnF("Failed to find shader #{}! Returning default.", id);
		return &_shaders.begin()->second->data;
	}

	return &_shaders.at(id)->data;
}
const Shader *Content::GetShaderContainer(const std::string &name) const
{
	for (const auto &[id, shader] : _shaders)
	{
		if (shader->name == name)
			return shader;
	}

	WarnF("Failed to find shader '{}'! Returning default.", name);
	return _shaders.begin()->second;
}
const Shader *Content::GetShaderContainer(const UINT id) const
{
	if (id == CONTENT_NULL || _shaders.find(id) == _shaders.end())
	{
		WarnF("Failed to find shader #{}! Returning default.", id);
		return _shaders.begin()->second;
	}

	return _shaders.at(id);
}

void Content::GetShadersOfType(std::vector<UINT> &shaders, ShaderType type)
{
	shaders.clear();
	shaders.reserve(_shaders.size() / 2);

	for (const auto &[id, shader] : _shaders)
	{
		if (shader->data.GetShaderType() == type)
			shaders.emplace_back(id);
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

	for (const auto &[id, texture] : _textures)
	{
		if (texture->name == lookupName)
			return true;
	}

	return false;
}
bool Content::HasCubemap(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return false;

	for (const auto &[id, cubemap] : _cubemaps)
	{
		if (cubemap->name == name)
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

	for (const auto &[id, texture] : _textures)
	{
		if (texture->name == lookupName)
			return id;
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
	auto it = _textures.find(id);
	if (it == _textures.end())
		return "Uninitialized";
	return it->second->name;
}
UINT Content::GetTextureIDByPath(const std::string &path) const
{
	for (const auto &[id, texture] : _textures)
	{
		if (texture->path == path)
			return id;
	}

	return CONTENT_NULL;
}
const Texture *Content::GetTextureContainer(UINT id) const
{
	auto it = _textures.find(id);
	if (it == _textures.end())
		return GetTextureContainer("Fallback");

	return it->second;
}
const Texture *Content::GetTextureContainer(const std::string &name) const
{
	UINT id = GetTextureID(name);
	return GetTextureContainer(id);
}
ShaderResourceTextureD3D11 *Content::GetTexture(const UINT id) const
{
	auto it = _textures.find(id);
	if (it == _textures.end())
		return GetTexture("Fallback");

	return &it->second->data;
}
ShaderResourceTextureD3D11 *Content::GetTexture(const std::string &name) const
{
	UINT id = GetTextureID(name);
	return GetTexture(id);
}

UINT Content::GetCubemapID(const std::string &name) const
{
	if (name.empty() || name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	for (const auto &[id, cubemap] : _cubemaps)
	{
		if (cubemap->name == name)
			return id;
	}

	return CONTENT_NULL;
}
std::string Content::GetCubemapName(UINT id) const
{
	auto it = _cubemaps.find(id);
	if (it == _cubemaps.end())
		return "Uninitialized";
	return it->second->name;
}
UINT Content::GetCubemapIDByPath(const std::string &path) const
{
	for (const auto &[id, cubemap] : _cubemaps)
	{
		if (cubemap->path == path)
			return id;
	}

	return CONTENT_NULL;
}
ShaderResourceTextureD3D11 *Content::GetCubemap(UINT id) const
{
	auto it = _cubemaps.find(id);
	if (it == _cubemaps.end())
		return GetCubemap("Fallback");

	return &it->second->data;
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

	for (const auto &[id, sampler] : _samplers)
	{
		if (sampler->name == lookupName)
			return id;
	}

	return CONTENT_NULL;
}
std::string Content::GetSamplerName(UINT id) const
{
	auto it = _samplers.find(id);
	if (it == _samplers.end())
		return "Uninitialized";
	return it->second->name;
}
SamplerD3D11 *Content::GetSampler(const std::string &name) const
{
	for (const auto &[id, sampler] : _samplers)
	{
		if (sampler->name == name)
			return &sampler->data;
	}

	DbgMsg(std::format("Failed to find sampler '{}'! Returning default.", name));
	return &_samplers.begin()->second->data;
}
SamplerD3D11 *Content::GetSampler(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsg(std::format("Failed to find sampler #{}! Returning default.", id));
		return &_samplers.begin()->second->data;
	}

	auto it = _samplers.find(id);
	if (it == _samplers.end())
	{
		DbgMsg(std::format("Failed to find sampler #{}! Returning default.", id));
		return &_samplers.begin()->second->data;
	}

	return &it->second->data;
}

UINT Content::GetBlendStateID(const std::string &name) const
{
	if (name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	for (const auto &[id, blendState] : _blendStates)
	{
		if (blendState->name == name)
			return id;
	}

	return CONTENT_NULL;
}
std::string Content::GetBlendStateName(UINT id) const
{
	auto it = _blendStates.find(id);
	if (it == _blendStates.end())
		return "Uninitialized";
	return it->second->name;
}
ID3D11BlendState *Content::GetBlendState(const std::string &name) const
{
	for (const auto &[id, blendState] : _blendStates)
	{
		if (blendState->name == name)
			return blendState->data.Get();
	}

	DbgMsgF("Failed to find blend state '{}'! Returning default.", name);
	return _blendStates.begin()->second->data.Get();
}
ID3D11BlendState *Content::GetBlendState(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return _blendStates.begin()->second->data.Get();
	}

	auto it = _blendStates.find(id);
	if (it == _blendStates.end())
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return _blendStates.begin()->second->data.Get();
	}

	return it->second->data.Get();
}
ComPtr<ID3D11BlendState> *Content::GetBlendStateAddress(const std::string &name) const
{
	for (const auto &[id, blendState] : _blendStates)
	{
		if (blendState->name == name)
			return &blendState->data;
	}

	DbgMsgF("Failed to find blend state '{}'! Returning default.", name);
	return &_blendStates.begin()->second->data;
}
ComPtr<ID3D11BlendState> *Content::GetBlendStateAddress(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return &_blendStates.begin()->second->data;
	}

	auto it = _blendStates.find(id);
	if (it == _blendStates.end())
	{
		DbgMsgF("Failed to find blend state #{}! Returning default.", id);
		return &_blendStates.begin()->second->data;
	}

	return &it->second->data;
}

UINT Content::GetFontAtlasID(const std::string &name) const
{
	if (name == "_" || name == "Uninitialized")
		return CONTENT_NULL;

	for (const auto &[id, font] : _textureFonts)
	{
		if (font->name == name)
			return id;
	}

	return CONTENT_NULL;
}
std::string Content::GetFontAtlasName(UINT id) const
{
	auto it = _textureFonts.find(id);
	if (it == _textureFonts.end())
		return "Uninitialized";
	return it->second->name;
}
FontAtlas *Content::GetFontAtlas(const std::string &name) const
{
	for (const auto &[id, font] : _textureFonts)
	{
		if (font->name == name)
			return &font->data;
	}

	DbgMsgF("Failed to find font atlas '{}'! Returning default.", name);
	return &_textureFonts.begin()->second->data;
}
FontAtlas *Content::GetFontAtlas(UINT id) const
{
	if (id == CONTENT_NULL)
	{
		DbgMsgF("Failed to find font atlas #{}! Returning default.", id);
		return &_textureFonts.begin()->second->data;
	}

	auto it = _textureFonts.find(id);
	if (it == _textureFonts.end())
	{
		DbgMsgF("Failed to find font atlas #{}! Returning default.", id);
		return &_textureFonts.begin()->second->data;
	}

	return &it->second->data;
}

UINT Content::GetInputLayoutID(const std::string &name) const
{
	for (const auto &[id, inputLayout] : _inputLayouts)
	{
		if (inputLayout->name == name)
			return id;
	}

	return CONTENT_NULL;
}
InputLayoutD3D11 *Content::GetInputLayout(const std::string &name) const
{
	for (const auto &[id, inputLayout] : _inputLayouts)
	{
		if (inputLayout->name == name)
			return &inputLayout->data;
	}

	DbgMsg(std::format("Failed to find input layout '{}'! Returning default.", name));
	return &_inputLayouts.begin()->second->data;
}
InputLayoutD3D11 *Content::GetInputLayout(const UINT id) const
{
	if (id == CONTENT_NULL)
	{
		ErrMsgF("Failed to find input layout #{}! Returning default.", id);
		return &_inputLayouts.begin()->second->data;
	}

	auto it = _inputLayouts.find(id);
	if (it == _inputLayouts.end())
	{
		ErrMsgF("Failed to find input layout #{}! Returning default.", id);
		return &_inputLayouts.begin()->second->data;
	}

	return &it->second->data;
}
