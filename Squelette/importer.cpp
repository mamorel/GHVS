#include "importer.h"

glm::mat4 convertAIMatrix(const aiMatrix4x4 &matrix)
{
	glm::mat4 result;
	result[0] = glm::vec4(matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0]);
	result[1] = glm::vec4(matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1]);
	result[2] = glm::vec4(matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2]);
	result[3] = glm::vec4(matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3]);

	return result;
}

bool loadModel(const char* file_name, 
	GLuint* vao, int* point_ctr, 
	glm::mat4* bone_offset_mats, 
	int* bone_ctr){

	const aiScene* scene = aiImportFile(file_name, aiProcess_Triangulate);
	if (!scene){
		fprintf(stderr, "ERROR reading the model %s\n", file_name);
		return false;
	}

	/* Imprime des informations sur le fichier importé */
	printf("Model information : \n");
	printf("  %i animations\n", scene->mNumAnimations);
	printf("  %i cameras\n", scene->mNumCameras);
	printf("  %i lights\n", scene->mNumLights);
	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	/* get first mesh in file only */
	const aiMesh* mesh = scene->mMeshes[0];
	printf("  %i vertices in model[0]\n\n", mesh->mNumVertices);

	/* pass back number of vertex points in mesh */
	*point_ctr = mesh->mNumVertices;

	/* generate a VAO, using the pass-by-reference parameter that we give to the
	function */
	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);

	GLfloat* points = NULL; // array of vertex points
	GLfloat* normals = NULL; // array of vertex normals
	GLfloat* texcoords = NULL; // array of texture coordinates
	GLint* bone_ids = NULL; // array of bone ids

	if (mesh->HasPositions()) {
		points = (GLfloat*)malloc(*point_ctr * 3 * sizeof(GLfloat));
		for (int i = 0; i < *point_ctr; i++) {
			const aiVector3D* vp = &(mesh->mVertices[i]);
			points[i * 3] = (GLfloat)vp->x;
			points[i * 3 + 1] = (GLfloat)vp->y;
			points[i * 3 + 2] = (GLfloat)vp->z;
		}
	}
	if (mesh->HasNormals()) {
		normals = (GLfloat*)malloc(*point_ctr * 3 * sizeof(GLfloat));
		for (int i = 0; i < *point_ctr; i++) {
			const aiVector3D* vn = &(mesh->mNormals[i]);
			normals[i * 3] = (GLfloat)vn->x;
			normals[i * 3 + 1] = (GLfloat)vn->y;
			normals[i * 3 + 2] = (GLfloat)vn->z;
		}
	}
	if (mesh->HasTextureCoords(0)) {
		texcoords = (GLfloat*)malloc(*point_ctr * 2 * sizeof(GLfloat));
		for (int i = 0; i < *point_ctr; i++) {
			const aiVector3D* vt = &(mesh->mTextureCoords[0][i]);
			texcoords[i * 2] = (GLfloat)vt->x;
			texcoords[i * 2 + 1] = (GLfloat)vt->y;
		}
	}
	/* extract bone weights */
	if (mesh->HasBones()){
		*bone_ctr = (int)mesh->mNumBones;
		bone_ids = (int*)malloc(*point_ctr * sizeof(int)); 
		/* an array of bones names, max 256 bones, max name length 64 */
		char bone_names[256][64];
		printf("Bone informations : \n");
		for (int b_i = 0; b_i < *bone_ctr; b_i++){ //pour tous les bones
			const aiBone* bone = mesh->mBones[b_i]; //récupère un bone
			strcpy(bone_names[b_i], bone->mName.data); //on copie son nom dans bone_names
			printf("bone_names[%i] = %s\n", b_i, bone_names[b_i]); //affiche son nom
			bone_offset_mats[b_i] = convertAIMatrix(bone->mOffsetMatrix); //récupère la matrice de transformation initiale

			/* get weights */
			int num_weights = (int)bone->mNumWeights; //nombre de poids du bone considéré
			for (int w_i = 0; w_i < num_weights; w_i++){ // pour chaque poids du bone
				aiVertexWeight weight = bone->mWeights[w_i]; //récupère le poids w_i
				int vertex_id = (int)weight.mVertexId; // le poids weight est lié à un vertex => vertex_id
				bone_ids[vertex_id] = b_i; // le vertex vertex_id est influencé par le bone b_i
			}
		}
	}

	/* copy mesh data into VBOs */
	if (mesh->HasPositions()) {
		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 3 * *point_ctr * sizeof(GLfloat), points, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(0);
		free(points);
	}
	if (mesh->HasNormals()) {
		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 3 * *point_ctr * sizeof(GLfloat), normals, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(1);
		free(normals);
	}
	if (mesh->HasTextureCoords(0)) {
		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 2 * *point_ctr * sizeof(GLfloat), texcoords, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(2);
		free(texcoords);
	}
	if (mesh->HasBones()){
		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, *point_ctr * sizeof(GLint), bone_ids, GL_STATIC_DRAW);
		glVertexAttribIPointer(3, 1, GL_INT, 0, NULL);
		glEnableVertexAttribArray(3);
		free(bone_ids);
	}

	aiReleaseImport(scene);
	printf("\nmodel loaded\n");

	return true;
}