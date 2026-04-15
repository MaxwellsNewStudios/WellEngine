// Automatically generated during build by BehaviourRegistration.
// Scans for all behaviour definitions and includes them here for the behaviour factory to use.

#include "stdafx.h"
#include "BehaviourRegistry.h"
#include "Behaviour.h"
#include "Behaviours/Debug/DebugPlayerBehaviour.h"
#include "Behaviours/Debug/ExampleBehaviour.h"
#include "Behaviours/Events/EndCutSceneBehaviour.h"
#include "Behaviours/Events/PlayerCutsceneBehaviour.h"
#include "Behaviours/Interaction/BreadcrumbBehaviour.h"
#include "Behaviours/Interaction/BreadcrumbPileBehaviour.h"
#include "Behaviours/Interaction/CameraItemBehaviour.h"
#include "Behaviours/Interaction/HideBehaviour.h"
#include "Behaviours/Interaction/InteractableBehaviour.h"
#include "Behaviours/Interaction/InteractorBehaviour.h"
#include "Behaviours/Interaction/PickupBehaviour.h"
#include "Behaviours/Inventory/CompassBehaviour.h"
#include "Behaviours/Inventory/FlashlightBehaviour.h"
#include "Behaviours/Inventory/InventoryBehaviour.h"
#include "Behaviours/Inventory/PictureBehaviour.h"
#include "Behaviours/Menu/ButtonBehaviours.h"
#include "Behaviours/Menu/CreditsBehaviour.h"
#include "Behaviours/Menu/FlashlightPropBehaviour.h"
#include "Behaviours/Menu/MenuCameraBehaviour.h"
#include "Behaviours/Misc/TrackerBehaviour.h"
#include "Behaviours/Monster/MonsterBehaviour.h"
#include "Behaviours/Monster/MonsterHintBehaviour.h"
#include "Behaviours/Navigation/GraphNodeBehaviour.h"
#include "Behaviours/Physics/ColliderBehaviour.h"
#include "Behaviours/Physics/ExampleCollisionBehaviour.h"
#include "Behaviours/Physics/Jolt/Colliders/BoxJoltColliderBehaviour.h"
#include "Behaviours/Physics/Jolt/Colliders/SphereJoltColliderBehaviour.h"
#include "Behaviours/Physics/Jolt/PhysicsForceBehaviour.h"
#include "Behaviours/Physics/SolidObjectBehaviour.h"
#include "Behaviours/Player/PlayerMovementBehaviour.h"
#include "Behaviours/Player/PlayerViewBehaviour.h"
#include "Behaviours/Player/RestrictedViewBehaviour.h"
#include "Behaviours/Rendering/Camera/CameraBehaviour.h"
#include "Behaviours/Rendering/Camera/CameraCubeBehaviour.h"
#include "Behaviours/Rendering/Lighting/PointLightBehaviour.h"
#include "Behaviours/Rendering/Lighting/SimplePointLightBehaviour.h"
#include "Behaviours/Rendering/Lighting/SimpleSpotLightBehaviour.h"
#include "Behaviours/Rendering/Lighting/SpotLightBehaviour.h"
#include "Behaviours/Rendering/Mesh/BillboardMeshBehaviour.h"
#include "Behaviours/Rendering/Mesh/MeshBehaviour.h"
#include "Behaviours/Rendering/Mesh/TextMeshBehaviour.h"
#include "Behaviours/Sound/AmbientSoundBehaviour.h"
#include "Behaviours/Sound/SoundBehaviour.h"


#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion

