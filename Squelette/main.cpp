#include <Windows.h>
#include <NuiApi.h>
#include <iostream>
#include "Kinect.h"
#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <stdlib.h>

#include "importer.h"
#include "matrixCalc.h"
#include "printScreen.h"

bool test = false;
#define MAX_BONES 32
int nb_bones = 6;
//char * fichierInit = "init_jean.txt";
char * fichierInit = "init_tshirt.txt";
//#define MODEL_FILE "Robe1-bonesW.dae"//"Sweat8AutoW3-10.dae"

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
"float a = scale;"
"mat3 window_scale = mat3("
"vec3(a, 0.0, 0.0),"
"vec3(0.0, a, 0.0),"
"vec3(0.0, 0.0, a)"
");"
"	mat4 boneTrans;"
"	boneTrans = bone_matrices[bone_ids[0]] * weights[0];"
"	boneTrans += bone_matrices[bone_ids[1]] * weights[1];"
"	boneTrans += bone_matrices[bone_ids[2]] * weights[2];"
"	boneTrans += bone_matrices[bone_ids[3]] * weights[3];"
"	st = vtexcoord;"
"	normal = vnormal;"
"	gl_Position = proj * view * model * boneTrans * vec4(vpos.x, vpos.y, vpos.z, 1.0);"
"}";

const GLchar* fragmentSource =
"#version 410 core\n"
"in vec3 normal;"
"in vec2 st;"
"out vec4 outColor;"

"void main(){"
"	outColor = vec4(0.5-normal-0.5, 1.0);"
"}";

const GLchar* fragmentSourceK =
"#version 410 core\n"
"in vec2 coordTexture;"
"uniform sampler2D tex;"
"out vec4 out_Color;"
"void main()"
"{"
"	out_Color = texture(tex, coordTexture);"
"}";

const GLchar* vertexSourceK =
"#version 410 core\n"
"in vec3 in_Vertex;"
"in vec2 in_TexCoord0;"
"out vec2 coordTexture;"
"uniform mat4 view;"
"uniform mat4 proj;"
"void main()"
"{"
"	gl_Position = proj * view * vec4(in_Vertex, 1.0);"
"	coordTexture = in_TexCoord0;"
"}";


//macro pour le vbo
#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(offset) ((char*)NULL + (offset))
#endif

GLFWwindow* initGLFW(int width, int weight, char* title);
void initGLEW();
GLuint createShader(GLenum type, const GLchar* src);
void updateTab(glm::vec3 ** Tab, float * maj);

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

