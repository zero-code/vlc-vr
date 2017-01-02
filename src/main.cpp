/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 * Copyright (C) 2013 Fredrik Hultin.
 * Copyright (C) 2013 Jakob Bornecrantz.
 * Distributed under the Boost 1.0 licence, see LICENSE for full text.
 */

/* OpenGL Test - Main Implementation */

#include <openhmd.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <iostream>

#include <vlc/vlc.h>

#include "gl.h"
#include "player.h"
#include <userinterface.h>
#include <playercontroller.h>

#define TEST_WIDTH 2160
#define TEST_HEIGHT 1200

#define EYE_WIDTH (TEST_WIDTH / 2 * 2)
#define EYE_HEIGHT (TEST_HEIGHT * 2)

char* read_file(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char *)calloc(1, len + 1);
    assert(buffer);

    size_t ret = fread(buffer, len, 1, f);
    assert(ret);

    fclose(f);

    return buffer;
}


void usage()
{
    std::cout << "Usage: vlc-vr media_path" << std::endl;
}


int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage();
        return 1;
    }

    ohmd_context* ctx = ohmd_ctx_create();
    int num_devices = ohmd_ctx_probe(ctx);
    printf("%d devices found.\n", num_devices);
    if(num_devices < 0){
        printf("failed to probe devices: %s\n", ohmd_ctx_get_error(ctx));
        return 1;
    }

    ohmd_device_settings* settings = ohmd_device_settings_create(ctx);

    // If OHMD_IDS_AUTOMATIC_UPDATE is set to 0, ohmd_ctx_update() must be called at least 10 times per second.
    // It is enabled by default.

    int auto_update = 1;
    ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update);

    ohmd_device* hmd = ohmd_list_open_device_s(ctx, 0, settings);

    ohmd_device_settings_destroy(settings);

    if(!hmd){
        printf("failed to open device: %s\n", ohmd_ctx_get_error(ctx));
        return 1;
    }

    Player *p = new Player();

    gl_ctx gl;
    init_gl(&gl, TEST_WIDTH, TEST_HEIGHT);

    SDL_ShowCursor(SDL_DISABLE);

    char* vertex = read_file("../shaders/test1.vert.glsl");
    char* fragment = read_file("../shaders/test1.frag.glsl");

    char* vertex2 = read_file("../shaders/test1.vert2.glsl");
    char* fragment2 = read_file("../shaders/test1.frag2.glsl");

    GLuint shader = compile_shader(vertex, fragment);
    GLuint shader2 = compile_shader(vertex2, fragment2);

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "warpTexture"), 0);
    glUseProgram(shader2);
    glUniform1i(glGetUniformLocation(shader2, "myTexture"), 0);

    GLuint left_color_tex = 0, left_depth_tex = 0, left_fbo = 0;
    create_fbo(EYE_WIDTH, EYE_HEIGHT, &left_fbo, &left_color_tex, &left_depth_tex);

    GLuint right_color_tex = 0, right_depth_tex = 0, right_fbo = 0;
    create_fbo(EYE_WIDTH, EYE_HEIGHT, &right_fbo, &right_color_tex, &right_depth_tex);

    /* User interface */
    UserInterface<PlayerController> intf(-0.2, -0.2, -0.4, 0.4, 0.1);
    UserInterface<PlayerController> intfScreen(-0.5, -0.5, -2.f, 1.f, 1.f);

    auto *play = new Button<PlayerController>(0.02, 0.02, 0.05, 0.05, "play.png");
    auto *pause = new Button<PlayerController>(0.02, 0.02, 0.05, 0.05, "pause.png");
    auto *slider = new Slider<PlayerController>(0.05, 0.09, 0.3, 0.01);
    auto *curTime = new Label<PlayerController>(0.01, 0.085, 14, "");
    auto *length = new Label<PlayerController>(0.355, 0.085, 14, "");
    auto *zoomIn = new Button<PlayerController>(0.34, 0.02, 0.02, 0.02, "../zoom_in.png");
    auto *zoomOut = new Button<PlayerController>(0.365, 0.02, 0.02, 0.02, "../zoom_out.png");


    auto *screen = new Screen<PlayerController>(0, 0, 1, 1);

    intf.addControl(play);
    intf.addControl(pause);
    intf.addControl(slider);
    intf.addControl(curTime);
    intf.addControl(length);
    intf.addControl(zoomIn);
    intf.addControl(zoomOut);

    intfScreen.addControl(screen);

    PlayerController c(slider, p, play, pause, curTime, length, &intfScreen);

    intf.setController(&c);

    p->setOnPositionChangedCallback(&c, &PlayerController::positionChanged);
    p->setPlayingCallback(&c, &PlayerController::playing);
    p->setPausedCallback(&c, &PlayerController::paused);
    p->setTimeChangedCallback(&c, &PlayerController::timeChanged);
    p->setLengthChangedCallback(&c, &PlayerController::lengthChanged);
    play->setOnCLickCallback(&PlayerController::playClick);
    pause->setOnCLickCallback(&PlayerController::pauseClick);
    slider->setOnUserChangedValueCallback(&PlayerController::userChangedPosition);
    slider->setOnLockCallback(&PlayerController::sliderLocked);
    slider->setOnUnlockCallback(&PlayerController::sliderUnlocked);
    zoomIn->setOnCLickCallback(&PlayerController::zoomIn);
    zoomOut->setOnCLickCallback(&PlayerController::zoomOut);

    p->startPlayback(argv[1]);


    bool done = false;
    float f_screenPos = -2.f;
    while(!done){
        ohmd_ctx_update(ctx);

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_KEYDOWN){
                switch(event.key.keysym.sym){
                case SDLK_ESCAPE:
                    done = true;
                    break;
                case SDLK_F1:
                    SDL_WM_ToggleFullScreen(gl.screen);
                    break;
                case SDLK_F2:
                    {
                        // reset rotation and position
                        float zero[] = {0, 0, 0, 1};
                        ohmd_device_setf(hmd, OHMD_ROTATION_QUAT, zero);
                        ohmd_device_setf(hmd, OHMD_POSITION_VECTOR, zero);
                    }
                    break;
                case SDLK_SPACE:
                    intf.clickEvent();
                default:
                    break;
                }
            }
        }

        // Focus with the pointer
        intf.pointerFocus(hmd);
        intfScreen.pointerFocus(hmd);

        // Update screen texture
        screen->updateTexture(p);

        // Update the size of the screen.
        {
            float f_ar = (float)p->i_width / p->i_height;
            intfScreen.setSize(1.f, 1 / f_ar);
            screen->setSize(1.f, 1 / f_ar);
            float x, y, z;
            intfScreen.getPosition(x, y, z);
            intfScreen.setPosition(-0.5f, -1 / f_ar / 2.f, z);
        }

        // Common scene state
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        float matrix[16];

        // set hmd rotation, for left eye.
        glMatrixMode(GL_PROJECTION);
        ohmd_device_getf(hmd, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, matrix);
        glLoadMatrixf(matrix);

        glMatrixMode(GL_MODELVIEW);
        ohmd_device_getf(hmd, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, matrix);
        glLoadMatrixf(matrix);

        // Draw scene into framebuffer.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, left_fbo);
        glViewport(0, 0, EYE_WIDTH, EYE_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        intf.draw();
        intfScreen.draw();
        intf.drawPointer(hmd);


        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);


        // set hmd rotation, for right eye.
        glMatrixMode(GL_PROJECTION);
        ohmd_device_getf(hmd, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, matrix);
        glLoadMatrixf(matrix);

        glMatrixMode(GL_MODELVIEW);
        ohmd_device_getf(hmd, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX, matrix);
        glLoadMatrixf(matrix);

        // Draw scene into framebuffer.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, right_fbo);
        glViewport(0, 0, EYE_WIDTH, EYE_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        intf.draw();
        intfScreen.draw();
        intf.drawPointer(hmd);

        // Clean up common draw state
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);


        // Setup ortho state.
        glUseProgram(shader);
        glViewport(0, 0, TEST_WIDTH, TEST_HEIGHT);
        glEnable(GL_TEXTURE_2D);
        glColor4d(1, 1, 1, 1);

        // Setup simple render state
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw left eye
        glBindTexture(GL_TEXTURE_2D, left_color_tex);
        glBegin(GL_QUADS);
        glTexCoord2d( 0,  0);
        glVertex3d(  -1, -1, 0);
        glTexCoord2d( 1,  0);
        glVertex3d(   0, -1, 0);
        glTexCoord2d( 1,  1);
        glVertex3d(   0,  1, 0);
        glTexCoord2d( 0,  1);
        glVertex3d(  -1,  1, 0);
        glEnd();

        // Draw right eye
        glBindTexture(GL_TEXTURE_2D, right_color_tex);
        glBegin(GL_QUADS);
        glTexCoord2d( 0,  0);
        glVertex3d(   0, -1, 0);
        glTexCoord2d( 1,  0);
        glVertex3d(   1, -1, 0);
        glTexCoord2d( 1,  1);
        glVertex3d(   1,  1, 0);
        glTexCoord2d( 0,  1);
        glVertex3d(   0,  1, 0);
        glEnd();

        // Clean up state.
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glUseProgram(shader2);

        // Da swap-dawup!
        SDL_GL_SwapBuffers();
        //SDL_Delay(10);
    }

    delete play;
    delete pause;

    ohmd_ctx_destroy(ctx);

    free(vertex);
    free(fragment);
    delete(p);

    return 0;
}
