#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdlib.h>

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
int numVet = 0;
//char * fichierInit = "init_exploit-new10.txt";
//char * fichierInit = "init_robe1.txt";
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

GLFWwindow* initGLFW(int width, int weight, char* title);
void initGLEW();
GLuint createShader(GLenum type, const GLchar* src);
void javaCommande(FILE* fichierComm, bool * quitter, bool * essai, int * vetement);

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	TCHAR chRequest[BUFSIZE];
	DWORD cbRead;
	TCHAR chReply[BUFSIZE];
	DWORD cbToWrite;
} PIPEINST, *LPPIPEINST;

double RightHandX = 7.387529837;
double RightHandY = 1.3673536;


VOID DisconnectAndClose(LPPIPEINST);
BOOL CreateAndConnectInstance(LPOVERLAPPED);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
char *GetAnswerToRequest(LPPIPEINST);
char *ConvertToCharPtr(_TCHAR tc[BUFSIZE]);
void fillingChart(char* tab, double a);

VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

HANDLE hPipe;

static HANDLE myThread = NULL;
DWORD WINAPI main_thread_proc(LPVOID lpThreadParameter);

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

int do_launch_access()
{
	DWORD thread_id;
	if (myThread) return 0;

	myThread = CreateThread(NULL, 1000, main_thread_proc, NULL, 0, &thread_id);
}

