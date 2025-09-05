#include "stdafx.h"
#include "Collision/CollisionHandler.h"
#include "Scenes/Scene.h"
#include "Collision/Intersections.h"

#include <vector>
#include <map>
#include <algorithm>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace Collisions;
using namespace DirectX;

bool CollisionHandler::Initialize(Scene *scene)
{
	ZoneScopedC(RandomUniqueColor());

	SceneHolder *sh = scene->GetSceneHolder();

	SceneContents::SceneIterator entIter = sh->GetEntities();
	_entitiesToCheck.reserve(entIter.size());
	_collidersToCheck.reserve(entIter.size());
	_colliderBehavioursToCheck.reserve(entIter.size());

	// Check if entities have colliders
	while (Entity *ent = entIter.Step())
	{
		ColliderBehaviour *colBehaviour;

		if (ent->GetBehaviourByType<ColliderBehaviour>(colBehaviour))
		{
			const Collider *col = colBehaviour->GetCollider();
			if (col)
			{
				if (col->colliderType != NULL_COLLIDER)
				{
					// Reset Intersecting status

					// Add collider and entity to be checked
					_collidersToCheck.emplace_back(col);
					_entitiesToCheck.emplace_back(ent);
					_colliderBehavioursToCheck.emplace_back(colBehaviour);
				}
			}
		}
	}

	return true;
}

bool CollisionHandler::CheckCollisions(TimeUtils &time, Scene *scene, ID3D11DeviceContext *context)
{
	ZoneScopedC(RandomUniqueColor());

	time.TakeSnapshot("CollisionChecks");
	
	std::unordered_multimap<const Collider *, CollisionData> intersections(_collidersToCheck.size()),
															 exiting(_collidersToCheck.size()),
															 entering(_collidersToCheck.size());

	SceneHolder *sh = scene->GetSceneHolder();

	if (sh->GetRecalculateColliders())
	{
		ZoneNamedNC(tracyRecalculateCollidersZone, "Recalculate Colliders", RandomUniqueColor(), true);

		SceneContents::SceneIterator entIter = sh->GetEntities();

		size_t prevEntToCheckSize = _entitiesToCheck.size();
		size_t prevColToCheckSize = _collidersToCheck.size();
		size_t prevColBehToCheckSize = _colliderBehavioursToCheck.size();

		_entitiesToCheck.clear();
		_collidersToCheck.clear();
		_colliderBehavioursToCheck.clear();

		_entitiesToCheck.reserve(entIter.size());
		_collidersToCheck.reserve(entIter.size());
		_colliderBehavioursToCheck.reserve(entIter.size());

		// Check if entities have colliders
		while (Entity *ent = entIter.Step())
		{
			std::vector<ColliderBehaviour*> colBehaviours;

			if (ent->GetBehavioursByType<ColliderBehaviour>(colBehaviours))
			{
				for (auto colBehaviour : colBehaviours)
				{
					const Collider *col = colBehaviour->GetCollider();
					if (col)
					{
						if (col->colliderType != NULL_COLLIDER)
						{
							// Add collider and entity to be checked
							_collidersToCheck.emplace_back(col);
							_entitiesToCheck.emplace_back(ent);
							_colliderBehavioursToCheck.emplace_back(colBehaviour);
						}
					}
				}
			}
		}
	}

	std::vector<bool> wasIntersecting(_entitiesToCheck.size());

	// TODO: Try insertions sort
	// TODO: Verify last index

	// Insertion sort
	{
		ZoneNamedNC(colliderSortingZone, "Collider Sorting", RandomUniqueColor(), true);

		int j;
		for (int i = 1; i < _collidersToCheck.size(); i++)
		{
			j = i;
			while (j > 0 && _collidersToCheck[j - 1]->GetMin().x > _collidersToCheck[j]->GetMin().x)
			{
				std::swap(_collidersToCheck[j], _collidersToCheck[j - 1]);
				std::swap(_entitiesToCheck[j], _entitiesToCheck[j - 1]);
				std::swap(_colliderBehavioursToCheck[j], _colliderBehavioursToCheck[j - 1]);
				j--;
			}
		}
	}

	{
		ZoneNamedNC(addingIntersectionStatusZone, "Adding Intersection Status", RandomUniqueColor(), true);

		for (int i = 0; i < _entitiesToCheck.size(); i++)
		{
			wasIntersecting[i] = _colliderBehavioursToCheck[i]->GetIntersecting();
			_colliderBehavioursToCheck[i]->SetIntersecting(false);
		}
	}


	/*
	// Bouble Sort
	for (int i = 0; i < _collidersToCheck.size()-1; i++)
	{
		for (int j = 0; j < _collidersToCheck.size()-1-i; j++)
		{
			if (_collidersToCheck[j + 1]->GetMin().x < _collidersToCheck[j]->GetMin().x)
			{
				std::swap(_collidersToCheck[j], _collidersToCheck[j + 1]);
				std::swap(_entitiesToCheck[j], _entitiesToCheck[j + 1]);

				{
					bool temp = wasIntersecting[j];
					wasIntersecting[j] = wasIntersecting[j + 1];
					wasIntersecting[j + 1] = temp;
				}
			}
		}
	}
	*/

	CollisionData data;
	ColliderBehaviour *colBehaviour, *colBehaviour2;

	// Check collisions
	{
		ZoneNamedNC(entityCheckZone, "Entity Check", RandomUniqueColor(), true);

		// TODO: Try making parallel solution work

		//#pragma omp parallel for num_threads(PARALLEL_THREADS)
		for (int i = 0; i < _collidersToCheck.size(); i++)
		{
			// Main Enity
			colBehaviour = _colliderBehavioursToCheck[i];

			// Only check collider that have moved
			if (colBehaviour->GetToCheck())
			{
				const Collider *col1 = _collidersToCheck[i];

				for (int j = 0; j < _collidersToCheck.size(); j++)
				{
					if (i == j) continue;

					const Collider *col2 = _collidersToCheck[j];

					if (col2->GetMin().x > col1->GetMax().x) break;
					if (col2->GetMin().y > col1->GetMax().y || col1->GetMin().y > col2->GetMax().y) continue;
					if (col2->GetMin().z > col1->GetMax().z || col1->GetMin().z > col2->GetMax().z) continue;

					if (CheckIntersection(col1, col2, data))
					{
						//#pragma omp critical
						{
							data.other = col2;

							colBehaviour->SetIntersecting(true);
							intersections.emplace(col1, data);

							if (!wasIntersecting[i])
								entering.emplace(col1, data);

							// Chnage data values
							data.normal = { -1 * data.normal.x, -1 * data.normal.y, -1 * data.normal.z };
							data.other = col1;

							// Other entity
							colBehaviour2 = _colliderBehavioursToCheck[j];

							colBehaviour2->SetIntersecting(true);
							intersections.emplace(col2, data);

							if (!wasIntersecting[j])
								entering.emplace(col2, data);
						}
					}
				}

				if (!colBehaviour->GetIntersecting())
					colBehaviour->CheckDone();
			}
		}
	}

	// Check exiting collisions
	for (int i = 0; i < _entitiesToCheck.size(); i++)
		if (!_colliderBehavioursToCheck[i]->GetIntersecting() && wasIntersecting[i])
			exiting.emplace(_collidersToCheck[i], data);

	time.TakeSnapshot("CollisionChecks");
	
	// Call Intersection functions

	for (auto const &[ent, data] : intersections)
		ent->Intersection(data);

	for (auto const &[ent, data] : entering)
		ent->OnCollisionEnter(data);

	// TODO: Remove data from exit
	for (auto const &[ent, data] : exiting)
		ent->OnCollisionExit(data);

	return true;
}

