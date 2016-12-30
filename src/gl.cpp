/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 * Copyright (C) 2013 Fredrik Hultin.
 * Copyright (C) 2013 Jakob Bornecrantz.
 * Distributed under the Boost 1.0 licence, see LICENSE for full text.
 */

/* OpenGL Test - GL Helper Functions Implementation */

#include <string.h>
#include <math.h>
#include <mutex>

#include "gl.h"
#include "player.h"

#ifdef __unix
#include <signal.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265359
#endif

GLuint texture[1];


void init_gl(gl_ctx* ctx, int w, int h)
{
	memset(ctx, 0, sizeof(gl_ctx));

	// == Initialize SDL ==
	int ret = SDL_Init(SDL_INIT_EVERYTHING);
	if(ret < 0){
		printf("SDL_Init failed\n");
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);

	ctx->screen = SDL_SetVideoMode(w, h, 0, SDL_OPENGL | SDL_GL_DOUBLEBUFFER);
	if(ctx->screen == NULL){
		printf("SDL_SetVideoMode failed\n");
		exit(-1);
	}

	// Disable ctrl-c catching on linux (and OS X?) 
	#ifdef __unix
	signal(SIGINT, SIG_DFL);
	#endif

	// Load extensions.
	glewInit();

	printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
	printf("OpenGL Version: %s\n", glGetString(GL_VERSION)); 

	// == Initialize OpenGL ==
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);

	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glEnable(GL_POLYGON_SMOOTH); 

	glViewport(0, 0, ctx->screen->w, ctx->screen->h);

    TTF_Init();
}

void draw_screen(float f_screenAR, float f_screenPos)
{
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        // Alpha null to tell the shader to apply the texture.
        glColor4f(0.f, 0.f, 0.f, 0.f);

        /* The width of the screen is 1.
         * We compute its height in function of the video AR. */

        float h = 1 / f_screenAR;

	glBegin(GL_QUADS);

        glTexCoord2f(0.0, 1.0);
        glVertex3f( -0.5f, -h / 2, f_screenPos);

        glTexCoord2f(0.0, 0.0);
        glVertex3f( -0.5f,  h / 2, f_screenPos);

        glTexCoord2f(1.0, 0.0);
        glVertex3f(  0.5f,  h / 2, f_screenPos);

        glTexCoord2f(1.0, 1.0);
        glVertex3f(  0.5f, -h / 2, f_screenPos);

	glEnd();
}


void initTexture()
{
    /* Create The Texture */
    glGenTextures(1, texture);

    /* Typical Texture Generation Using Data From The Bitmap */
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    printf("texture[0]: %d\n", texture[0]);

    /* Linear Filtering */
    glEnable(GL_TEXTURE_2D);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


void updateTexture(Player *p)
{
    std::lock_guard<std::mutex> lock(p->mutex);

    /* Generate The Texture */
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, p->i_width,
                 p->i_height, 0, GL_BGR,
                 GL_UNSIGNED_BYTE, p->p_imgData);
}


static void compile_shader_src(GLuint shader, const char* src)
{
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint status;
	GLint length;
	char log[4096] = {0};

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	glGetShaderInfoLog(shader, 4096, &length, log);
	if(status == GL_FALSE){
		printf("compile failed %s\n", log);
	}
}

GLuint compile_shader(const char* vertex, const char* fragment)
{
	// Create the handels
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint programShader = glCreateProgram();

	// Attach the shaders to a program handel.
	glAttachShader(programShader, vertexShader);
	glAttachShader(programShader, fragmentShader);

	// Load and compile the Vertex Shader
	compile_shader_src(vertexShader, vertex);

	// Load and compile the Fragment Shader
	compile_shader_src(fragmentShader, fragment);

	// The shader objects are not needed any more,
	// the programShader is the complete shader to be used.
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glLinkProgram(programShader);

	GLint status;
	GLint length;
	char log[4096] = {0};

	glGetProgramiv(programShader, GL_LINK_STATUS, &status);
	glGetProgramInfoLog(programShader, 4096, &length, log);
	if(status == GL_FALSE){
		printf("link failed %s\n", log);
	}

	return programShader;
}

void create_fbo(int eye_width, int eye_height, GLuint* fbo, GLuint* color_tex, GLuint* depth_tex)
{
	glGenTextures(1, color_tex);
	glGenTextures(1, depth_tex);
	glGenFramebuffers(1, fbo);

	glBindTexture(GL_TEXTURE_2D, *color_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, eye_width, eye_height, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, *depth_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, eye_width, eye_height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, *fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *color_tex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, *depth_tex, 0);

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	if(status != GL_FRAMEBUFFER_COMPLETE_EXT){
		printf("failed to create fbo %x\n", status);
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}
