#include "stdafx.h"
#include "Behaviours/DebugPlayerBehaviour.h"
#include "Entity.h"
#include "Debug/DebugData.h"
#include "Scenes/Scene.h"
#include "Behaviours/UIButtonBehaviour.h"
#include "Behaviours/FlashlightBehaviour.h"
#include "Behaviours/GraphNodeBehaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"
#include "Behaviours/PlayerMovementBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

#ifdef DEBUG_BUILD
bool DebugPlayerBehaviour::Start()
{
	if (_name.empty())
		_name = "DebugPlayerBehaviour"; // For categorization in ImGui.

	Scene *scene = GetScene();
	SceneHolder *sceneHolder = scene->GetSceneHolder();
	Content *content = scene->GetContent();

	if (scene->GetDebugPlayer())
	{
		if (scene->GetDebugPlayer() != this)
		{
			if (!sceneHolder->RemoveEntity(GetEntity()))
			{
				ErrMsg("Failed to remove second DebugPlayerBehaviour entity!");
				return false;
			}

			return true;
		}
	}

	// Create main camera
	{
		Entity *ent = nullptr;
		if (!scene->CreateEntity(&ent, "MainCamera", { {0,0,0},{.1f,.1f,.1f},{0,0,0,1} }, false))
		{
			ErrMsg("Failed to create MainCamera entity!");
			return false;
		}
		ent->GetTransform()->SetPosition({ 0.0f, 2.0f, -3.0f });

		ProjectionInfo projInfo = ProjectionInfo(
			65.0f * (XM_PI / 180.0f), 
			16.0f / 9.0f, 
			{ DebugData::Get().debugCamNearDist, DebugData::Get().debugCamFarDist }
		);

		CameraBehaviour *camera = new CameraBehaviour(projInfo);

		if (!camera->Initialize(ent))
		{
			ErrMsg("Failed to bind MainCamera behaviour!");
			return false;
		}

		ent->SetSerialization(false);
		camera->SetSerialization(false);

		_mainCamera = camera;
	}

#ifdef EDIT_MODE
	// Create orthographic camera
	{
		Entity *ent = nullptr;
		if (!scene->CreateEntity(&ent, "OrthoCamera", { {0,0,0},{.1f,.1f,.1f},{0,0,0,1} }, false))
		{
			ErrMsg("Failed to create OrthoCamera entity!");
			return false;
		}

		Transform &camTrans = *ent->GetTransform();
		const BoundingBox &sceneBounds = scene->GetSceneHolder()->GetBounds();

		camTrans.SetPosition({
			sceneBounds.Center.x,
			sceneBounds.Center.y + sceneBounds.Extents.y, 
			sceneBounds.Center.z
		});
		camTrans.SetLookDir({0, -1, 0}, {0, 0, 1}, World);
		ent->SetSerialization(false);

		ProjectionInfo projInfo = ProjectionInfo(
			2.0f * max(sceneBounds.Extents.x, sceneBounds.Extents.z), 
			16.0f / 9.0f, 
			{ 1.0f, 1000.0f }
		);
		CameraBehaviour *camera = new CameraBehaviour(projInfo, true);

		if (!camera->Initialize(ent))
		{
			ErrMsg("Failed to bind OrthoCamera behaviour!");
			return false;
		}

		_secondaryCamera = camera;
	}
#endif

	_currCameraPtr = _mainCamera;
	GetScene()->SetViewCamera(_currCameraPtr.GetAs<CameraBehaviour>());
	scene->SetDebugPlayer(this);

	QueueUpdate();

	return true;
}

