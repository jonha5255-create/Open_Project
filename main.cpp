#define _CRT_SECURE_NO_WARNINGS 
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <time.h>
#include <algorithm>
#include <vector>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include "character.h"

#define MAX_LINE_LENGTH 256

char* filetobuf(const char* file)
{
	FILE* fptr;
	long length;
	char* buf;
	fptr = fopen(file, "rb");
	if (!fptr)
		return NULL;
	fseek(fptr, 0, SEEK_END);
	length = ftell(fptr);
	buf = (char*)malloc(length + 1);
	fseek(fptr, 0, SEEK_SET);
	fread(buf, length, 1, fptr);
	fclose(fptr);
	buf[length] = 0;
	return buf;
}

void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
GLvoid drawScene();
GLvoid Reshape(int w, int h);
GLvoid keyboard(unsigned char key, int x, int y);
GLvoid keyboardUp(unsigned char key, int x, int y);  // 키 떼기 함수
GLvoid specialKeyboard(int key, int x, int y);
GLvoid specialKeyboardUp(int key, int x, int y);  // 특수키 떼기 함수
GLvoid timer(int value);

GLint width, height;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

// 캐릭터 위치 및 상태
glm::vec3 characterPosition = glm::vec3(0.0f, 0.0f, 0.0f);

// 카메라
glm::vec3 cameraPos = glm::vec3(0.0f, 2.2f, 5.0f);
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.8f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraRotationY = 0.0f;
float cameraOrbitAngle = 45.0f;
float cameraOrbitRadius = 5.0f;

// 테런 서바이벌 맵 카메라
static const float CAMERA_SIDE_OFFSET = 0.0f;   // 캐릭터 옆쪽 오프셋 (좌우)
static const float CAMERA_BACK_DISTANCE = 4.5f; // 캐릭터 뒤쪽 거리
static const float CAMERA_HEIGHT = 2.5f;        // 캐릭터 위의 높이
static const float CAMERA_FOLLOW_SPEED = 0.12f; // 카메라 따라가는 속도 (부드러움)
static const float CAMERA_TARGET_HEIGHT = 0.8f; // 카메라가 바라보는 높이

// 애니메이션 제어
bool cameraOrbitAnimation = false;         // 카메라 공전 애니메이션
bool allAnimationsStopped = false;         // 모든 움직임 정지

// 조명
glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

void InitBuffer();
void UpdateCameraPosition();

// 키 상태 추적
static bool keyStates[256] = {false};

// 특수키 상태 추적 (GLUT의 특수키용)
static const int SPECIAL_KEY_OFFSET = 300;
static bool specialKeyStates[SPECIAL_KEY_OFFSET] = {false};

void main(int argc, char** argv)
{
	width = 800;
	height = 800;
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(width, height);
	glutCreateWindow("Character Model - Act 20");
	glewExperimental = GL_TRUE;
	glewInit();

	make_vertexShaders();
	make_fragmentShaders();
	shaderProgramID = make_shaderProgram();

	InitBuffer();

	// 캐릭터 초기화 (OBJ 파일 경로 지정)
	if (!Character::initCharacter("character.obj", shaderProgramID)) {
		std::cerr << "캐릭터 초기화 실패" << std::endl;
		exit(1);
	}

	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);  // 키 떼기 콜백 추가
	glutSpecialFunc(specialKeyboard);
	glutSpecialUpFunc(specialKeyboardUp);  // 특수키 떼기 콜백 추가
	glutTimerFunc(16, timer, 0);

	srand((unsigned int)time(NULL));

	std::cout << "=== 캐릭터 조작법 ===" << std::endl;
	std::cout << "방향키: 캐릭터 XZ 평면 이동" << std::endl;
	std::cout << "W/A/S/D: 앞/왼/뒤/오른쪽 이동" << std::endl;
	std::cout << "\n=== 카메라 조작법 ===" << std::endl;
	std::cout << "Z/X: 카메라 Z축 이동" << std::endl;
	std::cout << "Y: 카메라 Y축 자전" << std::endl;
	std::cout << "o: 모든 움직임 멈추기/재개" << std::endl;
	std::cout << "c: 초기화" << std::endl;
	std::cout << "q: 종료" << std::endl;

	glutMainLoop();
}

