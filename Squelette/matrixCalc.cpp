#include "matrixCalc.h"

double getRot(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2){

	glm::vec3 ref = ref2 - ref1;
	glm::vec3 mov = mov2 - mov1;
	float normRef = sqrt(glm::dot(ref, ref));
	printf("norme Ref : %f\n", normRef);

	float normMov = sqrt(glm::dot(mov, mov));
	printf("norme Mov : %f\n", normMov);

	printf("produit scalaire : %f\n", glm::dot(ref, mov));

	double theta = acos(glm::dot(ref, mov) / (normRef * normMov)); // toujours calculable

	return theta;
}

glm::vec3 getTrans(glm::vec3 ref, glm::vec3 mov){

	glm::vec3 translation = mov - ref;
	return translation;
}

glm::vec3 getNormal(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2){
	glm::vec3 ref = ref2 - ref1;
	glm::vec3 mov = mov2 - mov1;
	glm::vec3 normal = glm::cross(ref, mov);
	float norm = sqrt(glm::dot(normal, normal));
	normal = normal / norm;
	return normal;
}