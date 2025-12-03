#pragma once
#include <vector>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

namespace Character {
    bool initCharacter(const char* objPath, GLuint shaderProg);
    void drawCharacter();
    void cleanup();
}
