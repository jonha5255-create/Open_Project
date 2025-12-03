#pragma once

#include <vector>
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>



// 전기 투사체
struct Electricity {
    glm::vec3 position;
    glm::vec3 direction;
    float speed;
    float lifetime;
    float maxLifetime;
    float radius;
    bool active;
};

// 플레이어 경직 상태
struct PlayerStun {
    bool isStunned;
    float stunDuration;
    float stunTimer;
};

namespace Enemy {
    // 초기화
    bool initOctopus(const char* objPath, GLuint shaderProg);

    // 매 프레임 업데이트 (플레이어 위치 전달)
    void updateOctopus(const glm::vec3& playerPos, float dt);

    // 렌더링
    void drawOctopus();

    // 전기 업데이트 및 렌더링
    void updateElectricity(float dt);
    void drawElectricity();

    // 충돌 검사 (플레이어와 전기 충돌 반환)
    bool checkElectricityCollision(const glm::vec3& playerPos, float playerRadius, PlayerStun& outStun);

    // 정리
    void cleanup();
}