bool CollisionHandler::CheckCollision(const Collider *col, Scene *scene, CollisionData &data)
{
	SceneHolder *sh = scene->GetSceneHolder();

	if (sh->GetRecalculateColliders())
	{
		ZoneNamedNC(tracyRecalculateCollidersZone, "Recalculate Colliders", RandomUniqueColor(), true);

		SceneContents::SceneIterator entIter = sh->GetEntities();

		size_t prevEntToCheckSize = _entitiesToCheck.size();
		size_t prevColToCheckSize = _collidersToCheck.size();
		size_t prevColBehToCheckSize = _colliderBehavioursToCheck.size();

		_entitiesToCheck.clear();
		_collidersToCheck.clear();
		_colliderBehavioursToCheck.clear();

		_entitiesToCheck.reserve(entIter.size());
		_collidersToCheck.reserve(entIter.size());
		_colliderBehavioursToCheck.reserve(entIter.size());

		// Check if entities have colliders
		while (Entity *ent = entIter.Step())
		{
			std::vector<ColliderBehaviour*> colBehaviours;

			if (ent->GetBehavioursByType<ColliderBehaviour>(colBehaviours))
			{
				for (auto colBehaviour : colBehaviours)
				{
					const Collider *col = colBehaviour->GetCollider();
					if (col)
					{
						if (col->colliderType != NULL_COLLIDER)
						{
							// Add collider and entity to be checked
							_collidersToCheck.emplace_back(col);
							_entitiesToCheck.emplace_back(ent);
							_colliderBehavioursToCheck.emplace_back(colBehaviour);
						}
					}
				}
			}
		}
	}

	for (int j = 0; j < _collidersToCheck.size(); j++)
	{
		const Collider *col2 = _collidersToCheck[j];

		dx::XMFLOAT3 colMin = col->GetMin();
		dx::XMFLOAT3 colMax = col->GetMax();
		dx::XMFLOAT3 col2Min = col->GetMin();
		dx::XMFLOAT3 col2Max = col->GetMax();

		if (col2Min.x > colMax.x)							break;
		if (col2Min.y > colMax.y || colMin.y > col2Max.y)	continue;
		if (col2Min.z > colMax.z || colMin.z > col2Max.z)	continue;

		if (CheckIntersection(col, col2, data))
		{
			data.other = col2;
			return true;
		}
	}

	return false;
}
