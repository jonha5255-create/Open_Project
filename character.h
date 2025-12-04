#pragma once
#include <vector>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

namespace Character {
    bool initCharacter(const char* objPath, GLuint shaderProg);
    void drawCharacter();
    void cleanup();
    
    // 로봇 움직임 제어
    void moveForward(float speed);
    void moveBackward(float speed);
    void moveLeft(float speed);
    void moveRight(float speed);
    void jump();
    void setRunning(bool running);

    // 로봇 회전 제어
    void setTargetRotation(float angle);
    
    // 로봇 위치 가져오기
    glm::vec3 getPosition();
}
