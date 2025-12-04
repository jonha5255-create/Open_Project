#include "character.h"
#include "octopus.h"

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <algorithm>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

static GLuint g_vao = 0;
static GLuint g_vbo = 0;
static GLuint g_ebo = 0;
static GLsizei g_indexCount = 0;
static GLuint g_shaderProg = 0;
static GLint g_modelUniform = -1;
static GLint g_colorUniform = -1;

static glm::vec3 g_position = glm::vec3(0.0f, 0.0f, 0.0f);
static glm::vec3 g_targetPosition = glm::vec3(0.0f, 0.0f, 0.0f);  // 목표 위치

// 변수 추가 (기존 변수들 아래)
static float g_yaw = 0.0f;
static float g_targetYaw = 0.0f;            // 목표 회전 각도
static const float ROTATION_SPEED = 0.15f;  // 회전 속도 (부드러움)

static float g_speed = 3.5f;
static bool g_running = false;
static bool g_jumpRequested = false;
static float g_verticalVel = 0.0f;
static bool g_grounded = true;
static double g_timeTotal = 0.0;
static std::chrono::steady_clock::time_point g_lastTime;

static PlayerStun g_playerStun = { false, 0.5f, 0.0f };

// 로봇 부위별 회전 각도
static float g_leftArmRotation = 0.0f;
static float g_rightArmRotation = 0.0f;
static float g_leftLegRotation = 0.0f;
static float g_rightLegRotation = 0.0f;

// 경계값
static const float BOUNDARY_X_MIN = -4.5f;
static const float BOUNDARY_X_MAX = 4.5f;
static const float BOUNDARY_Z_MIN = -4.5f;
static const float BOUNDARY_Z_MAX = 4.5f;

// 움직임 부드러움 계수 (0~1 사이, 값이 작을수록 더 부드러움)
static const float MOVEMENT_SMOOTHING = 0.15f;

static void mat4_identity(float m[16]) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_translate(float m[16], float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_scale(float m[16], float sx, float sy, float sz) {
    mat4_identity(m);
    m[0] = sx; m[5] = sy; m[10] = sz;
}

static void mat4_rotate_y(float m[16], float radians) {
    mat4_identity(m);
    float c = cosf(radians), s = sinf(radians);
    m[0] = c;  m[2] = s;
    m[8] = -s; m[10] = c;
}

static void mat4_rotate_x(float m[16], float radians) {
    mat4_identity(m);
    float c = cosf(radians), s = sinf(radians);
    m[5] = c;  m[10] = c;
    m[6] = s;  m[9] = -s;
}

static void mat4_mul(const float a[16], const float b[16], float out[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + r] * b[c * 4 + k];
            out[c * 4 + r] = sum;
        }
    }
}

// 선형 보간 함수
static float lerp(float current, float target, float speed) {
    float diff = target - current;
    if (fabsf(diff) < 0.001f) {
        return target;
    }
    return current + diff * speed;
}

// 큐브 정점 생성
static void createCube(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
    vertices.clear();
    indices.clear();

    Vertex cubeVerts[] = {
        // 앞면 (Z+)
        { -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f },
        // 뒷면 (Z-)
        {  0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f },
        { -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f },
        // 왼쪽 (X-)
        { -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f },
        { -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f },
        { -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f },
        // 오른쪽 (X+)
        {  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f },
        {  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f },
        // 윗면 (Y+)
        { -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f },
        // 아랫면 (Y-)
        { -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f },
    };

    unsigned int cubeIndices[] = {
        0, 1, 2, 0, 2, 3,       // 앞면
        4, 5, 6, 4, 6, 7,       // 뒷면
        8, 9, 10, 8, 10, 11,    // 왼쪽
        12, 13, 14, 12, 14, 15, // 오른쪽
        16, 17, 18, 16, 18, 19, // 윗면
        20, 21, 22, 20, 22, 23  // 아랫면
    };

    for (int i = 0; i < 24; i++) {
        vertices.push_back(cubeVerts[i]);
    }
    for (int i = 0; i < 36; i++) {
        indices.push_back(cubeIndices[i]);
    }
}

