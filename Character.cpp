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
	float r, g, b;
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

// [가속도 시스템 변수 추가]
static float g_currentSpeed = 0.0f;       // 현재 실제 속도
static const float MIN_SPEED = 0.0f;      // 정지 상태
static const float MAX_SPEED = 0.3f;      // 최대 속도 (너무 빠르면 조작 어려움)
static const float ACCELERATION = 0.025f;   // 가속도 (초당 증가량)
static const float FRICTION = 3.5f;       // 마찰력 (키 뗐을 때 멈추는 속도)

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
static const float Bondray_Limit = 4.5f;

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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    

    glBindVertexArray(0);

    g_position = glm::vec3(0.0f, 20.0f, 0.0f);
    g_targetPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    g_yaw = 0.0f;  // 초기값: 0도 (앞 방향)
    g_targetYaw = 0.0f;  // 목표 회전 각도도 0도로 초기화
    g_running = false;
    g_verticalVel = 0.0f;
    g_grounded = false;
	g_currentSpeed = 0.0f;
    g_timeTotal = 0.0;
    g_lastTime = std::chrono::steady_clock::now();
    g_playerStun.isStunned = false;
    g_playerStun.stunTimer = 0.0f;

    std::cout << "[로봇] 초기화 완료" << std::endl;
    return true;
}

void Character::moveForward(float speed) {
    if (g_playerStun.isStunned) return;
    g_targetPosition.z += g_currentSpeed;
}

void Character::moveBackward(float speed) {
    if (g_playerStun.isStunned) return;
    if (g_targetPosition.z > -5.0f) {
        g_targetPosition.z -= g_currentSpeed;

        if (g_targetPosition.z < -5.0f) {
            g_targetPosition.z = -5.0f;
        }
    }
    
}

void Character::moveLeft(float speed) {
    if (g_playerStun.isStunned) return;

    if (g_targetPosition.x < Bondray_Limit) {
        g_targetPosition.x += g_currentSpeed;

        // 만약 더해서 3.0을 넘으면 3.0으로 고정 (벽에 막힘)
        if (g_targetPosition.x > Bondray_Limit) {
            g_targetPosition.x = Bondray_Limit;
        }
    }
}

