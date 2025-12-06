#pragma once
#include <vector>
#include <gl/glew.h>
#include <gl/glm/glm.hpp>
#include "character.h"

enum AttackType {
    ATK_LOW_BAR,    // 하단 (1단점프)
    ATK_VERTICAL,    // 세로 (좌우/중앙 이동)
    ATK_HIGH_BAR     //상단
};

// 전기 공격 구조체
struct Electricity {
    glm::vec3 position;
    glm::vec3 direction;
    float speed;
    float lifetime;
    float maxLifetime;
    float radius;
    bool active;
	AttackType type;
};

// 문어 적 관련 함수들
namespace Enemy {
    // OBJ 파일 경로를 인자로 받습니다.
    bool initOctopus(const char* objPath, GLuint shaderProg);

    void updateOctopus(const glm::vec3& playerPos, float dt);
    void drawOctopus();
    void drawElectricity();
    bool checkElectricityCollision(const glm::vec3& playerPos, float playerRadius, PlayerStun& outStun);

    void cleanup();
}