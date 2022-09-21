#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

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
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//cubes:
	Scene::Transform *cube0 = nullptr;
	Scene::Transform *cube1 = nullptr;
	Scene::Transform *cube2 = nullptr;
	Scene::Transform *cube3 = nullptr;

	void PlayMode::handle_click(Sint32 x, Sint32 y);

	std::vector<uint8_t> target_sequence;
	std::vector<uint8_t> player_sequence;
	uint8_t starting_sequence_size = 1;

	//cube sample being played:
	std::shared_ptr< Sound::PlayingSample > current_sample;
	uint8_t index_to_play;
	bool finished_playing;

	// game state
	bool game_over;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
