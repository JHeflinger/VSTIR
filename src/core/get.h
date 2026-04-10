#pragma once

#include "core/editor.h"

#define _general (Editor::Get()->GetRenderer().GetBackend().Core().General())
#define _window (Editor::Get()->Window())
#define _metadata (Editor::Get()->GetRenderer().GetBackend().Metadata())
#define _interface (Editor::Get()->GetRenderer().GetBackend().Core().General().Interface())
#define _gpu (Editor::Get()->GetRenderer().GetBackend().Core().General().GPU())
#define _width (Editor::Get()->Width())
#define _height (Editor::Get()->Height())
#define _swapchain (Editor::Get()->GetRenderer().Swapchain())
#define _context (Editor::Get()->GetRenderer().GetBackend().Core().Context())
#define _shaders (Editor::Get()->GetRenderer().GetBackend().Core().Shaders())
#define _scheduler (Editor::Get()->GetRenderer().GetBackend().Core().Scheduler())
#define _data (Editor::Get()->GetRenderer().GetBackend().Core().Context().Data())
#define _core (Editor::Get()->GetRenderer().GetBackend().Core())
#define _renderer (Editor::Get()->GetRenderer())
