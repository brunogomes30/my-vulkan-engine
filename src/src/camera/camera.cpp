#include<camera/camera.h>

#include <iostream>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

bool leftMousePressed = false;
bool shiftPressed = false;

void Camera::update()
{
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e)
{
    const float speed = shiftPressed ? 3 : 1;
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = -speed; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = speed; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = -speed; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = speed; }
        if (e.key.keysym.sym == SDLK_LSHIFT) { shiftPressed = true; }
    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_LSHIFT) { shiftPressed = false;}
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        if(e.button.button == SDL_BUTTON_LEFT) {
            leftMousePressed = true;
        }
    }
    if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftMousePressed = false;
        }
    }
    if (e.type == SDL_MOUSEMOTION && leftMousePressed) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}

glm::mat4 Camera::getViewMatrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::init() {
    velocity = glm::vec3(0.f);
    position = glm::vec3(2.f, 2.f, 10.f);

    pitch = 0;
    yaw = 0;
}