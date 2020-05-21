#include "application.h"
#include "utils.h"
#include "mesh.h"
#include "texture.h"

#include "fbo.h"
#include "shader.h"
#include "input.h"
#include "includes.h"
#include "prefab.h"
#include "gltf_loader.h"
#include "renderer.h"
#include "Scene.h"


#include <cmath>
#include <string>
#include <cstdio>

Application* Application::instance = nullptr;
Vector4 bg_color(0.1, 0.1, 0.1, 1.0);

Camera* camera = nullptr;
GTR::Prefab* prefab_car = nullptr;
GTR::Prefab* prefab_plane = nullptr;
GTR::Prefab* prefab_house = nullptr;
GTR::Prefab* prefab_lamp = nullptr;
GTR::Renderer* renderer = nullptr;
FBO* fbo;

float cam_speed = 10;

Scene* Scene::scene = nullptr;
PrefabEntity* car,*car2,*plane,*house,*lamp;

Light* point,*point2, *spot,* directional; //{DIRECTIONAL, SPOT, POINT} 0,1,2

bool temp = false;

Application::Application(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	instance = this;
	must_exit = false;
	render_debug = true;
	render_gui = true;

	render_wireframe = false;

	fps = 0;
	frame = 0;
	time = 0.0f;
	elapsed_time = 0.0f;
	mouse_locked = false;
	Scene::scene = new Scene();
	//loads and compiles several shaders from one single file
    //change to "data/shader_atlas_osx.txt" if you are in XCODE
	if(!Shader::LoadAtlas("data/shader_atlas.txt"))
        exit(1);
    checkGLErrors();

	fbo = new FBO();
	fbo->create(window_width, window_height);

	car = new PrefabEntity(prefab_car, true);
	car2 = new PrefabEntity(prefab_car, true);
	house = new PrefabEntity(prefab_house, true);
	lamp = new PrefabEntity(prefab_lamp, true);
	//Lets load some object to render
	car->prefab = GTR::Prefab::Get("data/prefabs/gmc/scene.gltf");
	//car->prefab->root.material->metallic_roughness_texture = Texture::Get("data/prefabs/gmc/textures/Material_33_metallicRoughness.png");
	//car->model.translate(0, 0, -150);
	//car->model.rotate(45 * DEG2RAD, Vector3(0, 1, 0));
	car2->prefab = GTR::Prefab::Get("data/prefabs/gmc/scene.gltf");
	car2->model.translate(100, 0, -50);
	house->prefab = GTR::Prefab::Get("data/prefabs/house/scene.gltf");
	house->model.scale(0.5, 0.5, 0.5);
	house->model.translate(0.0, 0, 600.0);
	lamp->prefab = GTR::Prefab::Get("data/prefabs/lamp/scene.gltf");
	lamp->model.scale(10.0, 10.0, 10.0);

	point = new Light(Vector3(1, 0, 1), light_type::POINT_L, true, 0, Vector3(0, 250, 0), 500); //{DIRECTIONAL, SPOT, POINT} 0,1,2
	point->intensity = 2;
	point2 = new Light(Vector3(0, 1, 0), light_type::POINT_L, true, 0, Vector3(0, 350, 300), 500); //{DIRECTIONAL, SPOT, POINT} 0,1,2
	point2->intensity = 2;

	spot = new Light(Vector3(1, 1, 0), light_type::SPOT, true, 90 * DEG2RAD, Vector3(300, 400, 0), 500); //{DIRECTIONAL, SPOT, POINT} 0,1,2
	spot->has_shadow = false;
	spot->intensity = 3;
	directional = new Light(Vector3(0.2, 0.2, 0.2), light_type::DIRECTIONAL, true, 0*DEG2RAD, Vector3(0, 0, 0), INFINITY); //{DIRECTIONAL, SPOT, POINT} 0,1,2
	directional->has_shadow = true;
	directional->model.rotate(-45 * DEG2RAD, Vector3(1, 0, 0));
	directional->model.rotate(-45 * DEG2RAD, Vector3(0, 1, 0));
	directional->shadow_bias = 0.005;
	
	prefab_plane = new GTR::Prefab();
	Mesh *p = new Mesh();
	GTR::Material *m = new GTR::Material();
	p->createPlane(700);
	m->color_texture = Texture::Get("data/textures/floor.png");

	prefab_plane->root.mesh = p;
	prefab_plane->root.material = m;
	plane = new PrefabEntity(prefab_plane, true);
	plane->model.setTranslation(0.0, 3, 0.0);

	//add entities
	Scene::scene->entities.push_back(plane);
	Scene::scene->entities.push_back(car);
	//Scene::scene->entities.push_back(car2);
	//Scene::scene->entities.push_back(house);
	//Scene::scene->entities.push_back(lamp);
	Scene::scene->entities.push_back(spot);
	Scene::scene->entities.push_back(directional);
	Scene::scene->entities.push_back(point);
	//Scene::scene->entities.push_back(point2);

	// Create camera
	camera = new Camera();
	camera->lookAt(Vector3(-1000.f, 700.0f, -700.f), Vector3(0.f, 0.0f, 0.f), Vector3(0.f, 1.f, 0.f));
	camera->setPerspective( 45.f, window_width/(float)window_height, 1.0f, 10000.f);

	//This class will be the one in charge of rendering all 
	renderer = new GTR::Renderer(); //here so we have opengl ready in constructor

	//hide the cursor
	SDL_ShowCursor(!mouse_locked); //hide or show the mouse
}

