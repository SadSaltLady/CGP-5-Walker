#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint my_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > my_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("testworld.pnct"));
	my_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > my_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("testworld.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = my_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = my_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > my_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("testworld.w"));
	walkmesh = &ret->lookup("Plane");
	return ret;
});

PlayMode::PlayMode() : scene(*my_scene) {

	//load assests
	//store the leg parts in a vector
	std::vector<Scene::Transform *> leg_parts(3);
	for (auto &transform : scene.transforms) {
		if ( transform.name == "leg_u") leg_parts[0] = &transform;
		else if (transform.name == "leg_l") leg_parts[1] = &transform;
		else if (transform.name == "feet") leg_parts[2] = &transform;
	}
	if (leg_parts[0] == nullptr) throw std::runtime_error("male BIRD not found.");
	if (leg_parts[1] == nullptr) throw std::runtime_error("female BIRD not found.");
	if (leg_parts[2] == nullptr) throw std::runtime_error("heart not found.");
	//construct leg 
	test_leg = Leg(leg_parts[0], leg_parts[1], leg_parts[2]);

	//-------------------------------------------------------------
	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();
	//modify the transform to place it at the start point:
	player.transform->position = test_leg.hip->position + glm::vec3(0.0f, -10.0f, 0.0f);

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 1.8f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);


	//print out location of test leg and joints
	test_leg.printEverything();
		//testing leg ik
	test_leg.update(glm::vec3(5.5f, 0.0f, 0.0f));

	test_leg.printEverything();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} 
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
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
			glm::vec3 upDir = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, upDir) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//testing if leg ik works
	timer += elapsed;
	if (timer > 5.0f) {
		timer = 0.0f;
	}

	float percent = timer / 5.0f;
	glm::vec3 target = glm::mix(glm::vec3(6.f, -1.0f, 0.0f), glm::vec3(3.f, -1.0f, 0.0f), percent);
	test_leg.update(target);
	//player walking:
	{		
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

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

	scene.draw(*player.camera);

	/* In case you are wondering if your walkmesh is lining up with your scene, try:
	{
		glDisable(GL_DEPTH_TEST);
		DrawLines lines(player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()));
		for (auto const &tri : walkmesh->triangles) {
			lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		}
	}
	*/

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

//take target (in world space) and updates the ankle to target
void Leg::update(glm::vec3 const &target) {
	/*referenced this tutorial:
	https://www.alanzucconi.com/2018/05/02/ik-2d-1/ */
	//given a position, update the transforms to reach that position:
	glm::vec3 hip_world = hip->getWorldPosition();
	length_c = glm::length(target - hip_world);
	std::cout << "length_c: " << length_c << std::endl;
	//account for the case where target is too far away:
	//angle from hip to target (in radians)
	float theta = atan2(target.y - hip_world.y, target.x - hip_world.x);
	std::cout << "theta: " << theta << std::endl;
	//too far away
	if ( length_a + length_b < length_c) {
		std::cout << "too far away" << std::endl;
		angleA = theta;
		angleB = 0.0f;
	} 
	else
	{
		std::cout << "not too far away" << std::endl;
		//inner angle Alpha (in radians)
		float thing = (length_a * length_a + length_c * length_c - length_b * length_b);
		std::cout << "thing: " << thing << std::endl;
		float thing2 = (2 * length_a * length_c);
		std::cout << "thing2: " << thing2 << std::endl;
		float thing3 = thing / thing2;
		std::cout << "thing3: " << thing3 << std::endl;
		float alpha = acos((length_a * length_a + length_c * length_c - length_b * length_b) / (2.0f * length_a * length_c));
		//inner angle beta (in radians)
		float beta = acos((length_a * length_a + length_b * length_b - length_c * length_c) / (2.0f * length_a * length_b));
		std::cout << "alpha: " << alpha << std::endl;
		std::cout << "beta: " << beta << std::endl;

		//calculate the respective angles to be applied to the joints:
		angleA = theta - alpha; // of is it theta + alpha?
		angleB = (float)M_PI - beta;
	}

	//update the transforms:
	hip->rotation = glm::angleAxis(angleA, glm::vec3(0.0f, 1.0f, 0.0f));
	knee->rotation = glm::angleAxis(angleB, glm::vec3(0.0f, 1.0f, 0.0f));

}

void Leg::printEverything() {
	std::cout << "---------------------" << std::endl;
	//find world position 
	glm::vec3 hipworld = hip->make_local_to_world() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec3 kneeworld = knee->make_local_to_world() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec3 footworld = ankle->make_local_to_world() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	//print world position
	std::cout << "hipworld: " << hipworld.x << ", " << hipworld.y << ", " << hipworld.z << std::endl;
	std::cout << "kneeworld: " << kneeworld.x << ", " << kneeworld.y << ", " << kneeworld.z << std::endl;
	std::cout << "footworld: " << footworld.x << ", " << footworld.y << ", " << footworld.z << std::endl;
	//so what the fuck are these
	/*
	std::cout << "hip position: " << hip->position.x << ", " << hip->position.y << ", " << hip->position.z << std::endl;
	std::cout << "knee position: " << knee->position.x << ", " << knee->position.y << ", " << knee->position.z << std::endl;
	std::cout << "ankle position: " << ankle->position.x << ", " << ankle->position.y << ", " << ankle->position.z << std::endl;
	*/
	std::cout << "angleA: " << angleA << std::endl;
	std::cout << "angleB: " << angleB << std::endl;
	std::cout << "length_a: " << length_a << std::endl;
	std::cout << "length_b: " << length_b << std::endl;
	std::cout << "---------------------" << std::endl;
}