bool DebugPlayerBehaviour::Update(TimeUtils &time, const Input &input)
{
	Scene *scene = GetScene();
	SceneHolder *sceneHolder = scene->GetSceneHolder();
	std::vector<std::unique_ptr<Entity>> *globalEntities = scene->GetGlobalEntities();
	SpotLightCollection *spotlights = scene->GetSpotlights();
	PointLightCollection *pointlights = scene->GetPointlights();
	ID3D11Device *device = scene->GetDevice();
	Content *content = scene->GetContent();
	Graphics *graphics = scene->GetGraphics();
	DebugDrawer &debugDraw = DebugDrawer::Instance();
	DebugData &debugData = DebugData::Get();

	bool additiveSelect = BindingCollection::IsTriggered(InputBindings::InputAction::AdditiveSelect);

	if (_mainCamera.IsValid())
	{
		// Check if aspect ratio needs to be updated
		XMUINT2 sceneRenderSize = input.GetSceneRenderSize();
		float screenAspect = (float)sceneRenderSize.x / (float)sceneRenderSize.y;

		CameraBehaviour *mainCam = _mainCamera.GetAs<CameraBehaviour>();
		float camAspect = mainCam->GetAspectRatio();

		// approximate float check
		if (std::abs(camAspect - screenAspect) > 0.001f)
		{
			mainCam->SetAspectRatio(screenAspect);
		}

		debugData.debugCamNearDist = mainCam->GetPlanes().nearZ;
		debugData.debugCamFarDist = mainCam->GetPlanes().farZ;
	}

	if (!_secondaryCamera.IsValid())
	{
		Entity *secondaryCam = sceneHolder->GetEntityByName("playerCamera");
		if (secondaryCam)
		{
			CameraBehaviour *newSecondaryCam = nullptr;
			secondaryCam->GetBehaviourByType<CameraBehaviour>(newSecondaryCam);
			_secondaryCamera = newSecondaryCam;
		}
		else
		{
			secondaryCam = sceneHolder->GetEntityByName("OrthoCamera");
			if (secondaryCam)
			{
				CameraBehaviour *newSecondaryCam = nullptr;
				secondaryCam->GetBehaviourByType<CameraBehaviour>(newSecondaryCam);
				_secondaryCamera = newSecondaryCam;
			}
			else
			{
				_secondaryCamera = _mainCamera;
			}
		}
	}

	if (input.IsInFocus()) // Handle user input while window is in focus
	{
		if (input.GetKey(KeyCode::B) == KeyState::Pressed)	// Toggle ray cast method (from cam/mouse)
			_rayCastFromMouse = !_rayCastFromMouse;

		if (input.GetKey(KeyCode::Z) == KeyState::Pressed)
			_useMainCamera = !_useMainCamera;
		if (input.GetKey(KeyCode::Q) == KeyState::Pressed)
			_useMainCamera = true;

		if (_cursorPositioningTarget)
		{
			Entity *positioningTargetEnt = _cursorPositioningTarget.Get();
			Transform *positioningTargetTrans = positioningTargetEnt->GetTransform();

			if (input.GetKey(KeyCode::LeftAlt) == KeyState::Held)
			{
				// Rotate cursor positioning target with mouse scroll
				float scroll = input.GetMouse().scroll.y;

				if (input.GetKey(KeyCode::LeftShift) == KeyState::Held)
					scroll *= 45.0f;
				else if (input.GetKey(KeyCode::LeftControl) == KeyState::Held)
					scroll *= 5.0f;
				else
					scroll *= 15.0f;

				if (scroll != 0.0f)
				{
					positioningTargetTrans->Rotate(
						{ 0, scroll * DEG_TO_RAD, 0 },
						(ReferenceSpace)DebugData::Get().transformSpace
					);
				}
			}

			static bool skipRayCast = false;
			if (input.GetKey(KeyCode::M4) == KeyState::Pressed)
				skipRayCast = !skipRayCast;

			XMFLOAT3A cursorScenePos;
			XMFLOAT3A castDir;
			RaycastOut out;

			bool rayHit = false;
			if (!_rayCastFromMouse && input.IsCursorLocked())
				rayHit = RayCastFromCamera(out, cursorScenePos, castDir);
			else
				rayHit = RayCastFromMouse(out, cursorScenePos, castDir, input);

			if (rayHit && !skipRayCast)
			{
				positioningTargetTrans->SetPosition(cursorScenePos, World);
			}
			else if (_currCameraPtr.IsValid())
			{
				static float dist = 10.0f;

				float scroll = input.GetMouse().scroll.y;
				if (input.GetKey(KeyCode::LeftShift) == KeyState::Held)
					scroll *= 10.0f;
				if (input.GetKey(KeyCode::LeftControl) == KeyState::Held)
					scroll *= 0.2f;
				if (input.GetKey(KeyCode::LeftAlt) == KeyState::Held)
					scroll *= 0.0f;

				if (scroll != 0.0f)
				{
					dist += scroll * 0.25f;
					if (dist < 0.1f)
						dist = 0.1f;
				}

				const XMFLOAT3A camPos = _currCameraPtr.Get()->GetTransform()->GetPosition(World);
				const XMFLOAT3A placementPos = {
					camPos.x + castDir.x * dist,
					camPos.y + castDir.y * dist,
					camPos.z + castDir.z * dist
				};

				positioningTargetTrans->SetPosition(placementPos, World);
			}

			if (input.GetKey(KeyCode::Enter) == KeyState::Pressed ||
				input.GetKey(KeyCode::M2) == KeyState::Pressed ||
				input.GetKey(KeyCode::M3) == KeyState::Pressed)
			{
				PositionWithCursor(nullptr);
				Select(positioningTargetEnt);
			}
		}
		else if (_currSelection.size() == 1)
		{
			if (input.GetKey(KeyCode::M5) == KeyState::Pressed)
			{
				PositionWithCursor(_currSelection[0].Get());
			}
		}

		if (input.GetKey(KeyCode::M2) == KeyState::Pressed)
		{
			Entity *newSelectEnt = nullptr;

			RaycastOut out;
			if (_rayCastFromMouse || !input.IsCursorLocked())
			{
				if (RayCastFromMouse(out, input))
				{
					UINT index;
					if (IsSelected(out.entity, &index, true))
					{
						DeselectIndex(index);
					}
					else
					{
						newSelectEnt = out.entity;
					}
				}
				else if (!additiveSelect)
				{
					ClearSelection();
				}
			}
			else
			{
				if (RayCastFromCamera(out))
				{
					UINT index;
					if (IsSelected(out.entity, &index, true))
					{
						DeselectIndex(index);
					}
					else
					{
						newSelectEnt = out.entity;
					}
				}
				else if (!additiveSelect)
				{
					ClearSelection();
				}
			}

			if (newSelectEnt)
			{
				if (Entity *parent = newSelectEnt->GetParent())
				{
					if (parent->HasBehaviourOfType<BillboardMeshBehaviour>())
						newSelectEnt = parent;
				}

				Select(newSelectEnt, additiveSelect);
			}
		}

		// Select entity. If it has a UIButtonBehaviour, run it
		if (input.GetKey(KeyCode::M3) == KeyState::Pressed)
		{
			RaycastOut out;
			bool hasHit = false;

			if (_rayCastFromMouse)
			{
				hasHit = RayCastFromMouse(out, input);
			}
			else
			{
				hasHit = RayCastFromCamera(out);
			}

			if (hasHit)
			{
				if (!out.entity->InitialOnSelect())
				{
					ErrMsg("OnSelect Failed!");
					return false;
				}
			}
		}

		int newEditType = -1;
		if		(input.GetKey(KeyCode::Q) == KeyState::Pressed)	newEditType = (int)None;
		else if (input.GetKey(KeyCode::E) == KeyState::Pressed) newEditType = (int)Translate;
		else if (input.GetKey(KeyCode::R) == KeyState::Pressed)	newEditType = (int)Rotate;
		else if (input.GetKey(KeyCode::T) == KeyState::Pressed) newEditType = (int)Scale;
		else if (input.GetKey(KeyCode::Y) == KeyState::Pressed) newEditType = (int)Universal;
		else if (input.GetKey(KeyCode::U) == KeyState::Pressed) newEditType = (int)Bounds;

		if (newEditType >= 0)
			debugData.transformType = newEditType;

		if (!input.IsCursorLocked() && !_currSelection.empty() && BindingCollection::IsTriggered(InputBindings::InputAction::CopySelected))
		{
			// Copy selected entities by serializing and deserializing them
			std::vector<Entity *> newEnts;
			newEnts.reserve(_currSelection.size());

			for (auto &entRef : _currSelection)
			{
				Entity *toCopy = entRef.Get();

				json::Document doc;
				json::Value entObj = json::Value(json::kObjectType);

				if (!scene->SerializeEntity(doc.GetAllocator(), entObj, toCopy, true))
				{
					ErrMsg("Failed to serialize entity!");
					return false;
				}

				Entity *ent = nullptr;
				if (!scene->DeserializeEntity(entObj, &ent))
				{
					ErrMsg("Failed to deserialize entity!");
					return false;
				}

				scene->RunPostDeserializeCallbacks();

				ent->SetParent(toCopy->GetParent(), false);
				ent->GetTransform()->SetMatrix(toCopy->GetTransform()->GetMatrix(Local), Local);

				newEnts.push_back(ent);
			}

			Select(newEnts.data(), newEnts.size());
		}

		for (int i = 0; i < _duplicateBinds.size(); i++)
		{
			Entity *ent = sceneHolder->GetEntityByID(_duplicateBinds[i].second);

			if (ent)
				continue;

			RemoveDuplicateBind(_duplicateBinds[i].second);
			i--;
		}

		for (auto &duplicateBind : _duplicateBinds)
		{
			if (input.GetKey(duplicateBind.first) == KeyState::Pressed)
			{
				// Copy by serializing and deserializing the entity
				Entity *dupeEnt = sceneHolder->GetEntityByID(duplicateBind.second);

				if (dupeEnt)
				{
					json::Document doc;
					json::Value entObj = json::Value(json::kObjectType);

					if (!scene->SerializeEntity(doc.GetAllocator(), entObj, dupeEnt, true))
					{
						ErrMsg("Failed to serialize entity!");
						return false;
					}

					Entity *ent = nullptr;
					if (!scene->DeserializeEntity(entObj, &ent))
					{
						ErrMsg("Failed to deserialize entity!");
						return false;
					}

					scene->RunPostDeserializeCallbacks();

					if (ent)
						PositionWithCursor(ent);
				}
				else
				{
					RemoveDuplicateBind(duplicateBind.second);
				}
			}
		}

		if (_currSelection.size() == 1)
		{
			Entity *selectedEnt = _currSelection[0].Get();

			if (input.GetKey(KeyCode::Add, true) == KeyState::Pressed)
			{
				int selectionIndex = sceneHolder->GetEntityIndex(selectedEnt);
				Entity *upperEntity = sceneHolder->GetEntity(selectionIndex + 1);

				if (!upperEntity)
					upperEntity = sceneHolder->GetEntity(0);

				Select(upperEntity, additiveSelect);
			}
			else if (input.GetKey(KeyCode::Subtract, true) == KeyState::Pressed)
			{
				int selectionIndex = sceneHolder->GetEntityIndex(selectedEnt);
				Entity *lowerEntity = sceneHolder->GetEntity(selectionIndex - 1);

				if (!lowerEntity)
					lowerEntity = sceneHolder->GetEntity(sceneHolder->GetEntityCount() - 1);

				Select(lowerEntity, additiveSelect);
			}
		}

		if (input.GetKey(KeyCode::Q) == KeyState::Pressed)
			_currCamera = -3;

		static bool freezeCamera = false;
		if (input.GetKey(KeyCode::K) == KeyState::Pressed)
			freezeCamera = !freezeCamera;

		float currSpeed = 5.0f * debugData.movementSpeed;
		if (input.GetKey(KeyCode::LeftShift) == KeyState::Held)
			currSpeed *= 10.0f;
		if (input.GetKey(KeyCode::LeftControl) == KeyState::Held)
			currSpeed *= 0.2f;

		// Move camera
		if (_currCameraPtr && !freezeCamera)
		{
			const XMFLOAT2 mPos = input.GetLocalMousePos();
			const MouseState mState = input.GetMouse();
			Transform *camTransform = _currCameraPtr.Get()->GetTransform();

			if (input.IsCursorLocked())
			{
				if (BindingCollection::IsTriggered(InputBindings::InputAction::StrafeRight))
					camTransform->MoveRelative({ time.GetDeltaTime() * currSpeed, 0.0f, 0.0f }, World);
				else if (BindingCollection::IsTriggered(InputBindings::InputAction::StrafeLeft))
					camTransform->MoveRelative({ -time.GetDeltaTime() * currSpeed, 0.0f, 0.0f }, World);

				if (input.IsPressedOrHeld(KeyCode::Space))
					camTransform->MoveRelative({ 0.0f, time.GetDeltaTime() * currSpeed, 0.0f }, World);
				else if (input.IsPressedOrHeld(KeyCode::X))
					camTransform->MoveRelative({ 0.0f, -time.GetDeltaTime() * currSpeed, 0.0f }, World);

				if (BindingCollection::IsTriggered(InputBindings::InputAction::WalkForward))
					camTransform->MoveRelative({ 0.0f, 0.0f, time.GetDeltaTime() * currSpeed }, World);
				else if (BindingCollection::IsTriggered(InputBindings::InputAction::WalkBackward))
					camTransform->MoveRelative({ 0.0f, 0.0f, -time.GetDeltaTime() * currSpeed }, World);

				float sensitivity = input.GetMouseSensitivity();

				if (mState.delta.x != 0.0f)
				{
					XMFLOAT3A u = camTransform->GetUp();
					float invert = (u.y > 0 ? 1.0f : -1.0f);
					camTransform->RotateAxis({ 0, 1, 0 }, (sensitivity * static_cast<float>(mState.delta.x) / 360.0f) * invert, World);
				}

				if (mState.delta.y != 0)
				{
					camTransform->RotatePitch(sensitivity * static_cast<float>(mState.delta.y) / 360.0f, Local);
				}

				// Mouse scroll zoom or move
				if (mState.scroll.y != 0.0f && (!_cursorPositioningTarget || input.GetKey(KeyCode::LeftAlt) == KeyState::None))
				{
					CameraBehaviour *cam = _currCameraPtr.GetAs<CameraBehaviour>();

					bool shiftHeld = input.GetKey(KeyCode::LeftShift) == KeyState::Held;
					bool ctrlHeld = input.GetKey(KeyCode::LeftControl) == KeyState::Held;
					float scroll = mState.scroll.y;

					if (shiftHeld)
					{
						float speed = debugData.movementSpeed * (ctrlHeld ? 0.5f : 5.0f);

						if (input.IsCursorLocked())
						{
							// Move camera in the direction of the view ray
							speed *= 2.0f;
							camTransform->MoveRelative({ 0, 0, scroll * speed }, Local);
						}
						else if (_currCameraPtr)
						{
							// Move camera in the direction of the cursor position
							XMFLOAT3A origin{}, dir{};
							cam->GetViewRay(To2(mPos), origin, dir);

							XMVECTOR dirVec = Load(dir) * scroll * speed;
							Store(dir, dirVec);

							camTransform->Move(dir, World);
						}
					}
					else if (ctrlHeld)
					{
						// Zoom view camera's FOV
						if (cam->GetOrtho())
						{
							float currFOV = cam->GetFOV();
							float newFOV = max(currFOV * (1.0f + 0.05f * scroll), 0.1f);
							cam->SetFOV(newFOV);
						}
						else
						{
							float currFOV = cam->GetFOV() * RAD_TO_DEG;
							float newFOV = std::clamp(currFOV + scroll * 2.5f, 5.0f, 170.0f);
							cam->SetFOV(newFOV * DEG_TO_RAD);
						}
					}
				}
			}
			else if (!ImGuizmo::IsUsingAny() && input.HasKeyboardFocus())
			{
				// Control camera by dragging mouse
				// Space + Left Mouse Button - Pan
				// Left Alt + Left Mouse Button - Orbit

				enum class DragMode {
					None,
					Orbit,
					Pan
				};
				static DragMode dragMode = DragMode::None;

				static float fallbackDepth = 100.0f;
				static XMFLOAT3 dragOrigin3D{};
				static XMFLOAT2 dragOrigin2D{};
				static XMFLOAT3 forwardOriginDiff{};
				static XMFLOAT2 lastDragOrigin2D{};

				bool pressedM1 = input.GetKey(KeyCode::M1) == KeyState::Pressed;
				bool holdingM1 = input.GetKey(KeyCode::M1, true) == KeyState::Held;

				if (pressedM1)
				{
					bool holdingAlt = (int)input.GetKey(KeyCode::LeftAlt) & (int)KeyState::PressedHeld;
					bool holdingSpace = (int)input.GetKey(KeyCode::Space) & (int)KeyState::PressedHeld;

					lastDragOrigin2D = dragOrigin2D = mPos;
					XMFLOAT3A pos, dir;

					RaycastOut out;
					if (RayCastFromMouse(out, pos, dir, input))
					{
						dragOrigin3D = pos;
						fallbackDepth = out.distance;
					}
					else
					{
						XMFLOAT3A camPos = camTransform->GetPosition(World);
						dragOrigin3D = {
							camPos.x + dir.x * fallbackDepth,
							camPos.y + dir.y * fallbackDepth,
							camPos.z + dir.z * fallbackDepth
						};
					}

					dragMode = DragMode::None;

					if (holdingAlt)
					{
						dragMode = DragMode::Orbit;

						XMVECTOR camRight = Load(camTransform->GetRight(World));
						XMVECTOR camUp = Load(camTransform->GetUp(World));
						XMVECTOR camForward = Load(camTransform->GetForward(World));

						XMVECTOR toOrigin = XMVector3Normalize(Load(dragOrigin3D) - Load(camTransform->GetPosition(World)));
						XMVECTOR forwardDiff = toOrigin - camForward;

						XMVECTOR rightComp = XMVector3Dot(forwardDiff, camRight);
						XMVECTOR upComp = XMVector3Dot(forwardDiff, camUp);
						XMVECTOR forwardComp = XMVector3Dot(forwardDiff, camForward);

						forwardDiff = XMVectorSet(
							XMVectorGetX(rightComp),
							XMVectorGetX(upComp),
							XMVectorGetX(forwardComp),
							0.0f
						);

						Store(forwardOriginDiff, forwardDiff);
					}
					else if (holdingSpace)
					{
						dragMode = DragMode::Pan;
					}
					
				}
				else if (holdingM1 && dragMode != DragMode::None)
				{
					if (dragMode == DragMode::Orbit)
					{
						// Orbit camTransform around 3D drag origin
						XMFLOAT2 mouseDelta = {
							(mPos.x - lastDragOrigin2D.x),
							-(mPos.y - lastDragOrigin2D.y)
						};
						lastDragOrigin2D = mPos;

						float movementSqr = (mouseDelta.x * mouseDelta.x) + (mouseDelta.y * mouseDelta.y);
						constexpr float movementDeadzone = 0.0000001f;
						static bool skipNextDelta = false;

						if (!skipNextDelta && movementSqr >= movementDeadzone)
						{
							float orbitSensitivity = XM_PI;
							mouseDelta.x *= orbitSensitivity;
							mouseDelta.y *= orbitSensitivity;

							// Get current camera position and calculate orbit vector
							XMFLOAT3A currentCamPos = camTransform->GetPosition(World);
							XMVECTOR orbitCenter = Load(dragOrigin3D);
							XMVECTOR orbitToCam = Load(currentCamPos) - orbitCenter;

							// Calculate yaw rotation around world up axis (Y-axis)
							XMVECTOR worldUp = g_XMIdentityR1;
							XMMATRIX yawRotation = XMMatrixRotationAxis(worldUp, mouseDelta.x);

							// Apply yaw rotation to orbit vector
							orbitToCam = XMVector3Transform(orbitToCam, yawRotation);

							// Calculate pitch rotation around a horizontal right axis
							// Use cross product of current orbit vector and world up to get consistent right axis
							XMVECTOR rightAxis = XMVector3Cross(worldUp, orbitToCam);
								
							// Handle gimbal lock case (camera directly above/below orbit center)
							float rightAxisLength = XMVectorGetX(XMVector3Length(rightAxis));
							if (rightAxisLength < 0.001f) 
								rightAxis = g_XMIdentityR0; // Use world X-axis as fallback when orbit vector is parallel to world up
							else 
								rightAxis = XMVector3Normalize(rightAxis);

							XMMATRIX pitchRotation = XMMatrixRotationAxis(rightAxis, mouseDelta.y);

							// Apply pitch rotation to orbit vector
							orbitToCam = XMVector3Transform(orbitToCam, pitchRotation);

							// Calculate new camera position
							XMVECTOR newCamPos = orbitCenter + orbitToCam;
							XMFLOAT3A newCamPosFloat;
							Store(newCamPosFloat, newCamPos);

							// Set new camera position
							camTransform->SetPosition(newCamPosFloat, World);

							// Compensate for initial forward direction difference to avoid roll when orbiting
							// First, convert forwardOriginDiff to world space
							XMVECTOR forwardDiffWorld{};
							forwardDiffWorld += forwardOriginDiff.x * Load(camTransform->GetRight());
							forwardDiffWorld += forwardOriginDiff.y * Load(camTransform->GetUp());
							forwardDiffWorld += forwardOriginDiff.z * Load(camTransform->GetForward());

							// Then, add it to the look direction
							XMVECTOR lookDirVec = XMVector3Normalize(orbitCenter - newCamPos) - forwardDiffWorld;

							XMFLOAT3A lookDir;
							Store(lookDir, lookDirVec);
							camTransform->SetLookDir(lookDir, {0, 1, 0}, World);
						}

						skipNextDelta = Input::Instance().TryWrapMouse();
					}
					else if (dragMode == DragMode::Pan)
					{
						// Pan camTransform such that the 3D drag origin stays under the cursor
						XMFLOAT3A panUpDir = camTransform->GetUp();
						XMFLOAT3A panRightDir = camTransform->GetRight();

						// Calculate mouse movement in screen space
						XMFLOAT2 mouseDelta = {
							mPos.x - dragOrigin2D.x,
							mPos.y - dragOrigin2D.y
						};

						// Get camera direction for plane intersection
						XMFLOAT3A camForward = camTransform->GetForward(World);
						XMFLOAT3A camPos = camTransform->GetPosition(World);

						// Create a plane at the drag origin with the camera's forward direction as normal
						XMVECTOR planeNormal = XMVector3Normalize(Load(camForward));
						XMVECTOR planePoint = Load(dragOrigin3D);

						// Calculate the current mouse ray direction
						CameraBehaviour *camBeh = _currCameraPtr.GetAs<CameraBehaviour>();

						// Convert current mouse position to world ray direction
						const XMFLOAT2 currentMPos = mPos;
						float xNDC = (2.0f * currentMPos.x) - 1.0f;
						float yNDC = 1.0f - (2.0f * currentMPos.y);
						XMFLOAT3A rayClip = { xNDC, yNDC, 1.0f };

						// Transform to world space direction
						XMVECTOR rayClipVec = Load(rayClip);
						XMMATRIX projMatrix = Load(camBeh->GetProjectionMatrix());
						XMVECTOR rayEyeVec = XMVector4Transform(rayClipVec, XMMatrixInverse(nullptr, projMatrix));
						rayEyeVec = XMVectorSet(XMVectorGetX(rayEyeVec), XMVectorGetY(rayEyeVec), 1, 0.0);

						XMMATRIX viewMatrix = Load(camBeh->GetViewMatrix());
						XMVECTOR rayWorldVec = XMVector4Transform(rayEyeVec, XMMatrixInverse(nullptr, viewMatrix));
						rayWorldVec = XMVector3Normalize(rayWorldVec);

						// Calculate intersection with the plane
						XMVECTOR rayOrigin = Load(camPos);
						XMVECTOR rayDir = rayWorldVec;

						// Plane intersection
						float denominator = XMVectorGetX(XMVector3Dot(rayDir, planeNormal));

						if (std::abs(denominator) > 0.0001f) // Avoid division by zero
						{
							XMVECTOR toPlane = planePoint - rayOrigin;
							float t = XMVectorGetX(XMVector3Dot(toPlane, planeNormal)) / denominator;

							if (t > 0.0f) // Ray hits plane in front of camera
							{
								// Calculate intersection point
								XMVECTOR intersectionPoint = rayOrigin + (rayDir * t);

								// Calculate the movement needed to keep drag origin under cursor
								XMVECTOR panMovement = planePoint - intersectionPoint;

								XMFLOAT3A panMovementFloat;
								Store(panMovementFloat, panMovement);

								// Apply the pan movement to the camera
								camTransform->Move(panMovementFloat, World);
							}
						}
					}

					float scroll = mState.scroll.y * -0.1f;
					if (mState.scroll.y != 0.0f)
					{
						bool shiftHeld = (int)input.GetKey(KeyCode::LeftShift, true) & (int)KeyState::PressedHeld;
						bool ctrlHeld = (int)input.GetKey(KeyCode::LeftControl, true) & (int)KeyState::PressedHeld;

						if (shiftHeld)
							scroll *= 3.0f;
						else if (ctrlHeld)
							scroll *= 0.333f;

						if (scroll != 0.0f)
						{
							XMVECTOR dragOrigin = Load(dragOrigin3D);
							XMVECTOR originToCam = Load(camTransform->GetPosition(World)) - dragOrigin;
							originToCam *= (1.0f + scroll);

							XMFLOAT3A zoomedPos;
							Store(zoomedPos, dragOrigin + originToCam);

							camTransform->SetPosition(zoomedPos, World);
						}
					}
				}
				else
				{
					dragMode = DragMode::None;
					
					// Mouse scroll zoom
					if (mState.scroll.y != 0.0f && (!_cursorPositioningTarget || input.GetKey(KeyCode::LeftAlt) == KeyState::None))
					{
						bool ctrlHeld = input.GetKey(KeyCode::LeftControl) == KeyState::Held;

						if (ctrlHeld)
						{
							CameraBehaviour *cam = _currCameraPtr.GetAs<CameraBehaviour>();
							float scroll = mState.scroll.y;

							// Zoom view camera's FOV
							if (cam->GetOrtho())
							{
								float currFOV = cam->GetFOV();
								float newFOV = max(currFOV * (1.0f + 0.05f * scroll), 0.1f);
								cam->SetFOV(newFOV);
							}
							else
							{
								float currFOV = cam->GetFOV() * RAD_TO_DEG;
								float newFOV = std::clamp(currFOV + scroll * 2.5f, 5.0f, 170.0f);
								cam->SetFOV(newFOV * DEG_TO_RAD);
							}
						}
					}
				}
			}
		}
	}

	if (!_currSelection.empty() && input.GetKey(KeyCode::Delete) == KeyState::Pressed && input.HasMouseFocus())
	{
		for (auto &entRef : _currSelection)
		{
			Entity *selectedEnt;
			if (!entRef.TryGet(selectedEnt))
				continue;

			if (!sceneHolder->RemoveEntity(selectedEnt))
			{
				ErrMsg("Failed to remove entity!");
				return false;
			}
		}

		ClearSelection();
	}

	if (_addDuplicateBindForEntity >= 0)
	{
		Entity *ent = sceneHolder->GetEntityByID(_addDuplicateBindForEntity);

		if (ent)
		{
			KeyCode key = input.GetPressedKey();
			if (key != KeyCode::None)
			{
				if (IsValidDuplicateBind(key))
				{
					AddDuplicateBind(key, (UINT)_addDuplicateBindForEntity);
					_addDuplicateBindForEntity = -1;
				}
			}
		}
		else
		{
			_addDuplicateBindForEntity = -1;
		}
	}

	for (int i = 0; i < _duplicateBinds.size(); i++)
	{
		Entity *ent = sceneHolder->GetEntityByID(_duplicateBinds[i].second);

		if (ent)
			continue;

		RemoveDuplicateBind(_duplicateBinds[i].second);
		i--;
	}

	for (auto &duplicateBind : _duplicateBinds)
	{
		if (input.GetKey(duplicateBind.first) == KeyState::Pressed)
		{
			// Copy by serializing and deserializing the entity

			Entity *dupeEnt = sceneHolder->GetEntityByID(duplicateBind.second);

			if (dupeEnt)
			{
				json::Document doc;
				json::Value entObj = json::Value(json::kObjectType);

				if (!scene->SerializeEntity(doc.GetAllocator(), entObj, dupeEnt, true))
				{
					ErrMsg("Failed to serialize entity!");
					return false;
				}

				Entity *ent = nullptr;
				if (!scene->DeserializeEntity(entObj, &ent))
				{
					ErrMsg("Failed to deserialize entity!");
					return false;
				}

				scene->RunPostDeserializeCallbacks();

				if (ent)
					PositionWithCursor(ent);
			}
			else
			{
				RemoveDuplicateBind(duplicateBind.second);
			}
		}
	}

	if (!_currSelection.empty())
	{
		//if (input.GetKey(KeyCode::N) == KeyState::Pressed)
		//	DebugData::Get().transformRelative = !DebugData::Get().transformRelative;

		if (input.GetKey(KeyCode::M) == KeyState::Pressed)
			DebugData::Get().transformSpace = (DebugData::Get().transformSpace == (int)World) ? (int)Local : (int)World;
	}

	if (input.GetKey(KeyCode::M1) == KeyState::Held && (input.IsCursorLocked() || _rayCastFromMouse))
		_drawPointer = true;

	if (input.GetKey(KeyCode::C) == KeyState::Pressed || _currCamera == -3)
	{ // Change camera
		_currCamera++;

		const int spotlightCount = static_cast<int>(spotlights->GetNrOfLights());

		if (_currCamera - 2 - spotlightCount >= 0)
			_currCamera = -2;

		if (_currCamera < 0)
		{
			if (_secondaryCamera && _currCamera == -1)
			{
				scene->SetViewCamera(_secondaryCamera.GetAs<CameraBehaviour>());
			}
			else if (_mainCamera)
			{
				_currCamera = -2;
				scene->SetViewCamera(_mainCamera.GetAs<CameraBehaviour>());
			}
		}
		else if (_currCamera < spotlightCount)
		{
			scene->SetViewCamera(spotlights->GetLightBehaviour(_currCamera)->GetShadowCamera());
		}
	}

	if (!UpdateGlobalEntities(time, input))
	{
		ErrMsg("Failed to update global entities!");
		return false;
	}

	return true;
}

