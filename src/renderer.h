#pragma once
#include "prefab.h"
#include "scene.h"
#include "fbo.h"

//forward declarations
class Camera;

namespace GTR {

	class Prefab;
	class Material;
	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:
		FBO* gbuffers_fbo;
		FBO* ssao_fbo;
		Texture* ssao_blur;
		FBO* illumination_fbo;

		Renderer();

		void renderDeferred(Camera * camera);

		void renderScene(Camera * camera);

		void renderPrefab(const Matrix44 & model, GTR::Prefab * prefab, Camera * camera);

		void renderNode(const Matrix44 & prefab_model, GTR::Node * node, Camera * camera);

		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);

		void renderMeshWithLight(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera);

		void renderMeshDeferred(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera);

		void renderShadowmap();
	};

};