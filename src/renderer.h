#pragma once
#include "prefab.h"
#include "scene.h"
#include "fbo.h"
#include "sphericalharmonics.h"
#include "extra/hdre.h"

//forward declarations
class Camera;

namespace GTR {

	class Prefab;
	class Material;

	//struct to store probes
	struct sProbe {
		Vector3 pos; //where is located
		Vector3 local;
		int index; //its index in the array 
		SphericalHarmonics sh; //coeffs
	};
	struct sReflectionProbe {
		Vector3 pos;
		Texture* cubemap = NULL;
	};

	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:
		FBO* gbuffers_fbo;
		FBO* ssao_fbo;
		Texture* ssao_blur;
		Texture* probes_texture;
		FBO* illumination_fbo;
		FBO* irr_fbo;
		FBO* reflections_fbo;
		Texture * environment;
		Texture* environment1;
		//FBO* final_fbo;
		FBO* volumetric_fbo;
		FBO* planar_reflection_fbo;

		std::vector<Vector3> random_points;
		std::vector<sProbe> probes;
		float normalDistance = 1.0f;
		std::vector<sReflectionProbe*> reflection_probes;
		bool first = true;
		
		bool volumetric = false;
		float sampledensity = 0.02f;

		bool use_fx = true;
		float tonemapper_scale = 1.0f;
		float tonemapper_average_lum = 1.0f;
		float tonemapper_lumwhite2 = 1.0f;
		float tonemapper_igamma = 2.2f;

		bool ssao_blurring = false;

		Renderer();

		std::vector<Vector3> generateSpherePoints(int num, float radius, bool hemi);

		void renderDeferred(Camera * camera);

		void renderScene(Camera * camera, bool deferred);

		void renderPrefab(const Matrix44 & model, GTR::Prefab * prefab, Camera * camera, bool deferred);

		void renderNodeForward(const Matrix44 & prefab_model, GTR::Node * node, Camera * camera);

		void renderNodeDeferred(const Matrix44 & prefab_model, GTR::Node * node, Camera * camera);

		void renderMeshWithLight(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera);//forward

		void renderMeshDeferred(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera);

		void renderShadowmap();

		void renderSkyBox(Camera* camera, bool flag);

		void computeReflection();
		void computeIrradiance();

		void renderReflectionProbe(Vector3 pos, float size, Texture *cubemap);
		void renderProbe(Vector3 pos, float size, float* coeffs);

		Texture* CubemapFromHDRE(const char* filename);
	};


};