void make_vertexShaders()
{
	GLchar* vertexSource;
	vertexSource = filetobuf("acting3_vertex.glsl");

	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

void make_fragmentShaders()
{
	GLchar* fragmentSource;
	fragmentSource = filetobuf("acting3_fragment.glsl");

	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: frag_shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

GLuint make_shaderProgram()
{
	GLint result;
	GLchar errorLog[512];
	GLuint shaderID;
	shaderID = glCreateProgram();
	glAttachShader(shaderID, vertexShader);
	glAttachShader(shaderID, fragmentShader);
	glLinkProgram(shaderID);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(shaderID, 512, NULL, errorLog);
		std::cerr << "ERROR: shader program 연결 실패\n" << errorLog << std::endl;
		return false;
	}
	glUseProgram(shaderID);
	return shaderID;
}

void InitBuffer()
{
	// 버퍼 초기화는 그리기 함수에서 직접 처리
}

// 카메라 위치 업데이트 (공전)
void UpdateCameraPosition()
{
	cameraPos.x = cameraOrbitRadius * cos(glm::radians(cameraOrbitAngle));
	cameraPos.z = cameraOrbitRadius * sin(glm::radians(cameraOrbitAngle));
	cameraPos.y = 3.0f;
}

// 값을 부드럽게 보간하는 함수
float lerp(float current, float target, float speed)
{
	float diff = target - current;
	if (fabs(diff) < 0.1f) {
		return target; // 목표에 충분히 가까우면 정확히 목표값 반환
	}
	return current + diff * speed;
}

// 육면체 그리기 함수
void DrawCube(glm::mat4 modelMat, glm::vec3 color, glm::vec3 scale)
{
	GLfloat cubeVertices[] = {
		// 위치 (x,y,z) + 법선 (nx,ny,nz)
		// 앞면 (Z+)
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

		// 뒷면 (Z-)
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

		 // 왼쪽 면 (X-)
		 -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,

		 // 오른쪽 면 (X+)
		  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

		  // 윗면 (Y+)
		  -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		   0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		   0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		  -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		   0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		  -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,

		  // 아랫면 (Y-)
		  -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		   0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		   0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		  -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		   0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		  -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f
	};

	GLuint VAO, VBO, colorVBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &colorVBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

	// 위치 속성
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	// 법선 속성
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	// 색상 데이터
	GLfloat colors[108];
	for (int i = 0; i < 36; i++) {
		colors[i * 3 + 0] = color.r;
		colors[i * 3 + 1] = color.g;
		colors[i * 3 + 2] = color.b;
	}

	glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(1);

	// 변환 행렬 적용
	modelMat = glm::scale(modelMat, scale);

	// 카메라 y축 자전 적용
	glm::vec3 rotatedCameraPos = cameraPos;
	glm::mat4 cameraRotMat = glm::rotate(glm::mat4(1.0f), glm::radians(cameraRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::vec4 rotatedPos = cameraRotMat * glm::vec4(cameraPos, 1.0f);
	rotatedCameraPos = glm::vec3(rotatedPos);

	glm::mat4 viewMat = glm::lookAt(rotatedCameraPos, cameraTarget, cameraUp);
	glm::mat4 projectionMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);

	GLuint modelLoc = glGetUniformLocation(shaderProgramID, "model");
	GLuint viewLoc = glGetUniformLocation(shaderProgramID, "view");
	GLuint projectionLoc = glGetUniformLocation(shaderProgramID, "projection");
	GLuint lightPosLoc = glGetUniformLocation(shaderProgramID, "lightPos");
	GLuint lightColorLoc = glGetUniformLocation(shaderProgramID, "lightColor");
	GLuint viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMat));
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMat));
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMat));
	glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
	glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
	glUniform3fv(viewPosLoc, 1, glm::value_ptr(rotatedCameraPos));

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &colorVBO);
	glDeleteVertexArrays(1, &VAO);
}

// 바닥 그리기
//void DrawFloor()
//{
//	glm::mat4 modelMat = glm::mat4(1.0f);
//	modelMat = glm::translate(modelMat, glm::vec3(0.0f, -0.7f, 0.0f));
//	DrawCube(modelMat, glm::vec3(0.8f, 0.9f, 1.0f), glm::vec3(10.0f, 0.1f, 10.0f));
//}

