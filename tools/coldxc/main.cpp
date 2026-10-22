// coldxc: builds .rime packages from content projects. See coldx/content/build.h.

#include <coldx/content/build.h>

int main(int argc, char** argv) {
    return coldx::content::RunTool(argc, argv);
}
