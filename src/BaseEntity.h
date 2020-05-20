#ifndef BASEENTITY_HEADER
#define BASEENTITY_HEADER

#include "includes.h"
#include "utils.h"
#include "prefab.h"
#include "shader.h"
#include "fbo.h"
#include "camera.h"


enum eType { BASE_NODE, PREFAB, LIGHT };
enum light_type { DIRECTIONAL, SPOT, POINT_L };

class BaseEntity {
	public:
		unsigned int id; //: an unique number
		Matrix44 model; //: defines where it is
		bool visible; //: says if this object must be rendered
		eType type;

		Vector3 getLocalVector(Vector3 v) { return model.rotateVector(v); }
		
		
};
class PrefabEntity : public BaseEntity{
	public:
		/*We already have a class Prefab, but that class represents a type of prefab, we want to have another class that represents one prefab in our world.
		So the class PrefabEntity inherits from BaseEntity and only adds the reference to the prefab:
		Prefab* prefab;
		*/
		GTR::Prefab* prefab;

		PrefabEntity(GTR::Prefab* p,bool v);
};
class Light :public BaseEntity {
	public: 
		/*Represent an entity that casts light to the scene. It also inherits from BaseEntity
		For now you can specify it as:*/
		Vector3 color; //: the color of the light
		//float intensity; //: the amount of light emited
		
		Vector3 position;
		float exponent_factor;
		light_type l_type;
		float spotCutOff;
		float maxDist = 150;
		FBO *shadow_fbo;
		float shadow_bias = 0.1;
		Camera *light_camera = new Camera();
		bool has_shadow = false;
		float intensity = 1.0;

		Light(light_type t);

		Light(Vector3 c, light_type type, bool v, float rad, Vector3 pos, float max);

		void setUniforms(Shader *shader);
		
		
};
#endif 