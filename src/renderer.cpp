#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "application.h"
#include "sphericalharmonics.h"

using namespace GTR;

bool render_shadowmap = false;

//struct to store probes
struct sProbe {
	Vector3 pos; //where is located
	Vector3 index; //its index in the array 
	SphericalHarmonics sh; //coeffs
};

Renderer::Renderer()
{
	gbuffers_fbo = NULL;
	ssao_fbo = NULL;
	ssao_blur = NULL;
	illumination_fbo = NULL;
}

std::vector<Vector3> Renderer::generateSpherePoints(int num, float radius, bool hemi)
{
	std::vector<Vector3> points;
	points.resize(num);
	for (int i = 0; i < num; i += 3)
	{
		Vector3& p = points[i];
		float u = random();
		float v = random();
		float theta = u * 2.0 * PI;
		float phi = acos(2.0 * v - 1.0);
		float r = cbrt(random() * 0.9 + 0.1) * radius;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);
		p.x = r * sinPhi * cosTheta;
		p.y = r * sinPhi * sinTheta;
		p.z = r * cosPhi;
		if (hemi && p.z < 0)
			p.z *= -1.0;
	}
	return points;
}

void Renderer::renderProbe(Vector3 pos, float size, float* coeffs)
{
	Camera* camera = Camera::current;
	Shader* shader = Shader::Get("probe");
	Matrix44 model;
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);
	shader->enable();
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform3Array("u_coeffs", coeffs, 9);
	Mesh::Get("data/meshes/sphere.obj")->render(GL_TRIANGLES);
}