bool DebugPlayerBehaviour::UpdateGlobalEntities(TimeUtils &time, const Input &input)
{
	std::vector<std::unique_ptr<Entity>> *globalEntities = GetScene()->GetGlobalEntities();

	Entity *pointer = nullptr;

	for (auto &entPtr : *globalEntities)
	{
		if (entPtr->GetName() == "Pointer Gizmo")
		{
			pointer = entPtr.get();
		}
	}

	if (_drawPointer)
	{
		if (pointer)
		{
			RaycastOut out;
			XMFLOAT3A hitPos = { };
			XMFLOAT3A castDir = { };

			if (_rayCastFromMouse)
			{
				if (RayCastFromMouse(out, hitPos, castDir, input))
					pointer->GetTransform()->SetPosition(hitPos, World);
				else
					_drawPointer = false;
			}
			else
			{
				if (RayCastFromCamera(out, hitPos, castDir))
					pointer->GetTransform()->SetPosition(hitPos, World);
				else
					_drawPointer = false;
			}

			if (_drawPointer)
			{
				if (!pointer->InitialUpdate(time, input))
				{
					ErrMsg("Failed to update gizmo gizmo!");
					return false;
				}
			}
		}
		else
		{
			_drawPointer = false;
		}
	}

	return true;
}

bool DebugPlayerBehaviour::Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo)
{
	Scene *scene = GetScene();
	std::vector<std::unique_ptr<Entity>> *globalEntities = scene->GetGlobalEntities();

	if (_cursorPositioningTarget)
	{
		if (!_cursorPositioningTarget.Get()->InitialRender(queuer, rendererInfo))
		{
			ErrMsg("Failed to render cursor positioning target!");
			return false;
		}
	}

	Entity *playerEnt = scene->GetSceneHolder()->GetEntityByName("Player Entity");
	if (playerEnt)
	{
		if (!playerEnt->InitialRender(queuer, rendererInfo))
		{
			ErrMsg("Failed to render player entity!");
			return false;
		}
	}

	for (auto &entPtr : *globalEntities)
	{
		Entity *ent = entPtr.get();

		if (_drawPointer) // Render pointer
		{
			if (ent->GetName() == "Pointer Gizmo")
			{
				if (!ent->InitialRender(queuer, rendererInfo))
				{
					ErrMsg("Failed to render entity!");
					return false;
				}

				_drawPointer = false;
			}
		}

		if (ent->GetName() == "Culling Tree Wireframe" ||
			ent->GetName() == "Entity Bounds Wireframe" ||
			ent->GetName() == "Camera Culling Wireframe")
		{
			if (!ent->InitialRender(queuer, rendererInfo))
			{
				ErrMsg("Failed to render entity!");
				return false;
			}
		}
	}

	if (!_currSelection.empty())
	{
		Scene *scene = GetScene();
		Graphics *graphics = scene->GetGraphics();

		for (auto &entRef : _currSelection)
		{
			Entity *selectedEnt;
			if (!entRef.TryGet(selectedEnt))
				continue;

			graphics->AddOutlinedEntity(selectedEnt);
		}
	}

	return true;
}

