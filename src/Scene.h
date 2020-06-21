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
	bool gBuffers = false;
	bool has_gamma = true;
	float ssao_bias = 0.005;
	bool probes = false;
	bool showIrrText = false;
	Vector4 bg_color;
	Light* sun;

	Scene() { scene = this; };

	std::vector<PrefabEntity*> getPrefabs();

	std::vector<Light*> getVisibleLights();

	std::vector<Light*> getShadowLights();
	
};
#endif 