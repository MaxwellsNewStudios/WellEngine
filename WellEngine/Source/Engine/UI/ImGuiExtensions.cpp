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


static bool IsMouseHoveringLine(const ImVec2 &p1, const ImVec2 &p2, float thickness)
{
	ImVec2 lineDir = p2 - p1;
	float lineLength = sqrtf(lineDir.x * lineDir.x + lineDir.y * lineDir.y);
	if (lineLength == 0.0f)
		return false;

	lineDir.x /= lineLength;
	lineDir.y /= lineLength;

	ImVec2 mousePos = ImGui::GetIO().MousePos;
	ImVec2 toMouse = mousePos - p1;

	float t = toMouse.x * lineDir.x + toMouse.y * lineDir.y;
	t = max(0.0f, min(lineLength, t));

	ImVec2 closestPoint = p1 + ImVec2(lineDir.x * t, lineDir.y * t);
	ImVec2 diff = mousePos - closestPoint;

	float distanceSq = diff.x * diff.x + diff.y * diff.y;
	return distanceSq <= thickness * thickness;
}

static void MoveCP(ImVec2 &cp, ImVec2 &cpM, const ImVec2 &p, const ImVec2 &newPos, bool jointed)
{
	cp = newPos;
	if (jointed)
		cpM = p * 2.0f - cp; // Set mirror control point to be symmetric across the main point
}