bool DebugPlayerBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}
bool DebugPlayerBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	// Standard code for all behaviours deserialize

	return true;
}
void DebugPlayerBehaviour::PostDeserialize()
{
	// Set this behaviour as the scene's debug player
	Scene *scene = GetScene();
	if (!scene)
		return;

	scene->SetDebugPlayer(this);
}

void DebugPlayerBehaviour::SetCamera(CameraBehaviour *cam)
{
	_currCameraPtr = cam;
}

bool DebugPlayerBehaviour::IsSelected(Entity *ent, UINT *index, bool includeBillboard) const
{
	if (!ent)
		return false;

	if (_currSelection.size() <= 0)
		return false;

	if (includeBillboard)
	{
		if (Entity *parent = ent->GetParent())
		{
			BillboardMeshBehaviour *billboard = nullptr;
			if (parent->GetBehaviourByType<BillboardMeshBehaviour>(billboard))
			{
				if (IsSelected(parent, index))
					return true;
			}
		}
	}

	for (int i = 0; i < _currSelection.size(); i++)
	{
		if (_currSelection[i] != ent)
			continue;

		if (index)
			*index = i;
		return true;
	}

	return false;
}

void DebugPlayerBehaviour::Select(UINT id, bool additive)
{
	if (id == CONTENT_NULL)
		return;

	Select(GetScene()->GetSceneHolder()->GetEntityByID(id), additive);
}
void DebugPlayerBehaviour::Select(Entity *ent, bool additive)
{
	if (!ent)
		return;

	if (IsSelected(ent) && additive)
		return;

	if (!additive)
		_currSelection.clear();

	Ref<Entity> entRef = ent->AsRef(std::format("Select'{}'#{}", ent->GetName(), ent->GetID()).c_str());
	entRef.AddDestructCallback([this](Ref<Entity> *entRef) {
		// Remove from selection if the entity is destructed
		if (UINT index; IsSelected(entRef->Get(), &index))
			_currSelection.erase(_currSelection.begin() + index);
	});

	_currSelection.push_back(std::move(entRef));

	if (!ent->InitialOnDebugSelect())
	{
		ErrMsg("OnDebugSelect Failed!");
		return;
	}
}
void DebugPlayerBehaviour::Select(Entity **ents, UINT count, bool additive)
{
	if (!additive)
		ClearSelection();

	if (!ents || count <= 0)
		return;

	for (int i = 0; i < count; i++)
	{
		Select(ents[i], true);
	}
}

