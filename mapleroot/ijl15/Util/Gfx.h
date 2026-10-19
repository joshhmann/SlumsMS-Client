#pragma once

#include <cstdint>
#include "../Funcs.h"

const auto GFX_RESET_COUNTER = 2;
void ResetGfx();
void Hook_CStage_OnSetField(bool enable);

void EnableLFH();