int main(){

	//char* MODEL_FILE = "Jean-W.dae";
	//char* MODEL_FILE = "Robe1-bonesW2.dae";
	char* MODEL_FILE = "TSHIRT-P.dae";
	//char* MODEL_FILE = "Sweat8AutoW3-10.dae";

	FILE* fichierT;

	Kinect kinect;

	if (test){
		fichierT = fopen("\\Users\\Martin\\Desktop\\ColorBasics-D2D-fonctionnel\\skelcoordinates3.txt", "r"); //"bones-ordonnesTestJeu.txt"
		if (fichierT == NULL){
			printf("error loading the file skelcoordinates3.txt\n");
			exit(1);
		}
	}

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
	int screen_width = 1024;
	int screen_height = 768;
	int width = 1024;
	int height = 768;

	/* Initilisation GLFW, GLEW */
	GLFWwindow* window = initGLFW(width, height, "PACT");
	glfwMakeContextCurrent(window);
	initGLEW();
	glEnable(GL_DEPTH_TEST);

	//création du fond
	GLuint vertexShaderK = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShaderK, 1, &vertexSourceK, NULL);
	glCompileShader(vertexShaderK);
	GLuint fragmentShaderK = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderK, 1, &fragmentSourceK, NULL);
	glCompileShader(fragmentShaderK);

	GLuint shaderP = glCreateProgram();
	glAttachShader(shaderP, vertexShaderK);
	glAttachShader(shaderP, fragmentShaderK);
	//glBindFragDataLocation(shaderP, 0, "outColor");
	glLinkProgram(shaderP);
	glUseProgram(shaderP);

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

	loc = glGetAttribLocation(shaderP, "in_TexCoord0");
	glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), BUFFER_OFFSET(3 * sizeof(float)));
	glEnableVertexAttribArray(loc);


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

	// PANTALON : glm::vec3(0.5f, 92.5f, 0.5f)
	// ROBE : glm::vec3(0.5f, 0.5f, 92.5f) de travers !
	// PULL : 
	// TSHIRT : 

	glm::mat4 view = glm::lookAt(
		glm::vec3(3.0f, 3.0f, 3.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 proj = glm::perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);

	GLint uniModel = glGetUniformLocation(shaderProgram, "model");
	model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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

	glUseProgram(shaderP);
	GLint view_mat_locationK = glGetUniformLocation(shaderP, "view");
	glUniformMatrix4fv(view_mat_locationK, 1, GL_FALSE, glm::value_ptr(view));
	GLint proj_mat_locationK = glGetUniformLocation(shaderP, "proj");
	glUniformMatrix4fv(proj_mat_locationK, 1, GL_FALSE, glm::value_ptr(proj));

	FILE* commande;
	char ordre = '0';
	char ordrePrecedent = '1';
	double newTime = 0.0f;
	double elapsedTime = 0.0f;
	bool quitter = false;
	bool init = false;
	bool essai = true;
	
	while (!glfwWindowShouldClose(window) && !quitter){
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);

		if (init){
			/* LIRE nb_bones */
			switch (nb_bones){
			case 0 :
				fichierInit = "";
				MODEL_FILE = "";
				break;
			case 1 :
				fichierInit = "";
				MODEL_FILE = "";
				break;
			case 2:
				fichierInit = "";
				MODEL_FILE = "";
			}

			/* Appel du loader */
			int point_ctr = 0;
			int bone_ctr = 0;
			glm::mat4 bone_offset_matrices[MAX_BONES];
			loadModel(MODEL_FILE, &vao, &point_ctr, bone_offset_matrices, &bone_ctr);
			printf("\nNombre de bones : %i\n", bone_ctr);

			/* Initialisation des matrices de bones */
			glm::mat4 identity = glm::mat4(1.0f);
			int bone_matrices_loc[MAX_BONES];
			char name[64];
			for (int i = 0; i < MAX_BONES; i++){
				sprintf(name, "bone_matrices[%i]", i);
				bone_matrices_loc[i] = glGetUniformLocation(shaderProgram, name); // bone_matrices_loc : matrices de bones dans le programme C
				glUniformMatrix4fv(bone_matrices_loc[i], 1, GL_FALSE, glm::value_ptr(identity));
			}

			view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, 1.0f));
		}

		while (essai){
			kinect.update(texture);
			glUseProgram(shaderP);
			GLint view_mat_locationK = glGetUniformLocation(shaderP, "view");
			glUniformMatrix4fv(view_mat_locationK, 1, GL_FALSE, glm::value_ptr(view));
			GLint proj_mat_locationK = glGetUniformLocation(shaderP, "proj");
			glUniformMatrix4fv(proj_mat_locationK, 1, GL_FALSE, glm::value_ptr(proj));


			static double time = glfwGetTime();
			/*
			do{
			commande = fopen("commandeOuverture.txt", "r");
			if (commande == NULL){
			printf("error reading commande java\n");
			exit(1);
			}
			ordrePrecedent = ordre;
			fscanf(commande, "%c", &ordre);
			fclose(commande);

			if (ordrePrecedent == '1' && ordre == '0'){
			glfwHideWindow(window);
			}

			} while (ordre == '0');
			glfwShowWindow(window);
			*/

			/* Taille de la fenetre */
			glfwGetWindowSize(window, &width, &height);
			//glfwSetWindowPos(window, (int)(screen_width - width) / 4.0, (int)(screen_height - height)/2.0);

			glm::mat4 proj = glm::perspective(45.0f, (float)width / (float)height, 0.1f, 100.0f);
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
			glViewport(0, 0, width, height);

			/* Initialisation */
			glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			/* Rotation du modèle */
			rot1 = 0.0f;
			rot2 = 0.0f;
			if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
				rot1 += 0.07f;

			if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
				rot1 -= 0.07f;

			if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
				rot2 += 0.07f;

			if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
				rot2 -= 0.07f;

			//dessin du fond
			glUseProgram(shaderP);
			glBindVertexArray(vaoFond);

			glBindTexture(GL_TEXTURE_2D, textureFond);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture);
			//(GL_QUADS, 0, 4);
			glDrawPixels(640, 480, GL_BGRA, GL_UNSIGNED_BYTE, texture);
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindVertexArray(0);

			glUseProgram(shaderProgram);
			model = glm::rotate(model, glm::radians(rot2), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(rot1), glm::vec3(0.0f, 0.0f, 1.0f));
			glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));

			glUseProgram(shaderProgramB2);
			glUniformMatrix4fv(bones_model_mat_location2, 1, GL_FALSE, glm::value_ptr(model));

			/* on dessine le vetement */
			glEnable(GL_DEPTH_TEST);
			glUseProgram(shaderProgram);
			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, point_ctr);

			/* puis les positions des os */
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_PROGRAM_POINT_SIZE);
			glUseProgram(shaderProgramB2);
			glBindVertexArray(bones_vao2);
			glDrawArrays(GL_POINTS, 0, bone_ctr + 2);
			glDisable(GL_PROGRAM_POINT_SIZE);

			newTime = glfwGetTime();
			elapsedTime = newTime - time;

			/* readKinectData */
			if (!test){
				readData(Bones);
			}
			else{
				readDataTest(Bones, fichierT);
			}
			updateTab(Bones, bone_positions3);

			glUseProgram(shaderProgramB2);
			glBufferData(
				GL_ARRAY_BUFFER,
				3 * (bone_ctr + 2) * sizeof(float),
				bone_positions3,
				GL_STATIC_DRAW
				);

			glUseProgram(shaderProgram);
			glUniform1f(uniScale, scaleValue);

			/* update les matrices */
			updateData(Bones, bone_matrices);
			int l;
			for (l = 0; l < nb_bones; l++){
				glUseProgram(shaderProgram);
				glUniformMatrix4fv(bone_matrices_loc[l], 1, GL_FALSE, glm::value_ptr(bone_matrices[l]));
			}

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
	}
	glDeleteProgram(shaderProgram);
	glDeleteProgram(shaderProgramB2);
	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShaderB2);
	glDeleteShader(vertexShaderB2);
	glDeleteVertexArrays(1, &vao);
	glDeleteVertexArrays(1, &bones_vao2);

	if (test){
		fclose(fichierT);
	}

	glfwTerminate();
	return 0;
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