const std::map<std::string, std::function<Behaviour *(void)>> &BehaviourRegistry::Get()
{
    static const std::map<std::string, std::function<Behaviour *(void)>> behaviourMap = {
		{ "DebugPlayerBehaviour",        []() { return new DebugPlayerBehaviour(); }        },
		{ "ExampleBehaviour",            []() { return new ExampleBehaviour(); }            },
		{ "EndCutSceneBehaviour",        []() { return new EndCutSceneBehaviour(); }        },
		{ "PlayerCutsceneBehaviour",     []() { return new PlayerCutsceneBehaviour(); }     },
		{ "BreadcrumbBehaviour",         []() { return new BreadcrumbBehaviour(); }         },
		{ "BreadcrumbPileBehaviour",     []() { return new BreadcrumbPileBehaviour(); }     },
		{ "CameraItemBehaviour",         []() { return new CameraItemBehaviour(); }         },
		{ "HideBehaviour",               []() { return new HideBehaviour(); }               },
		{ "InteractableBehaviour",       []() { return new InteractableBehaviour(); }       },
		{ "InteractorBehaviour",         []() { return new InteractorBehaviour(); }         },
		{ "PickupBehaviour",             []() { return new PickupBehaviour(); }             },
		{ "CompassBehaviour",            []() { return new CompassBehaviour(); }            },
		{ "FlashlightBehaviour",         []() { return new FlashlightBehaviour(); }         },
		{ "InventoryBehaviour",          []() { return new InventoryBehaviour(); }          },
		{ "PictureBehaviour",            []() { return new PictureBehaviour(); }            },
		{ "PlayButtonBehaviour",         []() { return new PlayButtonBehaviour(); }         },
		{ "SaveButtonBehaviour",         []() { return new SaveButtonBehaviour(); }         },
		{ "NewSaveButtonBehaviour",      []() { return new NewSaveButtonBehaviour(); }      },
		{ "CreditsButtonBehaviour",      []() { return new CreditsButtonBehaviour(); }      },
		{ "ExitButtonBehaviour",         []() { return new ExitButtonBehaviour(); }         },
		{ "CreditsBehaviour",            []() { return new CreditsBehaviour(); }            },
		{ "FlashlightPropBehaviour",     []() { return new FlashlightPropBehaviour(); }     },
		{ "MenuCameraBehaviour",         []() { return new MenuCameraBehaviour(); }         },
		{ "TrackerBehaviour",            []() { return new TrackerBehaviour(); }            },
		{ "MonsterBehaviour",            []() { return new MonsterBehaviour(); }            },
		{ "MonsterHintBehaviour",        []() { return new MonsterHintBehaviour(); }        },
		{ "GraphNodeBehaviour",          []() { return new GraphNodeBehaviour(); }          },
		{ "ColliderBehaviour",           []() { return new ColliderBehaviour(); }           },
		{ "ExampleCollisionBehaviour",   []() { return new ExampleCollisionBehaviour(); }   },
		{ "BoxJoltColliderBehaviour",    []() { return new BoxJoltColliderBehaviour(); }    },
		{ "SphereJoltColliderBehaviour", []() { return new SphereJoltColliderBehaviour(); } },
		{ "PhysicsForceBehaviour",       []() { return new PhysicsForceBehaviour(); }       },
		{ "SolidObjectBehaviour",        []() { return new SolidObjectBehaviour(); }        },
		{ "PlayerMovementBehaviour",     []() { return new PlayerMovementBehaviour(); }     },
		{ "PlayerViewBehaviour",         []() { return new PlayerViewBehaviour(); }         },
		{ "RestrictedViewBehaviour",     []() { return new RestrictedViewBehaviour(); }     },
		{ "CameraBehaviour",             []() { return new CameraBehaviour(); }             },
		{ "CameraCubeBehaviour",         []() { return new CameraCubeBehaviour(); }         },
		{ "PointLightBehaviour",         []() { return new PointLightBehaviour(); }         },
		{ "SimplePointLightBehaviour",   []() { return new SimplePointLightBehaviour(); }   },
		{ "SimpleSpotLightBehaviour",    []() { return new SimpleSpotLightBehaviour(); }    },
		{ "SpotLightBehaviour",          []() { return new SpotLightBehaviour(); }          },
		{ "BillboardMeshBehaviour",      []() { return new BillboardMeshBehaviour(); }      },
		{ "MeshBehaviour",               []() { return new MeshBehaviour(); }               },
		{ "TextMeshBehaviour",           []() { return new TextMeshBehaviour(); }           },
		{ "AmbientSoundBehaviour",       []() { return new AmbientSoundBehaviour(); }       },
		{ "SoundBehaviour",              []() { return new SoundBehaviour(); }              },

    };

    return behaviourMap;
};

