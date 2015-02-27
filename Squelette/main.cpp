#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "importer.h"
#include "matrixCalc.h"
#include "printScreen.h"

#define MAX_BONES 32
int nb_bones = 8;
#define MODEL_FILE "Sweat8PaintedNamedNormal.dae"

/* Shaders */
const GLchar* fragmentSourceB =
"#version 410 core\n"
"out vec4 frag_colour;"
"void main() {"
"	frag_colour = vec4(1.0, 1.0, 0.0, 1.0);"
"}";

const GLchar* vertexSourceB =
"#version 410 core\n"
"layout(location = 0) in vec3 vp;"
"uniform mat4 proj, view, model;"
"void main() {"
"	gl_PointSize = 10.0;"
"	gl_Position = proj * view * model * vec4(vp, 1.0);"
"}";

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

"void main(){"
"	mat4 boneTrans;"
"	boneTrans = bone_matrices[bone_ids[0]] * weights[0];"
"	boneTrans += bone_matrices[bone_ids[1]] * weights[1];"
"	boneTrans += bone_matrices[bone_ids[2]] * weights[2];"
"	boneTrans += bone_matrices[bone_ids[3]] * weights[3];"
"	st = vtexcoord;"
"	normal = vnormal;"
"	gl_Position = proj * view * model * boneTrans * vec4(vpos, 1.0);"
"}";

const GLchar* vertexBoneSource =
"#version 410 core\n"
"layout(location = 0) in vec3 vp;"
"uniform mat4 proj, view;"
"void main() {"
"	gl_PointSize = 2.0;"
"	gl_Position = proj * view * vec4(vp, 1.0);"
"}";

const GLchar* fragBoneSource = 
"#version 410 core\n"
"out vec4 frag_colour;"
"void main() {"
"	frag_colour = vec4(1.0, 1.0, 0.0, 1.0);"
"}";


const GLchar* fragmentSource =
"#version 410 core\n"
"in vec3 normal;"
"in vec2 st;"
"out vec4 outColor;"

"void main(){"
"	outColor = vec4(normal, 1.0);"
"}";

GLFWwindow* initGLFW(int width, int weight, char* title);
void initGLEW();
GLuint createShader(GLenum type, const GLchar* src);

