#include "BaseEntity.h"


PrefabEntity::PrefabEntity(GTR::Prefab * p, bool v)
{
	this->prefab = p;
	this->type= eType::PREFAB;
	this->visible = v;
}

Light::Light(light_type type)
{
	this->color.set(1, 1, 1);
	//this->intensity = 1;
	this->l_type = type;
	this->type= eType::LIGHT;
	this->position.set(0, 10, 0);
	this->spotCutOff = 0.9;
	this->visible = true;
	this->exponent_factor = 0;
}

Light::Light(Vector3 c, light_type type, bool v, float rad, Vector3 pos,float max)
{
	this->color.set(c.x,c.y,c.z);
	//this->light_camera = new Camera();

	this->l_type = type;
	this->type = eType::LIGHT;
	this->position.set(pos.x, pos.y, pos.z);
	if(type==light_type::SPOT)
		this->model.rotate(rad, Vector3(1, 0, 0));
	else
		this->model.rotate(rad, Vector3(0, 1, 0));
	this->exponent_factor = 12;
	this->spotCutOff = 0.9;
	this->visible = v;
	this->maxDist = max;

}

void Light::setUniforms(Shader * shader)
{
	shader->setUniform("u_light_type", this->l_type);
	shader->setUniform("u_light_vector", this->getLocalVector(Vector3(0, 0, -1)));
	shader->setUniform("u_light_position", this->position);
	shader->setUniform("u_light_color", this->color);
	shader->setUniform("u_spotCutOff", this->spotCutOff);
	shader->setUniform("u_exponent", this->exponent_factor);
	shader->setUniform("u_light_maxdist", this->maxDist);
	shader->setUniform("u_light_intensity", this->intensity);
	shader->setUniform("u_shadows", this->has_shadow);
}
