#pragma once

#include "core/editor.h"

#define _metadata (Editor::Get()->GetRenderer().GetBackend().Metadata())
#define _interface (Editor::Get()->GetRenderer().GetBackend().Core().General().Interface())
#define _gpu (Editor::Get()->GetRenderer().GetBackend().Core().General().GPU())
#define _width (Editor::Get()->Width())
#define _height (Editor::Get()->Height())
#define _swapchain (Editor::Get()->GetRenderer().Swapchain())
#define _context (Editor::Get()->GetRenderer().GetBackend().Core().Context())
#define _shaders (Editor::Get()->GetRenderer().GetBackend().Core().Shaders())
