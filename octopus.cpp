#include "octopus.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

// [구조체] 3D 모델의 점(Vertex) 하나가 가지는 정보 정의
// 위치(x,y,z), 색상(r,g,b), 법선벡터(nx,ny,nz)를 포함합니다.
struct OctoVertex {
    float x, y, z;
    float r, g, b;
    float nx, ny, nz;
};

namespace Enemy {

    // --- 전역 변수 (문어 및 전기 공격 데이터 관리) ---
    static GLuint g_vao = 0; // 문어 모델의 VAO (Vertex Array Object)
    static GLuint g_vbo = 0; // 문어 모델의 VBO (Vertex Buffer Object)
    static GLuint g_shaderProg = 0; // 사용할 쉐이더 프로그램 ID
    static int g_vertexCount = 0;   // 문어 모델의 총 정점 개수

    // 문어의 위치 및 상태 설정
    static glm::vec3 g_pos = glm::vec3(0.0f, 0.0f, 30.0f); // 초기 위치: 플레이어보다 앞쪽(Z=20)
    static float g_rotation = -180.0f; // 초기 회전각: 뒤를 보고 시작 (플레이어를 바라봄)
    static float g_scale = 3.0f;      // 크기: 3배 확대 (거대 문어)

    // 전기 공격 관련 변수
    static std::vector<Electricity> g_electricAttacks; // 발사된 전깃줄들을 저장하는 리스트
    static float g_attackTimer = 0.0f; // 공격 쿨타임 계산용 타이머
    static GLuint g_elecVAO = 0; // 전깃줄(큐브) 모델의 VAO
    static GLuint g_elecVBO = 0; // 전깃줄(큐브) 모델의 VBO
    static float g_mapWidthForAtk = 5.0f;

   
    // [함수 1] 문자열 분리 헬퍼 함수
    // 역할: "1/2/3" 처럼 슬래시(/)나 공백으로 된 문자열을 쪼개서 리스트로 만듭니다.
    // 용도: OBJ 파일을 읽을 때 데이터를 파싱하기 위해 사용합니다.
    
