#ifndef GLM_H
#define GLM_H
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#endif
#include <stdio.h>


float getRot(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2);
glm::vec3 getTrans(glm::vec3 ref, glm::vec3 mov);
glm::vec3 getNormal(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2);
glm::mat4 updateMatrix(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2);
void updateData(glm::vec3 ** Bones, glm::mat4 * bone_matrices, int nb_bones, char * fichierInit); 
void readData(glm::vec3 ** Bones, int nb_bones);
void readDataTest(glm::vec3 ** Bones, int nb_bones);
void initData(glm::vec3 ** Bones, FILE* fichier, int nb_bones);
float getScale(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2, glm::vec3 ** Bones, char * fichierInit, int nb_bones);
void resetData(glm::vec3 ** Bones, int nb_bones);