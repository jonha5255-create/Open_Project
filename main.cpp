#define _CRT_SECURE_NO_WARNINGS 
#define STB_IMAGE_IMPLEMENTATION
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
#include "octopus.h"
#include "stb_image.h"

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
GLuint shaderProgramID, g_wallTextureID;
GLuint vertexShader;
GLuint fragmentShader;
GLuint tVAO = 0, tVBO = 0;
GLuint VAO, VBO;

// 캐릭터 위치 및 상태
glm::vec3 characterPosition = glm::vec3(0.0f, 0.0f, 0.0f);

// 카메라
glm::vec3 cameraPos = glm::vec3(0.0f, 2.2f, 5.0f);
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.8f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraRotationY = 0.0f;
float cameraOrbitAngle = 45.0f;
float cameraOrbitRadius = 5.0f;

// 카메라 설정 상수
static const float CAMERA_SIDE_OFFSET = 0.0f;   // 캐릭터 옆쪽 오프셋 (좌우)
static const float CAMERA_BACK_DISTANCE = 4.5f; // 캐릭터 뒤쪽 거리
static const float CAMERA_HEIGHT = 2.5f;        // 캐릭터 위의 높이
static const float CAMERA_FOLLOW_SPEED = 0.12f; // 카메라 따라가는 속도 (부드러움)
static const float CAMERA_TARGET_HEIGHT = 0.8f; // 카메라가 바라보는 높이

// 애니메이션 제어
bool cameraOrbitAnimation = false;         // 카메라 공전 애니메이션
bool allAnimationsStopped = false;         // 모든 움직임 정지


void InitBuffer();
void UpdateCameraPosition();
void InitTexture(const char* filename);
void DrawTexturedCube(GLuint shaderID, glm::mat4 modelMat, glm::vec3 scale);

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
	InitTexture("MAP_WALL.jpg");


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

// 1. 텍스처 파일 로드 함수 (PDF 방식 적용)
void InitTexture(const char* filename) {
	glGenTextures(1, &g_wallTextureID);
	glBindTexture(GL_TEXTURE_2D, g_wallTextureID);

	// 텍스처 파라미터 설정 (반복, 선형 보간)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// 이미지 로드
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // 이미지 뒤집힘 방지
	unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);

	if (data) {
		GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		std::cout << "텍스처 로드 실패: " << filename << std::endl;
	}
	stbi_image_free(data);
}

// 2. 텍스처가 적용된 큐브 그리기 함수 (새로 만듦)
void DrawTexturedCube(GLuint shaderID, glm::mat4 modelMat, glm::vec3 scale) {
	// 위치(3) + 법선(3) + UV좌표(2) = 8개 데이터
	static float texCubeVertices[] = {
		// 뒤
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
		// 앞
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
		// 왼쪽
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		// 오른쪽
		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		 // 아래
		 -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
		  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
		  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		 -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
		 -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
		 // 위
		 -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
		  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
		  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		 -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
		 -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
	};
	
	if (tVAO == 0) {
		glGenVertexArrays(1, &tVAO);
		glGenBuffers(1, &tVBO);
		glBindVertexArray(tVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(texCubeVertices), texCubeVertices, GL_STATIC_DRAW);

		// 0: 위치 (3개)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// 1: 법선 (3개) - 기존 Vertex Shader location 2번(Normal)에 맞추기 위해 조절 필요할 수 있음
		// 사용자 파일 기준: location 1=Color, 2=Normal 일수도 있음.
		// acting3_vertex.glsl 확인 결과: 0=Pos, 1=Color, 2=Normal 이었으나, 
		// 텍스처 매핑을 위해 Shader를 수정해야 함. (아래 3단계 참조)

		// **[중요]** 쉐이더 location 수정에 맞춰서 연결
		// 0: Pos, 1: Normal, 2: TexCoord 로 통일합니다.
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
	}

	glBindVertexArray(tVAO);

	// 모델 행렬 전송
	modelMat = glm::scale(modelMat, scale);
	glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(modelMat));

	// 텍스처 사용 플래그 켜기
	glUniform1i(glGetUniformLocation(shaderID, "useTexture"), true);

	// 텍스처 바인딩
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_wallTextureID);
	glUniform1i(glGetUniformLocation(shaderID, "wallTexture"), 0);

	glDrawArrays(GL_TRIANGLES, 0, 36);

	// 텍스처 사용 플래그 끄기 (다른 물체에 영향 안 주게)
	glUniform1i(glGetUniformLocation(shaderID, "useTexture"), false);

	glBindVertexArray(0);
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
	//GLuint lightPosLoc = glGetUniformLocation(shaderProgramID, "lightPos");
	//GLuint lightColorLoc = glGetUniformLocation(shaderProgramID, "lightColor");
	GLuint viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMat));
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMat));
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMat));
	//glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
	//glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
	glUniform3fv(viewPosLoc, 1, glm::value_ptr(rotatedCameraPos));

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &colorVBO);
	glDeleteVertexArrays(1, &VAO);
}

