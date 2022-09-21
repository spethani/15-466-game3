#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("game3.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("game3.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > sample0(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("audacity_sound0.wav"));
});
Load< Sound::Sample > sample1(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("audacity_sound1.wav"));
});
Load< Sound::Sample > sample2(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("audacity_sound2.wav"));
});
Load< Sound::Sample > sample3(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("audacity_sound3.wav"));
});
Load< Sound::Sample > silence(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("silence.wav"));
});

PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointers to cubes for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Cube.000") cube0 = &transform;
		else if (transform.name == "Cube.001") cube1 = &transform;
		else if (transform.name == "Cube.002") cube2 = &transform;
		else if (transform.name == "Cube.003") cube3 = &transform;
	}
	if (cube0 == nullptr) throw std::runtime_error("Cube 0 not found.");
	if (cube1 == nullptr) throw std::runtime_error("Cube 1 not found.");
	if (cube2 == nullptr) throw std::runtime_error("Cube 2 not found.");
	if (cube3 == nullptr) throw std::runtime_error("Cube 3 not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//create target sequence
	// random code taken from https://stackoverflow.com/questions/7560114/random-number-c-in-some-range
	std::random_device rd; // obtain a random number from hardware
	std::mt19937 gen(rd()); // seed the generator
	std::uniform_int_distribution<> distr(0, 3); // define the range
	for (int i = 0; i < starting_sequence_size; i++) {
		uint8_t sample_num = (uint8_t)distr(gen);
		assert(0 <= sample_num && sample_num <= 3);
		target_sequence.push_back(sample_num);
		std::cout << std::to_string(sample_num) << std::endl;
	}
	index_to_play = 0;

	// game state
	game_over = false;
	finished_playing = true;
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
		Sint32 screen_x = evt.button.x;
		Sint32 screen_y = evt.button.y;
		handle_click(screen_x, screen_y);
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

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	// check the sequence
	if (player_sequence.size() == target_sequence.size()) {
		for (int i = 0; i < player_sequence.size(); i++) {
			if (player_sequence[i] != target_sequence[i]) {
				game_over = true;
			}
		}
		if (!game_over) { // All notes played correctly
			// Add to the sequence
			static std::random_device random_device;
			static std::mt19937 generator(random_device()); 
			static std::uniform_int_distribution<> distribution(0, 3);
			uint8_t random_number = (uint8_t) distribution(generator);
			std::cout << std::to_string(random_number) << std::endl;
			target_sequence.push_back(random_number);
		}

		// Clear the player's sequence
		player_sequence.clear();
		assert(player_sequence.size() == 0);

		// Replay sequence of notes from start
		index_to_play = 0;
		finished_playing = false;
		game_over = false;
	}

	// update sound
	if ((finished_playing || current_sample->stopped) && index_to_play < target_sequence.size()) {
		finished_playing = false;
		uint8_t cube_played = target_sequence[index_to_play];
		if (cube_played == 0) {
			current_sample = Sound::play(*sample0);
			index_to_play++;
		}
		else if (cube_played == 1) {
			current_sample = Sound::play(*sample1);
			index_to_play++;
		}
		else if (cube_played == 2) {
			current_sample = Sound::play(*sample2);
			index_to_play++;
		}
		else if (cube_played == 3) {
			current_sample = Sound::play(*sample3);
			index_to_play++;
		}
	}
	else if (current_sample->stopped && index_to_play == target_sequence.size()) {
		finished_playing = true;
	}
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

		constexpr float H = 0.09f;
		std::string message = "Click on the cubes to match the tone sequence. Score: " + std::to_string(target_sequence.size() - starting_sequence_size);
		lines.draw_text(message,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(message,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

void PlayMode::handle_click(Sint32 x, Sint32 y) {
	if (!finished_playing) {
		return;
	}

	if (355 <= x && x <= 453 && 81 <= y && y <= 202) {
		// Cube 0
		current_sample = Sound::play(*sample0);
		finished_playing = false;
		player_sequence.push_back(0);
	}
	else if (229 <= x && x <= 402 && 434 <= y && y <= 594) {
		// Cube 1
		current_sample = Sound::play(*sample1);
		finished_playing = false;
		player_sequence.push_back(1);
	}
	else if (845 <= x && x <= 961 && 434 <= y && y <= 590) {
		// Cube 2
		current_sample = Sound::play(*sample2);
		finished_playing = false;
		player_sequence.push_back(2);
	}
	else if (772 <= x && x <= 862 && 81 <= y && y <= 199) {
		// Cube 3
		current_sample = Sound::play(*sample3);
		finished_playing = false;
		player_sequence.push_back(3);
	}
}