//what to do when the image has to be draw
void Application::render(void)
{
	//be sure no errors present in opengl before start
	checkGLErrors();

	//set the clear color (the background color)
	glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w );

	if (!temp)
		renderer->renderShadowmap();
	temp = true;

	//fbo->bind();
	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    checkGLErrors();
    
	//set the camera as default (used by some functions in the framework)
	camera->enable();

	//set default flags
	glDisable(GL_BLEND);
    
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	if(render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	renderer->renderDeferred(camera);

	//fbo->unbind();
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	/*Shader* shader_depth = Shader::Get("depth");
	shader_depth->enable();
	shader_depth->setUniform("u_camera_nearfar", Vector2(spot->light_camera->near_plane, spot->light_camera->far_plane));
	*/

	//fbo->color_textures[0]->toViewport();
	//glViewport(0, 0, 250, 250);
	//spot->shadow_fbo->depth_texture->toViewport(shader_depth);
	//glViewport(250, 0, 250, 250);
	//directional->shadow_fbo->depth_texture->toViewport();

	/*//Draw the floor grid, helpful to have a reference point
	if(render_debug)
		drawGrid();*/
}

void Application::update(double seconds_elapsed)
{
	float speed = seconds_elapsed * cam_speed; //the speed is defined by the seconds_elapsed so it goes constant
	float orbit_speed = seconds_elapsed * 0.5;
	
	//async input to move the camera around
	if (Input::isKeyPressed(SDL_SCANCODE_LSHIFT)) speed *= 10; //move faster with left shift
	if (Input::isKeyPressed(SDL_SCANCODE_W) || Input::isKeyPressed(SDL_SCANCODE_UP)) camera->move(Vector3(0.0f, 0.0f, 1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_S) || Input::isKeyPressed(SDL_SCANCODE_DOWN)) camera->move(Vector3(0.0f, 0.0f,-1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_A) || Input::isKeyPressed(SDL_SCANCODE_LEFT)) camera->move(Vector3(1.0f, 0.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_D) || Input::isKeyPressed(SDL_SCANCODE_RIGHT)) camera->move(Vector3(-1.0f, 0.0f, 0.0f) * speed);

	//mouse input to rotate the cam
	#ifndef SKIP_IMGUI
	if (!ImGuizmo::IsUsing())
	#endif
	{
		if (mouse_locked || Input::mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) //move in first person view
		{
			camera->rotate(-Input::mouse_delta.x * orbit_speed * 0.5, Vector3(0, 1, 0));
			Vector3 right = camera->getLocalVector(Vector3(1, 0, 0));
			camera->rotate(-Input::mouse_delta.y * orbit_speed * 0.5, right);
		}
		else //orbit around center
		{
			bool mouse_blocked = false;
			#ifndef SKIP_IMGUI
						mouse_blocked = ImGui::IsAnyWindowHovered() || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
			#endif
			if (Input::mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT) && !mouse_blocked) //is left button pressed?
			{
				camera->orbit(-Input::mouse_delta.x * orbit_speed, Input::mouse_delta.y * orbit_speed);
			}
		}
	}
	
	//move up or down the camera using Q and E
	if (Input::isKeyPressed(SDL_SCANCODE_Q)) camera->moveGlobal(Vector3(0.0f, -1.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_E)) camera->moveGlobal(Vector3(0.0f, 1.0f, 0.0f) * speed);

	//to navigate with the mouse fixed in the middle
	SDL_ShowCursor(!mouse_locked);
	#ifndef SKIP_IMGUI
		ImGui::SetMouseCursor(mouse_locked ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow);
	#endif
	if (mouse_locked)
	{
		Input::centerMouse();
		//ImGui::SetCursorPos(ImVec2(Input::mouse_position.x, Input::mouse_position.y));
	}
}

