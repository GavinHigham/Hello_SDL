#ifndef GRAPHICS_H
#define GRAPHICS_H

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
	#include <GLES2/gl2.h>
	#define GL_GEOMETRY_SHADER 42 //Just to get this to compile
#else
	#define OPENGL_BACKEND //Hard-coded to "on" for now, will change when I add sokol backend.
	#ifdef OPENGL_BACKEND
		// #ifdef _WIN32
		//     #define APIENTRY __stdcall
		// #endif
		// #include "glad/glad.h"
		#include <GL/glew.h> //TODO(Gavin): Try a different extension wrangler.

		#define CHECK_ERRORS() {int error = glGetError(); if (error != GL_NO_ERROR) printf("%s[%d]: %d\n", __FUNCTION__, __LINE__, error);}
		#define hframebuffer GLuint
	#endif
#endif

#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>

#endif