void Renderer::renderDeferred(Camera* camera)
{
	int w = Application::instance->window_width;
	int h = Application::instance->window_height;

	if (!gbuffers_fbo)
	{
		gbuffers_fbo = new FBO();
		gbuffers_fbo->create(w, h,
			3, 			//three textures
			GL_RGBA, 		//four channels
			GL_HALF_FLOAT, //1 byte
			true);		//add depth_texture
	}

	//start rendering inside the gbuffers
	gbuffers_fbo->bind();

	//we clear in several passes so we can control the clear color independently for every gbuffer

	//disable all but the GB0 (and the depth)
	gbuffers_fbo->enableSingleBuffer(0);

	//clear GB0 with the color (and depth)
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//and now enable the second GB to clear it to black
	gbuffers_fbo->enableSingleBuffer(1);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	gbuffers_fbo->enableSingleBuffer(2);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	gbuffers_fbo->enableSingleBuffer(3);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	//enable all buffers back
	gbuffers_fbo->enableAllBuffers();

	//render everything 
	std::vector<PrefabEntity*> prefab_vector = Scene::scene->getPrefabs();
	for(int i=0;i<prefab_vector.size();i++)
		renderPrefab(prefab_vector[i]->model, prefab_vector[i]->prefab, camera);

	//stop rendering to the gbuffers
	gbuffers_fbo->unbind();

	//SCREEN SPACE AMBIENT OCCLUSION
	if (!ssao_fbo)
	{
		ssao_fbo = new FBO();
		ssao_fbo->create(w, h);
	}

	if (!ssao_blur)
	{
		ssao_blur = new Texture();
		ssao_blur->create(w, h);
	}

	//start rendering inside the ssao texture
	ssao_fbo->bind();

	glDisable(GL_DEPTH_TEST);

	//get the shader for SSAO (remember to create it using the atlas)
	Shader* shader = Shader::Get("ssao");
	shader->enable();

	//send info to reconstruct the world position
	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();

	//bind the texture we want to change
	gbuffers_fbo->depth_texture->bind();
	//disable using mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//enable bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gbuffers_fbo->depth_texture->unbind();

	shader->setUniform("u_normal_texture", gbuffers_fbo->color_textures[1], 0);
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 1);
	//we need the pixel size so we can center the samples 
	shader->setUniform("u_iRes", Vector2(1.0 / (float)gbuffers_fbo->depth_texture->width, 1.0 / (float)gbuffers_fbo->depth_texture->height));
	//we will need the viewprojection to obtain the uv in the depthtexture of any random position of our world
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_bias", Scene::scene->ssao_bias);

	//send random points so we can fetch around
	shader->setUniform3Array("u_points", (float*)&random_points[0], random_points.size());

	//render fullscreen quad
	Mesh* ssao_quad = Mesh::getQuad();
	ssao_quad->render(GL_TRIANGLES);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS); //as default;
	shader->disable();

	//stop rendering to the texture
	ssao_fbo->unbind();
	
	if (ssao_blurring)
	{
		Shader* blur_shader = Shader::Get("blur");
		blur_shader->enable();
		blur_shader->setUniform("u_texture", ssao_fbo->color_textures[0]);
		blur_shader->setUniform("u_offset", Vector2(1.0 / (float)ssao_fbo->color_textures[0]->width, 1.0 / (float)ssao_fbo->color_textures[0]->height));
		ssao_fbo->color_textures[0]->copyTo(ssao_blur, blur_shader);
		blur_shader->enable();
		blur_shader->setUniform("u_offset", Vector2(1.0 / (float)ssao_fbo->color_textures[0]->width, 1.0 / (float)ssao_fbo->color_textures[0]->height)*2.0);
		ssao_blur->copyTo(ssao_fbo->color_textures[0], blur_shader);
		blur_shader->enable();
		blur_shader->setUniform("u_offset", Vector2(1.0 / (float)ssao_fbo->color_textures[0]->width, 1.0 / (float)ssao_fbo->color_textures[0]->height)*4.0);
		ssao_fbo->color_textures[0]->copyTo(ssao_blur, blur_shader);
	}
	/***************************/
	//ILLUMINATION
	if (!illumination_fbo)
	{
		illumination_fbo = new FBO();

		illumination_fbo->create(w, h,
			1, 			
			GL_RGB, 		
			GL_UNSIGNED_BYTE, 
			false);		
	}

	//start rendering to the illumination fbo
	illumination_fbo->bind();

	//clear GB0 with the color (and depth)
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	std::vector<Light*> light_vector = Scene::scene->getVisibleLights();
	for (int i = 0; i < light_vector.size(); i++)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		
		if (light_vector[i]->l_type != light_type::POINT_L)
		{
			//we need a fullscreen quad
			Mesh* quad = Mesh::getQuad();

			//we need a shader specially for this task, lets call it "deferred"
			Shader* sh = Shader::Get("deferred");
			sh->enable();

			sh->setUniform("u_camera_position", camera->eye);

			//pass the gbuffers to the shader
			sh->setUniform("u_color_texture", gbuffers_fbo->color_textures[0], 0);
			sh->setUniform("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
			sh->setUniform("u_extra_texture", gbuffers_fbo->color_textures[2], 2);
			sh->setUniform("u_depth_texture", gbuffers_fbo->depth_texture, 3);
			sh->setUniform("u_hasgamma", Scene::scene->has_gamma);
			sh->setUniform("u_ssao", ssao_fbo->color_textures[0], 4);

			//pass the inverse projection of the camera to reconstruct world pos.
			inv_vp = camera->viewprojection_matrix;
			inv_vp.inverse();
			sh->setUniform("u_inverse_viewprojection", inv_vp);
			//pass the inverse window resolution, this may be useful
			sh->setUniform("u_iRes", Vector2(1.0 / (float)w, 1.0 / (float)h));

			//pass all the information about the light and ambient…
			sh->setUniform("u_ambient_light", Scene::scene->ambient);

			light_vector[i]->setUniforms(sh);
			if (light_vector[i]->has_shadow) {
				//get the depth texture from the FBO
				Texture* shadowmap = light_vector[i]->shadow_fbo->depth_texture;

				//first time we create the FBO
				sh->setTexture("shadowmap", shadowmap, 8);

				//also get the viewprojection from the light
				Matrix44 shadow_proj = light_vector[i]->light_camera->viewprojection_matrix;

				//pass it to the shader
				sh->setUniform("u_shadow_viewproj", shadow_proj);

				//we will also need the shadow bias
				sh->setUniform("u_shadow_bias", light_vector[i]->shadow_bias);

			}

			glDisable(GL_DEPTH_TEST);

			//render a fullscreen quad
			quad->render(GL_TRIANGLES);

			sh->disable();
		}
		else
		{
			//we can use a sphere mesh for point lights
			Mesh* sphere = Mesh::Get("data/meshes/sphere.obj");
			
			//this deferred_ws shader uses the basic.vs instead of quad.vs
			Shader* sh = Shader::Get("deferred_ws");
			sh->enable();

			//remember to upload all the uniforms for gbuffers, ivp, etc...
			sh->setUniform("u_camera_position", camera->eye);

			//pass the gbuffers to the shader
			sh->setUniform("u_color_texture", gbuffers_fbo->color_textures[0], 0);
			sh->setUniform("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
			sh->setUniform("u_extra_texture", gbuffers_fbo->color_textures[2], 2);
			sh->setUniform("u_depth_texture", gbuffers_fbo->depth_texture, 3);
			sh->setUniform("u_hasgamma", Scene::scene->has_gamma);
			sh->setUniform("u_ssao", ssao_fbo->color_textures[0], 4);

			//pass the inverse projection of the camera to reconstruct world pos.
			Matrix44 inv_vp = camera->viewprojection_matrix;
			inv_vp.inverse();
			sh->setUniform("u_inverse_viewprojection", inv_vp);
			//pass the inverse window resolution, this may be useful
			sh->setUniform("u_iRes", Vector2(1.0 / (float)w, 1.0 / (float)h));

			//pass all the information about the light and ambient…
			sh->setUniform("u_ambient_light", Scene::scene->ambient);

			//basic.vs will need the model and the viewproj of the camera
			sh->setUniform("u_viewprojection", camera->viewprojection_matrix);

			//we must translate the model to the center of the light
			Matrix44 m;
			m.setTranslation(light_vector[i]->position.x, light_vector[i]->position.y, light_vector[i]->position.z);
			//and scale it according to the max_distance of the light
			m.scale(light_vector[i]->maxDist, light_vector[i]->maxDist, light_vector[i]->maxDist);
			//pass the model to the shader to render the sphere
			sh->setUniform("u_model", m);

			//pass all the info about this light…
			light_vector[i]->setUniforms(sh);

			//render only the backfacing triangles of the sphere
			glFrontFace(GL_CW);

			glDisable(GL_DEPTH_TEST);

			glEnable(GL_CULL_FACE);
			//and render the sphere
			sphere->render(GL_TRIANGLES);
			glDisable(GL_CULL_FACE);

			glFrontFace(GL_CCW);

			sh->disable();
		}
	}

	//stop rendering to the fbo, render to screen
	illumination_fbo->unbind();

	//be sure blending is not active
	glDisable(GL_BLEND);

	//and render the texture into the screen
	if (Scene::scene->gBuffers)
	{

		Shader* shader_depth = Shader::Get("depth");
		shader_depth->enable();
		shader_depth->setUniform("u_camera_nearfar", Vector2(camera->near_plane, camera->far_plane));

		glViewport(0, 0, w*0.5, h*0.5);
		gbuffers_fbo->color_textures[0]->toViewport();

		glViewport(w*0.5, 0, w*0.5, h*0.5);
		gbuffers_fbo->color_textures[1]->toViewport();

		glViewport(0, h*0.5, w*0.5, h*0.5);
		ssao_fbo->color_textures[0]->toViewport();
		
		glViewport(w*0.5, h*0.5, w*0.5, h*0.5);
		gbuffers_fbo->depth_texture->toViewport(shader_depth);

		glViewport(0, 0, w, h);
	}
	else
	{
		illumination_fbo->color_textures[0]->toViewport();
		if (Scene::scene->probes)
		{
			for (int x = -150; x < 150; x += 80)
			{
				for (int y = 10; y < 200; y += 80)
				{
					for (int z = -200; z < 200; z += 80)
					{
						sProbe p;
						p.pos.set(x, y, z);
						renderProbe(p.pos, 10, (float*)&p.sh);
					}
				}
			}
		}
	}
}

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
			//renderMeshWithLight(node_model, node->mesh, node->material, camera);
			renderMeshDeferred(node_model, node->mesh, node->material, camera);
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
			if (light_vector[i]->has_shadow) 
			{
				
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


			if (!light_vector[i]->shadow_fbo)
			{
				light_vector[i]->shadow_fbo = new FBO();
				light_vector[i]->shadow_fbo->create(1024, 1024);
			}


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

void Renderer::renderMeshDeferred(const Matrix44 model, Mesh * mesh, GTR::Material * material, Camera * camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;
	Texture* texture_met_rough = NULL;


	texture = material->color_texture;
	texture_met_rough = material->metallic_roughness_texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	Shader* shader_shadow = NULL;
	shader_shadow = Shader::Get("shadow");

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

		shader = Shader::Get("multi");

		//no shader? then nothing to render
		if (!shader)
			return;
		shader->enable();

		//upload uniforms
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);
		shader->setUniform("u_model", model);
		shader->setUniform("u_camera_near_far", Vector2(camera->near_plane, camera->far_plane));
		shader->setUniform("u_hasgamma", Scene::scene->has_gamma);

		shader->setUniform("u_color", material->color);
		if (texture)
			shader->setUniform("u_texture", texture, 0);

		if (texture_met_rough)
		{
			shader->setUniform("u_metal_roughness", texture_met_rough, 1);
			shader->setUniform("u_hasmetal", true);
		}
		else
			shader->setUniform("u_hasmetal", false);
		
		shader->setUniform("u_metalness", material->metallic_factor);
		shader->setUniform("u_roughness", material->roughness_factor);

		//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
		shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);

		//disable shader
		shader->disable();

		//set the render state as it was before to avoid problems with future renders
		glDisable(GL_BLEND);
	}
	
}