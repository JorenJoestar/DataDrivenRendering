#pragma once

//
//  Hydra Application v0.22
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/24, 21.50
//
// Files /////////////////////////////////
//
// application.hpp/.cpp, game_application.hpp/.cpp, window.hpp/.cpp
//
// Revision history //////////////////////
//
//      0.22 (2021/10/21): + Moved GameApplication to use Renderer instead of GPU.
//		0.21 (2021/09/28): + Added game application class, with variable and fixed updates and rendering interpolation.
//      0.20 (2021/06/10): + Separated into different files. + Added Window class.
//      0.19 (2021/01/11): + Added option to disable camera input.
//      0.18 (2020/12/29): + Fixed resize callback happening only when size is different from before.
//      0.17 (2020/12/28): + Added app_reload method. + Clearer names for app load/unload.
//      0.16 (2020/12/27): + Fixed camera movement taking only one key at the time.
//      0.15 (2020/12/18): + Added reload resources callback.
//      0.14 (2020/12/16): + Added camera input and camera movement classes.
//      0.13 (2020/09/15): + Fixed passing of CommandBuffer to execution context.
//      0.12 (2020/04/12): + Added renderer to application struct. + Added configuration to enable renderer.
//      0.11 (2020/04/05): + Added optional macros to remove strict dependency to enkits and optick.
//      0.10 (2020/03/09): + Major overhaul. Added different type of apps: command line, sdl+hydra
//      0.01 (2019/09/24): + Initial implementation.
//
// API Documentation /////////////////////////
//
//
// Customization /////////////////////////
//
//      HYDRA_CMD_APP
//      Creates a command line application.
//
//      HYDRA_SDL_APP
//      Creates a barebone SDL + OpenGL application.
//
//      HYDRA_IMGUI_APP
//      Adds support for ImGui.
//
//      HYDRA_APPLICATION_CAMERA
//      Add support for a simple camera system.