    static std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    bool loadOBJ(const char* path, std::vector<OctoVertex>& outVertices) {
        std::vector<glm::vec3> temp_vertices;
        std::vector<glm::vec3> temp_normals;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "OBJ 파일 열기 실패: " << path << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // 'v'로 시작하면 정점 위치 데이터
            if (line.substr(0, 2) == "v ") {
                std::istringstream s(line.substr(2));
                glm::vec3 v; s >> v.x >> v.y >> v.z;
                temp_vertices.push_back(v);
            }
            // 'vn'으로 시작하면 법선(Normal) 데이터
            else if (line.substr(0, 3) == "vn ") {
                std::istringstream s(line.substr(3));
                glm::vec3 n; s >> n.x >> n.y >> n.z;
                temp_normals.push_back(n);
            }
            // 'f'로 시작하면 면(Face) 데이터 -> 이를 이용해 실제 삼각형을 구성
            else if (line.substr(0, 2) == "f ") {
                std::string parts = line.substr(2);
                std::vector<std::string> faceTokens = split(parts, ' ');
                // 사각형(Quad)인 경우 삼각형 2개로 쪼개기 위해 루프 횟수 조정
                int loopCount = (faceTokens.size() == 4) ? 6 : 3;
                int indexOrder[] = { 0, 1, 2, 0, 2, 3 };

                for (int i = 0; i < loopCount; ++i) {
                    std::string& token = faceTokens[indexOrder[i]];
                    std::vector<std::string> indices = split(token, '/');
                    OctoVertex v;

                    // 정점 위치 인덱스 파싱 (OBJ는 1부터 시작하므로 -1 해줌)
                    int vIdx = std::stoi(indices[0]) - 1;
                    v.x = temp_vertices[vIdx].x;
                    v.y = temp_vertices[vIdx].y;
                    v.z = temp_vertices[vIdx].z;

                    // 문어 색상 강제 지정 (붉은색 느낌)
                    v.r = 1.0f; v.g = 0.5f; v.b = 0.0f;

                    // 법선 인덱스 파싱
                    if (indices.size() >= 3 && !indices[2].empty()) {
                        int nIdx = std::stoi(indices[2]) - 1;
                        v.nx = temp_normals[nIdx].x;
                        v.ny = temp_normals[nIdx].y;
                        v.nz = temp_normals[nIdx].z;
                    }
                    else {
                        v.nx = 0; v.ny = 1; v.nz = 0;
                    }
                    outVertices.push_back(v);
                }
            }
        }
        return true;
    }

  
    // [함수 3] 전깃줄 그래픽 초기화

    void initElectricityCube() {
        float vertices[] = {
            // 앞면과 뒷면 위주로 구성된 큐브 정점 데이터
            // (데이터 생략: 이전 코드와 동일)
            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
             0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,

             -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
              0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
              0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
              0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
             -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
             -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        };

        glGenVertexArrays(1, &g_elecVAO);
        glBindVertexArray(g_elecVAO);
        glGenBuffers(1, &g_elecVBO);
        glBindBuffer(GL_ARRAY_BUFFER, g_elecVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // 쉐이더 속성 연결: 0번(위치), 1번(색상), 2번(법선)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    }

    
    // [함수 4] 문어 초기화 (메인)

    bool initOctopus(const char* objPath, GLuint shaderProg) {
        g_shaderProg = shaderProg;
        std::vector<OctoVertex> vertices;

        // 1. OBJ 파일 로드
        if (!loadOBJ(objPath, vertices)) return false;

        g_vertexCount = vertices.size();

        // 2. 문어용 VAO, VBO 생성 및 데이터 전송
        glGenVertexArrays(1, &g_vao);
        glBindVertexArray(g_vao);
        glGenBuffers(1, &g_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(OctoVertex), vertices.data(), GL_STATIC_DRAW);

        // 정점 속성 설정 (위치, 색상, 법선)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(OctoVertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(OctoVertex), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(OctoVertex), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // 3. 전깃줄 큐브 초기화도 같이 실행
        initElectricityCube();
        return true;
    }

    // [함수 5] 문어 상태 업데이트 (핵심 로직)
    void updateOctopus(const glm::vec3& playerPos, float dt) {

        // 1. 위치 고정 로직:
        // 문어는 맵의 정중앙(X=0)에서만 앞뒤로 움직입니다. 좌우로는 절대 움직이지 않습니다.
        g_pos.x = 0.0f;
        g_pos.y = 0.0f;

        float normalSpeed = 6.0f;   // 평소 이동 속도
        float panicSpeed = 12.0f;    // 도망갈 때 속도
        float safeDistance = 15.0f; // 플레이어와의 안전 거리

        // 플레이어와의 거리 계산 (Z축 기준)
        float distance = g_pos.z - playerPos.z;

        // 플레이어가 가까이 오면 빠르게 뒤로 도망 (+Z 방향)
        if (distance < safeDistance) {
            g_pos.z += panicSpeed * dt;
        }
        else {
            g_pos.z += normalSpeed * dt; // 평소에는 천천히 이동
        }

        // 2. 회전 로직:
        // 항상 플레이어를 바라보도록 회전 각도(g_rotation)를 계산합니다.
        glm::vec3 lookDir = playerPos - g_pos;
        g_rotation = glm::degrees(atan2(lookDir.x, lookDir.z));

        // 3. 공격 생성 로직:
        // 일정 시간(2.5초)마다 전깃줄을 생성합니다.
        g_attackTimer += dt;
        if (g_attackTimer > 2.0f) {
            g_attackTimer = 0.0f;
            Electricity elec;
            elec.direction = glm::vec3(0.0f, 0.0f, -1.0f);
            elec.speed = 15.0f;
            elec.lifetime = 20.0f;
            elec.active = true;

            // [수정] 0, 1, 2 패턴만 사용 (숙이기 없음)
            int pattern = rand() % 5;

            switch (pattern) {
            case 0: // 하단 가로
                elec.type = ATK_LOW_BAR;
                elec.position = glm::vec3(0.0f, 0.4f, g_pos.z);
                elec.radius = g_mapWidthForAtk;
                break;
            case 1: // 왼쪽 막음 (오른쪽으로 피하기)
                elec.type = ATK_VERTICAL;
                elec.position = glm::vec3(-g_mapWidthForAtk / 2.0f, 1.5f, g_pos.z);
                elec.radius = g_mapWidthForAtk / 2.0f;
                break;
            case 2: // 오른쪽 막음 (왼쪽으로 피하기)
                elec.type = ATK_VERTICAL;
                elec.position = glm::vec3(g_mapWidthForAtk / 2.0f, 1.5f, g_pos.z);
                elec.radius = g_mapWidthForAtk / 2.0f;
                break;
            case 3: // 중앙 막음 (양쪽으로 피하기)
                elec.type = ATK_VERTICAL;
                elec.position = glm::vec3(0.0f, 1.5f, g_pos.z);
                elec.radius = g_mapWidthForAtk / 3.0f;
				break;
            case 4: // 2단 점프
                elec.type = ATK_HIGH_BAR;
                elec.position = glm::vec3(0.0f, 1.5f, g_pos.z);
                elec.radius = g_mapWidthForAtk;
				break;
            }
            g_electricAttacks.push_back(elec);
        }

        // 4. 발사된 전깃줄들의 위치 업데이트
        for (auto& elec : g_electricAttacks) {
            if (elec.active) {
                elec.position += elec.direction * elec.speed * dt;
                elec.lifetime -= dt;
                if (elec.lifetime <= 0.0f) elec.active = false;
            }
        }

        // 2. [중요] 비활성(죽은) 전깃줄을 메모리에서 삭제 (렉 방지 핵심!)
        g_electricAttacks.erase(
            std::remove_if(g_electricAttacks.begin(), g_electricAttacks.end(),
                [](const Electricity& e) { return !e.active; }),
            g_electricAttacks.end()
        );
    }

    // [함수 6] 문어 그리기
    void drawOctopus() {
        if (g_vao == 0) return;
        glUseProgram(g_shaderProg);
        glBindVertexArray(g_vao);
     

        // 모델 행렬 계산: 이동 -> 회전 -> 크기조절
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, g_pos);
        model = glm::rotate(model, glm::radians(g_rotation), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(g_scale));

        // 쉐이더에 행렬 전송
        glUniformMatrix4fv(glGetUniformLocation(g_shaderProg, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // 원래 색상 유지 (흰색 빛)
        GLuint colorLoc = glGetUniformLocation(g_shaderProg, "objectColor");
        if (colorLoc >= 0) glUniform3f(colorLoc, 1.0f, 0.5f, 0.0f);

        glDrawArrays(GL_TRIANGLES, 0, g_vertexCount);
    }

    // --------------------------------------------------------
    // [함수 7] 전깃줄 그리기
    // 역할: 생성된 전깃줄들을 화면에 그립니다.
    // 특징: 큐브를 가로(X축)로 10배 늘려서 맵 전체를 막는 '벽'처럼 보이게 만듭니다.
    // --------------------------------------------------------
    void drawElectricity() {
        if (g_elecVAO == 0) return;
        glUseProgram(g_shaderProg);
        glBindVertexArray(g_elecVAO);


        for (const auto& elec : g_electricAttacks) {
            if (!elec.active) continue;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, elec.position);

            if (elec.type == ATK_VERTICAL) {
                glUniform3f(glGetUniformLocation(g_shaderProg, "objectColor"), 1.0f, 0.2f, 0.2f);
                model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 0, 1));
                model = glm::scale(model, glm::vec3(3.0f, elec.radius * 2.0f, 0.3f));
            }
            else if (elec.type == ATK_LOW_BAR) {
                glUniform3f(glGetUniformLocation(g_shaderProg, "objectColor"), 1.0f, 0.2f, 0.2f);
                model = glm::scale(model, glm::vec3(elec.radius * 2.0f, 0.3f, 0.3f));
            }
            else if (elec.type == ATK_HIGH_BAR) {
                glUniform3f(glGetUniformLocation(g_shaderProg, "objectColor"), 1.0f, 0.2f, 0.2f);
                model = glm::scale(model, glm::vec3(elec.radius * 2.0f, 0.3f, 0.3f));
			}

            glUniformMatrix4fv(glGetUniformLocation(g_shaderProg, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 12);
        }
        glBindVertexArray(0);
    }

    // --------------------------------------------------------
    // [함수 8] 충돌 체크
    // 역할: 플레이어가 전깃줄에 닿았는지 확인합니다.
    // 원리: 
    // 1. Z축 거리가 가까운지 확인 (두께 체크)
    // 2. 플레이어의 Y축(높이) 확인 (점프했는지 체크)
    // 3. 둘 다 해당되면 충돌로 처리
    // --------------------------------------------------------
    bool checkElectricityCollision(const glm::vec3& playerPos, float playerRadius, PlayerStun& outStun) {
        for (auto& elec : g_electricAttacks) {
            if (!elec.active) continue;
            float zDist = abs(elec.position.z - playerPos.z);
            if (zDist > 0.5f) continue;

            bool collision = false;
            switch (elec.type) {
            case ATK_LOW_BAR:
                if (playerPos.y < 0.5f) collision = true; // 점프 안했으면
                break;
            case ATK_VERTICAL:
                if (playerPos.x > elec.position.x - elec.radius && playerPos.x < elec.position.x + elec.radius) collision = true;
                break;
			case ATK_HIGH_BAR:
				if (playerPos.y < 1.5f) collision = true; // 2단 점프 안했으면
            }

            if (collision) {
                outStun.isStunned = true;
                outStun.stunDuration = 1.5f;
                outStun.stunTimer = 0.0f;
                elec.active = false;
                return true;
            }
        }
        return false;
    }

    void cleanup() {
        if (g_vbo) glDeleteBuffers(1, &g_vbo);
        if (g_vao) glDeleteVertexArrays(1, &g_vao);
        if (g_elecVBO) glDeleteBuffers(1, &g_elecVBO);
        if (g_elecVAO) glDeleteVertexArrays(1, &g_elecVAO);
    }
}