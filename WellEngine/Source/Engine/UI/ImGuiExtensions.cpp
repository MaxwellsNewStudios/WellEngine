#include "stdafx.h"
#include "ImGuiExtensions.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

void ImDrawCallback_ImplDX11_SetSampler(const ImDrawList *parent_list, const ImDrawCmd *cmd)
{
	ImGui_ImplDX11_RenderState *state = (ImGui_ImplDX11_RenderState *)ImGui::GetPlatformIO().Renderer_RenderState;
	ID3D11SamplerState *sampler = cmd->UserCallbackData ? (ID3D11SamplerState *)cmd->UserCallbackData : state->SamplerDefault;
	state->DeviceContext->PSSetSamplers(0, 1, &sampler);
}

bool ImGui::CurveEdit(const char *label, std::vector<BezierPoint> *points, const ImVec2 &size, const ImRect &pointBounds, float thickness, ImGuiCurveEditFlags flags)
{
	bool changed = false;

	ImGui::BeginChild(label, size, true);
	ImGuiWindow *window = GetCurrentWindow();
	ImGuiStorage *storage = ImGui::GetStateStorage();
	ImDrawList *drawList = ImGui::GetWindowDrawList();

	if (window->SkipItems)
	{
		ImGui::EndChild();
		return false;
	}

	if (!points)
	{
		ImGui::EndChild();
		return false;
	}

	int pointCount = (int)points->size();

	if (pointCount <= 1)
	{
		ImGui::EndChild();
		return false;
	}

	ImVec2 windowPos = ImGui::GetWindowPos();
	ImRect drawArea = ImRect(windowPos, windowPos + size);

	ImVec2 pOffset = windowPos + ImVec2(-pointBounds.Min.x, -pointBounds.Min.y + size.y);
	ImVec2 pScale = ImVec2(size.x / pointBounds.GetWidth(), -(size.y / pointBounds.GetHeight()));

	drawList->PushClipRect(drawArea.Min, drawArea.Max, true);

	bool quadratic = (flags & ImGuiCurveEditFlags_Quadratic) != 0;
	bool jointed = (flags & ImGuiCurveEditFlags_Jointed) != 0;
	bool readOnly = (flags & ImGuiCurveEditFlags_ReadOnly) != 0;
	bool noLabels = (flags & ImGuiCurveEditFlags_NoLabels) != 0;
	bool noPoints = (flags & ImGuiCurveEditFlags_NoPoints) != 0;

	for (int i = 0; i < pointCount - 1; i++)
	{
		if (quadratic)
		{
			ImVec2 p1 = (*points)[i].position * pScale + pOffset;
			ImVec2 p2 = (*points)[i].controlPoint1 * pScale + pOffset;
			ImVec2 p3 = (*points)[i + 1ll].position * pScale + pOffset;

			drawList->AddBezierQuadratic(p1, p2, p3, IM_COL32(255, 255, 255, 255), thickness);
		}
		else
		{
			ImVec2 p1 = (*points)[i].position * pScale + pOffset;
			ImVec2 p2 = (*points)[i].controlPoint2 * pScale + pOffset;
			ImVec2 p3 = (*points)[i + 1ll].controlPoint1 * pScale + pOffset;
			ImVec2 p4 = (*points)[i + 1ll].position * pScale + pOffset;

			drawList->AddBezierCubic(p1, p2, p3, p4, IM_COL32(255, 255, 255, 255), thickness);
		}
	}

	bool isDragging = false;
	bool mouseAbsorbed = false;

	for (int i = 0; i < pointCount; i++)
	{
		ImVec2 pPos = (*points)[i].position * pScale + pOffset;

		// Handle point selection
		ImGuiID pointID = window->GetID(std::format("##Point{}", i).c_str());
		ImGuiID pointPrevID = window->GetID(std::format("##PointPrev{}", i).c_str());
		ImGuiID pointDragID = window->GetID(std::format("##PointDrag{}", i).c_str());

		int pointSelectState = storage->GetInt(pointID);
		int pointPrevSelectState = storage->GetInt(pointPrevID);
		int pointDragState = storage->GetInt(pointDragID);

		if (!mouseAbsorbed && ImGui::IsMouseHoveringRect(pPos - ImVec2(thickness, thickness) * 1.5f, pPos + ImVec2(thickness, thickness) * 1.5f))
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				storage->SetInt(pointID, 2);
				storage->SetInt(pointPrevID, (pointSelectState > 0) ? 1 : 0);
				storage->SetInt(pointDragID, 0);
				mouseAbsorbed = true;
			}
			else if (!readOnly && pointCount > 2 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				// Handle point deletion
				points->erase(points->begin() + i);
				storage->SetInt(pointID, 0);
				storage->SetInt(pointPrevID, 0);
				storage->SetInt(pointDragID, 0);
				i--;
				changed = true;
				mouseAbsorbed = true;
				continue;
			}
		}

		if (pointSelectState == 2)
		{
			mouseAbsorbed = true;

			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				storage->SetInt(pointID, (pointDragState == 0) ? (1 - pointPrevSelectState) : pointPrevSelectState);
				storage->SetInt(pointPrevID, 0);
			}
			else if (!readOnly)
			{
				// Handle point dragging
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, thickness) && !isDragging)
				{
					storage->SetInt(pointDragID, 1);

					ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
					ImVec2 delta = mouseDelta / pScale; // Convert back to curve space

					(*points)[i].position += delta;
					(*points)[i].controlPoint1 += delta;
					(*points)[i].controlPoint2 += delta;

					changed = true;
					isDragging = true;
				}
			}
		}

		if (pointSelectState == 1 || pointPrevSelectState == 1)
		{
			for (int j = 0; j < 2; j++)
			{
				if (quadratic)
				{
					// For quadratic curves, skip cp2 for all points and cp1 for the last point
					if (j == 1)
						continue;
					if (i == pointCount - 1)
						continue;
				}
				else
				{
					// For cubic curved, skip cp1 for the first point and cp2 for the last point
					if (j == 0 && i == 0)
						continue;
					if (j == 1 && i == pointCount - 1)
						continue;
				}

				std::string cpLabel = std::format("##CP{}:{}", j + 1, i);
				ImVec2 *cp = (j == 0) ? (&(*points)[i].controlPoint1) : (&(*points)[i].controlPoint2);
				ImVec2 *cpMirror = (j == 0) ? (&(*points)[i].controlPoint2) : (&(*points)[i].controlPoint1);
				ImColor cpColor = (j == 0) ? ImColor(192, 64, 64, 255) : ImColor(64, 64, 192, 255);

				ImVec2 cpPos = (*cp) * pScale + pOffset;

				drawList->AddLine(cpPos, pPos, IM_COL32(128, 128, 128, 192), thickness * 0.75f);
				drawList->AddCircleFilled(cpPos, thickness * 1.5f, cpColor);

				// Handle control point dragging
				ImGuiID cpID = window->GetID(cpLabel.c_str());

				int cpState = storage->GetInt(cpID);
				bool isCPSelected = cpState == 1;

				if (!mouseAbsorbed && ImGui::IsMouseHoveringRect(cpPos - ImVec2(thickness, thickness) * 1.5f, cpPos + ImVec2(thickness, thickness) * 1.5f))
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						storage->SetInt(cpID, isCPSelected ? 0 : 1);

					mouseAbsorbed = true;
				}

				if (isCPSelected)
				{
					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						storage->SetInt(cpID, 0);
					}
					else if (!readOnly && ImGui::IsMouseDragging(ImGuiMouseButton_Left, thickness))
					{
						ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
						ImVec2 delta = mouseDelta / pScale; // Convert back to curve space

						(*cp) += delta;
						if (jointed)
						{
							// Set mirror control point to be symmetric across the main point
							(*cpMirror) = (*points)[i].position * 2.0f - (*cp);
						}

						changed = true;
					}

					mouseAbsorbed = true;
				}
			}
		}

		if (!noPoints)
			drawList->AddCircleFilled(pPos, thickness * 1.5f, IM_COL32(80, 255, 80, 255));

		if (!noLabels)
		{
			// Draw point number label
			ImVec2 labelSize = ImGui::CalcTextSize(std::to_string(i).c_str());
			drawList->AddText(pPos + ImVec2(thickness * 3.0f, -thickness * 3.0f) - labelSize * 0.65f, IM_COL32(255, 255, 255, 255), std::to_string(i + 1).c_str());
		}
	}

	if (!readOnly && !mouseAbsorbed)
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			// Check if user has clicked on a curve and add a new point if so
			ImVec2 mousePos = ImGui::GetIO().MousePos;

			for (int i = 0; i < pointCount - 1; i++)
			{
				if (quadratic)
				{
					// TODO
				}
				else
				{
					ImVec2 p1 = (*points)[i].position * pScale + pOffset;
					ImVec2 p2 = (*points)[i].controlPoint2 * pScale + pOffset;
					ImVec2 p3 = (*points)[i + 1ll].controlPoint1 * pScale + pOffset;
					ImVec2 p4 = (*points)[i + 1ll].position * pScale + pOffset;

					ImVec2 closestPoint = ImBezierCubicClosestPointCasteljau(p1, p2, p3, p4, mousePos, ImGui::GetStyle().CurveTessellationTol);
					float distanceSq = ImLengthSqr(mousePos - closestPoint);

					if (distanceSq < thickness * thickness * 3.0f + 1.0f)
					{
						// Find t parameter for closest point
						float t;
						{
							// Sample 20 points along the curve and find the closest one, 
							// then sample 20 more points between the closest point and its neighbors to refine t

							float closestT = 0.0f;
							float closestDistanceSq = FLT_MAX;
							for (int j = 0; j <= 20; j++)
							{
								float sampleT = j / 20.0f;
								ImVec2 samplePoint = ImBezierCubicCalc(p1, p2, p3, p4, sampleT);
								float sampleDistanceSq = ImLengthSqr(mousePos - samplePoint);

								if (sampleDistanceSq < closestDistanceSq)
								{
									closestDistanceSq = sampleDistanceSq;
									closestT = sampleT;
								}
							}

							t = closestT;
							for (int j = 0; j < 20; j++)
							{
								float sampleT1 = t - 0.01f;
								float sampleT2 = t + 0.01f;

								ImVec2 samplePoint1 = ImBezierCubicCalc(p1, p2, p3, p4, sampleT1);
								ImVec2 samplePoint2 = ImBezierCubicCalc(p1, p2, p3, p4, sampleT2);

								float sampleDistanceSq1 = ImLengthSqr(mousePos - samplePoint1);
								float sampleDistanceSq2 = ImLengthSqr(mousePos - samplePoint2);

								if (sampleDistanceSq1 < closestDistanceSq)
								{
									closestDistanceSq = sampleDistanceSq1;
									t = sampleT1;
								}
								else if (sampleDistanceSq2 < closestDistanceSq)
								{
									closestDistanceSq = sampleDistanceSq2;
									t = sampleT2;
								}
								else
								{
									break;
								}
							}
						}

						float boundsDim = max(pointBounds.GetWidth(), pointBounds.GetHeight());

						// Insert new point into curve
						BezierPoint newPoint;
						newPoint.position = ImBezierCubicCalc((*points)[i].position, (*points)[i].controlPoint2, (*points)[i + 1ll].controlPoint1, (*points)[i + 1ll].position, t);
						newPoint.controlPoint1 = ImBezierCubicCalc((*points)[i].position, (*points)[i].controlPoint2, (*points)[i + 1ll].controlPoint1, (*points)[i + 1ll].position, t - 0.01f);
						newPoint.controlPoint2 = ImBezierCubicCalc((*points)[i].position, (*points)[i].controlPoint2, (*points)[i + 1ll].controlPoint1, (*points)[i + 1ll].position, t + 0.01f);

						dx::XMFLOAT2 point{}, cp1ToCp2{};
						point.x = newPoint.position.x;
						point.y = newPoint.position.y;
						cp1ToCp2.x = newPoint.controlPoint2.x - newPoint.controlPoint1.x;
						cp1ToCp2.y = newPoint.controlPoint2.y - newPoint.controlPoint1.y;

						dx::XMVECTOR pointVec = Load(point);
						dx::XMVECTOR tangent = dx::XMVector2Normalize(Load(cp1ToCp2));

						dx::XMFLOAT2 cp1, cp2;
						Store(cp1, dx::XMVectorMultiplyAdd(tangent, dx::XMVectorReplicate(-0.25f * boundsDim), pointVec));
						Store(cp2, dx::XMVectorMultiplyAdd(tangent, dx::XMVectorReplicate(0.25f * boundsDim), pointVec));

						newPoint.controlPoint1 = ImVec2(cp1.x, cp1.y);
						newPoint.controlPoint2 = ImVec2(cp2.x, cp2.y);

						points->insert(points->begin() + i + 1ll, newPoint);
						changed = true;
						break;
					}
				}
			}
		}
	}

	drawList->PopClipRect();

	ImGui::EndChild();
	return changed;
}
