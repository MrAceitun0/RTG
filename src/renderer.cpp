#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "application.h"


using namespace GTR;

bool render_shadowmap = false;

void GTR::Renderer::renderScene(Camera * camera)
{
	std::vector<PrefabEntity*> prefab_vector = Scene::scene->getPrefabs();

	for (int i = 0; i < prefab_vector.size();i++) {
		renderPrefab(prefab_vector[i]->model, prefab_vector[i]->prefab, camera);
	}

}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
{
	if (!node->visible)
		return;
	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			//render node mesh
			renderMeshWithLight(node_model, node->mesh, node->material, camera);
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	texture = material->color_texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;

	//select the blending
	if (material->alpha_mode == GTR::AlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	//chose a shader
	if (texture)
		shader = Shader::Get("texture");
	else
		shader = Shader::Get("flat");

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);

	shader->setUniform("u_color", material->color);
	if (texture)
		shader->setUniform("u_texture", texture, 0);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}
void Renderer::renderMeshWithLight(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;

	//define locals to simplify coding
	Shader* shader = NULL;
	Shader* shader_shadow = NULL;
	shader_shadow = Shader::Get("shadow");
	Texture* texture = NULL;

	texture = material->color_texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;


	if (render_shadowmap) { //if we are rendering shadowmap

		shader_shadow->enable();

		//upload uniforms
		shader_shadow->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader_shadow->setUniform("u_camera_position", camera->eye);
		shader_shadow->setUniform("u_model", model);

		mesh->render(GL_TRIANGLES);
		shader_shadow->disable();
	}
	else {

		//allow to render pixels that have the same depth as the one in the depth buffer
		glDepthFunc(GL_LEQUAL);
		//select if render both sides of the triangles
		if (material->two_sided)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);

		//chose a shader
		
		shader = Shader::Get("texture");

		//no shader? then nothing to render
		if (!shader)
			return;
		std::vector<Light*> light_vector = Scene::scene->getVisibleLights();

		//set blending mode to additive
		//this will collide with materials with blend...
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		shader->enable();

		//upload uniforms
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);
		shader->setUniform("u_model", model);

		shader->setUniform("u_color", material->color);
		if (texture)
			shader->setUniform("u_texture", texture, 0);
		shader->setUniform("u_ambient_light", Scene::scene->ambient);
		//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
		shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

		//light
		for (int i = 0;i < light_vector.size();i++) {

			//first pass doesn't use blending
			if (i == 0) {
				if (material->alpha_mode == GTR::AlphaMode::BLEND)
				{
					glEnable(GL_BLEND);
				}
				else
					glDisable(GL_BLEND);
			}
			else {
				glEnable(GL_BLEND);
			}
			if (light_vector[i]->has_shadow) {
				//get the depth texture from the FBO
				Texture* shadowmap = light_vector[i]->shadow_fbo->depth_texture;

				//first time we create the FBO
				shader->setTexture("shadowmap", shadowmap, 8);

				//also get the viewprojection from the light
				Matrix44 shadow_proj = light_vector[i]->light_camera->viewprojection_matrix;

				//pass it to the shader
				shader->setUniform("u_shadow_viewproj", shadow_proj);

				//we will also need the shadow bias
				shader->setUniform("u_shadow_bias", light_vector[i]->shadow_bias);
				
			}

			//pass the light data to the shader
			light_vector[i]->setUniforms(shader);
			mesh->render(GL_TRIANGLES);
		}

		//disable shader
		shader->disable();

		//set the render state as it was before to avoid problems with future renders
		glDisable(GL_BLEND);
		glDepthFunc(GL_LESS); //as default*/
	}

}

void GTR::Renderer::renderShadowmap()
{
	render_shadowmap = true;

	std::vector<Light*> light_vector = Scene::scene->getShadowLights();
	if (light_vector.size() != 0) {
		for (int i = 0;i < light_vector.size();i++) {
			Camera *cam = new Camera();

			if (light_vector[i]->l_type == light_type::DIRECTIONAL) {
				cam->lookAt(light_vector[i]->position, light_vector[i]->getLocalVector(Vector3(0, 0, -1)), Vector3(0.f, 1.f, 0.f));
				cam->setOrthographic(-900, 900, -900, 900, 900, -900);
			}
			else {
				cam->lookAt(light_vector[i]->position, light_vector[i]->getLocalVector(Vector3(0, 0, -1)), Vector3(0, 1, 0));
				cam->setPerspective(acos(light_vector[i]->spotCutOff)*RAD2DEG, 1.0, 1.0f, light_vector[i]->maxDist);
			}

			light_vector[i]->light_camera = cam;


			light_vector[i]->shadow_fbo = new FBO();
			light_vector[i]->shadow_fbo->create(1024, 1024);


			//enable it to render inside the texture
			light_vector[i]->shadow_fbo->bind();

			//you can disable writing to the color buffer to speed up the rendering as we do not need it
			glColorMask(false, false, false, false);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//set the camera as default (used by some functions in the framework)

			//set default flags
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);

			light_vector[i]->light_camera->enable();

			//render
			renderScene(light_vector[i]->light_camera);

			//disable it to render back to the screen
			light_vector[i]->shadow_fbo->unbind();
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			glColorMask(true, true, true, true);

			//allow to render back to the color buffer*/
		}
	}
	
	render_shadowmap = false;
}
