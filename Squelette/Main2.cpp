#include <Windows.h>
#include <glew.h>
#include <glfw3.h>
#include <NuiApi.h>
#include <iostream>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "importer.h"
#include "matrixCalc.h"
#include "printScreen.h"
#include "Kinect.h"

#define MAX_BONES 32
int nb_bones = 6;
//char * fichierInit = "init_jean.txt";
char * fichierInit = "init_tshirt.txt";
//#define MODEL_FILE "Robe1-bonesW.dae"//"Sweat8AutoW3-10.dae"

const GLchar* fragmentSourceK =
"#version 410 core\n"
"in vec2 coordTexture;"
"uniform sampler2D tex;"
//"in vec3 Color;"
"out vec4 out_Color;"
"void main() {"
//"   outColor = vec4(Color, 1.0f);"
"	out_Color = texture(tex, coordTexture);"
"}";

const GLchar* vertexSourceK =
"#version 410 core\n"
"in vec3 pos;"
"in vec2 texPos;"
"uniform mat4 model;"
"uniform mat4 view;"
"uniform mat4 proj;"
//"out vec3 Color;"
"out vec2 coordTexture;"
"void main() {"
//"	Color = vec3(texPos, 0.0f);"
"	coordTexture = texPos;"
"   gl_Position = proj*view*model*vec4(pos, 1.0);"
"}";

/* Shaders */

/* Shader pour les points de la Kinect destination */
const GLchar* fragmentSourceB2 =
"#version 410 core\n"
"out vec4 frag_colour;"
"void main() {"
"	frag_colour = vec4(1.0, 0.0, 0.0, 1.0);"
"}";

const GLchar* vertexSourceB2 =
"#version 410 core\n"
"layout(location = 0) in vec3 vp;"
"uniform mat4 proj, view, model;"
"void main() {"
"	gl_PointSize = 7.0;"
"	gl_Position = proj * view * model * vec4(vp, 1.0);"
"}";

/* Shaders pour le vetement */
const GLchar* vertexSource =
"#version 410 core\n"
"layout(location = 0) in vec3 vpos;"
"layout(location = 1) in vec3 vnormal;"
"layout(location = 2) in vec2 vtexcoord;"
"layout(location = 3) in ivec4 bone_ids;"
"layout(location = 4) in vec4 weights;"

"out vec3 normal;"
"out vec2 st;"

"uniform mat4 model;"
"uniform mat4 view;"
"uniform mat4 proj;"
"uniform mat4 bone_matrices[32];"
"uniform float scale;"

"void main(){"
"float a = 0.50;"
"mat4 window_scale = mat4("
"vec4(a, 0.0, 0.0, 0.0),"
"vec4(0.0, a, 0.0, 0.0),"
"vec4(0.0, 0.0, a, 0.0),"
"vec4(0.0, 0.0, 0.0, 1.0)); "
"	mat4 boneTrans;"
"	boneTrans = bone_matrices[bone_ids[0]] * weights[0];"
"	boneTrans += bone_matrices[bone_ids[1]] * weights[1];"
"	boneTrans += bone_matrices[bone_ids[2]] * weights[2];"
"	boneTrans += bone_matrices[bone_ids[3]] * weights[3];"
"	st = vtexcoord;"
"	normal = vnormal;"
"	gl_Position = proj * view * model * boneTrans * window_scale * vec4(vpos.x, vpos.y, vpos.z, 1.0);"
"}";

const GLchar* fragmentSource =
"#version 410 core\n"
"in vec3 normal;"
"in vec2 st;"
"out vec4 outColor;"

"void main(){"
"	outColor = vec4(0.5-normal-0.5, 1.0);"
"}";

GLFWwindow* initGLFW(int width, int weight, char* title);
void initGLEW();
GLuint createShader(GLenum type, const GLchar* src);
void updateTab(glm::vec3 ** Tab, float * maj);

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