int main()
{
	do_launch_access();
	while (1) {/* variables */
		float rot1 = 0.0f;
		float rot2 = 0.0f;
		float rot3 = 0.0f;
		float vitRot = 1.0f;
		float vitTrans = 1.0f;
		float rotShoulders = 0.0f;
		glm::vec3 normShoulders = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 transla = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 transla2 = glm::vec3(0.0f, 0.0f, 0.0f);
		int screen_width = 1024;
		int screen_height = 768;
		int width = 864;
		int height = 1152;

		Kinect kinect;

		FILE* fichierCommande = fopen("commandes.txt", "r+");
		if (fichierCommande == NULL){
			printf("Error loading the command file\n");
			exit(1);
		}

		fprintf(fichierCommande, "0 1 0");

		bool quitter = false;
		bool essai = false;
		int vetement = 0;
		int ancienVetement = -1;

		glm::vec3 hipRef = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 hipPos = glm::vec3(0.0f, 0.0f, 0.0f);

		glm::vec3 refRight = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 posRight = glm::vec3(0.0f, 0.0f, 0.0f);

		glm::vec3 refLeft = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 posLeft = glm::vec3(0.0f, 0.0f, 0.0f);

		glm::vec3 centerRef = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 centerPos = glm::vec3(0.0f, 0.0f, 0.0f);

		//char* MODEL_FILE = "Robe1-bonesW2.dae";
		//char* MODEL_FILE = "Robe1-bonesW2.dae";
		char* MODEL_FILE = "TSHIRT-PS.dae";
		//char* MODEL_FILE = "Sweat8AutoW3-10.dae";

		/*if (!Send::activer())
		cout << "Erreur lors de la création du pipe" << endl;*/

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


		//glfwSetErrorCallback(error_callback);

		if (!glfwInit())
			exit(EXIT_FAILURE);

		//glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		//glfwWindowHint(GLFW_DECORATED, GL_FALSE);

		GLFWwindow* window = glfwCreateWindow(height, width, "", NULL, NULL);
		if (!window)
		{
			glfwTerminate();
			exit(EXIT_FAILURE);
		}

		glfwSetWindowPos(window, 150, 150);
		glfwShowWindow(window);
		glfwMakeContextCurrent(window);
		glfwSetKeyCallback(window, key_callback);
		

		initGLEW();
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

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
		// PULL : glm::vec3(0.5f, 3.5f, 0.5f),
		// TSHIRT : glm::vec3(0.5f, 10.5f, 0.5f)

		glm::mat4 view = glm::lookAt(
			glm::vec3(0.1f, 10.5f, 0.1f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 proj = glm::perspective(45.0f, (float)height / (float)width, 0.1f, 200.0f);

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

		/* Deuxieme vêtement PULL */
		GLuint vaoX;
		glGenVertexArrays(1, &vaoX);
		glBindVertexArray(vaoX);

		/* Appel du loader */
		int point_ctrX = 0;
		int bone_ctrX = 0;
		glm::mat4 bone_offset_matricesX[MAX_BONES];
		char * MODEL_FILEX = "Sweat8AutoW3-10.dae";
		loadModel(MODEL_FILEX, &vaoX, &point_ctrX, bone_offset_matricesX, &bone_ctrX);
		printf("\nNombre de bones : %i\n", bone_ctrX);

		/* Gestion des shaders du modele de vetement */
		GLuint vertexShaderX = createShader(GL_VERTEX_SHADER, vertexSource);
		GLuint fragmentShaderX = createShader(GL_FRAGMENT_SHADER, fragmentSource);

		GLuint shaderProgramX = glCreateProgram();
		glAttachShader(shaderProgramX, vertexShaderX);
		glAttachShader(shaderProgramX, fragmentShaderX);
		glBindFragDataLocation(shaderProgramX, 0, "outColor");
		glLinkProgram(shaderProgramX);
		glUseProgram(shaderProgramX);

		/* Initialisation des matrices de bones */
		glm::mat4 identityX = glm::mat4(1.0f);
		int bone_matrices_locX[MAX_BONES];
		char nameX[64];
		for (int i = 0; i < MAX_BONES; i++){
			sprintf(nameX, "bone_matrices[%i]", i);
			bone_matrices_locX[i] = glGetUniformLocation(shaderProgramX, nameX); // bone_matrices_loc : matrices de bones dans le programme C
			glUniformMatrix4fv(bone_matrices_locX[i], 1, GL_FALSE, glm::value_ptr(identityX));
		}

		/* Les matrices model, view, projection sont initialisées */
		glm::mat4 modelX = glm::mat4(1.0f);
		modelX = glm::rotate(modelX, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		glm::mat4 viewX = glm::lookAt(
			glm::vec3(0.1f, 3.5f, 0.1f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 projX = glm::perspective(45.0f, (float)height / (float)width, 0.1f, 200.0f);

		GLint uniModelX = glGetUniformLocation(shaderProgram, "model");
		glUniformMatrix4fv(uniModelX, 1, GL_FALSE, glm::value_ptr(modelX));

		GLint uniViewX = glGetUniformLocation(shaderProgram, "view");
		glUniformMatrix4fv(uniViewX, 1, GL_FALSE, glm::value_ptr(viewX));

		GLint uniProjX = glGetUniformLocation(shaderProgramX, "proj");
		glUniformMatrix4fv(uniProjX, 1, GL_FALSE, glm::value_ptr(projX));

		/* gere le scaling du modele */
		float scaleValueX = 1.0f;
		GLint uniScaleX = glGetUniformLocation(shaderProgram, "scale");
		glUniform1f(uniScaleX, scaleValueX);


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
			-25.0f, 25.0f, -18.0f, 1.0f, 1.0f, //modifier troisieme coordonnée pour gérer la profondeur
			25.0f, 25.0f, -18.0f, 0.0f, 1.0f,

			25.0f, -25.0f, -18.0f, 0.0f, 0.0f,
			25.0f, -25.0f, -18.0f, 0.0f, 0.0f,

			-25.0f, -25.0f, -18.0f, 1.0f, 0.0f,//
			-25.0f, 25.0f, -18.0f, 1.0f, 1.0f,//
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
		glm::mat4 viewP = glm::lookAt(
			glm::vec3(0.1f, 13.5f, 0.1f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
		modelP = glm::rotate(modelP, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(uniModel2, 1, GL_FALSE, glm::value_ptr(modelP));
		glUniformMatrix4fv(uniView2, 1, GL_FALSE, glm::value_ptr(viewP));
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
		bool verif = false;

		/* boucle intégration */
		while (!quitter && !glfwWindowShouldClose(window)){
			if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GL_TRUE);

			glUseProgram(shaderProgram);
			glfwHideWindow(window);
			javaCommande(fichierCommande, &quitter, &essai, &vetement);
			kinect.update(texture, Bones);

			if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS){
				fseek(fichierCommande, 0, SEEK_SET);
				fprintf(fichierCommande, "0 1 1");
			}

			if (vetement != ancienVetement){
				javaCommande(fichierCommande, &quitter, &essai, &vetement);
				if (vetement == 0){
					printf("TEST\n");
					nb_bones = 6;
					fichierInit = "init_tshirt.txt";
					numVet = 0;
					vitRot = 1.0f;
					vitTrans = 7.0f;

					MODEL_FILE = "TSHIRT-PS.dae";
					hipRef = Bones[0][0];
					hipPos = Bones[0][2];
					refRight = Bones[2][1];
					posRight = Bones[2][3];

					refLeft = Bones[4][1];
					posLeft = Bones[4][3];

					centerRef = Bones[1][1];
					centerPos = Bones[1][3];

					glUseProgram(shaderProgram);
					view = glm::lookAt(
						glm::vec3(0.5f, 32.5f, 0.5f),
						glm::vec3(0.0f, 0.0f, 0.0f),
						glm::vec3(0.0f, 0.0f, 1.0f));
					glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
				}
				if (vetement == 1){
					printf("SWEAT\n");
					nb_bones = 10;
					fichierInit = "init_exploit-new10.txt";
					numVet = 1;
					vitRot = 0.2f;
					vitTrans = 0.8f;

					MODEL_FILE = "Sweat8AutoW3-10.dae";
					hipRef = Bones[0][0];
					hipPos = Bones[0][2];

					refRight = Bones[2][1];
					posRight = Bones[2][3];

					refLeft = Bones[3][1];
					posLeft = Bones[3][3];

					centerRef = Bones[1][1];
					centerPos = Bones[1][3];

					glUseProgram(shaderProgram);
					view = glm::lookAt(
						glm::vec3(0.5f, 3.5f, 0.5f),
						glm::vec3(0.0f, 0.0f, 0.0f),
						glm::vec3(0.0f, 0.0f, 1.0f));
					glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
				}
			}
			javaCommande(fichierCommande, &quitter, &essai, &vetement);
			if (essai){
					glDeleteBuffers(1, &vao);
					glDeleteShader(vertexShader);
					glDeleteShader(fragmentShader);
					glDeleteProgram(shaderProgram);

					/* le vao du vetement */
					GLuint vao;
					glGenVertexArrays(1, &vao);
					glBindVertexArray(vao);

					printf("chargement\n");

					/* Appel du loader */
					point_ctr = 0;
					bone_ctr = 0;
					//glm::mat4 bone_offset_matrices[MAX_BONES];
					loadModel(MODEL_FILE, &vao, &point_ctr, bone_offset_matrices, &bone_ctr);
					printf("\nNombre de bones : %i\n", bone_ctr);

					/* Gestion des shaders du modele de vetement */
					vertexShader = createShader(GL_VERTEX_SHADER, vertexSource);
					fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentSource);

					shaderProgram = glCreateProgram();
					glAttachShader(shaderProgram, vertexShader);
					glAttachShader(shaderProgram, fragmentShader);
					glBindFragDataLocation(shaderProgram, 0, "outColor");
					glLinkProgram(shaderProgram);
					glUseProgram(shaderProgram);

					/* Initialisation des matrices de bones */
					identity = glm::mat4(1.0f);

					for (int i = 0; i < MAX_BONES; i++){
						sprintf(name, "bone_matrices[%i]", i);
						bone_matrices_loc[i] = glGetUniformLocation(shaderProgram, name); // bone_matrices_loc : matrices de bones dans le programme C
						glUniformMatrix4fv(bone_matrices_loc[i], 1, GL_FALSE, glm::value_ptr(identity));
					}

					/* Les matrices model, view, projection sont initialisées */
					model = glm::mat4(1.0f);
					model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

					// PANTALON : glm::vec3(0.5f, 92.5f, 0.5f)
					// ROBE : glm::vec3(0.5f, 0.5f, 92.5f) de travers !
					// PULL : glm::vec3(0.5f, 3.5f, 0.5f),
					// TSHIRT : glm::vec3(0.5f, 10.5f, 0.5f)

					view = glm::lookAt(
						glm::vec3(0.5f, 32.5f, 0.5f),
						glm::vec3(0.0f, 0.0f, 0.0f),
						glm::vec3(0.0f, 0.0f, 1.0f));
					 proj = glm::perspective(45.0f, (float)height / (float)width, 0.1f, 200.0f);

					uniModel = glGetUniformLocation(shaderProgram, "model");
					glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));

					uniView = glGetUniformLocation(shaderProgram, "view");
					glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

					uniProj = glGetUniformLocation(shaderProgram, "proj");
					glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
					ancienVetement = vetement;
					verif = true;
				}
			//}

			while (!quitter && essai && verif && !glfwWindowShouldClose(window)){ //!quitter && (essai && 
				//printf("essai !\n");
				if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
					glfwSetWindowShouldClose(window, GL_TRUE);
				//affichage
				javaCommande(fichierCommande, &quitter, &essai, &vetement);
				glfwShowWindow(window);


				if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS){
					fseek(fichierCommande, 0, SEEK_SET);
					fprintf(fichierCommande, "0 0 1");
				}

				kinect.update(texture, Bones);
				readData(Bones);
				static double time = glfwGetTime();

				glfwGetWindowSize(window, &width, &height);

				glm::mat4 proj = glm::perspective(45.0f, (float)width / (float)height, 0.1f, 200.0f);
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

				vitRot = 1.0f;
				vitTrans = 15.0f;

				hipRef = Bones[0][0];
				hipPos = Bones[0][2];
				refRight = Bones[2][1];
				posRight = Bones[2][3];

				refLeft = Bones[4][1];
				posLeft = Bones[4][3];

				centerRef = Bones[1][1];
				centerPos = Bones[1][3];

				transla = vitTrans*getTrans(hipRef, hipPos);
				rotShoulders = vitRot*getRot(refRight, refLeft, posRight, posLeft);
				normShoulders = getNormal(refRight, refLeft, posRight, posLeft);
				transla2 = getTrans(centerRef, centerPos);

				glUseProgram(shaderProgram);
				model = glm::translate(model, glm::vec3(0.0f, rot1, 0.0f));
				model = glm::translate(model, transla);
				model = glm::translate(model, transla2);
				model = glm::translate(model, glm::vec3(0.0f, 0.0f, rot3));
				model = glm::rotate(model, -rotShoulders, normShoulders);
				model = glm::scale(model, glm::vec3(1.0, rot2, 1.0));
				glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));
				glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
				glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

				/* on dessine le vetement */
				glEnable(GL_CULL_FACE);
				glUseProgram(shaderProgram);
				glBindVertexArray(vao);
				glDrawArrays(GL_TRIANGLES, 0, point_ctr);

				/* on dessine le fond */
				glDisable(GL_CULL_FACE);
				glUseProgram(shaderP);
				glBindVertexArray(vaoF);
				glBindTexture(GL_TEXTURE_2D, textureFond);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				glBindTexture(GL_TEXTURE_2D, 0);
				glBindVertexArray(0);

				glUseProgram(shaderProgram);
				glUniform1f(uniScale, scaleValue);

				/* update les matrices */
				updateData(Bones, bone_matrices);
				int l;
				for (l = 0; l < nb_bones; l++){
					glUseProgram(shaderProgram);
					glUniformMatrix4fv(bone_matrices_loc[l], 1, GL_FALSE, glm::value_ptr(bone_matrices[l]));
				}

				/* controle des fps */
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

		glUseProgram(0);
		glfwDestroyWindow(window);
		glfwTerminate();

		system("PAUSE");
		exit(EXIT_SUCCESS);
	}
}



DWORD WINAPI main_thread_proc(LPVOID lpThreadParameter)
{
	HANDLE hConnectEvent;
	OVERLAPPED oConnect;
	LPPIPEINST lpPipeInst;
	DWORD dwWait, cbRet;
	BOOL fSuccess, fPendingIO;

	// Create one event object for the connect operation. 

	hConnectEvent = CreateEvent(
		NULL,    // default security attribute
		TRUE,    // manual reset event 
		TRUE,    // initial state = signaled 
		NULL);   // unnamed event object 

	if (hConnectEvent == NULL)
	{
		printf("CreateEvent failed with %d.\n", GetLastError());
		return 0;
	}

	oConnect.hEvent = hConnectEvent;

	// Call a subroutine to create one instance, and wait for 
	// the client to connect. 

	fPendingIO = CreateAndConnectInstance(&oConnect);

	while (1)
	{
		// Wait for a client to connect, or for a read or write 
		// operation to be completed, which causes a completion 
		// routine to be queued for execution. 

		dwWait = WaitForSingleObjectEx(
			hConnectEvent,  // event object to wait for 
			INFINITE,       // waits indefinitely 
			TRUE);          // alertable wait enabled 

		switch (dwWait)
		{
			// The wait conditions are satisfied by a completed connect 
			// operation. 
		case 0:
			// If an operation is pending, get the result of the 
			// connect operation. 

			if (fPendingIO)
			{
				fSuccess = GetOverlappedResult(
					hPipe,     // pipe handle 
					&oConnect, // OVERLAPPED structure 
					&cbRet,    // bytes transferred 
					FALSE);    // does not wait 
				if (!fSuccess)
				{
					printf("ConnectNamedPipe (%d)\n", GetLastError());
					return 0;
				}
			}

			// Allocate storage for this instance. 

			lpPipeInst = (LPPIPEINST)GlobalAlloc(
				GPTR, sizeof(PIPEINST));
			if (lpPipeInst == NULL)
			{
				printf("GlobalAlloc failed (%d)\n", GetLastError());
				return 0;
			}

			lpPipeInst->hPipeInst = hPipe;

			// Start the read operation for this client. 
			// Note that this same routine is later used as a 
			// completion routine after a write operation. 

			lpPipeInst->cbToWrite = 0;
			CompletedWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

			// Create new pipe instance for the next client. 

			fPendingIO = CreateAndConnectInstance(
				&oConnect);
			break;

			// The wait is satisfied by a completed read or write 
			// operation. This allows the system to execute the 
			// completion routine. 

		case WAIT_IO_COMPLETION:
			break;

			// An error occurred in the wait function. 

		default:
		{
			printf("WaitForSingleObjectEx (%d)\n", GetLastError());
			return 0;
		}
		}
	}
	return 0;
}

// CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as a completion routine after writing to 
// the pipe, or when a new client has connected to a pipe instance.
// It starts another read operation. 

VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fRead = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The write operation has finished, so read the next request (if 
	// there is no error). 

	if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
		fRead = ReadFileEx(
		lpPipeInst->hPipeInst,
		lpPipeInst->chRequest,
		BUFSIZE*sizeof(TCHAR),
		(LPOVERLAPPED)lpPipeInst,
		(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);

	// Disconnect if an error occurred. 

	if (!fRead)
		DisconnectAndClose(lpPipeInst);
}

// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as an I/O completion routine after reading 
// a request from the client. It gets data and writes it to the pipe. 

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fWrite = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	//printf("CompletedReadRoutine atteint");

	// The read operation has finished, so write a response (if no 
	// error occurred). 

	if ((dwErr == 0) && (cbBytesRead != 0))
	{
		GetAnswerToRequest(lpPipeInst);

		fWrite = WriteFileEx(
			lpPipeInst->hPipeInst,
			lpPipeInst->chReply,
			lpPipeInst->cbToWrite,
			(LPOVERLAPPED)lpPipeInst,
			(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
	}

	// Disconnect if an error occurred. 

	if (!fWrite)
		DisconnectAndClose(lpPipeInst);
}

// DisconnectAndClose(LPPIPEINST) 
// This routine is called when an error occurs or the client closes 
// its handle to the pipe. 

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)
{
	// Disconnect the pipe instance. 

	if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	// Close the handle to the pipe instance. 

	CloseHandle(lpPipeInst->hPipeInst);

	// Release the storage for the pipe instance. 

	if (lpPipeInst != NULL)
		GlobalFree(lpPipeInst);
}

// CreateAndConnectInstance(LPOVERLAPPED) 
// This function creates a pipe instance and connects to the client. 
// It returns TRUE if the connect operation is pending, and FALSE if 
// the connection has been completed. 

BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	hPipe = CreateNamedPipe(
		lpszPipename,             // pipe name 
		PIPE_ACCESS_DUPLEX |      // read/write access 
		FILE_FLAG_OVERLAPPED,     // overlapped mode 
		PIPE_TYPE_MESSAGE |       // message-type pipe 
		PIPE_READMODE_MESSAGE |   // message read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // unlimited instances 
		BUFSIZE*sizeof(TCHAR),    // output buffer size 
		BUFSIZE*sizeof(TCHAR),    // input buffer size 
		PIPE_TIMEOUT,             // client time-out 
		NULL);                    // default security attributes
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	// Call a subroutine to connect to the new client. 

	return ConnectToNewClient(hPipe, lpoOverlap);
}

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;

	// Start an overlapped connection for this pipe instance. 
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
	case ERROR_IO_PENDING:
		fPendingIO = TRUE;
		break;

		// Client is already connected, so signal an event. 

	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
			break;

		// If an error occurs during the connect operation... 
	default:
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}
	}
	return fPendingIO;
}