void DebugPlayerBehaviour::Deselect(UINT id)
{
	if (id == CONTENT_NULL)
		return;

	Deselect(GetScene()->GetSceneHolder()->GetEntityByID(id));
}
void DebugPlayerBehaviour::Deselect(Entity *ent)
{
	if (!ent)
		return;

	UINT index = CONTENT_NULL;
	if (!IsSelected(ent, &index))
		return;

	_currSelection.erase(_currSelection.begin() + index);
}
void DebugPlayerBehaviour::Deselect(Entity **ents, UINT count)
{
	if (!ents || count <= 0)
		return;

	for (int i = 0; i < count; i++)
	{
		Deselect(ents[i]);
	}
}
void DebugPlayerBehaviour::DeselectIndex(UINT index)
{
	if (index >= _currSelection.size())
		return;
	_currSelection.erase(_currSelection.begin() + index);
}

void DebugPlayerBehaviour::ClearSelection()
{
	_currSelection.clear();
}

const UINT DebugPlayerBehaviour::GetSelectionSize() const
{
	return _currSelection.size();
}
Entity *DebugPlayerBehaviour::GetPrimarySelection() const
{
	if (_currSelection.empty())
		return nullptr;
	return _currSelection.back().Get();
}
const std::vector<Ref<Entity>> &DebugPlayerBehaviour::GetSelection()
{
	return _currSelection;
}
void DebugPlayerBehaviour::GetParentSelection(std::vector<Ref<Entity>> &selectedParents)
{
	selectedParents.clear();
	selectedParents.reserve(_currSelection.size() / 2);

	for (const auto &entRef : _currSelection)
	{
		Entity *ent;
		if (!entRef.TryGet(ent))
			continue;

		if (ent->GetParent())
		{
			bool isChild = false;
			for (const auto &otherEntRef : _currSelection)
			{
				Entity *otherEnt;
				if (!otherEntRef.TryGet(otherEnt))
					continue;

				if (!ent->IsChildOf(otherEnt))
					continue;

				isChild = true;
				break;
			}

			if (isChild)
				continue;
		}

		selectedParents.push_back(std::move(ent->AsRef()));
	}
}

