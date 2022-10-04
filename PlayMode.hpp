#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>


//testing IK: leg
struct Leg {
	//basic transformations:
	Scene::Transform *hip = nullptr;
	Scene::Transform *knee = nullptr;
	Scene::Transform *ankle = nullptr;
	//length of leg segments:
	float length_a = 0.0f;
	float length_b = 0.0f;
	float length_c = 0.0f;
	//supplementary angles for IK:
	float angleA = 0.0f;
	float angleB = 0.0f;
	//make a default constructor:
	Leg() = default;
	//make a constructor that takes in the three joints:
	Leg(Scene::Transform *hip_, Scene::Transform *knee_, Scene::Transform *ankle_) : hip(hip_), knee(knee_), ankle(ankle_) {
		//compute lengths of leg segments:
		length_a = glm::length(hip->getWorldPosition() - knee->getWorldPosition());
		length_b = glm::length(knee->getWorldPosition() - ankle->getWorldPosition());
	}

	//update function to move the leg to a given position:
	void update(glm::vec3 const &target);
	//debug print function:
	void printEverything();
};

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, hip, knee;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	Leg test_leg;

	float timer = 0.0f;
};
