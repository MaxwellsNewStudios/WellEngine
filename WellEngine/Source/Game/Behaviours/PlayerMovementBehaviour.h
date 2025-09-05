#pragma once
#include "Behaviour.h"
#include "DirectXMath.h"
#include "Behaviours/ColliderBehaviour.h"
#include "Behaviours/SoundBehaviour.h"

class [[register_behaviour]] PlayerMovementBehaviour : public Behaviour
{
private:
	dx::XMFLOAT3A _spawnPosition = { 0, 0, 0 };
	std::vector<dx::XMFLOAT3A> _spawnPositions;

	float _velocity = 0.0f;	// Per second
	dx::XMFLOAT3A _movementDir = { 0.0f, 0.0f, 0.0f };
	dx::XMFLOAT3A _lastMoveDir = { 0.0f, 0.0f, 0.0f };
	float _lastMoveSpeed = 0.0f;

	bool _isLocked = false;

	bool _collidingWithWall = false;

	// States
	float _walkVelocity = 4.0f;
	float _runVelocity = 8.5f;
	float _sneakVelocity = 2.0f;
	float _solidFootingDot = 0.15f; // Maximum difference in dot product of the player's up vector and the terrain's normal, used for determining if the player is on solid footing

	bool _canRun = true;
	bool _isStill = true;
	bool _isSneaking = false; // crouch walk (not crouching alone)
	bool _isWalking = false;
	bool _isRunning = false;
	bool _isHiding = false;
	bool _isCaught = false;
	int _caughtStage = 0;
	float _caughtStageTimer = 0.0f;

	// Multipliers
	float _forwardsMultiplier = 1.0f;
	float _sidewaysMultiplier = 0.9f;
	float _backwardsMultiplier = 0.8f;

	// Stamina
	float _stamina = 100.0f;
	float _staminaMax = 100.0f;
	float _staminaDrainRate = 33.0f; // Per second
	float _staminaRecoveryRate = 10.0f; // Per second

	float _stepProgress = 0.0f; // Degrees on the cosinus curve 

	// Step sounds
	float _stepTimer = 0.0f;
	float _stepInterval = 0.5f;

	// For debugging
	bool _flyMode = false; 

	Entity *_flashlight = nullptr;
	CameraBehaviour *_playerCamera = nullptr;
	ColliderBehaviour *_playerCollider = nullptr;
	Collisions::Ray _rayCollider;

	const Collisions::Terrain 
		*_terrainFloor = nullptr, 
		*_terrainRoof = nullptr,
		*_terrainWalls = nullptr;

	const ColliderBehaviour 
		*_floorColliderBehaviour = nullptr,
		*_roofColliderBehaviour = nullptr,
		*_wallsColliderBehaviour = nullptr;

	Entity *_headTrackerEntity = nullptr;
	float _cameraHeight = 1.8f;

	// Audio
	std::vector<SoundBehaviour *> _steps;
	SoundBehaviour *_collapseSound = nullptr;
	std::unordered_map<std::string, SoundBehaviour*> _breathSounds;
	SoundBehaviour* _currentBreathSound = nullptr;

	void AdjustForCollision(const Collisions::CollisionData &data);
	bool GetTerrain();

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
	[[nodiscard]] bool LateUpdate(TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	void PostDeserialize() override;

public:
	PlayerMovementBehaviour() = default;
	~PlayerMovementBehaviour() = default;

	[[nodiscard]] bool IsLocked() const;
	void Lock();
	void Unlock();

	bool SetPlayerCamera(CameraBehaviour *camera);
	void SetHeadTracker(Entity *ent);

	[[nodiscard]] Transform *GetPlayerCameraDesiredPos() const;

	[[nodiscard]] float GetCameraHeight() const;

	[[nodiscard]] bool IsCaught() const;
	[[nodiscard]] bool Catch();
	void PlayCaughtAnimnation(TimeUtils &time);
	void RespawnPlayer();

	void SetHiding(bool value);
	void SetStill(bool value);
	void SetSneaking(bool value);
	void SetWalking(bool value);
	void SetRunning(bool value);

	[[nodiscard]] bool IsHiding() const;
	[[nodiscard]] bool IsStill() const;
	[[nodiscard]] bool IsSneaking() const;
	[[nodiscard]] bool IsWalking() const;
	[[nodiscard]] bool IsRunning() const;
	[[nodiscard]] bool IsFlying() const;

	bool MoveToPoint(TimeUtils &time, dx::XMFLOAT3 point, float velocity, float threshold2 = 0.5);
};