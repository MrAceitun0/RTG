#pragma once
#include "prefab.h"
#include "scene.h"
#include "fbo.h"
#include "sphericalharmonics.h"

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
		std::vector<Vector3> random_points;
		std::vector<sProbe> probes;
		bool first = true;

		bool ssao_blurring = false;

		Renderer();

		std::vector<Vector3> generateSpherePoints(int num, float radius, bool hemi);

		void renderProbe(Vector3 pos, float size, float* coeffs);

		void computeIrradiance();

		void renderDeferred(Camera * camera);

		void renderScene(Camera * camera, bool deferred);

		void renderPrefab(const Matrix44 & model, GTR::Prefab * prefab, Camera * camera, bool deferred);

		void renderNodeForward(const Matrix44 & prefab_model, GTR::Node * node, Camera * camera);
		void renderNodeDeferred(const Matrix44 & prefab_model, GTR::Node * node, Camera * camera);

		void renderMeshWithLight(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera);//forward

		void renderMeshDeferred(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera);

		void renderShadowmap();
	};

};