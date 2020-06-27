#ifndef SCENE_HEADER
#define SCENE_HEADER

#include "BaseEntity.h"
#include "includes.h"
#include "utils.h"

class Scene {
public:
	static Scene* scene; //singleton

	enum{ FORWARD,DEFERRED};
	enum { DIRECTIONAL, POINT, SPOT };

	std::vector<BaseEntity*> entities;
	Vector3 ambient=Vector3(.1,.1,.1);
	bool pbr = false;
	bool gBuffers = false;
	bool has_gamma = true;
	float ssao_bias = 0.005;
	bool probes = false;
	bool ref_probes = false;
	bool showIrrText = false;
	bool deferred = true;
	bool show_reflections = false;
	bool planar_reflection = false;
	Vector4 bg_color;
	Light* sun;
	char render_type;
	char volumetric_type;
	Scene() { scene = this; };

	std::vector<PrefabEntity*> getPrefabs();

	std::vector<Light*> getVisibleLights();

	std::vector<Light*> getShadowLights();
	
};
#endif 