// 로봇 메시 생성 (팔다리와 머리가 몸통에 자연스럽게 연결됨)
static void createRobotMesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
    vertices.clear();
    indices.clear();

    std::vector<Vertex> tempVerts;
    std::vector<unsigned int> tempIndices;
    unsigned int baseIndex = 0;

    // 1. 로봇 몸체 (중심)
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.4f;
        v.py *= 0.6f;
        v.pz *= 0.3f;
        v.py += 0.6f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 2. 로봇 머리 (몸통 위에 붙음)
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.35f;
        v.py *= 0.35f;
        v.pz *= 0.35f;
        v.py += 1.075f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 3. 왼팔 위쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.12f;
        v.py *= 0.35f;
        v.pz *= 0.12f;
        v.px -= 0.3f;
        v.py += 0.7f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 4. 오른팔 위쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.12f;
        v.py *= 0.35f;
        v.pz *= 0.12f;
        v.px += 0.3f;
        v.py += 0.7f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 5. 왼팔 아래쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.1f;
        v.py *= 0.35f;
        v.pz *= 0.1f;
        v.px -= 0.3f;
        v.py += 0.35f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 6. 오른팔 아래쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.1f;
        v.py *= 0.35f;
        v.pz *= 0.1f;
        v.px += 0.3f;
        v.py += 0.35f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 7. 왼쪽 다리 위쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.13f;
        v.py *= 0.4f;
        v.pz *= 0.13f;
        v.px -= 0.13f;
        v.py += 0.1f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 8. 오른쪽 다리 위쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.13f;
        v.py *= 0.4f;
        v.pz *= 0.13f;
        v.px += 0.13f;
        v.py += 0.1f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 9. 왼쪽 다리 아래쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.12f;
        v.py *= 0.4f;
        v.pz *= 0.12f;
        v.px -= 0.13f;
        v.py += -0.3f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }
    baseIndex = vertices.size();

    // 10. 오른쪽 다리 아래쪽
    createCube(tempVerts, tempIndices);
    for (auto& v : tempVerts) {
        v.px *= 0.12f;
        v.py *= 0.4f;
        v.pz *= 0.12f;
        v.px += 0.13f;
        v.py += -0.3f;
        vertices.push_back(v);
    }
    for (auto idx : tempIndices) {
        indices.push_back(idx + baseIndex);
    }

    g_indexCount = (GLsizei)indices.size();
}

bool Character::initCharacter(const char* objPath, GLuint shaderProg) {
    (void)objPath;

    g_shaderProg = shaderProg;
    g_modelUniform = glGetUniformLocation(shaderProg, "model");
    g_colorUniform = glGetUniformLocation(shaderProg, "objectColor");

    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    createRobotMesh(verts, idx);

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &g_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, px));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

    glBindVertexArray(0);

    g_position = glm::vec3(0.0f, 0.0f, 0.0f);
    g_targetPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    g_yaw = 0.0f;  // 초기값: 0도 (앞 방향)
    g_targetYaw = 0.0f;  // 목표 회전 각도도 0도로 초기화
    g_running = false;
    g_verticalVel = 0.0f;
    g_grounded = true;
    g_timeTotal = 0.0;
    g_lastTime = std::chrono::steady_clock::now();
    g_playerStun.isStunned = false;
    g_playerStun.stunTimer = 0.0f;

    std::cout << "[로봇] 초기화 완료" << std::endl;
    return true;
}

void Character::moveForward(float speed) {
    g_targetPosition.z -= speed;
}

void Character::moveBackward(float speed) {
    g_targetPosition.z += speed;
}

void Character::moveLeft(float speed) {
    g_targetPosition.x -= speed;
}

void Character::moveRight(float speed) {
    g_targetPosition.x += speed;
}

// 변수 추가 (기존 변수들 아래)
static int g_jumpCount = 0;          // 현재 점프 횟수
static const int MAX_JUMPS = 2;       // 최대 점프 횟수 (더블점프)
static const float DOUBLE_JUMP_FORCE = 5.0f;  // 더블점프 힘

// jump 함수 수정
void Character::jump() {
    if (g_jumpCount < MAX_JUMPS) {
        g_jumpRequested = true;
        g_jumpCount++;
    }
}


// setTargetRotation 함수 추가
void Character::setTargetRotation(float angle) {
    g_targetYaw = angle;
}

