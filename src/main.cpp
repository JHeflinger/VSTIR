#include "core/editor.h"
#include "util/log.h"

int main() {
    VSTIR::Editor editor(1280, 720);
    editor.Run();
    INFO("See ya, space cowboy!"); // EOL message for sanity check on silent segfaults
}
