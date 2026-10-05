#include "game/Game.h"

bool done = false;

void MainExit() { done = true; }

void ExitWithSendSave() { MainExit(); }
