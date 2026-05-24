#pragma once

enum SCENE {
	SCENE_TITLE = 0,
	SCENE_GAME,
	SCENE_RESULT,
	SCENE_DEBUG_MODEL,
	SCENE_DEBUG_LIGHTING,
	SCENE_DEBUG_SCORE,
	SCENE_MAX,
	SCENE_NONE,
};

void Init(void);
void Update(void);
void Draw(void);
void Finalize(void);

void SetScene(SCENE id);
SCENE GetScene(void);
