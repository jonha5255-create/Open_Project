#include "octopus.h"

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <cmath>
#include <algorithm>

// 정점 구조체
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

// 간단한 수학 구조체
struct Vec3 {
    float x, y, z;
};

struct Vec2 {
    float u, v;
};

// 전역 상태
static GLuint g_octopusVAO = 0;
static GLuint g_octopusVBO = 0;
static GLuint g_octopusEBO = 0;
static GLsizei g_octopusIndexCount = 0;
static GLuint g_shaderProg = 0;
static GLint g_modelUniform = -1;

// 문어 상태
static glm::vec3 g_octopusPos = glm::vec3(5.0f, 1.0f, -10.0f);
static glm::vec3 g_octopusVelocity = glm::vec3(0.0f);
static float g_escapeDistance = 15.0f; // 플레이어와의 안전거리
static float g_moveSpeed = 8.0f; // 도망 속도

// 전기 상태
static std::vector<Electricity> g_electricityList;
static float g_shootCooldown = 0.0f;
static float g_shootInterval = 1.5f; // 1.5초마다 발사
static float g_electricitySpeed = 10.0f;
static float g_electricityLifetime = 3.0f;
static float g_electricityRadius = 0.3f;

// OBJ 파일 로드 함수
static bool loadOBJ(const char* path, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "OBJ 로드 실패: " << path << "\n";
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;
    std::unordered_map<std::string, unsigned int> indexMap;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.size() < 2) continue;
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vec3 p;
            iss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (prefix == "vn") {
            Vec3 n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (prefix == "vt") {
            Vec2 t;
            iss >> t.u >> t.v;
            texcoords.push_back(t);
        }
        else if (prefix == "f") {
            std::string a, b, c;
            iss >> a >> b >> c;
            std::string face[3] = { a, b, c };

            for (int i = 0; i < 3; ++i) {
                const std::string& tok = face[i];
                auto it = indexMap.find(tok);
                if (it != indexMap.end()) {
                    outIndices.push_back(it->second);
                }
                else {
                    int vi = 0, ti = 0, ni = 0;
                    std::string t = tok;
                    for (char& ch : t) if (ch == '/') ch = ' ';
                    std::istringstream sst(t);
                    sst >> vi;
                    if (!(sst >> ti)) ti = 0;
                    if (!(sst >> ni)) ni = 0;

                    Vertex vert = {};
                    if (vi > 0 && vi <= (int)positions.size()) {
                        Vec3& p = positions[vi - 1];
                        vert.px = p.x; vert.py = p.y; vert.pz = p.z;
                    }
                    if (ni > 0 && ni <= (int)normals.size()) {
                        Vec3& n = normals[ni - 1];
                        vert.nx = n.x; vert.ny = n.y; vert.nz = n.z;
                    }
                    if (ti > 0 && ti <= (int)texcoords.size()) {
                        Vec2& t2 = texcoords[ti - 1];
                        vert.u = t2.u; vert.v = t2.v;
                    }

                    unsigned int newIndex = (unsigned int)outVertices.size();
                    outVertices.push_back(vert);
                    outIndices.push_back(newIndex);
                    indexMap.emplace(tok, newIndex);
                }
            }
        }
    }

    // 법선 계산 (필요한 경우)
    bool needNormals = true;
    for (const auto& v : outVertices) {
        if (v.nx != 0.0f || v.ny != 0.0f || v.nz != 0.0f) {
            needNormals = false;
            break;
        }
    }

    if (needNormals) {
        for (size_t i = 0; i < outIndices.size(); i += 3) {
            unsigned int ia = outIndices[i + 0], ib = outIndices[i + 1], ic = outIndices[i + 2];
            Vec3 a = { outVertices[ia].px, outVertices[ia].py, outVertices[ia].pz };
            Vec3 b = { outVertices[ib].px, outVertices[ib].py, outVertices[ib].pz };
            Vec3 c = { outVertices[ic].px, outVertices[ic].py, outVertices[ic].pz };
            Vec3 u = { b.x - a.x, b.y - a.y, b.z - a.z };
            Vec3 v = { c.x - a.x, c.y - a.y, c.z - a.z };
            Vec3 n = { u.y * v.z - u.z * v.y,
                       u.z * v.x - u.x * v.z,
                       u.x * v.y - u.y * v.x };
            float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (len > 1e-6f) { n.x /= len; n.y /= len; n.z /= len; }
            outVertices[ia].nx += n.x; outVertices[ia].ny += n.y; outVertices[ia].nz += n.z;
            outVertices[ib].nx += n.x; outVertices[ib].ny += n.y; outVertices[ib].nz += n.z;
            outVertices[ic].nx += n.x; outVertices[ic].ny += n.y; outVertices[ic].nz += n.z;
        }

        for (auto& v : outVertices) {
            float lx = v.nx, ly = v.ny, lz = v.nz;
            float llen = std::sqrt(lx * lx + ly * ly + lz * lz);
            if (llen > 1e-6f) { v.nx = lx / llen; v.ny = ly / llen; v.nz = lz / llen; }
        }
    }

    return true;
}