void DrawSurvivalMap()
{
	const int MAP_WIDTH = 5; // 맵의 크기를 넓게 설정
	const int MAP_LENGTH = 150; // 맵의 길이
	const int TUNNEL_HEIGHT = 5; // 터널 높이

	// 텍스처 유니폼 위치 가져오기
	GLuint useTextureLoc = glGetUniformLocation(shaderProgramID, "useTexture");

	// 벽 그리기용 텍스처 바인딩
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_wallTextureID); // LoadTexture로 생성한 ID 변수명 확인 필요

	// 바닥을 구성할 작은 사각형(Quad)을 반복문으로 배치합니다.
	for (int z = -10; z < MAP_LENGTH; ++z) {
		for (int x = -MAP_WIDTH; x < MAP_WIDTH; ++x) {
			// 1. 바닥 (Floor) 그리기 - [수정] 빨간 발판 조건문 제거
			glm::mat4 modelFloor = glm::translate(glm::mat4(1.0f), glm::vec3(x * 1.0f, -1.0f, z * 1.0f));

			DrawTexturedCube(shaderProgramID, modelFloor, glm::vec3(1.5f, 0.1f, 1.0f));

			// 벽 그리기
			// 3. 양옆 벽 (Walls) 그리기
			if (abs(x) == MAP_WIDTH) {
				float wallH = (float)TUNNEL_HEIGHT + 1.0f;
				float wallY = (wallH / 2.0f) - 1.0f;

				glm::mat4 modelWall = glm::translate(glm::mat4(1.0f), glm::vec3(x * 1.0f, wallY, z * 1.0f));
				glm::mat4 modelWall2 = glm::translate(glm::mat4(1.0f), glm::vec3(-x * 1.0f, wallY, z * 1.0f));

				glUniform1i(useTextureLoc, true);

				DrawTexturedCube(shaderProgramID, modelWall, glm::vec3(0.5f, wallH, 1.0f));
				DrawTexturedCube(shaderProgramID, modelWall2, glm::vec3(0.5f, wallH, 1.0f));
			}
		}
	}
	glUniform1i(useTextureLoc, false);
}

GLvoid drawScene()
{
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glUseProgram(shaderProgramID);

	// 캐릭터의 현재 위치 가져오기
	glm::vec3 characterPos = Character::getPosition();
	// 목표 카메라 위치 계산
	glm::vec3 cameraGoalPos = glm::vec3(
		characterPos.x + CAMERA_SIDE_OFFSET,      // 캐릭터 옆쪽
		characterPos.y + CAMERA_HEIGHT,           // 캐릭터 위에서
		characterPos.z - CAMERA_BACK_DISTANCE     // 캐릭터 뒤쪽
	);
	// 카메라 위치를 부드럽게 이동 (선형 보간)
	cameraPos.x = cameraPos.x + (cameraGoalPos.x - cameraPos.x) * CAMERA_FOLLOW_SPEED;
	cameraPos.y = cameraPos.y + (cameraGoalPos.y - cameraPos.y) * CAMERA_FOLLOW_SPEED;
	cameraPos.z = cameraPos.z + (cameraGoalPos.z - cameraPos.z) * CAMERA_FOLLOW_SPEED;

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

	// 조명 위치와 색상 설정
	glm::vec3 lightPos = glm::vec3(characterPos.x, characterPos.y + 5.0f, characterPos.z - 0.5f);
	glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

	GLuint lightPosLoc = glGetUniformLocation(shaderProgramID, "lightPos");
	GLuint lightColorLoc = glGetUniformLocation(shaderProgramID, "lightColor");
	glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
	glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
	
	// 카메라가 바라보는 지점: 캐릭터 중심 약간 위
	cameraTarget.x = characterPos.x;
	cameraTarget.y = characterPos.y + CAMERA_TARGET_HEIGHT;
	cameraTarget.z = characterPos.z;

	//DrawAxes();
	DrawSurvivalMap();
	Character::drawCharacter();

	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
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

		bool ismoving = moveUp or moveDown or moveLeft or moveRight;

		Character::setRunning(ismoving);

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

		if (moveUp && moveRight) {
			Character::setTargetRotation(glm::radians(-45.0f)); 
		}
		else if (moveUp && moveLeft) {
			Character::setTargetRotation(glm::radians(45.0f)); 
		}
		else if (moveDown && moveRight) {
			Character::setTargetRotation(glm::radians(-135.0f)); 
		}
		else if (moveDown && moveLeft) {
			Character::setTargetRotation(glm::radians(135.0f)); 
		}
		else if (moveUp) {
			// 정면(+Z)으로 갈 때 0도 (모델이 +Z를 보고 있다고 가정)
			Character::setTargetRotation(glm::radians(0.0f));
		}
		else if (moveDown) {
			// 뒤(-Z)로 갈 때 180도
			Character::setTargetRotation(glm::radians(180.0f));
		}
		else if (moveLeft) {
			// 왼쪽(+X) -> 카메라가 뒤집혔으므로 왼쪽 키 누르면 +X 방향
			Character::setTargetRotation(glm::radians(90.0f));
		}
		else if (moveRight) {
			// 오른쪽(-X)
			Character::setTargetRotation(glm::radians(-90.0f));
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