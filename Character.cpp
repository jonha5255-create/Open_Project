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

struct Vec3 { float x, y, z; };
struct Vec2 { float u, v; };

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

static glm::vec3 g_position = glm::vec3(0.0f, 0.0f, 0.0f);
static float g_yaw = 0.0f;
static float g_speed = 3.5f;
static bool g_running = true;
static bool g_jumpRequested = false;
static float g_verticalVel = 0.0f;
static bool g_grounded = true;
static double g_timeTotal = 0.0;
static std::chrono::steady_clock::time_point g_lastTime;

static PlayerStun g_playerStun = { false, 0.5f, 0.0f };

static void mat4_identity(float m[16]) {
    for (int i = 0;i < 16;i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_translate(float m[16], float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_scale(float m[16], float s) {
    mat4_identity(m);
    m[0] = s; m[5] = s; m[10] = s;
}

static void mat4_rotate_y(float m[16], float radians) {
    mat4_identity(m);
    float c = cosf(radians), s = sinf(radians);
    m[0] = c;  m[2] = s;
    m[8] = -s; m[10] = c;
}

static void mat4_mul(const float a[16], const float b[16], float out[16]) {
    for (int r = 0;r < 4;++r) {
        for (int c = 0;c < 4;++c) {
            float sum = 0.0f;
            for (int k = 0;k < 4;++k) sum += a[k * 4 + r] * b[c * 4 + k];
            out[c * 4 + r] = sum;
        }
    }
}

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
    outVertices.clear();
    outIndices.clear();

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.size() < 2) continue;
        std::istringstream iss(line);
        std::string prefix; iss >> prefix;
        if (prefix == "v") {
            Vec3 p; iss >> p.x >> p.y >> p.z; positions.push_back(p);
        }
        else if (prefix == "vn") {
            Vec3 n; iss >> n.x >> n.y >> n.z; normals.push_back(n);
        }
        else if (prefix == "vt") {
            Vec2 t; iss >> t.u >> t.v; texcoords.push_back(t);
        }
        else if (prefix == "f") {
            std::string a, b, c;
            iss >> a >> b >> c;
            std::string face[3] = { a,b,c };
            for (int i = 0;i < 3;++i) {
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
                    else {
                        vert.nx = vert.ny = vert.nz = 0.0f;
                    }
                    if (ti > 0 && ti <= (int)texcoords.size()) {
                        Vec2& t2 = texcoords[ti - 1];
                        vert.u = t2.u; vert.v = t2.v;
                    }
                    else {
                        vert.u = vert.v = 0.0f;
                    }

                    unsigned int newIndex = (unsigned int)outVertices.size();
                    outVertices.push_back(vert);
                    outIndices.push_back(newIndex);
                    indexMap.emplace(tok, newIndex);
                }
            }
        }
    }

    bool needNormals = true;
    for (const auto& v : outVertices) { if (v.nx != 0.0f || v.ny != 0.0f || v.nz != 0.0f) { needNormals = false; break; } }
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

bool Character::initCharacter(const char* objPath, GLuint shaderProg) {
    g_shaderProg = shaderProg;
    g_modelUniform = glGetUniformLocation(shaderProg, "model");
    if (g_modelUniform < 0) {
        std::cerr << "[Character] 경고: 셰이더에 'model' 유니폼이 없습니다.\n";
    }

    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    if (!loadOBJ(objPath, verts, idx)) {
        std::cerr << "캐릭터 OBJ 로딩 실패\n";
        return false;
    }

    g_indexCount = (GLsizei)idx.size();

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
    g_yaw = 0.0f;
    g_running = true;
    g_verticalVel = 0.0f;
    g_grounded = true;
    g_timeTotal = 0.0;
    g_lastTime = std::chrono::steady_clock::now();
    g_playerStun.isStunned = false;
    g_playerStun.stunTimer = 0.0f;

    return true;
}

void Character::drawCharacter() {
    if (g_vao == 0 || g_indexCount == 0) return;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - g_lastTime).count();
    g_lastTime = now;
    if (dt > 0.1) dt = 0.1;
    g_timeTotal += dt;

    // 경직 상태 업데이트
    if (g_playerStun.isStunned) {
        g_playerStun.stunTimer += dt;
        if (g_playerStun.stunTimer >= g_playerStun.stunDuration) {
            g_playerStun.isStunned = false;
            g_playerStun.stunTimer = 0.0f;
        }
    }

    // 경직 중이 아닐 때만 이동
    if (!g_playerStun.isStunned) {
        if (g_running) {
            g_position.z -= g_speed * (float)dt;
        }

        if (g_jumpRequested && g_grounded) {
            g_verticalVel = 5.5f;
            g_grounded = false;
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
            }
        }
    }

    float bobOffset = 0.0f;
    if (g_running && g_grounded && !g_playerStun.isStunned) {
        const float freq = 10.0f;
        const float amp = 0.06f;
        bobOffset = sinf((float)g_timeTotal * freq) * amp;
    }

    float T[16]; mat4_translate(T, g_position.x, g_position.y + bobOffset, g_position.z);
    float R[16]; mat4_rotate_y(R, g_yaw);
    float S[16]; mat4_scale(S, 1.0f);
    float TR[16]; mat4_mul(T, R, TR);
    float TRS[16]; mat4_mul(TR, S, TRS);

    if (g_modelUniform >= 0) {
        glUseProgram(g_shaderProg);
        glUniformMatrix4fv(g_modelUniform, 1, GL_FALSE, TRS);
    }

    glBindVertexArray(g_vao);
    glDrawElements(GL_TRIANGLES, g_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // 문어 시스템
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
    Enemy::cleanup();
}