void DebugPlayerBehaviour::SetEditSpace(ReferenceSpace space)
{
	DebugData::Get().transformSpace = (int)space;
}
ReferenceSpace DebugPlayerBehaviour::GetEditSpace() const
{
	return (ReferenceSpace)DebugData::Get().transformSpace;
}
void DebugPlayerBehaviour::SetEditType(TransformationType type)
{
	DebugData::Get().transformType = (int)type;
}
TransformationType DebugPlayerBehaviour::GetEditType() const
{
	return (TransformationType)DebugData::Get().transformType;
}
void DebugPlayerBehaviour::SetEditOriginMode(TransformOriginMode mode)
{
	DebugData::Get().transformOriginMode = (int)mode;
}
TransformOriginMode DebugPlayerBehaviour::GetEditOriginMode() const
{
	return (TransformOriginMode)DebugData::Get().transformOriginMode;
}

void DebugPlayerBehaviour::AssignDuplicateToKey(UINT id)
{
	_addDuplicateBindForEntity = id;
}
bool DebugPlayerBehaviour::IsAssigningDuplicateToKey(UINT id) const
{
	return id == _addDuplicateBindForEntity;
}
void DebugPlayerBehaviour::AddDuplicateBind(KeyCode key, UINT id)
{
	if (id == CONTENT_NULL)
		return;

	_duplicateBinds.emplace_back(std::pair<KeyCode, UINT>(key, id));
}
void DebugPlayerBehaviour::RemoveDuplicateBind(UINT id)
{
	if (id == CONTENT_NULL)
		return;

	for (auto it = _duplicateBinds.begin(); it != _duplicateBinds.end();)
	{
		if (it->second == id)
			it = _duplicateBinds.erase(it);
		else
			++it;
	}
}
bool DebugPlayerBehaviour::HasDuplicateBind(UINT id) const
{
	if (id == CONTENT_NULL)
		return false;

	for (auto it = _duplicateBinds.begin(); it != _duplicateBinds.end();)
	{
		if (it->second == id)
			return true;
		else
			++it;
	}

	return false;
}
KeyCode DebugPlayerBehaviour::GetDuplicateBind(UINT id)
{
	if (id == CONTENT_NULL)
		return KeyCode::None;

	for (auto it = _duplicateBinds.begin(); it != _duplicateBinds.end();)
	{
		if (it->second == id)
			return it->first;
		else
			++it;
	}

	return KeyCode::None;
}
void DebugPlayerBehaviour::ClearDuplicateBinds()
{
	_duplicateBinds.clear();
}
bool DebugPlayerBehaviour::IsValidDuplicateBind(KeyCode key) const
{
	return (key != KeyCode::W)
		&& (key != KeyCode::A)
		&& (key != KeyCode::S)
		&& (key != KeyCode::D)
		&& (key != KeyCode::X)
		&& (key != KeyCode::Space)
		&& (key != KeyCode::E)
		&& (key != KeyCode::R)
		&& (key != KeyCode::T)
		&& (key != KeyCode::M1)
		&& (key != KeyCode::M2)
		&& (key != KeyCode::Enter)
		&& (key != KeyCode::Delete)
		&& (key != KeyCode::Tab)
		&& (key != KeyCode::LeftControl)
		&& (key != KeyCode::LeftShift)
		&& (key != KeyCode::LeftAlt)
		&& (key != KeyCode::Q)
		&& (key != KeyCode::C)
		&& (key != KeyCode::F5);
}

