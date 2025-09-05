// Automatically generated during build by BehaviourRegistration.
// Scans for all behaviour definitions and includes them here for the behaviour factory to use.

#include "stdafx.h"
#include "BehaviourRegistry.h"
#include "Behaviour.h"
#include "Behaviours/AmbientSoundBehaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"
#include "Behaviours/BreadcrumbBehaviour.h"
#include "Behaviours/BreadcrumbPileBehaviour.h"
#include "Behaviours/ButtonBehaviours.h"
#include "Behaviours/CameraBehaviour.h"
#include "Behaviours/CameraCubeBehaviour.h"
#include "Behaviours/CameraItemBehaviour.h"
#include "Behaviours/ColliderBehaviour.h"
#include "Behaviours/CompassBehaviour.h"
#include "Behaviours/CreditsBehaviour.h"
#include "Behaviours/DebugPlayerBehaviour.h"
#include "Behaviours/EndCutSceneBehaviour.h"
#include "Behaviours/ExampleBehaviour.h"
#include "Behaviours/ExampleCollisionBehaviour.h"
#include "Behaviours/FlashlightBehaviour.h"
#include "Behaviours/FlashlightPropBehaviour.h"
#include "Behaviours/GraphNodeBehaviour.h"
#include "Behaviours/HideBehaviour.h"
#include "Behaviours/InteractableBehaviour.h"
#include "Behaviours/InteractorBehaviour.h"
#include "Behaviours/InventoryBehaviour.h"
#include "Behaviours/MenuCameraBehaviour.h"
#include "Behaviours/MeshBehaviour.h"
#include "Behaviours/MonsterBehaviour.h"
#include "Behaviours/MonsterHintBehaviour.h"
#include "Behaviours/PickupBehaviour.h"
#include "Behaviours/PictureBehaviour.h"
#include "Behaviours/PlayerCutsceneBehaviour.h"
#include "Behaviours/PlayerMovementBehaviour.h"
#include "Behaviours/PlayerViewBehaviour.h"
#include "Behaviours/PointLightBehaviour.h"
#include "Behaviours/RestrictedViewBehaviour.h"
#include "Behaviours/SimplePointLightBehaviour.h"
#include "Behaviours/SimpleSpotLightBehaviour.h"
#include "Behaviours/SolidObjectBehaviour.h"
#include "Behaviours/SoundBehaviour.h"
#include "Behaviours/SpotLightBehaviour.h"
#include "Behaviours/TrackerBehaviour.h"


#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion

const std::map<std::string_view, std::function<Behaviour *(void)>> &BehaviourRegistry::Get()
{
    static const std::map<std::string_view, std::function<Behaviour *(void)>> behaviourMap = {
		{ "AmbientSoundBehaviour",     []() { return new AmbientSoundBehaviour(); }     },
		{ "BillboardMeshBehaviour",    []() { return new BillboardMeshBehaviour(); }    },
		{ "BreadcrumbBehaviour",       []() { return new BreadcrumbBehaviour(); }       },
		{ "BreadcrumbPileBehaviour",   []() { return new BreadcrumbPileBehaviour(); }   },
		{ "PlayButtonBehaviour",       []() { return new PlayButtonBehaviour(); }       },
		{ "SaveButtonBehaviour",       []() { return new SaveButtonBehaviour(); }       },
		{ "NewSaveButtonBehaviour",    []() { return new NewSaveButtonBehaviour(); }    },
		{ "CreditsButtonBehaviour",    []() { return new CreditsButtonBehaviour(); }    },
		{ "ExitButtonBehaviour",       []() { return new ExitButtonBehaviour(); }       },
		{ "CameraBehaviour",           []() { return new CameraBehaviour(); }           },
		{ "CameraCubeBehaviour",       []() { return new CameraCubeBehaviour(); }       },
		{ "CameraItemBehaviour",       []() { return new CameraItemBehaviour(); }       },
		{ "ColliderBehaviour",         []() { return new ColliderBehaviour(); }         },
		{ "CompassBehaviour",          []() { return new CompassBehaviour(); }          },
		{ "CreditsBehaviour",          []() { return new CreditsBehaviour(); }          },
		{ "DebugPlayerBehaviour",      []() { return new DebugPlayerBehaviour(); }      },
		{ "EndCutSceneBehaviour",      []() { return new EndCutSceneBehaviour(); }      },
		{ "ExampleBehaviour",          []() { return new ExampleBehaviour(); }          },
		{ "ExampleCollisionBehaviour", []() { return new ExampleCollisionBehaviour(); } },
		{ "FlashlightBehaviour",       []() { return new FlashlightBehaviour(); }       },
		{ "FlashlightPropBehaviour",   []() { return new FlashlightPropBehaviour(); }   },
		{ "GraphNodeBehaviour",        []() { return new GraphNodeBehaviour(); }        },
		{ "HideBehaviour",             []() { return new HideBehaviour(); }             },
		{ "InteractableBehaviour",     []() { return new InteractableBehaviour(); }     },
		{ "InteractorBehaviour",       []() { return new InteractorBehaviour(); }       },
		{ "InventoryBehaviour",        []() { return new InventoryBehaviour(); }        },
		{ "MenuCameraBehaviour",       []() { return new MenuCameraBehaviour(); }       },
		{ "MeshBehaviour",             []() { return new MeshBehaviour(); }             },
		{ "MonsterBehaviour",          []() { return new MonsterBehaviour(); }          },
		{ "MonsterHintBehaviour",      []() { return new MonsterHintBehaviour(); }      },
		{ "PickupBehaviour",           []() { return new PickupBehaviour(); }           },
		{ "PictureBehaviour",          []() { return new PictureBehaviour(); }          },
		{ "PlayerCutsceneBehaviour",   []() { return new PlayerCutsceneBehaviour(); }   },
		{ "PlayerMovementBehaviour",   []() { return new PlayerMovementBehaviour(); }   },
		{ "PlayerViewBehaviour",       []() { return new PlayerViewBehaviour(); }       },
		{ "PointLightBehaviour",       []() { return new PointLightBehaviour(); }       },
		{ "RestrictedViewBehaviour",   []() { return new RestrictedViewBehaviour(); }   },
		{ "SimplePointLightBehaviour", []() { return new SimplePointLightBehaviour(); } },
		{ "SimpleSpotLightBehaviour",  []() { return new SimpleSpotLightBehaviour(); }  },
		{ "SolidObjectBehaviour",      []() { return new SolidObjectBehaviour(); }      },
		{ "SoundBehaviour",            []() { return new SoundBehaviour(); }            },
		{ "SpotLightBehaviour",        []() { return new SpotLightBehaviour(); }        },
		{ "TrackerBehaviour",          []() { return new TrackerBehaviour(); }          },

    };

    return behaviourMap;
};