bool ImGui::CurveEdit(const char *label, std::vector<BezierPoint> *points, const ImVec2 &size, const ImRect &pointBounds, float thickness, ImRect padding, ImVec2i gridLines, ImGuiCurveEditFlags flags)
{
	bool linear			= (flags & ImGuiCurveEditFlags_Linear) != 0;
	bool quadratic		= (flags & ImGuiCurveEditFlags_Quadratic) != 0;
	bool jointed		= (flags & ImGuiCurveEditFlags_Jointed) != 0;
	bool forceSpanWidth = (flags & ImGuiCurveEditFlags_ForceSpanWidth) != 0;
	bool readOnly		= (flags & ImGuiCurveEditFlags_ReadOnly) != 0;
	bool noLabels		= (flags & ImGuiCurveEditFlags_NoLabels) != 0;
	bool noPoints		= (flags & ImGuiCurveEditFlags_NoPoints) != 0;
	bool clampX			= (flags & ImGuiCurveEditFlags_ClampX) != 0;
	bool clampY			= (flags & ImGuiCurveEditFlags_ClampY) != 0;

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
	ImRect contentArea = ImRect(drawArea.Min + ImVec2(padding.Min.x, padding.Max.y), drawArea.Max - ImVec2(padding.Max.x, padding.Min.y));
	ImVec2 contentSize = contentArea.GetSize();

	ImVec2 pScale = ImVec2(contentArea.GetWidth() / pointBounds.GetWidth(), -contentArea.GetHeight() / pointBounds.GetHeight());
	ImVec2 pOffset = windowPos + ImVec2(padding.Min.x - (pointBounds.Min.x * pScale.x), padding.Max.y + contentArea.GetHeight() - (pointBounds.Min.y * pScale.y));

	drawList->PushClipRect(drawArea.Min, drawArea.Max, true);

	// Draw barrier rect around content area
	drawList->AddRect(contentArea.Min, contentArea.Max, IM_COL32(96, 96, 96, 255), 0.0f, 0, 2.0f);

	// Draw grid lines
	if (gridLines.x > 0)
	{
		for (int i = 0; i <= gridLines.x; i++)
		{
			float t = (float)i / gridLines.x;
			ImVec2 p1 = ImVec2(contentArea.Min.x + t * contentSize.x, contentArea.Min.y);
			ImVec2 p2 = ImVec2(contentArea.Min.x + t * contentSize.x, contentArea.Max.y);
			drawList->AddLine(p1, p2, IM_COL32(80, 80, 80, 255), 1.0f);

			// Label x-value to the bottom of the grid line
			if (!noLabels)
			{
				char label[32];
				snprintf(label, 32, "%.2f", pointBounds.Min.x + t * pointBounds.GetWidth());
				ImVec2 textSize = ImGui::CalcTextSize(label);
				drawList->AddText(ImVec2(p1.x - textSize.x * 0.5f, contentArea.Max.y + 8), IM_COL32(255, 255, 255, 255), label);
			}
		}
	}
	if (gridLines.y > 0)
	{
		for (int i = 0; i <= gridLines.y; i++)
		{
			float t = (float)i / gridLines.y;
			ImVec2 p1 = ImVec2(contentArea.Min.x, contentArea.Min.y + t * contentSize.y);
			ImVec2 p2 = ImVec2(contentArea.Max.x, contentArea.Min.y + t * contentSize.y);
			drawList->AddLine(p1, p2, IM_COL32(80, 80, 80, 255), 1.0f);

			// Label y-value to the left of the grid line
			if (!noLabels)
			{
				char label[32];
				snprintf(label, 32, "%.2f", pointBounds.Max.y - t * pointBounds.GetHeight());
				ImVec2 textSize = ImGui::CalcTextSize(label);
				drawList->AddText(ImVec2(contentArea.Min.x - textSize.x - 8, p1.y - textSize.y * 0.5f), IM_COL32(255, 255, 255, 255), label);
			}
		}
	}

	for (int i = 0; i < pointCount - 1; i++)
	{
		if (linear)
		{ 
			ImVec2 p1 = (*points)[i].position * pScale + pOffset;
			ImVec2 p2 = (*points)[i + 1ll].position * pScale + pOffset;

			drawList->AddLine(p1, p2, IM_COL32(128, 128, 128, 255), thickness);
		}
		else if (quadratic)
		{
			ImVec2 p1 = (*points)[i].position * pScale + pOffset;
			ImVec2 p2 = (*points)[i].controlPoint1 * pScale + pOffset;
			ImVec2 p3 = (*points)[i + 1ll].position * pScale + pOffset;

			drawList->AddBezierQuadratic(p1, p2, p3, IM_COL32(128, 128, 128, 255), thickness);
		}
		else
		{
			ImVec2 p1 = (*points)[i].position * pScale + pOffset;
			ImVec2 p2 = (*points)[i].controlPoint2 * pScale + pOffset;
			ImVec2 p3 = (*points)[i + 1ll].controlPoint1 * pScale + pOffset;
			ImVec2 p4 = (*points)[i + 1ll].position * pScale + pOffset;

			drawList->AddBezierCubic(p1, p2, p3, p4, IM_COL32(128, 128, 128, 255), thickness);
		}
	}

	bool isDragging = false;
	bool mouseAbsorbed = false;
	bool changed = false;

	if (forceSpanWidth)
	{
		// Ensure first and last points span the entire width of the bounds
		ImVec2 &firstPos = (*points)[0].position;
		ImVec2 &lastPos = (*points)[pointCount - 1ll].position;

		if (firstPos.x != pointBounds.Min.x)
		{
			firstPos.x = pointBounds.Min.x;
			changed = true;
		}

		if (lastPos.x != pointBounds.Max.x)
		{
			lastPos.x = pointBounds.Max.x;
			changed = true;
		}
	}

	for (int i = 0; i < pointCount; i++)
	{
		ImVec2 pPos = (*points)[i].position * pScale + pOffset;

		static ImVec2 deltaBuffer = ImVec2(0, 0);

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
				deltaBuffer = ImVec2(0, 0);
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

					// Delta must be used to neutralize buffer before the point can move again
					// This keeps the cursor following to the point even when returning from out of bounds
					if (deltaBuffer.x != 0 || deltaBuffer.y != 0)
					{
						if (deltaBuffer.x != 0 && delta.x != 0 && std::signbit(deltaBuffer.x) != std::signbit(delta.x))
						{
							if (fabsf(delta.x) > fabsf(deltaBuffer.x))
							{
								delta.x += deltaBuffer.x;
								deltaBuffer.x = 0;
							}
							else
							{
								deltaBuffer.x += delta.x;
								delta.x = 0;
							}
						}

						if (deltaBuffer.y != 0 && delta.y != 0 && std::signbit(deltaBuffer.y) != std::signbit(delta.y))
						{
							if (fabsf(delta.y) > fabsf(deltaBuffer.y))
							{
								delta.y += deltaBuffer.y;
								deltaBuffer.y = 0;
							}
							else
							{
								deltaBuffer.y += delta.y;
								delta.y = 0;
							}
						}
					}

					if (clampX)
					{
						float newPosX = (*points)[i].position.x + delta.x;
						newPosX = CLAMP(newPosX, pointBounds.Min.x, pointBounds.Max.x);

						float newDeltaX = newPosX - (*points)[i].position.x;
						deltaBuffer.x += (delta.x - newDeltaX); // Track how much delta was negated
						delta.x = newDeltaX;
					}
					if (clampY)
					{
						float newPosY = (*points)[i].position.y + delta.y;
						newPosY = max(pointBounds.Min.y, min(pointBounds.Max.y, newPosY));

						float newDeltaY = newPosY - (*points)[i].position.y;
						deltaBuffer.y += (delta.y - newDeltaY); // Track how much delta was negated
						delta.y = newDeltaY;
					}

					(*points)[i].position += delta;
					(*points)[i].controlPoint1 += delta;
					(*points)[i].controlPoint2 += delta;

					changed = true;
					isDragging = true;

					if (!noLabels)
					{
						// While dragging, draw vector coords to bottom right
						char label[64];
						snprintf(label, 64, "X%.3f\nY%.3f", (*points)[i].position.x, (*points)[i].position.y);
						ImVec2 textSize = ImGui::CalcTextSize(label);
						drawList->AddText(ImVec2(contentArea.Max.x - textSize.x - 8, contentArea.Max.y - textSize.y - 8), IM_COL32(255, 255, 255, 255), label);
					}
				}
			}
		}

		if (!linear && (pointSelectState == 1 || pointPrevSelectState == 1))
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

				drawList->AddLine(cpPos, pPos, IM_COL32(255, 255, 255, 255), thickness * 0.5f);
				drawList->AddCircleFilled(cpPos, thickness, cpColor);

				// Handle control point dragging
				ImGuiID cpID = window->GetID(cpLabel.c_str());

				int cpState = storage->GetInt(cpID);
				bool isCPSelected = cpState == 1;

				if (!mouseAbsorbed)
				{
					bool isHoveringLine = IsMouseHoveringLine(pPos, cpPos, thickness);
					bool isHoveringRect = ImGui::IsMouseHoveringRect(cpPos - ImVec2(thickness, thickness) * 2.0f, cpPos + ImVec2(thickness, thickness) * 2.0f);

					if (isHoveringLine || isHoveringRect)
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						{
							if (!readOnly && isHoveringLine && !isHoveringRect)
							{
								// Snap control point to mouse when clicking on line
								ImVec2 mousePos = ImGui::GetIO().MousePos;
								mousePos = (mousePos - pOffset) / pScale; // Convert to curve space

								MoveCP(*cp, *cpMirror, (*points)[i].position, mousePos, jointed);
								changed = true;
							}

							storage->SetInt(cpID, isCPSelected ? 0 : 1);
						}

						mouseAbsorbed = true;
					}
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

						MoveCP(*cp, *cpMirror, (*points)[i].position, (*cp) + delta, jointed);
						changed = true;
					}

					mouseAbsorbed = true;
				}
			}
		}

		if (!noPoints)
			drawList->AddCircleFilled(pPos, thickness * 1.6f, IM_COL32(80, 255, 80, 255));

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
				if (linear)
				{
					ImVec2 p1 = (*points)[i].position * pScale + pOffset;
					ImVec2 p2 = (*points)[i + 1ll].position * pScale + pOffset;

					if (!IsMouseHoveringLine(p1, p2, thickness))
						continue;
					
					ImVec2 closestPoint = ImVec2(
						CLAMP(mousePos.x, min(p1.x, p2.x), max(p1.x, p2.x)),
						CLAMP(mousePos.y, min(p1.y, p2.y), max(p1.y, p2.y))
					);

					float distanceSq = ImLengthSqr(mousePos - closestPoint);

					if (distanceSq > thickness * thickness * 2.5f + 1.0f)
						continue;

					// Insert new point into curve
					BezierPoint newPoint;
					newPoint.position = (closestPoint - pOffset) / pScale; // Convert back to curve space
					newPoint.controlPoint1 = newPoint.position - ImVec2(0.1f, 0);
					newPoint.controlPoint2 = newPoint.position + ImVec2(0.1f, 0);

					points->insert(points->begin() + i + 1ll, newPoint);
					changed = true;
					break;
					
				}
				else if (quadratic)
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

					if (distanceSq > thickness * thickness * 2.5f + 1.0f)
						continue;
					
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

	drawList->PopClipRect();

	ImGui::EndChild();
	return changed;
}