void DrawSurvivalMap()
{
	const int MAP_SIZE = 10; // 맵의 크기를 넓게 설정

	// 바닥을 구성할 작은 사각형(Quad)을 반복문으로 배치합니다.
	for (int z = -MAP_SIZE * 10 ; z < MAP_SIZE; ++z) {
		for (int x = -MAP_SIZE/2; x < MAP_SIZE/2; ++x) {
			glm::mat4 modelMat = glm::mat4(1.0f);

			// 바닥 위치 설정 (y=-1.0f 높이에 배치)
			modelMat = glm::translate(modelMat, glm::vec3(x * 1.0f + 0.5f, -1.0f, z * 1.0f + 0.5f));

			// 색상 패턴: 체크무늬 (어두운 회색 계열)
			glm::vec3 blockColor;
			if ((abs(x) + abs(z)) % 2 == 0)
				blockColor = glm::vec3(0.3f, 0.3f, 0.3f); // 어두운 색
			else
				blockColor = glm::vec3(0.5f, 0.5f, 0.5f); // 밝은 색

			// 블럭 그리기 (크기는 1.0 x 0.01 x 1.0의 아주 납작한 사각형)
			DrawCube(modelMat, blockColor, glm::vec3(1.0f, 0.01f, 1.0f));
		}
	}

	// 이전에 추가했던 배경 장식 (Floating Cubes) 코드는 삭제 또는 주석 처리되었습니다.
}

// 좌표축 그리기
void DrawAxes()
{
	GLfloat axes[] = {
		-3.0f, 0.0f, 0.0f,   3.0f, 0.0f, 0.0f,  // X축
		 0.0f,-3.0f, 0.0f,   0.0f, 3.0f, 0.0f,  // Y축
		 0.0f, 0.0f,-3.0f,   0.0f, 0.0f, 3.0f   // Z축
	};

	GLfloat axesColors[] = {
		1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,  // 빨강
		0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 0.0f,  // 초록
		0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f   // 파랑
	};

	GLuint VAO, VBO, colorVBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &colorVBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axesColors), axesColors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(1);

	// 카메라 회전 적용
	glm::vec3 rotatedCameraPos = cameraPos;
	glm::mat4 cameraRotMat = glm::rotate(glm::mat4(1.0f), glm::radians(cameraRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::vec4 rotatedPos = cameraRotMat * glm::vec4(cameraPos, 1.0f);
	rotatedCameraPos = glm::vec3(rotatedPos);

	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 view = glm::lookAt(rotatedCameraPos, cameraTarget, cameraUp);
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);

	GLuint modelLoc = glGetUniformLocation(shaderProgramID, "model");
	GLuint viewLoc = glGetUniformLocation(shaderProgramID, "view");
	GLuint projectionLoc = glGetUniformLocation(shaderProgramID, "projection");

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

	glDrawArrays(GL_LINES, 0, 6);

	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &colorVBO);
	glDeleteVertexArrays(1, &VAO);
}

GLvoid drawScene()
{
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glUseProgram(shaderProgramID);

	// 캐릭터의 현재 위치 가져오기
	glm::vec3 characterPos = Character::getPosition();
	
	// 테일즈런너 스타일 카메라: 측면 상단에서 비스듬하게 봄
	// 목표 카메라 위치 계산
	glm::vec3 cameraGoalPos = glm::vec3(
		characterPos.x + CAMERA_SIDE_OFFSET,      // 캐릭터 옆쪽
		characterPos.y + CAMERA_HEIGHT,           // 캐릭터 위에서
		characterPos.z + CAMERA_BACK_DISTANCE     // 캐릭터 뒤쪽
	);
	
	// 카메라 위치를 부드럽게 이동 (선형 보간)
	cameraPos.x = cameraPos.x + (cameraGoalPos.x - cameraPos.x) * CAMERA_FOLLOW_SPEED;
	cameraPos.y = cameraPos.y + (cameraGoalPos.y - cameraPos.y) * CAMERA_FOLLOW_SPEED;
	cameraPos.z = cameraPos.z + (cameraGoalPos.z - cameraPos.z) * CAMERA_FOLLOW_SPEED;
	
	// 카메라가 바라보는 지점: 캐릭터 중심 약간 위
	cameraTarget.x = characterPos.x;
	cameraTarget.y = characterPos.y + CAMERA_TARGET_HEIGHT;
	cameraTarget.z = characterPos.z;

	DrawAxes();
	//DrawFloor();
	DrawSurvivalMap();
	Character::drawCharacter();

	glutSwapBuffers();
}

