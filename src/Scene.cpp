#include "Scene.h"

std::vector<PrefabEntity*> Scene::getPrefabs()
{
	std::vector<PrefabEntity*> prefab_vector;
	for (int i = 0; i < Scene::scene->entities.size();i++) {
		
		if (Scene::scene->entities[i]->type == eType::PREFAB) {
			if  (Scene::scene->entities[i]->visible) {
				PrefabEntity *downcast = (PrefabEntity*)Scene::scene->entities[i];
				prefab_vector.push_back(downcast);
			}
		}
	}
	return prefab_vector;
}
std::vector<Light*> Scene::getVisibleLights()
{
	std::vector<Light*> light_vector;

	for (int i = 0; i < Scene::scene->entities.size();i++) {

		if (Scene::scene->entities[i]->type == eType::LIGHT) {
			if (Scene::scene->entities[i]->visible) {
				Light *downcast = (Light*)Scene::scene->entities[i];
				light_vector.push_back(downcast);
			}
		}
	}
	return light_vector;
}

std::vector<Light*> Scene::getShadowLights()
{
	std::vector<Light*> light_vector;

	for (int i = 0; i < Scene::scene->entities.size();i++) {

		if (Scene::scene->entities[i]->type == eType::LIGHT) {
			Light *downcast = (Light*)Scene::scene->entities[i];
			if (downcast->has_shadow)
				light_vector.push_back(downcast);
			
		}
	}
	return light_vector;
}