char *GetAnswerToRequest(LPPIPEINST pipe)
{
	char text[BUFSIZE];
	int i = 0;
	char* motDeJava = ConvertToCharPtr(pipe->chRequest);
	char* absci = (char*)malloc(8 * sizeof(char));
	char* ordo = (char*)malloc(8 * sizeof(char));
	char* coord = (char*)malloc(17 * sizeof(char));

	fillingChart(absci, RightHandX);
	fillingChart(ordo, RightHandY);

	sprintf(coord, "%c%d.%d%d%d%d%d%d %c%d.%d%d%d%d%d%d\n", absci[0], absci[1], absci[2], absci[3], absci[4], absci[5], absci[6], absci[7], ordo[0], ordo[1], ordo[2], ordo[3], ordo[4], ordo[5], ordo[6], ordo[7]);
	//printf("%f %f\n", RightHandX, RightHandY);


	size_t newsize = strlen(coord) + 1;
	wchar_t * wcstring = new wchar_t[newsize];
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, wcstring, newsize, coord, _TRUNCATE);

	/*for(i=0;i<BUFSIZE;i++)
	{
	text[i]=*motDeJava;
	motDeJava++;
	}*/

	_tprintf(TEXT("%s"), pipe->chRequest);
	StringCchCopy(pipe->chReply, BUFSIZE, wcstring);

	//StringCchCopy( pipe->chReply, BUFSIZE,wcstring)
	pipe->cbToWrite = (lstrlen(pipe->chReply) + 1)*sizeof(TCHAR);
	return motDeJava;
}