int main(){

	glm::vec3 ** Bones;
	glm::mat4 * bone_matrices = (glm::mat4 *)malloc(nb_bones * sizeof(glm::mat4));
	Bones = (glm::vec3 **)malloc(nb_bones * sizeof(glm::vec3 *));
	int b1;
	for (b1 = 0; b1 < nb_bones; b1++){
		Bones[b1] = (glm::vec3 *)malloc(4 * sizeof(glm::vec3));
	}

	FILE* fichier = fopen("twoframes-8-ordonne.txt", "r");
	if (fichier == NULL){
		printf("ERROR loading the file\n");
	}
	//readData(fichier, Bones);
	//updateData(Bones, bone_matrices);

	/*
	Tableau de bones. Un bone = un tableau contenant 4 glm::vec3 (2 pts, une fois pour la frame n-1, une fois pour la frame n).
	A chaque frame, on récupère les positions de tous les points, on met à jour le tableau de bones.
	On calcule toutes les translations, normales et rotations. => Calcul des matrices de bones.
	Nécessité de translater le bone à l'origine avant de lui appliquer la transformation ??
	*/
	printf("***** TESTS MATRIXCALC *****\n");
	glm::vec3 nul = glm::vec3(0.0f, 0.0f, 0.0f); /* origine 1 */
	glm::vec3 vec1 = glm::vec3(0.0f, 0.0f, 1.0f); /* extremite 1 */

	glm::vec3 vec12 = glm::vec3(0.0f, 0.0f, 2.0f); /* origine 2 */
	glm::vec3 vec2 = glm::vec3(1.0f, 1.0f, 1.0f); /* extremite 2 */

	double angle = getRot(vec1, vec12, nul, vec2);
	glm::vec3 translation = getTrans(vec1, nul);
	glm::vec3 normal = getNormal(vec1, vec12, nul, vec2);
	printf("Angle de la rotation : %f\n", angle);

	float a = normal.x;
	float b = normal.y;
	float c = normal.z;
	printf("Vecteur normal : (%f, %f, %f)\n", a, b, c);
	printf("Translation : (%f, %f, %f)\n", translation.x, translation.y, translation.z);
	printf("***** FIN TESTS *****\n\n");

	float rot1 = 0.0f;
	float rot2 = 0.0f;
	int screen_width = 1920;
	int screen_height = 1080;
	int width = 1024;
	int height = 768;
	GLFWwindow* window = initGLFW(width, height, "PACT");
	glfwMakeContextCurrent(window);
	initGLEW();

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Appel du loader */
	int point_ctr = 0;
	int bone_ctr = 0;
	glm::mat4 bone_offset_matrices[MAX_BONES];
	loadModel(MODEL_FILE, &vao, &point_ctr, bone_offset_matrices, &bone_ctr);
	printf("\nNombre de bones : %i\n", bone_ctr);

	int i,j;
	for (i = 0; i < 8; i++){
		for (j = 0; j < 3; j++){
			printf("(3, %d) = %f\n", j, (float)bone_offset_matrices[i][3][j]);
		}
		printf("\n");
	}

	/* create a buffer of bone positions for visualising the bones */
	int c0 = 0;
	/*for (int i = 0; i < bone_ctr; i++) {

		// get the x y z translation elements from the last column in the array
		bone_positions[c0++] = -bone_offset_matrices[i][3][0];
		bone_positions[c0++] = -bone_offset_matrices[i][3][1];
		bone_positions[c0++] = -bone_offset_matrices[i][3][2];
	}*/
	float bone_positions[] = { 0.0f, 0.0f, 0.0f, 0.002, -0.020, -0.406, -0.007, 0.021, 0.999, 0.021, 0.017, 0.982, 0.401, -0.017, 0.872,
		-0.442, 0.049, 0.847, -0.966, 0.074, 0.597, 0.903, -0.075, 0.592 };
	GLuint bones_vao;
	glGenVertexArrays(1, &bones_vao);
	glBindVertexArray(bones_vao);
	GLuint bones_vbo;
	glGenBuffers(1, &bones_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, bones_vbo);
	glBufferData(
		GL_ARRAY_BUFFER,
		3 * bone_ctr * sizeof(float),
		bone_positions,
		GL_STATIC_DRAW
		);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);
	GLuint vertexShaderB = createShader(GL_VERTEX_SHADER, vertexSourceB);
	GLuint fragmentShaderB = createShader(GL_FRAGMENT_SHADER, fragmentSourceB);

	GLuint shaderProgramB = glCreateProgram();
	glAttachShader(shaderProgramB, vertexShaderB);
	glAttachShader(shaderProgramB, fragmentShaderB);
	glBindFragDataLocation(shaderProgramB, 0, "outColor");
	glLinkProgram(shaderProgramB);

	/* Gestion des shaders */
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

	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 view = glm::lookAt(
		glm::vec3(0.0f, 3.0f, 3.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 proj = glm::perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);

	GLint uniModel = glGetUniformLocation(shaderProgram, "model");
	glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));

	GLint uniView = glGetUniformLocation(shaderProgram, "view");
	glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

	GLint uniProj = glGetUniformLocation(shaderProgram, "proj");
	glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

	float theta0 = 0.0f;
	float theta1 = 0.0f;
	float theta2 = 0.0f;
	float theta3 = 0.0f;
	float theta4 = 0.0f;
	float theta5 = 0.0f;
	float theta6 = 0.0f;
	float theta7 = 0.0f;

	glUseProgram(shaderProgramB);
	GLint bones_view_mat_location = glGetUniformLocation(shaderProgramB, "view");
	glUniformMatrix4fv(bones_view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
	GLint bones_proj_mat_location = glGetUniformLocation(shaderProgramB, "proj");
	glUniformMatrix4fv(bones_proj_mat_location, 1, GL_FALSE, glm::value_ptr(proj));
	GLint bones_model_mat_location = glGetUniformLocation(shaderProgramB, "model");
	glUniformMatrix4fv(bones_model_mat_location, 1, GL_FALSE, glm::value_ptr(model));


	/* stocke les pixels */
	unsigned char* pixels = (unsigned char*)malloc(width*height * 3);

	int nbPrint = 0; // compte le nombre de captures d'écran
	int nbFrame = 0;

	while (!glfwWindowShouldClose(window)){
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);
		nbFrame++;
		static double prevSec = glfwGetTime();
		double curSec = glfwGetTime();
		double gap = curSec - prevSec;
		prevSec = curSec;

		//while (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS){}

		/* Taille de la fenetre */
		glfwGetWindowSize(window, &width, &height);
		glfwSetWindowPos(window, (screen_width - width) / 2.0, (screen_height - height));

		glm::mat4 proj = glm::perspective(45.0f, (float)width / (float)height, 0.1f, 100.0f);
		glUseProgram(shaderProgram);
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
		glViewport(0, 0, width, height);

		glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		/* Gere bone 0 : tronc bas */
		bone_matrices[0] = glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS){
			theta0 += 0.07f;
			bone_matrices[0] = glm::inverse(bone_offset_matrices[0]) * glm::rotate(bone_matrices[0], glm::radians(theta0), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[0];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[0], 1, GL_FALSE, glm::value_ptr(bone_matrices[0]));
		}

		if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS){
			theta0 -= 0.07f;
			bone_matrices[0] = glm::inverse(bone_offset_matrices[0]) * glm::rotate(bone_matrices[0], glm::radians(theta0), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[0];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[0], 1, GL_FALSE, glm::value_ptr(bone_matrices[0]));
		}

		/* Gere bone 1 : tronc haut  */
		bone_matrices[1] = glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS){
			theta1 += 10.0f*gap;
			printf("theta1 apres +: %f (frame %d)\n", theta1, nbFrame);
			bone_matrices[1]= glm::inverse(bone_offset_matrices[1]) * glm::translate(bone_matrices[1], glm::vec3(0, 0, theta1)) * bone_offset_matrices[1];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[1], 1, GL_FALSE, glm::value_ptr(bone_matrices[1]));
		}
		if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS){
			theta1 -= 10.0f*gap;
			printf("theta1 apres -: %f (frame %d)\n", theta1, nbFrame);
			bone_matrices[1] = glm::inverse(bone_offset_matrices[1]) * glm::translate(bone_matrices[1], glm::vec3(0, 0, theta1)) * bone_offset_matrices[1];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[1], 1, GL_FALSE, glm::value_ptr(bone_matrices[1]));
		}

		/* Gere bone 2 : epaule gauche */
		bone_matrices[2]= glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS){
			theta2 += 0.07f;
			bone_matrices[2]= glm::inverse(bone_offset_matrices[2]) * glm::rotate(bone_matrices[2], glm::radians(theta2), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[2];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[2], 1, GL_FALSE, glm::value_ptr(bone_matrices[2]));
		}

		if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS){
			theta2 -= 0.07f;
			bone_matrices[2] = glm::inverse(bone_offset_matrices[2]) * glm::rotate(bone_matrices[2], glm::radians(theta2), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[2];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[2], 1, GL_FALSE, glm::value_ptr(bone_matrices[2]));
		}

		/* Gere bone 3 : epaule droite */
		bone_matrices[3]= glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS){
			theta3 += 0.07f;
			bone_matrices[3]= glm::inverse(bone_offset_matrices[3]) * glm::rotate(bone_matrices[3], glm::radians(theta3), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[3];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[3], 1, GL_FALSE, glm::value_ptr(bone_matrices[3]));
		}

		if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS){
			theta3 -= 0.07f;
			bone_matrices[3] = glm::inverse(bone_offset_matrices[3]) * glm::rotate(bone_matrices[3], glm::radians(theta3), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[3];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[3], 1, GL_FALSE, glm::value_ptr(bone_matrices[3]));
		}

		/* Gere bone 4 : bras droit */
		bone_matrices[4]= glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS){
			theta4 += 0.07f;
			bone_matrices[4]= glm::inverse(bone_offset_matrices[4]) * glm::rotate(bone_matrices[4], glm::radians(theta4), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[4];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[4], 1, GL_FALSE, glm::value_ptr(bone_matrices[4]));
		}

		if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS){
			theta4 -= 0.07f;
			bone_matrices[4] = glm::inverse(bone_offset_matrices[4]) * glm::rotate(bone_matrices[4], glm::radians(theta4), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[4];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[4], 1, GL_FALSE, glm::value_ptr(bone_matrices[4]));
		}

		/* Gere bone 5 : avant-bras droit */
		bone_matrices[5]= glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
			theta5 += 10.0f*gap;
			bone_matrices[5] = glm::inverse(bone_offset_matrices[5]) * glm::translate(bone_matrices[5], glm::vec3(theta5, 0,0)) * bone_offset_matrices[5];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[5], 1, GL_FALSE, glm::value_ptr(bone_matrices[5]));
		}

		if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS){
			theta5 -= 10.0f*gap;
			bone_matrices[5] = glm::inverse(bone_offset_matrices[5]) * glm::translate(bone_matrices[5], glm::vec3(theta5, 0, 0)) * bone_offset_matrices[5];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[5], 1, GL_FALSE, glm::value_ptr(bone_matrices[5]));
		}

		/* Gere bone 6 : bras gauche */
		bone_matrices[6]= glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS){
			theta6 += 0.07f;
			bone_matrices[6] = glm::inverse(bone_offset_matrices[6]) * glm::rotate(bone_matrices[6], glm::radians(theta6), glm::vec3(1.0f, 0.0f, 0.0f)) * bone_offset_matrices[6];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[6], 1, GL_FALSE, glm::value_ptr(bone_matrices[6]));
		}

		if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS){
			theta6 -= 0.07f;
			bone_matrices[6] = glm::inverse(bone_offset_matrices[6]) * glm::rotate(bone_matrices[6], glm::radians(theta6), glm::vec3(1.0f, 0.0f, 0.0f)) * bone_offset_matrices[6];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[6], 1, GL_FALSE, glm::value_ptr(bone_matrices[6]));
		}

		/* Gere bone 7 : avant-bras gauche */
		bone_matrices[7] = glm::mat4(1.0f);
		if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS){
			theta7 += 0.07f;
			bone_matrices[7] = glm::inverse(bone_offset_matrices[7]) * glm::rotate(bone_matrices[7], glm::radians(theta7), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[7];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[7], 1, GL_FALSE, glm::value_ptr(bone_matrices[7]));
		}

		if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS){
			theta7 -= 0.07f;
			bone_matrices[7] = glm::inverse(bone_offset_matrices[7]) * glm::rotate(bone_matrices[7], glm::radians(theta7), glm::vec3(0.0f, 1.0f, 0.0f)) * bone_offset_matrices[7];
			glUseProgram(shaderProgram);
			glUniformMatrix4fv(bone_matrices_loc[7], 1, GL_FALSE, glm::value_ptr(bone_matrices[7]));
		}

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

		model = glm::rotate(model, glm::radians(rot2), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rot1), glm::vec3(0.0f, 0.0f, 1.0f));
		glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));

		glEnable(GL_DEPTH_TEST);
		glUseProgram(shaderProgram);
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0,point_ctr);

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_PROGRAM_POINT_SIZE);
		glUseProgram(shaderProgramB);
		glBindVertexArray(bones_vao);
		glDrawArrays(GL_POINTS, 0, bone_ctr);
		glDisable(GL_PROGRAM_POINT_SIZE);
		
		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS){
			nbPrint++;
			printf("Début image write %d\n", nbPrint);
			float t1 = glfwGetTime();
			glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
			char name2[1024];
			sprintf(name2, "screenshot_%d.png", nbPrint);
			unsigned char* last_row = pixels + (width * 3 * (height - 1));
			if (!stbi_write_png(name2, width, height, 3, last_row, -3 * width)){
				fprintf(stderr, "ERROR: could not write screenshot file %s\n", name2);
			}
			float t2 = glfwGetTime();
			printf("Fin image write %d\n", nbPrint);
			printf("temps a l'export %f s pour %ld pixels\n\n", t2 - t1, width*height);
		}
	}

	glDeleteProgram(shaderProgram);
	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteVertexArrays(1, &vao);

	glfwTerminate();

	return 0;
}

GLFWwindow* initGLFW(int width, int height, char* title){
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	// glfwWindowHint(GLFW_DECORATED, GL_FALSE); // enleve decoration fenetre

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