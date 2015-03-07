#include "matrixCalc.h"
extern int nb_bones;

// des +x, +z, +y dans Kinect

float getScale(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2){
	glm::vec3 ref = ref2 - ref1;
	glm::vec3 mov = mov2 - mov1;
	float s = sqrt(glm::dot(mov, mov)) / sqrt(glm::dot(ref, ref));

	return s;
}

/* obtient l'angle de rotation */
float getRot(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2){

	glm::vec3 ref = ref2 - ref1;
	glm::vec3 mov = mov2 - mov1;
	float normRef = sqrt(glm::dot(ref, ref));

	float normMov = sqrt(glm::dot(mov, mov));

	float thetaC = glm::dot(ref, mov) / (normRef * normMov); // toujours calculable
	glm::vec3 cross = glm::cross(ref, mov);
	float normCross = sqrt(glm::dot(cross, cross));
	float thetaS = normCross / (normRef * normMov);

	if (thetaC > 0){
		//printf("+");
		return -acos(thetaC);
	}
	else{
		//printf("-");
		return +acos(thetaC);
	}
}

/* obtient le vecteur translation */
glm::vec3 getTrans(glm::vec3 ref, glm::vec3 mov){

	glm::vec3 translation;
	translation.x  = (mov - ref).x;
	translation.y = (mov - ref).y;
	translation.z = (mov - ref).z;
	//printf("trans.X : %f / trans.Y : %f / trans.Z : %f\n", translation.x, translation.y, translation.z);
	return translation;
}

/* calcule la normale */
glm::vec3 getNormal(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2){
	glm::vec3 ref = ref2 - ref1;
	glm::vec3 mov = mov2 - mov1;
	glm::vec3 normal = glm::cross(ref, mov);
	float norm = sqrt(glm::dot(normal, normal));
	normal = normal / norm;
	normal.x = -normal.x;
	normal.y = -normal.y;
	return normal;
}

/* Calcule la matrice de transf. d'un bone */
glm::mat4 updateMatrix(glm::vec3 ref1, glm::vec3 ref2, glm::vec3 mov1, glm::vec3 mov2){
	float angl = 0.0;
	glm::vec3 transl;
	glm::mat4 res = glm::mat4(1.0f);
	glm::vec3 normal;

	angl = getRot(ref1, ref2, mov1, mov2);
	transl = getTrans(ref1, mov1);
	normal = getNormal(ref1, ref2, mov1, mov2);

	res = glm::translate(res, transl);
	res = glm::rotate(res, angl, normal);
	return res;
}

void scaleData(glm::vec3 ** Bones){
	int i, j;
	float s = 0.0f;
	for (i = 0; i < nb_bones; i++){
		s = getScale(Bones[i][0], Bones[i][1], Bones[i][2], Bones[i][3]);
		Bones[i][0].x = s*Bones[i][0].x;
		Bones[i][0].y = s*Bones[i][0].y;
		Bones[i][0].z = s*Bones[i][0].z;
		Bones[i][1].x = s*Bones[i][1].x;
		Bones[i][1].y = s*Bones[i][1].y;
		Bones[i][1].z = s*Bones[i][1].z;
	}
}

/* Calcule la matrice de transformation de chaque bone et la range dans le tableau correspodant */
void updateData(glm::vec3 ** Bones, glm::mat4 * bone_matrices){
	int i;
	scaleData(Bones);
	for (i = 0; i < nb_bones; i++){
		bone_matrices[i] = updateMatrix(Bones[i][0], Bones[i][1], Bones[i][2], Bones[i][3]);
	}
}

/* range dans le tableau des os les positions des os par défaut du modèle (pose au repos) */
void initData(glm::vec3 ** Bones, FILE* fichier){
	int i, j;
	for (i = 0; i < nb_bones; i++){
		fscanf(fichier, "%f %f %f", &Bones[i][0].x, &Bones[i][0].y, &Bones[i][0].z);
		fscanf(fichier, "%f %f %f", &Bones[i][1].x, &Bones[i][1].y, &Bones[i][1].z);
	}
	//for (i = 0; i < nb_bones;i++)
	//	printf("Bone %d, frame 1. \n\told1 = (%f, %f, %f)\n\told2 = (%f, %f, %f)\n\n", i, Bones[i][0].x, Bones[i][0].y, Bones[i][0].z, Bones[i][1].x, Bones[i][1].y, Bones[i][1].z);
}

/* Lit les données Kinect et les range dans le tableau de Bones(lui même tableau de vec3 */
void readData(glm::vec3 ** Bones){
	FILE* fichier = fopen("\\Users\\Martin\\Desktop\\ColorBasics-D2D-fonctionnel\\skelcoordinates.txt", "r"); //"bones-ordonnesTestJeu.txt"
	if (fichier == NULL){
		printf("error loading the file skelcoordinates.txt\n");
		exit(1);
	}
	int i,j;
	for (i = 0; i < nb_bones; i++){
		fscanf(fichier, "%f %f %f", &Bones[i][2].x, &Bones[i][2].y, &Bones[i][2].z);
		fscanf(fichier, "%f %f %f", &Bones[i][3].x, &Bones[i][3].y, &Bones[i][3].z);
		//printf("Bone %d : (%f, %f, %f) -> (%f, %f, %f)\n", i, Bones[i][2].x, Bones[i][2].y, Bones[i][2].z, Bones[i][3].x, Bones[i][3].y, Bones[i][3].z);
	}
	fclose(fichier);
}