char *ConvertToCharPtr(_TCHAR tc[BUFSIZE])
{
	char *buf = (char *)calloc(BUFSIZE + 2, 1);
	char *p;
	char c;
	p = buf;
	int i = 0;
	for (i = 0; i<BUFSIZE; i++)
	{
		c = (char)tc[i];
		*p = c;
		p++;
	}
	return buf;
}

void fillingChart(char* tab, double a)
{

	if (a >= 0)
	{
		tab[0] = '+';
		tab[1] = (char)a;
		a = (a - tab[1]) * 10;
		tab[2] = (char)a;
		for (int i = 2; i<7; i++)
		{
			a = (a - tab[i]) * 10;
			tab[i + 1] = (char)a;
		}
	}
	else
	{
		tab[0] = '-';
		a = 0 - a;
		tab[1] = (char)a;
		a = (a + tab[1]) * 10;
		tab[2] = (char)a;
		for (int i = 2; i<7; i++)
		{
			a = (a - tab[i]) * 10;
			tab[i + 1] = (char)a;
		}
	}
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

void javaCommande(FILE* fichierComm, bool * quitter, bool * essai, int * vetement){
	int quit;
	int ess;
	int vet;
	fseek(fichierComm, 0, SEEK_SET);
	fscanf(fichierComm, "%d %d %d", &quit, &ess, &vet);
	//printf("%d %d %d\n", quit, ess, vet);
	if (quit == 0){
		*quitter = false;
	}
	else if (quit == 1){
		*quitter = true;
	}
	else{
		printf("QUITTER DEFAUT\n");
	}

	if (ess == 0){
		*essai = false;
	}
	else if (ess == 1){
		*essai = true;
	}
	else{
		//	printf("ESSAI DEFAUT\n");
	}

	if (vet == 0){
		*vetement = 0;
	}
	else if (vet == 1){
		*vetement = 1;
	}
	else{
		//	printf("VETEMENT DEFAUT\n");
	}
	return;
}