int main()
{

	//char* MODEL_FILE = "Jean-W.dae";
	//char* MODEL_FILE = "Robe1-bonesW2.dae";
	char* MODEL_FILE = "TSHIRT-PS.dae";
	//char* MODEL_FILE = "Sweat8AutoW3-10.dae";

	/*if (!Send::activer())
	cout << "Erreur lors de la création du pipe" << endl;*/

	FILE* fichierT;

	/* Le tableau de bones : contiendra les positions des os */
	glm::vec3 ** Bones;
	glm::mat4 * bone_matrices = (glm::mat4 *)malloc(nb_bones * sizeof(glm::mat4));
	Bones = (glm::vec3 **)malloc(nb_bones * sizeof(glm::vec3 *));
	int b1;
	for (b1 = 0; b1 < nb_bones; b1++){
		Bones[b1] = (glm::vec3 *)malloc(4 * sizeof(glm::vec3));
	}

	/* positions initiales des os du modele dans un txt pour traitement */
	FILE* fichier2 = fopen(fichierInit, "r");
	if (fichier2 == NULL){
		printf("Error loading the init file\n");
		exit(1);
	}
	/* charge les données précédentes */
	initData(Bones, fichier2);
	fclose(fichier2);

	/* variables */
	float rot1 = 0.0f;
	float rot2 = 0.0f;
	float rot3 = 0.0f;
	float rotShoulders = 0.0f;
	glm::vec3 normShoulders = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 transla = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 transla2 = glm::vec3(0.0f, 0.0f, 0.0f);
	int screen_width = 1024;
	int screen_height = 768;
	int width = 1024;
	int height = 768;

	Kinect kinect;

	//glfwSetErrorCallback(error_callback);

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

	initGLEW();
	glEnable(GL_DEPTH_TEST);


	/* le vao du vetement */
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Appel du loader */
	int point_ctr = 0;
	int bone_ctr = 0;
	glm::mat4 bone_offset_matrices[MAX_BONES];
	loadModel(MODEL_FILE, &vao, &point_ctr, bone_offset_matrices, &bone_ctr);
	printf("\nNombre de bones : %i\n", bone_ctr);

	/* Les positions des os du modele de vetement et des données Kinect pour representation */
	float * bone_positions3 = (float *)malloc(27 * sizeof(float));
	float bone_positions4[] = {
		0.031702, -0.305855, 0.561678,
		-0.053113, -0.306185, 0.490401,
		-0.136382, -0.286400, 0.348958,
		-0.212196, -0.227865, 0.227038,
		0.032215, -0.317140, 0.336961,
		0.033871, -0.300125, 0.293703,
		0.110854, -0.314255, 0.495051,
		0.269807, -0.312715, 0.207836,
		0.192558, -0.337995, 0.328809,
	};
	int h;
	for (h = 0; h < 27; h++){
		bone_positions3[h] = bone_positions4[h];
	}

	/* de même pour les os Kinect */
	GLuint bones_vao2;
	glGenVertexArrays(1, &bones_vao2);
	glBindVertexArray(bones_vao2);
	GLuint bones_vbo2;
	glGenBuffers(1, &bones_vbo2);
	glBindBuffer(GL_ARRAY_BUFFER, bones_vbo2);
	glBufferData(
		GL_ARRAY_BUFFER,
		3 * (bone_ctr + 2) * sizeof(float),
		bone_positions3,
		GL_STATIC_DRAW
		);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);
	GLuint vertexShaderB2 = createShader(GL_VERTEX_SHADER, vertexSourceB2);
	GLuint fragmentShaderB2 = createShader(GL_FRAGMENT_SHADER, fragmentSourceB2);

	GLuint shaderProgramB2 = glCreateProgram();
	glAttachShader(shaderProgramB2, vertexShaderB2);
	glAttachShader(shaderProgramB2, fragmentShaderB2);
	glBindFragDataLocation(shaderProgramB2, 0, "outColor");
	glLinkProgram(shaderProgramB2);

	/* Gestion des shaders du modele de vetement */
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentSource);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glBindFragDataLocation(shaderProgram, 0, "outColor");
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);

	/* Initialisation des matrices de bones */
	glm::mat4 identity = glm::mat4(1.0f);
	int bone_matrices_loc[MAX_BONES];
	char name[64];
	for (int i = 0; i < MAX_BONES; i++){
		sprintf(name, "bone_matrices[%i]", i);
		bone_matrices_loc[i] = glGetUniformLocation(shaderProgram, name); // bone_matrices_loc : matrices de bones dans le programme C
		glUniformMatrix4fv(bone_matrices_loc[i], 1, GL_FALSE, glm::value_ptr(identity));
	}

	/* Les matrices model, view, projection sont initialisées */
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	// PANTALON : glm::vec3(0.5f, 92.5f, 0.5f)
	// ROBE : glm::vec3(0.5f, 0.5f, 92.5f) de travers !
	// PULL : 
	// TSHIRT : glm::vec3(0.5f, 32.5f, 0.5f)

	glm::mat4 view = glm::lookAt(
		glm::vec3(0.5f, 10.5f, 0.5f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 proj = glm::perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);

	GLint uniModel = glGetUniformLocation(shaderProgram, "model");
	glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));

	GLint uniView = glGetUniformLocation(shaderProgram, "view");
	glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

	GLint uniProj = glGetUniformLocation(shaderProgram, "proj");
	glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

	/* gere le scaling du modele */
	float scaleValue = 1.0f;
	GLint uniScale = glGetUniformLocation(shaderProgram, "scale");
	glUniform1f(uniScale, scaleValue);

	/* lien avec les uniform mat des 2 shaders des os */
	glUseProgram(shaderProgramB2);
	GLint bones_view_mat_location2 = glGetUniformLocation(shaderProgramB2, "view");
	glUniformMatrix4fv(bones_view_mat_location2, 1, GL_FALSE, glm::value_ptr(view));
	GLint bones_proj_mat_location2 = glGetUniformLocation(shaderProgramB2, "proj");
	glUniformMatrix4fv(bones_proj_mat_location2, 1, GL_FALSE, glm::value_ptr(proj));
	GLint bones_model_mat_location2 = glGetUniformLocation(shaderProgramB2, "model");
	glUniformMatrix4fv(bones_model_mat_location2, 1, GL_FALSE, glm::value_ptr(model));

	//création du fond
	glViewport(0, 0, 640, 480);

	GLuint vaoF;
	glGenVertexArrays(1, &vaoF);
	glBindVertexArray(vaoF);

	//Vertex Buffer Object
	GLuint vboF;
	glGenBuffers(1, &vboF); // Generate 1 buffer

	float vertices[] = {
		// premiere face
		-25.0f, 25.0f, -12.0f, 1.0f, 1.0f, //modifier troisieme coordonnée pour gérer la profondeur
		25.0f, 25.0f, -12.0f, 0.0f, 1.0f, 

		25.0f, -25.0f, -12.0f, 0.0f, 0.0f,
		25.0f, -25.0f, -12.0f, 0.0f, 0.0f,

		-25.0f, -25.0f, -12.0f, 1.0f, 0.0f,//
		-25.0f, 25.0f, -12.0f, 1.0f, 1.0f,//
	};
	glBindBuffer(GL_ARRAY_BUFFER, vboF);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);


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

	//Fait le lien entre les données des vertex et les attributs
	GLint posAttrib = glGetAttribLocation(shaderP, "pos");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);

	GLint texAttrib = glGetAttribLocation(shaderP, "texPos");
	glEnableVertexAttribArray(texAttrib);
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));

	GLint uniModel2 = glGetUniformLocation(shaderP, "model");
	GLint uniView2 = glGetUniformLocation(shaderP, "view");
	GLint uniProj2 = glGetUniformLocation(shaderP, "proj");

	glm::mat4 modelP = glm::mat4(1.0f);
	modelP = glm::rotate(modelP, glm::radians(-90.0f), glm::vec3(1.0f,0.0f, 0.0f));
	glUniformMatrix4fv(uniModel2, 1, GL_FALSE, glm::value_ptr(modelP));
	glUniformMatrix4fv(uniView2, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(uniProj2, 1, GL_FALSE, glm::value_ptr(proj));

	glUseProgram(shaderP);

	GLuint textureFond;
	glGenTextures(1, &textureFond);

	glBindTexture(GL_TEXTURE_2D, textureFond);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	GLubyte* texture = (GLubyte *)malloc(sizeof(char) * 640 * 480 * 4);
	memset(texture, 0xFF, sizeof(char) * 640 * 480 * 4);

	double newTime = 0.0f;
	double elapsedTime = 0.0f;

	while (!glfwWindowShouldClose(window))		//boucle principale
	{
		kinect.update(texture, Bones);
		readData(Bones);
		static double time = glfwGetTime();

		glfwGetWindowSize(window, &width, &height);

		glm::mat4 proj = glm::perspective(45.0f, (float)width / (float)height, 0.1f, 100.0f);
		glUseProgram(shaderProgram);
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
		glViewport(0, 0, width, height);

		glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		/* Rotation du modèle */
		model = glm::mat4(1.0f);
		model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		rot1 = 0.0f;
		rot2 = 1.0f;
		transla = glm::vec3(0.0f, 0.0f, 0.0f);



		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
			rot1 += 0.07f;

		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
			rot1 -= 0.07f;

		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			rot2 += 0.07f;

		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			rot2 -= 0.07f;

		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
			rot3 += 0.07f;

		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
			rot3 -= 0.07f;

		transla = 7.0f*getTrans(Bones[0][0], Bones[0][2]);
		rotShoulders = getRot(Bones[2][1], Bones[4][1], Bones[2][3], Bones[4][3]);
		normShoulders = getNormal(Bones[2][0], Bones[4][0], Bones[2][3], Bones[4][3]);
		transla2 = getTrans(Bones[1][1], Bones[1][3]);

		glUseProgram(shaderProgram);
		model = glm::translate(model, glm::vec3(0.0f, rot1, 0.0f));
		model = glm::translate(model, transla);
		model = glm::translate(model, transla2);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, rot3));
		model = glm::rotate(model, -rotShoulders, normShoulders);
		model = glm::scale(model, glm::vec3(1.0, rot2, 1.0));
		glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));

		/* on dessine le vetement */
		glUseProgram(shaderProgram);
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, point_ctr);
	
		/* puis les positions des os */
