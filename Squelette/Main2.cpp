#include <Windows.h>
#include <glew.h>
#include <glfw3.h>
#include <NuiApi.h>
#include <iostream>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include "Kinect.h"

/*const GLchar* vertexSourceK =
"#version 150 core\n"
"in vec3 in_Vertex;"
"in vec2 in_TexCoord0;"
"out vec2 coordTexture;"
"void main()"
"{"
"	gl_Position = vec4(in_Vertex, 1.0);"
"	coordTexture = in_TexCoord0;"
"}";

const GLchar* fragmentSourceK =
"#version 150 core\n"
"in vec2 coordTexture;"
"uniform sampler2D tex;"
"out vec4 out_Color;"
"void main()"
"{"
"	out_Color = texture(tex, coordTexture);"
"}";*/


//macro pour le vbo
#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(offset) ((char*)NULL + (offset))
#endif

using namespace std;
using namespace glm;

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}


int main2()
{
	/*if (!Send::activer())
	cout << "Erreur lors de la création du pipe" << endl;*/

	Kinect kinect;

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_DECORATED, GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(640, 480, "", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetWindowPos(window, 100, 100);
	glfwShowWindow(window);

	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);

	glewExperimental = GL_TRUE;
	glewInit();

	glEnable(GL_DEPTH_TEST);

	mat4 proj = glm::perspective(70.0f*3.14f / 180.0f, 640.0f / 480.0f, 0.1f, 100.0f);
	mat4 view = mat4(1.0);

	//on gère le fond
	
	/*
	Shader shaderFond("VertexShaderFond.txt", "FragmentShaderFond.txt");
	shaderFond.charger();
	*/

	
	GLuint vertexShaderK = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShaderK, 1, &vertexSourceK, NULL);
	glCompileShader(vertexShaderK);
	GLuint fragmentShaderK = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderK, 1, &fragmentSourceK, NULL);
	glCompileShader(fragmentShaderK);

	GLuint shaderP = glCreateProgram();
	glAttachShader(shaderP, vertexShaderK);
	glAttachShader(shaderP, fragmentShaderK);
	glBindFragDataLocation(shaderP, 0, "outColor");
	glLinkProgram(shaderP);
	glUseProgram(shaderP);
	

	glViewport(0, 0, 640, 480);

	float fond[] = //coordonnées à changer plus tard
	{
		//coordonnées 3D	coord texture
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,	//bas gauche
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,	//haut gauche
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,	//bas droit
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f	//haut droit
	};

	GLuint vboFond, vaoFond;

	glUseProgram(shaderP);

	glGenBuffers(1, &vboFond);
	glBindBuffer(GL_ARRAY_BUFFER, vboFond);

	glGenVertexArrays(1, &vaoFond);
	glBindVertexArray(vaoFond);

	glBufferData(GL_VERTEX_ARRAY, 20 * sizeof(float), fond, GL_STATIC_DRAW);


	GLint loc = glGetAttribLocation(shaderP, "in_Vertex");
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(loc);

	GLint loc2 = glGetAttribLocation(shaderP, "in_TexCoord0");
	glVertexAttribPointer(loc2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), BUFFER_OFFSET(3 * sizeof(float)));
	glEnableVertexAttribArray(loc2);


	glBindBuffer(GL_VERTEX_ARRAY, 0);
	glBindVertexArray(0);

	GLuint textureFond;
	glGenTextures(1, &textureFond);

	glBindTexture(GL_TEXTURE_2D, textureFond);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	GLubyte* texture = (GLubyte *)malloc(sizeof(char) * 640 * 480 * 4);
	memset(texture, 0xFF, sizeof(char) * 640 * 480 * 4);

	view = glm::lookAt(
		vec3(3.0f, 3.0f, 3.0f),
		vec3(0.0f, 0.0f, 0.0f),
		vec3(0.0f, 1.0f, 0.0f));

	while (!glfwWindowShouldClose(window))		//boucle principale
	{
		kinect.update(texture);

		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		GLint uniProj = glGetUniformLocation(shaderP, "proj");
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

		GLint uniView = glGetUniformLocation(shaderP, "view");
		glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

		//dessin du fond

		glBindVertexArray(vaoFond);

		glBindTexture(GL_TEXTURE_2D, textureFond);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture);

		glDrawArrays(GL_QUADS, 0, 4);

		glBindTexture(GL_TEXTURE_2D, 0);

		glBindVertexArray(0);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glUseProgram(0);
	glfwDestroyWindow(window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}