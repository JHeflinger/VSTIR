#pragma once

#include "core/editor.h"

#define _general (Editor::Get()->GetRenderer().GetBackend().Core().General())
#define _window (Editor::Get()->Window())
#define _metadata (Editor::Get()->GetRenderer().GetBackend().Metadata())
#define _interface (Editor::Get()->GetRenderer().GetBackend().Core().General().Interface())
#define _gpu (Editor::Get()->GetRenderer().GetBackend().Core().General().GPU())
#define _width (Editor::Get()->Width())
#define _height (Editor::Get()->Height())
#define _viewport_width_ratio (Editor::Get()->ViewportWidthRatio())
#define _viewport_height_ratio (Editor::Get()->ViewportHeightRatio())
#define _viewport_width (Editor::Get()->ViewportWidth())
#define _viewport_height (Editor::Get()->ViewportHeight())

#define _camera (Editor::Get()->GetRenderer().GetCamera())

#define _render_width  ((uint32_t)std::max<size_t>(1, (size_t)(_viewport_width  * _render_settings.resolution_scale)))
#define _render_height ((uint32_t)std::max<size_t>(1, (size_t)(_viewport_height * _render_settings.resolution_scale)))
#define _swapchain (Editor::Get()->GetRenderer().Swapchain())
#define _context (Editor::Get()->GetRenderer().GetBackend().Core().Context())
#define _shaders (Editor::Get()->GetRenderer().GetBackend().Core().Shaders())
#define _scheduler (Editor::Get()->GetRenderer().GetBackend().Core().Scheduler())
#define _data (Editor::Get()->GetRenderer().GetBackend().Core().Context().Data())
#define _core (Editor::Get()->GetRenderer().GetBackend().Core())
#define _renderer (Editor::Get()->GetRenderer())
#define _render_settings (Editor::Get()->GetRenderer().GetSettings())
