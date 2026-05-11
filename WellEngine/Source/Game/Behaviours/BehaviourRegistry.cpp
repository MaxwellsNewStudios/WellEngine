// Automatically generated during build by BehaviourRegistration.
// Scans for all behaviour definitions and includes them here for the behaviour factory to use.
// NOTE: DO NOT MODIFY MANUALLY!

#include "stdafx.h"
#include "BehaviourRegistry.h"
#include "Behaviour.h"
#include "Game/Behaviours/Debug/B_DebugManager.h"
#include "Game/Behaviours/Debug/B_Example.h"
#include "Game/Behaviours/Misc/B_TransformTracker.h"
#include "Game/Behaviours/Physics/B_PhysicsForce.h"
#include "Game/Behaviours/Physics/Colliders/B_BoxCollider.h"
#include "Game/Behaviours/Physics/Colliders/B_SphereCollider.h"
#include "Game/Behaviours/Rendering/Camera/B_Camera.h"
#include "Game/Behaviours/Rendering/Camera/B_CameraCube.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightPoint.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightPointSimple.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightSpot.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightSpotSimple.h"
#include "Game/Behaviours/Rendering/Mesh/B_Mesh.h"
#include "Game/Behaviours/Rendering/Mesh/B_MeshBillboard.h"
#include "Game/Behaviours/Rendering/Mesh/B_MeshSkinned.h"
#include "Game/Behaviours/Rendering/Mesh/B_MeshText.h"
#include "Game/Behaviours/Sound/B_SoundListener.h"
#include "Game/Behaviours/Sound/B_SoundSource.h"
#include "Game/Behaviours/Sound/B_SoundSourceAmbient.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

const std::map<std::string, std::function<Behaviour *(void)>> &BehaviourRegistry::Get()
{
	static const std::map<std::string, std::function<Behaviour *(void)>> behaviourMap = {
		{ "DebugManager",       []() { return new B_DebugManager(); }       },
		{ "Example",            []() { return new B_Example(); }            },
		{ "TransformTracker",   []() { return new B_TransformTracker(); }   },
		{ "PhysicsForce",       []() { return new B_PhysicsForce(); }       },
		{ "BoxCollider",        []() { return new B_BoxCollider(); }        },
		{ "SphereCollider",     []() { return new B_SphereCollider(); }     },
		{ "Camera",             []() { return new B_Camera(); }             },
		{ "CameraCube",         []() { return new B_CameraCube(); }         },
		{ "LightPoint",         []() { return new B_LightPoint(); }         },
		{ "LightPointSimple",   []() { return new B_LightPointSimple(); }   },
		{ "LightSpot",          []() { return new B_LightSpot(); }          },
		{ "LightSpotSimple",    []() { return new B_LightSpotSimple(); }    },
		{ "Mesh",               []() { return new B_Mesh(); }               },
		{ "MeshBillboard",      []() { return new B_MeshBillboard(); }      },
		{ "MeshSkinned",        []() { return new B_MeshSkinned(); }        },
		{ "MeshText",           []() { return new B_MeshText(); }           },
		{ "SoundListener",      []() { return new B_SoundListener(); }      },
		{ "SoundSource",        []() { return new B_SoundSource(); }        },
		{ "SoundSourceAmbient", []() { return new B_SoundSourceAmbient(); } },

	};

	return behaviourMap;
};

#ifdef DEBUG_BUILD
const std::map<std::string, std::string> &BehaviourRegistry::GetCategories()
{
	static const std::map<std::string, std::string> behaviourCategoryMap = {
		{ "DebugManager",       "Debug/"              },
		{ "Example",            "Debug/"              },
		{ "TransformTracker",   "Misc/"               },
		{ "PhysicsForce",       "Physics/"            },
		{ "BoxCollider",        "Physics/Colliders/"  },
		{ "SphereCollider",     "Physics/Colliders/"  },
		{ "Camera",             "Rendering/Camera/"   },
		{ "CameraCube",         "Rendering/Camera/"   },
		{ "LightPoint",         "Rendering/Lighting/" },
		{ "LightPointSimple",   "Rendering/Lighting/" },
		{ "LightSpot",          "Rendering/Lighting/" },
		{ "LightSpotSimple",    "Rendering/Lighting/" },
		{ "Mesh",               "Rendering/Mesh/"     },
		{ "MeshBillboard",      "Rendering/Mesh/"     },
		{ "MeshSkinned",        "Rendering/Mesh/"     },
		{ "MeshText",           "Rendering/Mesh/"     },
		{ "SoundListener",      "Sound/"              },
		{ "SoundSource",        "Sound/"              },
		{ "SoundSourceAmbient", "Sound/"              },

	};

	return behaviourCategoryMap;
};
#endif