// drawCharacter 함수 수정 (점프 처리 부분)
void Character::drawCharacter() {
    if (g_vao == 0 || g_indexCount == 0) return;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - g_lastTime).count();
    g_lastTime = now;
    if (dt > 0.1) dt = 0.1;
    g_timeTotal += dt;

    // 기절 상태 업데이트
    if (g_playerStun.isStunned) {
        g_playerStun.stunTimer += dt;
        if (g_playerStun.stunTimer >= g_playerStun.stunDuration) {
            g_playerStun.isStunned = false;
            g_playerStun.stunTimer = 0.0f;
        }
    }

    // 현재 위치를 목표 위치로 부드럽게 이동
    g_position.x = lerp(g_position.x, g_targetPosition.x, MOVEMENT_SMOOTHING);
    g_position.z = lerp(g_position.z, g_targetPosition.z, MOVEMENT_SMOOTHING);

    // 회전 부드럽게 적용
    const float PI = 3.14159265f;
    float diff = g_targetYaw - g_yaw;

    // 차이가 -PI ~ PI (-180도 ~ 180도) 사이가 되도록 보정
    while (diff > PI) diff -= 2 * PI;
    while (diff < -PI) diff += 2 * PI;

    // 최단 경로로 부드럽게 회전
    g_yaw += diff * ROTATION_SPEED;

    // 점프 처리 (더블점프 포함)
    if (g_jumpRequested) {
        if (g_jumpCount == 1) {
            // 첫 점프 (지면에서)
            g_verticalVel = 5.5f;
            g_grounded = false;
        } else if (g_jumpCount == 2) {
            // 더블점프 (공중에서)
            g_verticalVel = DOUBLE_JUMP_FORCE;
        }
        g_jumpRequested = false;
    }

    if (!g_grounded) {
        const float gravity = -9.81f;
        g_verticalVel += gravity * (float)dt;
        g_position.y += g_verticalVel * (float)dt;
        if (g_position.y <= 0.0f) {
            g_position.y = 0.0f;
            g_verticalVel = 0.0f;
            g_grounded = true;
            g_jumpCount = 0;  // 지면에 닿으면 점프 횟수 초기화
            std::cout << "착지! 점프 횟수 초기화" << std::endl;
        }
    }

    // 로봇 팔다리 애니메이션 (걷기 동작)
    float walkAnimSpeed = 6.0f;
    g_leftArmRotation = sinf((float)g_timeTotal * walkAnimSpeed) * 0.5f;
    g_rightArmRotation = -sinf((float)g_timeTotal * walkAnimSpeed) * 0.5f;
    g_leftLegRotation = sinf((float)g_timeTotal * walkAnimSpeed + 3.14159f) * 0.4f;
    g_rightLegRotation = -sinf((float)g_timeTotal * walkAnimSpeed + 3.14159f) * 0.4f;

    float bobOffset = 0.0f;

    float R[16]; mat4_rotate_y(R, g_yaw);
    float T[16]; mat4_translate(T, g_position.x, g_position.y + bobOffset, g_position.z);
    float S[16]; mat4_scale(S, 1.0f, 1.0f, 1.0f);
    float TR[16]; mat4_mul(T, R, TR);
    float TRS[16]; mat4_mul(TR, S, TRS);

    if (g_modelUniform >= 0) {
        glUseProgram(g_shaderProg);
        glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, TRS);
    }

    if (g_colorUniform >= 0) {
        glUniform3f(g_colorUniform, 0.2f, 0.7f, 0.9f);
    }

    glBindVertexArray(g_vao);
    glDrawElements(GL_TRIANGLES, g_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    Enemy::updateOctopus(g_position, (float)dt);
    Enemy::drawOctopus();

    Enemy::updateElectricity((float)dt);
    Enemy::checkElectricityCollision(g_position, 0.5f, g_playerStun);
    Enemy::drawElectricity();
}

void Character::cleanup() {
    if (g_ebo) { glDeleteBuffers(1, &g_ebo); g_ebo = 0; }
    if (g_vbo) { glDeleteBuffers(1, &g_vbo); g_vbo = 0; }
    if (g_vao) { glDeleteVertexArrays(1, &g_vao); g_vao = 0; }
    g_indexCount = 0;
    g_shaderProg = 0;
    g_modelUniform = -1;
    g_colorUniform = -1;
    Enemy::cleanup();
}

glm::vec3 Character::getPosition() {
    return g_position;
}