void DebugPlayerBehaviour::PositionWithCursor(Entity *ent)
{
	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();

	if (_cursorPositioningTarget)
	{
		if (_includePositioningTargetInTree)
		{
			if (!sceneHolder->IncludeEntityInTree(_cursorPositioningTarget.Get()))
			{
				ErrMsg("Failed to include previous positioned entity in tree!");
				return;
			}
		}

		std::vector<Entity *> children;
		_cursorPositioningTarget.Get()->GetChildrenRecursive(children);
		for (Entity *child : children)
		{
			child->SetRaycastTarget(true);
		}

		_cursorPositioningTarget = nullptr;
	}

	if (!ent)
		return;

	_cursorPositioningTarget = ent;
	_includePositioningTargetInTree = sceneHolder->IsEntityIncludedInTree(ent);

	if (_includePositioningTargetInTree)
	{
		if (!sceneHolder->ExcludeEntityFromTree(_cursorPositioningTarget.Get()))
		{
			ErrMsg("Failed to exclude positioning entity in tree!");
			return;
		}
	}

	std::vector<Entity *> children;
	_cursorPositioningTarget.Get()->GetChildrenRecursive(children);
	for (Entity *child : children)
	{
		child->SetRaycastTarget(false);
	}
}

void DebugPlayerBehaviour::AddGizmoBillboard(BillboardMeshBehaviour *gizmo)
{
	if (!gizmo)
		return;

	_gizmoBillboards.emplace_back(gizmo);

	DebugData &debugData = DebugData::Get();
	gizmo->SetEnabled(debugData.billboardGizmosDraw);
	gizmo->GetMeshBehaviour()->SetOverlay(debugData.billboardGizmosOverlay);
	gizmo->SetSize(debugData.billboardGizmosSize);
}
void DebugPlayerBehaviour::RemoveGizmoBillboard(BillboardMeshBehaviour *gizmo)
{
	if (!gizmo)
		return;

	for (int i = 0; i < _gizmoBillboards.size(); i++)
	{
		if (gizmo != _gizmoBillboards[i])
			continue;

		_gizmoBillboards.erase(_gizmoBillboards.begin() + i);
		return;
	}
}
void DebugPlayerBehaviour::UpdateGizmoBillboards()
{
	DebugData &debugData = DebugData::Get();

	for (auto &gizmo : _gizmoBillboards)
	{
		if (!gizmo)
			continue;

		gizmo->SetEnabled(debugData.billboardGizmosDraw);
		gizmo->GetMeshBehaviour()->SetOverlay(debugData.billboardGizmosOverlay);
		gizmo->SetSize(debugData.billboardGizmosSize);
	}
}

