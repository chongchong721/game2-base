#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("../maze.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("../maze.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*level) {

	scene.bboxes.clear();
	//Need to create bounding boxes
	for (auto &mesh_object: meshes->meshes){
		std::string name = mesh_object.first;
		if(name == "Sphere"){
			continue;
		}else if (name.find("bg_")!=std::string::npos){
			continue;
		}
		scene.bboxes.emplace_back();
		scene.bboxes.back().name = name;
		scene.bboxes.back().min = mesh_object.second.min;
		scene.bboxes.back().max = mesh_object.second.max;


	}



	//get pointers to things we need to modify
	for (auto &transform : scene.transforms) {
		if (transform.name == "Plane") transform_plane = &transform;
		else if (transform.name == "Sphere") transform_ball = &transform;
	}
	if (transform_ball == nullptr) throw std::runtime_error("ball not found.");
	if (transform_plane == nullptr) throw std::runtime_error("level not found.");

	ball_rotation = transform_ball->rotation;
	plane_rotation = transform_plane->rotation;
	ball_translation = transform_ball->position;

	{
		// Find the ball and initialize
		auto ball = meshes->lookup("Sphere");
		auto d = ball.max - ball.min;
		//std::cout << d.x << d.y << d.z << std::endl;
		ball_metadata.radius = d.x * transform_ball->scale.x / 2;
		ball_metadata.original_position = ball_translation;
		ball_metadata.current_position = ball_translation;

		glm::mat4 test_trans = transform_plane->make_world_to_local();
		ball_metadata.original_position_local = test_trans * glm::vec4(ball_metadata.original_position,1);
	}

	update_ball_to_plane();

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			x_pos.downs += 1;
			x_pos.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			x_neg.downs += 1;
			x_neg.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			y_neg.downs += 1;
			y_neg.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			y_pos.downs += 1;
			y_pos.pressed = true;
			return true;
		}else if (evt.key.keysym.sym == SDLK_q){
			z_neg.downs += 1;
			z_neg.pressed = true;
			return true;
		}else if (evt.key.keysym.sym == SDLK_e){
			z_pos.downs += 1;
			z_pos.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			x_pos.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			x_neg.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			y_neg.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			y_pos.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) {
			z_neg.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_e) {
			z_pos.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	static glm::vec3 x_axis_world{1.0,0.0,0.0};
	static glm::vec3 y_axis_world{0.0,1.0,0.0};
	static glm::vec3 z_axis_world{0.0,0.0,1.0};

	// Rotate the plane
	{
		glm::vec3 rotate(0.0f);
		if (x_neg.pressed && !x_pos.pressed) rotate.x = -1.0f;
		if (!x_neg.pressed && x_pos.pressed) rotate.x = 1.0f;
		if (y_neg.pressed && !y_pos.pressed) rotate.y = -1.0f;
		if (!y_neg.pressed && y_pos.pressed) rotate.y = 1.0f;
		if (z_neg.pressed && !z_pos.pressed) rotate.z = -1.0f;
		if (!z_neg.pressed && z_pos.pressed) rotate.z = 1.0f;

		auto x_rotation = glm::angleAxis(glm::radians(rotate.x * 50.0f * elapsed),x_axis_world);
		auto y_rotation = glm::angleAxis(glm::radians(rotate.y * 50.0f * elapsed),y_axis_world);
		auto z_rotation = glm::angleAxis(glm::radians(rotate.z * 50.0f * elapsed),z_axis_world);
		transform_plane->rotation = transform_plane->rotation * x_rotation;
		transform_plane->rotation = transform_plane->rotation * y_rotation;
		transform_plane->rotation = transform_plane->rotation * z_rotation;
	}


	update_ball_to_plane();
	handle_ball(elapsed);

	{
		//Write ball transformation
		transform_ball->position = ball_metadata.current_position;
		// Ball Rotation?
		// This is some weird rotation based on intuition
		// Can not figure out the physics-based rotation
		glm::mat4 trans_plane = transform_plane->make_local_to_world();
		glm::vec3 x_axis_plane = trans_plane * glm::vec4{x_axis_world,0};
		glm::vec3 y_axis_plane = trans_plane * glm::vec4{y_axis_world,0};
		auto speed = ball_metadata.plane_speed;

		auto x_rotation = glm::angleAxis(glm::radians(-speed.x * 100.0f * elapsed),x_axis_plane);
		auto y_rotation = glm::angleAxis(glm::radians(-speed.y * 100.0f * elapsed),y_axis_plane);
		transform_ball->rotation = transform_ball->rotation * x_rotation;
		transform_ball->rotation = transform_ball->rotation * y_rotation;
	}


	


	if(success){
		end = std::chrono::system_clock::now();
		elapsed_time = end - *start;
		if(elapsed_time.count() > exit_timer){
			Mode::set_current(nullptr);
		}
	}



	//reset button press counters:
	x_neg.downs = 0;
	x_pos.downs = 0;
	y_neg.downs = 0;
	y_pos.downs = 0;
	z_neg.downs = 0;
	z_pos.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		std::string text;

		if(!success){
			text = "Mouse motion rotates camera; WASDQE rotates the maze; escape ungrabs mouse";
		}else{
			char out[100];
			auto length = sprintf(out,"Congrats! You Win! Game will end in %.1f seconds",exit_timer - elapsed_time.count());
			text = std::string(out,length);
		}

		constexpr float H = 0.09f;
		lines.draw_text(text,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(text,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

}


// Update ball location and bbox to plane spae
// This function does not update any world position!
void PlayMode::update_ball_to_plane(){
	//* way to transfer sphere to plane space!

	glm::mat4 test_trans = transform_plane->make_world_to_local();
	// glm::mat4 test_ball_trans = transform_ball->make_local_to_world();
	// // This is the way to transform ball location to plane space
	// auto local_pos_ball = test_trans * test_ball_trans;
	// or
	auto local_pos_ball_v = test_trans * glm::vec4(ball_metadata.current_position,1);
	
	ball_metadata.current_position_local = local_pos_ball_v;
	ball_metadata.bbox_world.min = ball_metadata.current_position - ball_metadata.radius;
	ball_metadata.bbox_world.max = ball_metadata.current_position + ball_metadata.radius;
	ball_metadata.bbox_plane.min = local_pos_ball_v - ball_metadata.radius;
	ball_metadata.bbox_plane.max = local_pos_ball_v + ball_metadata.radius;
}


// All colllision check happens in "PLANE SPACE"
// block_location is indicating what value the sphere center's location should be set to
std::array<std::pair<bool,bool>,3> PlayMode::ball_collision_status(std::array<std::pair<float,float>,3> & block_location){
	auto ball_plane_pos = ball_metadata.current_position_local;
	auto radius = ball_metadata.radius;

	// Use sphere-AABB collision test?
	// ret_val : [0] true,collision
	// https://developer.mozilla.org/en-US/docs/Games/Techniques/3D_collision_detection
	auto check_collision = [ball_plane_pos,radius](Scene::Bbox bbox)-> std::array<std::pair<bool,bool>,3>{
		float x = std::max(bbox.min.x,std::min(bbox.max.x,ball_plane_pos.x));
		float y = std::max(bbox.min.y,std::min(bbox.max.y,ball_plane_pos.y));
		float z = std::max(bbox.min.z,std::min(bbox.max.z,ball_plane_pos.z));

		auto close_to = [](float x,float x0)->bool{
			if((x - x0) > -0.05 && (x - x0) < 0.05 )
				return true;
			else
				return false;
		};

		auto distance = std::sqrt(
			(x-ball_plane_pos.x) * (x-ball_plane_pos.x) +
			(y-ball_plane_pos.y) * (y-ball_plane_pos.y) +
			(z-ball_plane_pos.z) * (z-ball_plane_pos.z)
		);

		std::array<std::pair<bool,bool>,3>ret{std::make_pair(false,false)};

		if(distance > radius + 0.05){
			return ret;
		}else{
			// Check the collision direction
			// Six possibility
			if(close_to(x,bbox.max.x)){
				ret[0].first = true;
				ret[0].second = true;
			}else if(close_to(x,bbox.min.x)){
				ret[0].first = true;
				ret[0].second = false;				
			}else if(close_to(y,bbox.max.y)){
				ret[1].first = true;
				ret[1].second = true;
			}else if(close_to(y,bbox.min.y)){
				ret[1].first = true;
				ret[1].second = false;
			}else if(close_to(z,bbox.max.z)){
				ret[2].first = true;
				ret[2].second = true;
			}else if(close_to(z,bbox.min.z)){
				ret[2].first = true;
				ret[2].second = false;
			}else{
				std::runtime_error("Collision bug");
			}

			return ret;
		}
	};

	std::array<std::pair<bool,bool>,3> collision_flag{std::make_pair(false,false)};

	for(auto bbox : scene.bboxes){
		auto ret = check_collision(bbox);
		if(ret[0].first){
			if(!collision_flag[0].first){
				collision_flag[0].first = true;
				collision_flag[0].second = ret[0].second;
				//set blocked location
				if(collision_flag[0].second){
					block_location[0].first = bbox.max.x + radius + 0.02;
				}else{
					block_location[0].second = bbox.min.x - radius - 0.02;
				}
			}
		}
		if(ret[1].first){
			if(!collision_flag[1].first){
				collision_flag[1].first = true;
				collision_flag[1].second = ret[1].second;
				if(collision_flag[1].second){
					block_location[1].first = bbox.max.y + radius + 0.02;
				}else{
					block_location[1].second = bbox.min.y - radius - 0.02;
				}
			}
		}
		if(ret[2].first){
			if(!collision_flag[2].first){
				collision_flag[2].first = true;
				collision_flag[2].second = ret[2].second;
				if(collision_flag[2].second){
					// Success condition?
					if(bbox.name == "redcube"){
						success = true;
						if(start == nullptr){
							start = new std::chrono::time_point<std::chrono::system_clock>;
							*start = std::chrono::system_clock::now();
						}
					}
					block_location[2].first = bbox.max.z + radius + 0.02;
				}else{
					block_location[2].second = bbox.min.z - radius - 0.02;
				}
			}
		}
	}

	return collision_flag;
}


/*
In plane space, there are only four(+-x,+-y) + one(-z) directions that will 
prevent tha ball from moving. We detect how many  directions are blocked.
The velocity in these directions will always be 0

So first thing is to transform the gravity in plane space
Detect collison, update speed
Transform the speed back to world space
Update speed
*/ 
void PlayMode::handle_ball(float elapsed){

	glm::mat4 trans = transform_plane->make_world_to_local();
	//gravity is a direction, w should be 0
	glm::vec3 plane_gravity = trans * glm::vec4(world_gravity,0);

	//printf("gravity:%f,%f,%f",plane_gravity.x,plane_gravity.y,plane_gravity.z);

	std::array<std::pair<float,float>,3> blocked_location{std::make_pair(-100.f,-100.0f)};

	auto collision_status = ball_collision_status(blocked_location);

	float elapsed_square = elapsed * elapsed;



	// Update location and speed using backward Euler?
	// Assuming there is no block
	// https://gamedev.stackexchange.com/questions/112892/is-this-a-correct-backward-euler-implementation
	// previous plane-space location
	auto x0 = ball_metadata.current_position_local.x;
	auto y0 = ball_metadata.current_position_local.y;
	auto z0 = ball_metadata.current_position_local.z;
	// previous plane-space speed
	auto vx0 = ball_metadata.plane_speed.x;
	auto vy0 = ball_metadata.plane_speed.y;
	auto vz0 = ball_metadata.plane_speed.z;
	// updated plane-space location
	auto x = x0 + vx0 * elapsed + 0.5 * plane_gravity.x * elapsed_square;
	auto y = y0 + vy0 * elapsed + 0.5 * plane_gravity.y * elapsed_square;
	auto z = z0 + vz0 * elapsed + 0.5 * plane_gravity.z * elapsed_square;
	// updated plane-space speed
	auto vx = vx0 + plane_gravity.x * elapsed;
	auto vy = vy0 + plane_gravity.y * elapsed;
	auto vz = vz0 + plane_gravity.z * elapsed;

	
	// If the direction is blocked, the location should be set to close the wall's location

	if(collision_status[0].first){ 
		if(collision_status[0].second){ // x- is blocked
			if(x < x0){
				//x = x0;
				x = blocked_location[0].first;
				vx = 0;
			}
		}else{ // x+ is blocked
			if(x > x0){
				x = blocked_location[0].second;
				vx = 0;
			}
		}
	}

	if(collision_status[1].first){ 
		if(collision_status[1].second){ // y- is blocked
			if(y < y0){
				y = blocked_location[1].first;
				vy = 0;
			}
		}else{ // y+ is blocked
			if(y > y0){
				y = blocked_location[1].second;
				vy = 0;
			}
		}
	}	
			

	if(collision_status[2].first){ 
		if(collision_status[2].second){ // z- is blocked
			if(z < z0){
				z = blocked_location[2].first;
				vz = 0;
			}
		}else{ // z+ is blocked
			if(z > z0){
				z = blocked_location[2].second;
				vz = 0;
			}
		}
	}

	

	ball_metadata.current_position_local = glm::vec3(x,y,z);
	ball_metadata.plane_speed = glm::vec3(vx,vy,vz);

	// Now update world space location and speed;
	glm::mat4 trans_inv = transform_plane->make_local_to_world();

	glm::vec3 updated_world_position = trans_inv * glm::vec4(ball_metadata.current_position_local,1);
	glm::vec3 updated_world_speed = trans_inv * glm::vec4(ball_metadata.plane_speed,0);

	ball_metadata.current_position = updated_world_position;
	ball_metadata.world_speed = updated_world_speed;

	// If ball is too far, return it to original position
	if(
		ball_metadata.current_position_local.x < -12 || 
		ball_metadata.current_position_local.x > 12  ||
		ball_metadata.current_position_local.y < -12 ||
		ball_metadata.current_position_local.y > 12  ||
		ball_metadata.current_position_local.z > 10  ||
		ball_metadata.current_position_local.z < -10
	){
		//Reset
		ball_metadata.current_position = ball_metadata.original_position;
		ball_metadata.current_position.z += 5;

		ball_metadata.plane_speed = glm::vec3{0.0f};
		ball_metadata.world_speed = glm::vec3{0.0f};
		update_ball_to_plane();
	}

	// Is this needed here?
	//update_ball_to_plane();

	return;

}