void Character::moveRight(float speed) {
    if (g_playerStun.isStunned) return;

    if (g_targetPosition.x > -Bondray_Limit) {
        g_targetPosition.x -= g_currentSpeed;

        // 만약 더해서 3.0을 넘으면 3.0으로 고정 (벽에 막힘)
        if (g_targetPosition.x < -Bondray_Limit) {
            g_targetPosition.x = -Bondray_Limit;
        }
    }
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

	glUseProgram(g_shaderProg);
    glBindVertexArray(g_vao);

	// 시간 계산
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - g_lastTime).count();
    g_lastTime = now;
    if (dt > 0.1) dt = 0.1;
    g_timeTotal += dt;

    // --- [가속도 로직 구현] ---
    if (g_running && !g_playerStun.isStunned) {
        // 달리는 중: 속도 증가
        g_currentSpeed += ACCELERATION * (float)dt;
        if (g_currentSpeed > MAX_SPEED) g_currentSpeed = MAX_SPEED;
    }
    else {
        // 멈춤/스턴: 속도 감소 (마찰력)
        g_currentSpeed -= FRICTION * (float)dt;
        if (g_currentSpeed < 0.0f) g_currentSpeed = 0.0f;
    }

    // 기절 상태 업데이트
    if (g_playerStun.isStunned) {
        g_playerStun.stunTimer += dt;
		g_currentSpeed = 0.0f;
		g_jumpCount = 0; 
        g_jumpRequested = false;
        if (g_playerStun.stunTimer >= g_playerStun.stunDuration) {
            g_playerStun.isStunned = false;
            g_playerStun.stunTimer = 0.0f;
        }
    }

    // g_colorUniform 변수가 올바르게 연결되었다면 아래 숫자로 색이 바뀝니다.
    if (g_colorUniform >= 0) {
        if (g_playerStun.isStunned) {
            // 스턴 상태: 빨간색으로 깜빡임
            float blink = sin((float)g_timeTotal * 20.0f);
            if (blink > 0) glUniform3f(g_colorUniform, 1.0f, 0.0f, 0.0f); // 빨강
            else glUniform3f(g_colorUniform, 1.0f, 1.0f, 0.0f);          // 노랑
        }
        else {
            // 평소 상태: 파란색 (캐릭터 고유색)
            // 바닥 색과 분리하기 위해 여기서 확실하게 파란색을 넣어줍니다.
            glUniform3f(g_colorUniform, 0.2f, 0.6f, 1.0f);
        }
    }

    // 현재 위치를 목표 위치로 부드럽게 이동
    g_position.x = lerp(g_position.x, g_targetPosition.x, MOVEMENT_SMOOTHING);
    g_position.z = lerp(g_position.z, g_targetPosition.z, MOVEMENT_SMOOTHING);

    // 회전할때 보는 각도 설정
    const float PI = 3.14159265f;
    float diff = g_targetYaw - g_yaw;
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
    float walkAnimSpeed = 10.0f;
    float swingAngle = 0.0f;
    if (g_running || !g_grounded) {
        swingAngle = sinf((float)g_timeTotal * walkAnimSpeed);
    }
    // 팔다리 회전을 위한 렌더링
    float armRot = swingAngle * 0.8f;
    float legRot = swingAngle * 0.6f;

    glUseProgram(g_shaderProg);
    glBindVertexArray(g_vao);
   

    // 기본 행렬 (몸통 기준)
    glm::mat4 rootModel = glm::mat4(1.0f);
    rootModel = glm::translate(rootModel, g_position);
    rootModel = glm::rotate(rootModel, g_yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    // [1] 몸통 그리기 (인덱스 0~36)
    if (g_modelUniform >= 0)
        glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(rootModel));

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(0 * sizeof(unsigned int)));


    // [2] 머리 그리기 (인덱스 36~72)
    // 머리는 몸통과 같은 행렬을 쓰되 약간의 위치 차이가 obj에 포함됨
    if (g_modelUniform >= 0)
        glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(rootModel));

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(36 * sizeof(unsigned int)));


    // 피벗 포인트 (회전축)
    glm::vec3 shoulderL(-0.3f, 0.9f, 0.0f);
    glm::vec3 shoulderR(0.3f, 0.9f, 0.0f);
    glm::vec3 hipL(-0.13f, 0.1f, 0.0f);
    glm::vec3 hipR(0.13f, 0.1f, 0.0f);


    // [3] 왼쪽 팔 통째로 그리기 (상박 + 하박)
    {
        glm::mat4 lArmMat = rootModel;
        lArmMat = glm::translate(lArmMat, shoulderL);              // 1. 어깨 위치로 이동
        lArmMat = glm::rotate(lArmMat, armRot, glm::vec3(1, 0, 0));  // 2. 회전
        lArmMat = glm::translate(lArmMat, -shoulderL);             // 3. 다시 원점으로

        // 행렬 전송
        if (g_modelUniform >= 0)
            glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(lArmMat));

        // 상박 그리기 (인덱스 72번부터 36개)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(72 * sizeof(unsigned int)));
        // 하박 그리기 (인덱스 144번부터 36개) - 같은 행렬을 쓰므로 붙어 다님
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(144 * sizeof(unsigned int)));
    }


    // [4] 오른쪽 팔 통째로 그리기
    {
        glm::mat4 rArmMat = rootModel;
        rArmMat = glm::translate(rArmMat, shoulderR);
        rArmMat = glm::rotate(rArmMat, -armRot, glm::vec3(1, 0, 0)); // 반대로 회전
        rArmMat = glm::translate(rArmMat, -shoulderR);

        if (g_modelUniform >= 0)
            glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(rArmMat));

        // 상박 (108번부터)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(108 * sizeof(unsigned int)));
        // 하박 (180번부터)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(180 * sizeof(unsigned int)));
    }


    // [5] 왼쪽 다리 통째로 그리기
    {
        glm::mat4 lLegMat = rootModel;
        lLegMat = glm::translate(lLegMat, hipL);
        lLegMat = glm::rotate(lLegMat, -legRot, glm::vec3(1, 0, 0));
        lLegMat = glm::translate(lLegMat, -hipL);

        if (g_modelUniform >= 0)
            glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(lLegMat));

        // 허벅지 (216번부터)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(216 * sizeof(unsigned int)));
        // 종아리 (288번부터)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(288 * sizeof(unsigned int)));
    }


    // [6] 오른쪽 다리 통째로 그리기
    {
        glm::mat4 rLegMat = rootModel;
        rLegMat = glm::translate(rLegMat, hipR);
        rLegMat = glm::rotate(rLegMat, legRot, glm::vec3(1, 0, 0));
        rLegMat = glm::translate(rLegMat, -hipR);

        if (g_modelUniform >= 0)
            glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(rLegMat));

        // 허벅지 (252번부터)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(252 * sizeof(unsigned int)));
        // 종아리 (324번부터)
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(324 * sizeof(unsigned int)));
    }

    glBindVertexArray(0);

    Enemy::updateOctopus(g_position, (float)dt);
    Enemy::drawOctopus();
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
}

glm::vec3 Character::getPosition() {
    return g_position;
}

void Character::setRunning(bool running) {
    g_running = running;
}

bool Character::isStunned() {
    return g_playerStun.isStunned;
}

void Character::applyStun(float duration) {
    g_playerStun.isStunned = true;
    g_playerStun.stunDuration = duration;
    g_playerStun.stunTimer = 0.0f;
	g_currentSpeed = 0.0f;
}