bool DebugPlayerBehaviour::RayCastFromCamera(RaycastOut &out)
{
	if (!_currCameraPtr)
		return false;

	Transform *camTransform = _currCameraPtr.Get()->GetTransform();

	const XMFLOAT3A
		camPos = camTransform->GetPosition(World),
		camDir = camTransform->GetForward(World);

	return GetScene()->GetSceneHolder()->RaycastScene(
		{ camPos.x, camPos.y, camPos.z },
		{ camDir.x, camDir.y, camDir.z },
		out, false);
}
bool DebugPlayerBehaviour::RayCastFromCamera(RaycastOut &out, XMFLOAT3A &pos, XMFLOAT3A &dir)
{
	if (!_currCameraPtr)
		return false;

	Transform *camTransform = _currCameraPtr.Get()->GetTransform();

	const XMFLOAT3A
		camPos = camTransform->GetPosition(World),
		camDir = camTransform->GetForward(World);
	dir = camDir;

	if (!GetScene()->GetSceneHolder()->RaycastScene(
		{ camPos.x, camPos.y, camPos.z },
		{ camDir.x, camDir.y, camDir.z },
		out, false))
	{
		return false;
	}

	pos = {
		camPos.x + (camDir.x * out.distance),
		camPos.y + (camDir.y * out.distance),
		camPos.z + (camDir.z * out.distance)
	};

	return true;
}
bool DebugPlayerBehaviour::RayCastFromMouse(RaycastOut &out, const Input &input)
{
	if (!_currCameraPtr)
		return false;

	CameraBehaviour *cam = _currCameraPtr.GetAs<CameraBehaviour>();

	// Get window width and height
	const XMFLOAT2 mPos = input.GetLocalMousePos();

	// Wiewport to NDC coordinates
	float xNDC = (2.0f * mPos.x) - 1.0f;
	float yNDC = 1.0f - (2.0f * mPos.y); // can also be - 1 depending on coord-system
	float zNDC = 1.0f;	// not really needed yet (specified anyways)
	XMFLOAT3A rayClip = { xNDC, yNDC, zNDC };

	// Wiew space -> clip space
	XMVECTOR rayClipVec = Load(rayClip);
	XMMATRIX projMatrix = Load(cam->GetProjectionMatrix());
	XMVECTOR rayEyeVec = XMVector4Transform(rayClipVec, XMMatrixInverse(nullptr, projMatrix));

	// Set z and w to mean forwards and not a point
	rayEyeVec = XMVectorSet(XMVectorGetX(rayEyeVec), XMVectorGetY(rayEyeVec), 1, 0.0);

	// Clip space -> world space
	XMMATRIX viewMatrix = Load(cam->GetViewMatrix());
	XMVECTOR rayWorldVec = XMVector4Transform(rayEyeVec, XMMatrixInverse(nullptr, viewMatrix));

	rayWorldVec = XMVector3Normalize(rayWorldVec);
	XMFLOAT3A dir; Store(dir, rayWorldVec);

	// Camera 
	XMFLOAT3A camPos = cam->GetTransform()->GetPosition(World);

	// Perform raycast
	if (!GetScene()->GetSceneHolder()->RaycastScene(camPos, dir, out, false))
	{
		return false;
	}

	return true;
}
bool DebugPlayerBehaviour::RayCastFromMouse(RaycastOut &out, XMFLOAT3A &pos, XMFLOAT3A &dir, const Input &input)
{
	if (!_currCameraPtr)
		return false;

	CameraBehaviour *cam = _currCameraPtr.GetAs<CameraBehaviour>();

	// Get window width and height
	const XMFLOAT2 mPos = input.GetLocalMousePos();

	// Wiewport to NDC coordinates
	float xNDC = (2.0f * mPos.x) - 1.0f;
	float yNDC = 1.0f - (2.0f * mPos.y); // can also be - 1 depending on coord-system
	float zNDC = 1.0f;	// not really needed yet (specified anyways)
	XMFLOAT3A rayClip = { xNDC, yNDC, zNDC };

	// Wiew space -> clip space
	XMVECTOR rayClipVec = Load(rayClip);
	XMMATRIX projMatrix = Load(cam->GetProjectionMatrix());
	XMVECTOR rayEyeVec = XMVector4Transform(rayClipVec, XMMatrixInverse(nullptr, projMatrix));

	// Set z and w to mean forwards and not a point
	rayEyeVec = XMVectorSet(XMVectorGetX(rayEyeVec), XMVectorGetY(rayEyeVec), 1, 0.0);

	// Clip space -> world space
	XMMATRIX viewMatrix = Load(cam->GetViewMatrix());
	XMVECTOR rayWorldVec = XMVector4Transform(rayEyeVec, XMMatrixInverse(nullptr, viewMatrix));

	rayWorldVec = XMVector3Normalize(rayWorldVec);
	Store(dir, rayWorldVec);

	// Camera 
	XMFLOAT3A camPos = cam->GetTransform()->GetPosition(World);

	// Perform raycast
	if (!GetScene()->GetSceneHolder()->RaycastScene(camPos, dir, out, false))
		return false;

	pos = {
		camPos.x + (dir.x * out.distance),
		camPos.y + (dir.y * out.distance),
		camPos.z + (dir.z * out.distance)
	};

	return true;
}
#endif