#ifdef DEBUG_BUILD
const std::map<std::string, std::string> &BehaviourRegistry::GetCategories()
{
    static const std::map<std::string, std::string> behaviourCategoryMap = {
		{ "DebugPlayerBehaviour",        "Debug/"                  },
		{ "ExampleBehaviour",            "Debug/"                  },
		{ "EndCutSceneBehaviour",        "Events/"                 },
		{ "PlayerCutsceneBehaviour",     "Events/"                 },
		{ "BreadcrumbBehaviour",         "Interaction/"            },
		{ "BreadcrumbPileBehaviour",     "Interaction/"            },
		{ "CameraItemBehaviour",         "Interaction/"            },
		{ "HideBehaviour",               "Interaction/"            },
		{ "InteractableBehaviour",       "Interaction/"            },
		{ "InteractorBehaviour",         "Interaction/"            },
		{ "PickupBehaviour",             "Interaction/"            },
		{ "CompassBehaviour",            "Inventory/"              },
		{ "FlashlightBehaviour",         "Inventory/"              },
		{ "InventoryBehaviour",          "Inventory/"              },
		{ "PictureBehaviour",            "Inventory/"              },
		{ "PlayButtonBehaviour",         "Menu/"                   },
		{ "SaveButtonBehaviour",         "Menu/"                   },
		{ "NewSaveButtonBehaviour",      "Menu/"                   },
		{ "CreditsButtonBehaviour",      "Menu/"                   },
		{ "ExitButtonBehaviour",         "Menu/"                   },
		{ "CreditsBehaviour",            "Menu/"                   },
		{ "FlashlightPropBehaviour",     "Menu/"                   },
		{ "MenuCameraBehaviour",         "Menu/"                   },
		{ "TrackerBehaviour",            "Misc/"                   },
		{ "MonsterBehaviour",            "Monster/"                },
		{ "MonsterHintBehaviour",        "Monster/"                },
		{ "GraphNodeBehaviour",          "Navigation/"             },
		{ "ColliderBehaviour",           "Physics/"                },
		{ "ExampleCollisionBehaviour",   "Physics/"                },
		{ "BoxJoltColliderBehaviour",    "Physics/Jolt/Colliders/" },
		{ "SphereJoltColliderBehaviour", "Physics/Jolt/Colliders/" },
		{ "PhysicsForceBehaviour",       "Physics/Jolt/"           },
		{ "SolidObjectBehaviour",        "Physics/"                },
		{ "PlayerMovementBehaviour",     "Player/"                 },
		{ "PlayerViewBehaviour",         "Player/"                 },
		{ "RestrictedViewBehaviour",     "Player/"                 },
		{ "CameraBehaviour",             "Rendering/Camera/"       },
		{ "CameraCubeBehaviour",         "Rendering/Camera/"       },
		{ "PointLightBehaviour",         "Rendering/Lighting/"     },
		{ "SimplePointLightBehaviour",   "Rendering/Lighting/"     },
		{ "SimpleSpotLightBehaviour",    "Rendering/Lighting/"     },
		{ "SpotLightBehaviour",          "Rendering/Lighting/"     },
		{ "BillboardMeshBehaviour",      "Rendering/Mesh/"         },
		{ "MeshBehaviour",               "Rendering/Mesh/"         },
		{ "TextMeshBehaviour",           "Rendering/Mesh/"         },
		{ "AmbientSoundBehaviour",       "Sound/"                  },
		{ "SoundBehaviour",              "Sound/"                  },

    };

    return behaviourCategoryMap;
};
#endif
