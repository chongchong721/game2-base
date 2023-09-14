#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <chrono>




struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void handle_ball(float elapsed);
	void update_ball_to_plane();
	std::array<std::pair<bool,bool>,3> ball_collision_status(std::array<std::pair<float,float>,3> &);

	//----- game state -----

	//input tracking:

	// left 
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} x_neg, x_pos, y_neg, y_pos, z_neg,z_pos;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	struct{
		glm::vec3 original_position; // original position in world space
		glm::vec3 original_position_local; // original position in plane space
		glm::vec3 current_position;	// current position in world space
		glm::vec3 current_position_local; // current position in plane space
		float radius;
		//Axie aligned bbox in world space
		struct{
			glm::vec3 min;
			glm::vec3 max;
		} bbox_world;
		//Axis aligned bbox in plane space
		struct{
			glm::vec3 min;
			glm::vec3 max;
		}bbox_plane;
		glm::vec3 world_speed{0.0f};
		glm::vec3 plane_speed{0.0f};
	}ball_metadata;

	//Ball to move
	Scene::Transform *transform_ball = nullptr;
	// Plane to rotate
	Scene::Transform *transform_plane = nullptr;
	glm::quat ball_rotation;
	glm::quat plane_rotation;
	glm::vec3 ball_translation;
	float wobble = 0.0f;

	Scene::Bbox *plane_bbox = nullptr;

	
	//camera:
	Scene::Camera *camera = nullptr;


	// Speed
	glm::vec3 world_gravity{0.0f,0.0f,-2.0f};

	// Success flag
	bool success = false;
	float exit_timer = 5.0f;
	std::chrono::time_point<std::chrono::system_clock> * start = nullptr;
	std::chrono::time_point<std::chrono::system_clock> end;
	std::chrono::duration<double> elapsed_time;

};