GLvoid Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	width = w;
	height = h;
}

GLvoid timer(int value)
{
	const float moveSpeed = 0.15f;

	if (!allAnimationsStopped) {
		// 8방향 입력 처리
		bool moveUp = specialKeyStates[GLUT_KEY_UP];
		bool moveDown = specialKeyStates[GLUT_KEY_DOWN];
		bool moveLeft = specialKeyStates[GLUT_KEY_LEFT];
		bool moveRight = specialKeyStates[GLUT_KEY_RIGHT];

		// 상하좌우 이동
		if (moveUp) {
			Character::moveForward(moveSpeed);
		}
		if (moveDown) {
			Character::moveBackward(moveSpeed);
		}
		if (moveLeft) {
			Character::moveLeft(moveSpeed);
		}
		if (moveRight) {
			Character::moveRight(moveSpeed);
		}

		// 방향키 입력 시에만 회전 (그 방향을 정면으로)
		if (moveUp && moveRight) {
			// 오른쪽 앞 45도
			Character::setTargetRotation(glm::radians(45.0f));
		}
		else if (moveUp && moveLeft) {
			// 왼쪽 앞 45도
			Character::setTargetRotation(glm::radians(315.0f));
		}
		else if (moveDown && moveRight) {
			// 오른쪽 뒤 135도
			Character::setTargetRotation(glm::radians(135.0f));
		}
		else if (moveDown && moveLeft) {
			// 왼쪽 뒤 135도
			Character::setTargetRotation(glm::radians(225.0f));
		}
		else if (moveUp) {
			// 앞 (0도)
			Character::setTargetRotation(glm::radians(360.0f));
		}
		else if (moveDown) {
			// 뒤 (180도)
			Character::setTargetRotation(glm::radians(180.0f));
		}
		else if (moveLeft) {
			// 왼쪽 (-90도)
			Character::setTargetRotation(glm::radians(270.0f));
		}
		else if (moveRight) {
			// 오른쪽 (90도)
			Character::setTargetRotation(glm::radians(90.0f));
		}

		// 스페이스바로 점프 (더블점프 가능)
		if (keyStates[' ']) {
			Character::jump();
			keyStates[' '] = false;
		}
	}

	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);
}

GLvoid keyboard(unsigned char key, int x, int y)
{
	const float cameraSpeed = 0.3f;

	// 일반 키: 상태 저장
	keyStates[key] = true;

	switch (key) {

	case 'o': case 'O': // 모든 움직임 멈추기
		allAnimationsStopped = !allAnimationsStopped;
		std::cout << "모든 움직임: " << (allAnimationsStopped ? "정지" : "재개") << std::endl;
		break;

	case 'c': case 'C': // 초기화
		characterPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		cameraPos = glm::vec3(0.0f, 2.2f, 4.5f);
		cameraRotationY = 0.0f;
		cameraOrbitAngle = 45.0f;
		allAnimationsStopped = false;
		std::cout << "모든 설정 초기화" << std::endl;
		break;

	case 'q': case 'Q': // 종료
		std::cout << "프로그램 종료" << std::endl;
		Character::cleanup();
		exit(0);
		break;
	}
	glutPostRedisplay();
}

// 키를 떼었을 때 호출되는 함수
GLvoid keyboardUp(unsigned char key, int x, int y)
{
	keyStates[key] = false;
}

GLvoid specialKeyboard(int key, int x, int y)
{
	// 특수키: 상태 저장 (방향키)
	specialKeyStates[key] = true;
}

// 특수키를 떼었을 때 호출되는 함수
GLvoid specialKeyboardUp(int key, int x, int y)
{
	specialKeyStates[key] = false;
}