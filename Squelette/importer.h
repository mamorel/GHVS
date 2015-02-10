#ifndef GLEW_H
#define GLEW_H
#include <glew.h>
#endif

#ifndef GLM_H
#define GLM_H
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#endif

#ifndef ASSIMP_H
#define ASSIMP_H
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#endif

#include <stdlib.h>

glm::mat4 convertAIMatrix(const aiMatrix4x4 &matrix);

bool loadModel(const char* file_name,
	GLuint* vao, int* point_ctr,
	glm::mat4* bone_offset_mats,
	int* bone_ctr);