void Application::renderDebugGizmo()
{
	if (!car->prefab)
		return;

	//example of matrix we want to edit, change this to the matrix of your entity
	Matrix44& matrix = car->prefab->root.model;

	#ifndef SKIP_IMGUI

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	if (ImGui::IsKeyPressed(90))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(69))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(82)) // r Key
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(matrix.m, matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation, 3);
	ImGui::InputFloat3("Rt", matrixRotation, 3);
	ImGui::InputFloat3("Sc", matrixScale, 3);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.m);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
	static bool useSnap(false);
	if (ImGui::IsKeyPressed(83))
		useSnap = !useSnap;
	ImGui::Checkbox("", &useSnap);
	ImGui::SameLine();
	static Vector3 snap;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		//snap = config.mSnapTranslation;
		ImGui::InputFloat3("Snap", &snap.x);
		break;
	case ImGuizmo::ROTATE:
		//snap = config.mSnapRotation;
		ImGui::InputFloat("Angle Snap", &snap.x);
		break;
	case ImGuizmo::SCALE:
		//snap = config.mSnapScale;
		ImGui::InputFloat("Scale Snap", &snap.x);
		break;
	}
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	ImGuizmo::Manipulate(camera->view_matrix.m, camera->projection_matrix.m, mCurrentGizmoOperation, mCurrentGizmoMode, matrix.m, NULL, useSnap ? &snap.x : NULL);
	#endif
}


