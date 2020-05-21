#ifndef SCENE_HEADER
#define SCENE_HEADER

#include "BaseEntity.h"
#include "includes.h"
#include "utils.h"

class Scene {
public:
	
	static Scene* scene; //singleton

	std::vector<BaseEntity*> entities;
	Vector3 ambient=Vector3(.1,.1,.1);
	bool pbr = false;
	
	Scene() { scene = this; };

	std::vector<PrefabEntity*> getPrefabs();

	std::vector<Light*> getVisibleLights();

	std::vector<Light*> getShadowLights();
	
};
#endif 