bool Enemy::initOctopus(const char* objPath, GLuint shaderProg) {
    g_shaderProg = shaderProg;
    g_modelUniform = glGetUniformLocation(shaderProg, "model");

    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    if (!loadOBJ(objPath, verts, idx)) {
        std::cerr << "문어 OBJ 로딩 실패\n";
        return false;
    }

    g_octopusIndexCount = (GLsizei)idx.size();

    glGenVertexArrays(1, &g_octopusVAO);
    glBindVertexArray(g_octopusVAO);

    glGenBuffers(1, &g_octopusVBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_octopusVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &g_octopusEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_octopusEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, px));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

    glBindVertexArray(0);

    g_octopusPos = glm::vec3(5.0f, 1.0f, -10.0f);
    g_shootCooldown = 0.0f;
    g_electricityList.clear();

    return true;
}

void Enemy::updateOctopus(const glm::vec3& playerPos, float dt) {
    // 플레이어로부터 도망치기
    glm::vec3 dirToPlayer = playerPos - g_octopusPos;
    float distToPlayer = glm::length(dirToPlayer);

    // 안전 거리보다 가까우면 도망치기
    if (distToPlayer < g_escapeDistance && distToPlayer > 0.1f) {
        glm::vec3 escapeDir = -glm::normalize(dirToPlayer); // 반대 방향
        g_octopusVelocity = escapeDir * g_moveSpeed;
    }
    else {
        g_octopusVelocity = glm::vec3(0.0f); // 안전 거리 이상이면 정지
    }

    // 위치 업데이트
    g_octopusPos += g_octopusVelocity * dt;

    // 맵 경계 제한
    if (g_octopusPos.z > 5.0f) {
        g_octopusPos.z = 5.0f;
        g_octopusVelocity.z = 0.0f;
    }
    if (g_octopusPos.z < -30.0f) {
        g_octopusPos.z = -30.0f;
        g_octopusVelocity.z = 0.0f;
    }

    // 맵 좌우 경계
    if (g_octopusPos.x > 8.0f) {
        g_octopusPos.x = 8.0f;
        g_octopusVelocity.x = 0.0f;
    }
    if (g_octopusPos.x < -8.0f) {
        g_octopusPos.x = -8.0f;
        g_octopusVelocity.x = 0.0f;
    }

    // 쿨다운 감소
    g_shootCooldown -= dt;

    // 전기 발사
    if (g_shootCooldown <= 0.0f && distToPlayer < 25.0f) {
        // 플레이어를 향해 발사
        glm::vec3 shootDir = glm::normalize(dirToPlayer);

        Electricity e;
        e.position = g_octopusPos + glm::vec3(0.0f, 0.5f, 0.0f);
        e.direction = shootDir;
        e.speed = g_electricitySpeed;
        e.lifetime = 0.0f;
        e.maxLifetime = g_electricityLifetime;
        e.radius = g_electricityRadius;
        e.active = true;

        g_electricityList.push_back(e);
        g_shootCooldown = g_shootInterval;
    }
}

void Enemy::updateElectricity(float dt) {
    for (auto& e : g_electricityList) {
        if (!e.active) continue;

        e.position += e.direction * e.speed * dt;
        e.lifetime += dt;

        if (e.lifetime > e.maxLifetime) {
            e.active = false;
        }
    }

    // 비활성화된 전기 제거
    g_electricityList.erase(
        std::remove_if(g_electricityList.begin(), g_electricityList.end(),
            [](const Electricity& e) { return !e.active; }),
        g_electricityList.end()
    );
}

void Enemy::drawOctopus() {
    if (g_octopusVAO == 0 || g_octopusIndexCount == 0) return;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), g_octopusPos);
    model = glm::scale(model, glm::vec3(1.5f));

    if (g_modelUniform >= 0) {
        glUseProgram(g_shaderProg);
        glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(model));
    }

    glBindVertexArray(g_octopusVAO);
    glDrawElements(GL_TRIANGLES, g_octopusIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Enemy::drawElectricity() {
    if (g_electricityList.empty()) return;

    glUseProgram(g_shaderProg);

    for (const auto& e : g_electricityList) {
        if (!e.active) continue;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), e.position);
        model = glm::scale(model, glm::vec3(e.radius));

        if (g_modelUniform >= 0) {
            glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, glm::value_ptr(model));
        }

        // 간단한 포인트로 표시
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glColor3f(1.0f, 1.0f, 0.0f);
        glVertex3fv(glm::value_ptr(e.position));
        glEnd();
        glPointSize(1.0f);
    }
}

bool Enemy::checkElectricityCollision(const glm::vec3& playerPos, float playerRadius, PlayerStun& outStun) {
    for (auto& e : g_electricityList) {
        if (!e.active) continue;

        float dist = glm::distance(playerPos, e.position);
        if (dist < playerRadius + e.radius) {
            e.active = false;
            outStun.isStunned = true;
            outStun.stunTimer = 0.0f;
            outStun.stunDuration = 0.5f;
            return true;
        }
    }
    return false;
}

void Enemy::cleanup() {
    if (g_octopusEBO) { glDeleteBuffers(1, &g_octopusEBO); g_octopusEBO = 0; }
    if (g_octopusVBO) { glDeleteBuffers(1, &g_octopusVBO); g_octopusVBO = 0; }
    if (g_octopusVAO) { glDeleteVertexArrays(1, &g_octopusVAO); g_octopusVAO = 0; }
    g_octopusIndexCount = 0;
    g_shaderProg = 0;
    g_modelUniform = -1;
    g_electricityList.clear();
}