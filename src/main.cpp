#include "core/editor.h"
#include "util/log.h"

int main() {
    VSTIR::Editor::Initialize(1280, 720);
    VSTIR::Editor::Run();
    VSTIR::Editor::Clean();
    INFO("See ya, space cowboy!"); // EOL message for sanity check on silent segfaults
}
