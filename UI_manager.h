#pragma once
#include <string>
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/glm/glm.hpp>

class UIManager {
public:
    static void Init();
    static void DrawTitleScreen(int winW, int winH, GLuint textureID);
    static void DrawFinishScreen(int winW, int winH, GLuint textureID, float finalTime);
    static void DrawAll(int winW, int winH, float currentZ, float totalDist, const std::string& timerText, bool isStunned);

private:
    static void DrawTopRightTimer(int winW, int winH, const std::string& text);
    static void DrawBottomRunningBar(int winW,int winH, float progress);
    static void Begin2D(int winW, int winH);
    static void End2D();
    static void DrawRect(float x, float y, float w, float h, glm::vec3 color);
    static void DrawRectOutline(float x, float y, float w, float h, glm::vec3 color, float lineWidth = 2.0f);
    static void DrawCircle(float cx, float cy, float r, glm::vec3 color);
    static void DrawText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18, glm::vec3 color = glm::vec3(1, 1, 1));
};