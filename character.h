#pragma once
#include <gl/glew.h>
#include <gl/glm/glm.hpp>

struct PlayerStun {
    bool isStunned;
    float stunDuration;
    float stunTimer;
};

namespace Character {
    bool initCharacter(const char* objPath, GLuint shaderProg);
    void drawCharacter();
    void cleanup();

    // 이동 및 액션
    void moveForward(float speed);
    void moveBackward(float speed);
    void moveLeft(float speed);
    void moveRight(float speed);
    void jump();

    // 상태 설정 및 조회
    void setTargetRotation(float angle);
    void setRunning(bool running);

	// 위치 조회
    glm::vec3 getPosition();

    bool isStunned();
    void applyStun(float duration);
}