/*		glEnable(GL_PROGRAM_POINT_SIZE);
		glUseProgram(shaderProgramB2);
		glBindVertexArray(bones_vao2);
		glDrawArrays(GL_POINTS, 0, bone_ctr + 2);
		glDisable(GL_PROGRAM_POINT_SIZE);
*/
		glUseProgram(shaderP);
		uniProj2 = glGetUniformLocation(shaderP, "proj");
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

		uniView2 = glGetUniformLocation(shaderP, "view");
		glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

		glEnable(GL_DEPTH_TEST);
		glBindVertexArray(vaoF);
		glBindTexture(GL_TEXTURE_2D, textureFond);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);

		newTime = glfwGetTime();
		elapsedTime = newTime - time;


		glUseProgram(shaderProgramB2);
/*		glBufferData(
			GL_ARRAY_BUFFER,
			3 * (bone_ctr + 2) * sizeof(float),
			bone_positions3,
			GL_STATIC_DRAW
			);
*/
		glUseProgram(shaderProgram);
		glUniform1f(uniScale, scaleValue);

		/* update les matrices */
		updateData(Bones, bone_matrices);
		int l;
		for (l = 0; l < nb_bones; l++){
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[l], 1, GL_FALSE, glm::value_ptr(bone_matrices[l]));
		}
		
		glUseProgram(shaderProgramB2);
		glUniformMatrix4fv(bones_model_mat_location2, 1, GL_FALSE, glm::value_ptr(model));
		
		newTime = glfwGetTime();
		elapsedTime = newTime - time;
		while (elapsedTime < 0.04){
			newTime = glfwGetTime();
			elapsedTime = newTime - time;
		}
		time = newTime;

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glUseProgram(0);
	glfwDestroyWindow(window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}


GLFWwindow* initGLFW(int width, int height, char* title){
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_DECORATED, GL_FALSE); // enleve decoration fenetre

	return glfwCreateWindow(width, height, title, NULL, NULL);
}

void initGLEW(){
	glewExperimental = GL_TRUE;
	glewInit();
}

GLuint createShader(GLenum type, const GLchar* src){
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	return shader;
}

void updateTab(glm::vec3 ** Tab, float * maj){
	int i, k;
	k = 0;
	for (i = 0; i < nb_bones; i++){
		maj[k] = Tab[i][3].x;
		k++;
		maj[k] = Tab[i][3].y;
		k++;
		maj[k] = Tab[i][3].z;
		k++;
	}
	maj[k] = Tab[0][3].x;
	maj[k + 1] = Tab[0][3].y;
	maj[k + 2] = Tab[0][3].z;
}