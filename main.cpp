#define _CRT_SECURE_NO_WARNINGS 
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <time.h> //랜덤함수
#include <algorithm>
#include <vector>
#include "Character.cpp"

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>



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
	//--- 프로그램맨앞에선언할것
	// Open file for reading 
	// Return NULL on failure 
	// Seek to the end of the file 
	// Find out how many bytes into the file we are 
	// Allocate a buffer for the entire length of the file and a null terminator 
	// Go back to the beginning of the file 
	// Read the contents of the file in to the buffer 
	// Close the file 
	// Null terminator 
	// Return the buffer 
}




void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
GLvoid drawScene();
GLvoid Reshape(int w, int h);
//--- 필요한변수선언


GLint width, height;
GLuint shaderProgramID; //--- 세이더 프로그램 이름
GLuint vertexShader;    //--- 버텍스세이더객체
GLuint fragmentShader;





void main(int argc, char** argv)
//--- 윈도우출력하고콜백함수설정
{
	width = 500;
	height = 500;
	//--- 윈도우생성하기
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(width, height);
	glutCreateWindow("Example 8");
	//--- GLEW 초기화하기
	glewExperimental = GL_TRUE;
	glewInit();
	//--- 세이더읽어와서세이더프로그램만들기: 사용자정의함수호출
	make_vertexShaders();
	//--- 버텍스세이더만들기
	make_fragmentShaders();
	//--- 프래그먼트세이더만들기
	shaderProgramID = make_shaderProgram();
	//--- 세이더프로그램만들기
	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);

	//랜덤 시드 초기화
	srand((unsigned int)time(NULL));

	glutMainLoop();
}

void make_vertexShaders()
{
	GLchar* vertexSource;
	//--- 버텍스세이더읽어저장하고컴파일하기
	//--- filetobuf: 사용자정의 함수로 텍스트를읽어서문자열에저장하는함수
	vertexSource = filetobuf("vertex.glsl");

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
//--- 프래그먼트세이더객체만들기
void make_fragmentShaders()
{
	GLchar* fragmentSource;
	//--- 프래그먼트세이더읽어저장하고컴파일하기
	fragmentSource = filetobuf("fragment.glsl");    // 프래그세이더 읽어오기

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
	GLchar* errorLog = NULL;
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
		//--- 세이더프로그램만들기
	   //--- 세이더프로그램에버텍스세이더붙이기
	   //--- 세이더프로그램에프래그먼트세이더붙이기
	   //--- 세이더프로그램링크하기
	   //--- 세이더객체를세이더프로그램에링크했음으로,세이더객체자체는삭제가능
	   // ---세이더가 잘연결되었는지체크하기
		std::cerr << "ERROR: shader program 연결 실패\n" << errorLog << std::endl;
		return false;
	}
	glUseProgram(shaderID);
	return shaderID;
	//--- 만들어진세이더프로그램사용하기
   //--- 여러 개의세이더프로그램만들수있고, 그중한개의프로그램을사용하려면
   //--- glUseProgram 함수를 호출하여사용할특정프로그램을지정한다.
	//--- 사용하기직전에호출할수있다.
}
// 유니폼 함수는 선언을 해야 에러가 안뜬다.

GLvoid drawScene()
{
	//--- 콜백 함수: 그리기콜백함수
	GLfloat rColor, gColor, bColor;
	rColor = gColor = 0.7;
	bColor = 0.8;
	glClearColor(rColor, gColor, bColor, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(shaderProgramID);
	// 도형 그리기


	glBindVertexArray(0);
	glutSwapBuffers();
}
//--- 다시그리기콜백함수
GLvoid Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	//--- 배경색을파랑색으로설정
	width = w;
	height = h;
	//--- 렌더링하기: 0번인덱스에서1개의버텍스를사용하여점그리기
	// 화면에출력하기
	//--- 콜백 함수: 다시 그리기 콜백 함수
}