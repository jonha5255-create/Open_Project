#include "UI_Manager.h"
#include <cmath>
#include <iostream>
#include <cstdio>

void UIManager::Init() {}

void UIManager::DrawTitleScreen(int winW, int winH, GLuint textureID) {
    Begin2D(winW, winH);

    // 텍스처가 있을 때만 그리기
    if (textureID != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // 이미지 원래 색상을 그대로 표현 (흰색 필터)
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        // 화면 전체(0,0 ~ winW, winH)에 텍스처 매핑
        // 주의: 이미지가 거꾸로 보인다면 t좌표를 조절해야 함 (stbi_set_flip_vertically_on_load 사용 시 정상)
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);           // 좌하단
        glTexCoord2f(1.0f, 0.0f); glVertex2f((float)winW, 0.0f);    // 우하단
        glTexCoord2f(1.0f, 1.0f); glVertex2f((float)winW, (float)winH); // 우상단
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)winH);    // 좌상단
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }

    End2D();
}

void UIManager::DrawFinishScreen(int winW, int winH, GLuint textureID, float finalTime) {
    Begin2D(winW, winH);

    // 1. 배경 이미지 그리기 (타이틀과 동일)
    if (textureID != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f((float)winW, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f((float)winW, (float)winH);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)winH);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }

    // 2. 중앙에 반투명 검은 박스 (글자 잘 보이게)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f); // 70% 투명도 검은색

    float boxW = 400.0f;
    float boxH = 250.0f;
    float boxX = (winW - boxW) / 2.0f;
    float boxY = (winH - boxH) / 2.0f;

    glBegin(GL_QUADS);
    glVertex2f(boxX, boxY);
    glVertex2f(boxX + boxW, boxY);
    glVertex2f(boxX + boxW, boxY + boxH);
    glVertex2f(boxX, boxY + boxH);
    glEnd();
    glDisable(GL_BLEND);

    // 3. 텍스트 출력
    // (1) "GAME CLEAR!" 문구
    const char* titleMsg = "GAME CLEAR!";
    DrawText(boxX + 130.0f, boxY + 180.0f, titleMsg, GLUT_BITMAP_TIMES_ROMAN_24, glm::vec3(1.0f, 1.0f, 0.0f)); // 노란색

    // (2) 기록 시간 포맷팅 (MM:SS:ms)
    int min = (int)finalTime / 60;
    int sec = (int)finalTime % 60;
    int ms = (int)((finalTime - (int)finalTime) * 100);

    char timeBuf[50];
    sprintf_s(timeBuf, "RECORD: %02d:%02d:%02d", min, sec, ms);

    // 기록 출력
    DrawText(boxX + 110.0f, boxY + 120.0f, timeBuf, GLUT_BITMAP_TIMES_ROMAN_24, glm::vec3(1.0f, 1.0f, 1.0f)); // 흰색

    // (3) 안내 문구
    DrawText(boxX + 100.0f, boxY + 50.0f, "Press [Q] to Quit", GLUT_BITMAP_HELVETICA_18, glm::vec3(0.7f, 0.7f, 0.7f));

    End2D();
}

void UIManager::DrawAll(int winW, int winH, float currentZ, float totalDist, const std::string& timerText, bool isStunned) {
    Begin2D(winW, winH);

    DrawTopRightTimer(winW, winH, timerText);

    float progress = currentZ / totalDist;
    if (progress > 1.0f) progress = 1.0f;
    DrawBottomRunningBar(winW, winH, progress);

    if (isStunned) {
        const char* msg = "!! STUNNED !!";
        DrawText(winW / 2 - 60, winH / 2, msg, GLUT_BITMAP_TIMES_ROMAN_24, glm::vec3(1, 0, 0));
    }
    End2D();
}

void UIManager::DrawTopRightTimer(int winW, int winH, const std::string& text) {
    float w = 150.0f; float h = 50.0f; float x = winW - w - 20.0f; float y = winH - h - 20.0f;
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w + 20, y + h); glVertex2f(x - 20, y + h);
    glEnd();
    glDisable(GL_BLEND);
    DrawText(x + 20, y + 15, text.c_str(), GLUT_BITMAP_TIMES_ROMAN_24, glm::vec3(1, 1, 0));
    DrawText(x + 40, y + 38, "TIME", GLUT_BITMAP_HELVETICA_10);
}

void UIManager::DrawBottomRunningBar(int winW, int winH, float progress) {
    float w = winW * 0.6f;  // 너비를 60%로 줄여서 타이머와 겹치지 않게 조절
    float h = 15.0f;        // 높이를 조금 얇게

    // [위치 계산 핵심] 화면 높이(winH)에서 40만큼 내려온 위치
    float x = (winW - w) / 2.0f;
    float y = winH - 40.0f;

    // 전체 트랙 (밝은 회색)
    DrawRect(x, y, w, h, glm::vec3(0.8f, 0.8f, 0.8f));

    // 진행된 구간 (그라데이션)
    if (progress > 0.0f) {
        glBegin(GL_QUADS);
        glColor3f(0.1f, 0.1f, 0.8f); glVertex2f(x, y);
        glColor3f(0.0f, 0.8f, 1.0f); glVertex2f(x + w * progress, y);
        glVertex2f(x + w * progress, y + h);
        glColor3f(0.1f, 0.1f, 0.8f); glVertex2f(x, y + h);
        glEnd();
    }

    // 테두리
    DrawRectOutline(x, y, w, h, glm::vec3(0, 0, 0));

    // 현재 위치 마커 (빨간 공)
    float markerX = x + w * progress;
    DrawCircle(markerX, y + h / 2.0f, h / 1.0f, glm::vec3(1.0f, 0.2f, 0.2f));

    // 텍스트 (바 바로 아래에 표시)
    DrawText(x - 45, y, "START", GLUT_BITMAP_HELVETICA_12, glm::vec3(1, 1, 1));
    DrawText(x + w + 5, y, "GOAL", GLUT_BITMAP_HELVETICA_12, glm::vec3(1, 1, 1));
}

void UIManager::Begin2D(int winW, int winH) {
    glUseProgram(0);

    // 2. [매우 중요] 텍스처 끄기
    // 이걸 안 하면 UI에 벽돌 그림이 씌워지거나 투명해집니다.
    glDisable(GL_TEXTURE_2D);

    // 3. 조명, 깊이 테스트 끄기 (2D는 깊이/조명이 필요 없음)
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
}
void UIManager::End2D() {
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glEnable(GL_DEPTH_TEST);
}
void UIManager::DrawRect(float x, float y, float w, float h, glm::vec3 color) {
    // 색상은 외부 설정값 따름 (alpha 포함 시)
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
void UIManager::DrawRectOutline(float x, float y, float w, float h, glm::vec3 color, float lineWidth) {
    glColor3f(color.r, color.g, color.b); glLineWidth(lineWidth);
    glBegin(GL_LINE_LOOP); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
void UIManager::DrawCircle(float cx, float cy, float r, glm::vec3 color) {
    glColor3f(color.r, color.g, color.b); glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
    for (int i = 0; i <= 30; ++i) { float t = 2.0f * 3.14159f * i / 30.0f; glVertex2f(cx + r * cosf(t), cy + r * sinf(t)); } glEnd();
}
void UIManager::DrawText(float x, float y, const char* text, void* font, glm::vec3 color) {
    glColor3f(color.r, color.g, color.b); glWindowPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) glutBitmapCharacter(font, *c);
}