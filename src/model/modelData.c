#include "modelData.h"
#include <stdlib.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>

static void assimpNodeTraversal(ModelNode* node, struct aiNode* assimpNode) {

  // Transform
  struct aiMatrix4x4 m = assimpNode->mTransformation;
  aiTransposeMatrix4(&m);
  node->transform = mat4_copy((float*) &m);

  // Meshes
  vec_init(&node->meshes);
  vec_pusharr(&node->meshes, assimpNode->mMeshes, assimpNode->mNumMeshes);

  // Children
  vec_init(&node->children);
  for (int n = 0; n < assimpNode->mNumChildren; n++) {
    ModelNode* child = malloc(sizeof(ModelNode));
    assimpNodeTraversal(child, assimpNode->mChildren[n]);
    vec_push(&node->children, child);
  }
}

ModelData* lovrModelDataCreate(void* data, int size) {
  ModelData* modelData = malloc(sizeof(ModelData));
  const struct aiScene* scene = aiImportFileFromMemory(data, size, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph, NULL);

  // Meshes
  vec_init(&modelData->meshes);
  for (int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    ModelMesh* mesh = malloc(sizeof(ModelMesh));
    vec_push(&modelData->meshes, mesh);

    // Faces
    vec_init(&mesh->faces);
    for (int f = 0; f < assimpMesh->mNumFaces; f++) {
      struct aiFace assimpFace = assimpMesh->mFaces[f];

      // Skip lines and points, polygons are triangulated
      if (assimpFace.mNumIndices != 3) {
        continue;
      }

      ModelFace face;
      vec_init(&face.indices);

      // Indices
      for (int i = 0; i < assimpFace.mNumIndices; i++) {
        vec_push(&face.indices, assimpFace.mIndices[i]);
      }

      vec_push(&mesh->faces, face);
    }

    // Vertices
    vec_init(&mesh->vertices);
    for (int v = 0; v < assimpMesh->mNumVertices; v++) {
      ModelVertex vertex;
      vertex.x = assimpMesh->mVertices[v].x;
      vertex.y = assimpMesh->mVertices[v].y;
      vertex.z = assimpMesh->mVertices[v].z;
      vec_push(&mesh->vertices, vertex);
    }

    // Normals
    if (!assimpMesh->mNormals) {
      error("Model must have normals");
    }

    modelData->hasNormals = 1;
    vec_init(&mesh->normals);
    for (int n = 0; n < assimpMesh->mNumVertices; n++) {
      ModelVertex normal;
      normal.x = assimpMesh->mNormals[n].x;
      normal.y = assimpMesh->mNormals[n].y;
      normal.z = assimpMesh->mNormals[n].z;
      vec_push(&mesh->normals, normal);
    }
  }

  // Nodes
  modelData->root = malloc(sizeof(ModelNode));
  assimpNodeTraversal(modelData->root, scene->mRootNode);

  aiReleaseImport(scene);
  return modelData;
}

void lovrModelDataDestroy(ModelData* modelData) {
  for (int i = 0; i < modelData->meshes.length; i++) {
    ModelMesh* mesh = modelData->meshes.data[i];

    for (int f = 0; f < mesh->faces.length; f++) {
      vec_deinit(&mesh->faces.data[f].indices);
    }

    vec_deinit(&mesh->faces);
    vec_deinit(&mesh->vertices);
    free(mesh);
  }

  vec_void_t nodes;
  vec_init(&nodes);
  vec_push(&nodes, modelData->root);
  while (nodes.length > 0) {
    ModelNode* node = vec_first(&nodes);
    vec_extend(&nodes, &node->children);
    mat4_deinit(node->transform);
    vec_deinit(&node->meshes);
    vec_deinit(&node->children);
    vec_splice(&nodes, 0, 1);
  }

  vec_deinit(&modelData->meshes);
  free(modelData);
}