//called to render the GUI from
void Application::renderDebugGUI(void)
{
#ifndef SKIP_IMGUI //to block this code from compiling if we want

	//System stats
	ImGui::Text(getGPUStats().c_str());					   // Display some text (you can use a format strings too)

	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::ColorEdit4("BG color", bg_color.v);

	//add info to the debug panel about the camera
	if (ImGui::TreeNode(camera, "Camera")) {
		camera->renderInMenu();
		ImGui::TreePop();
	}

	//example to show prefab info: first param must be unique!
	if (car->prefab && ImGui::TreeNode(car->prefab, "Prefab")) {
		car->prefab->root.renderInMenu();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode(directional, "Lights")) {

		ImGui::DragFloat3("Ambient", &(Scene::scene->ambient.x), 0.05f, 0.0f, 1.0f);
		ImGui::Checkbox("reload", &temp);


		if (ImGui::TreeNode(directional, "Directional Light")) {

			float matrixTranslation[3], matrixRotation[3], matrixScale[3];
			ImGuizmo::DecomposeMatrixToComponents(directional->model.m, matrixTranslation, matrixRotation, matrixScale);
			ImGui::DragFloat3("Rotation", matrixRotation, 0.1f);
			ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, directional->model.m);
			ImGui::DragFloat3("Color", &(directional->color.x), 0.1f, 0.0f, 1.0f);
			ImGui::SliderFloat("Bias", &(directional->shadow_bias), 0.0f, 1.f);
			ImGui::SliderFloat("Intensity", &(directional->intensity), 0.0f, 10.f);
			ImGui::Checkbox("visible", &(directional->visible));

		}
		if (ImGui::TreeNode(point, "Point Light Purple")) {

			ImGui::DragFloat3("Position", &(point->position.x), 0.1f);
			ImGui::DragFloat3("Color", &(point->color.x), 0.1f, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Distance", &(point->maxDist), 0.0f, 500.0f);
			ImGui::SliderFloat("Intensity", &(point->intensity), 0.0f, 10.f);
			ImGui::Checkbox("visible", &(point->visible));
		}
		if (ImGui::TreeNode(point2, "Point Light Green")) {


			ImGui::DragFloat3("Position", &(point2->position.x), 0.1f);
			ImGui::DragFloat3("Color", &(point2->color.x), 0.1f, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Distance", &(point2->maxDist), 0.0f, 500.0f);
			ImGui::SliderFloat("Intensity", &(point2->intensity), 0.0f, 10.f);
			ImGui::Checkbox("visible", &(point2->visible));
		}
		if (ImGui::TreeNode(spot, "Spot Light")) {

			float matrixTranslation[3], matrixRotation[3], matrixScale[3];
			ImGuizmo::DecomposeMatrixToComponents(spot->model.m, matrixTranslation, matrixRotation, matrixScale);
			ImGui::DragFloat3("Position", &(spot->position.x), 0.1f);
			ImGui::DragFloat3("Rotation", matrixRotation, 0.1f);
			ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, spot->model.m);
			ImGui::DragFloat3("Color", &(spot->color.x), 0.1f, 0.0f, 1.0f);
			ImGui::SliderFloat("cutoff", &(spot->spotCutOff), 0.0f, 1.0f);
			ImGui::SliderFloat("exponent", &(spot->exponent_factor), 0.0f, 100.0f);
			ImGui::SliderFloat("Max Distance", &(spot->maxDist), 0.0f, 500.0f);
			ImGui::SliderFloat("Bias", &(spot->shadow_bias), 0.0f, 1.f);
			ImGui::SliderFloat("Intensity", &(spot->intensity), 0.0f, 10.f);
			ImGui::Checkbox("visible", &(spot->visible));
		}

	}

#endif
}

//Keyboard event handler (sync input)
void Application::onKeyDown( SDL_KeyboardEvent event )
{
	switch(event.keysym.sym)
	{
		case SDLK_ESCAPE: must_exit = true; break; //ESC key, kill the app
		case SDLK_F1: render_debug = !render_debug; break;
		case SDLK_f: camera->center.set(0, 0, 0); camera->updateViewMatrix(); break;
		case SDLK_F5: Shader::ReloadAll(); break;
	}
}

void Application::onKeyUp(SDL_KeyboardEvent event)
{
}

void Application::onGamepadButtonDown(SDL_JoyButtonEvent event)
{

}

void Application::onGamepadButtonUp(SDL_JoyButtonEvent event)
{

}

void Application::onMouseButtonDown( SDL_MouseButtonEvent event )
{
	if (event.button == SDL_BUTTON_MIDDLE) //middle mouse
	{
		//Input::centerMouse();
		mouse_locked = !mouse_locked;
		SDL_ShowCursor(!mouse_locked);
	}
}

void Application::onMouseButtonUp(SDL_MouseButtonEvent event)
{
}

void Application::onMouseWheel(SDL_MouseWheelEvent event)
{
	bool mouse_blocked = false;

	#ifndef SKIP_IMGUI
		ImGuiIO& io = ImGui::GetIO();
		if(!mouse_locked)
		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
			{
				if (event.x > 0) io.MouseWheelH += 1;
				if (event.x < 0) io.MouseWheelH -= 1;
				if (event.y > 0) io.MouseWheel += 1;
				if (event.y < 0) io.MouseWheel -= 1;
			}
		}
		mouse_blocked = ImGui::IsAnyWindowHovered();
	#endif

	if (!mouse_blocked && event.y)
	{
		if (mouse_locked)
			cam_speed *= 1 + (event.y * 0.1);
		else
			camera->changeDistance(event.y * 0.5);
	}
}

void Application::onResize(int width, int height)
{
    std::cout << "window resized: " << width << "," << height << std::endl;
	glViewport( 0,0, width, height );
	camera->aspect =  width / (float)height;
	window_width = width;
	window_height = height;
}

