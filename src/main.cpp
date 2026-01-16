#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <ImGui/imgui.h>
#include <glm/glm.hpp>

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Delay(2000);
    SDL_Log("Hello\n");
    SDL_Quit();
}
