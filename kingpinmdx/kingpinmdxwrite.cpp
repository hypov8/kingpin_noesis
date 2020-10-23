//i'm writing this while completely drunk. apologies for any errors!

#include "stdafx.h"
#include "kingpinmdx.h"

#define MDX_PLAYER_MODEL

extern mdxOpts_t *g_opts;

char *exportFileNames[MDX_PLAYER_MAX] = {
	"head.mdx",
	"body.mdx",
	"legs.mdx",
	"w_bazooka.mdx",
	"w_flamethrower.mdx",
	"w_grenade launcher.mdx",
	"w_heavy machinegun.mdx",
	"w_pipe.mdx",
	"w_pistol.mdx",
	"w_shotgun.mdx",
	"w_tommygun.mdx"
};

typedef struct mdxAnimHold_s
{
	modelMatrix_t		*mats;
	int					numFrames;
	float				frameRate;
	int					numBones;
} mdxAnimHold_t;

//retrives animation data
static mdxAnimHold_t *Model_MDX_GetAnimData(noeRAPI_t *rapi)
{
	int animDataSize;
	BYTE *animData = rapi->Noesis_GetExtraAnimData(animDataSize);
	if (!animData)
	{
		return NULL;
	}

	noesisAnim_t *anim = rapi->Noesis_AnimAlloc("animout", animData, animDataSize); //animation containers are pool-allocated, so don't worry about freeing them
	//copy off the raw matrices for the animation frames
	mdxAnimHold_t *amdx = (mdxAnimHold_t *)rapi->Noesis_PooledAlloc(sizeof(mdxAnimHold_t));
	memset(amdx, 0, sizeof(mdxAnimHold_t));
	amdx->mats = rapi->rpgMatsFromAnim(anim, amdx->numFrames, amdx->frameRate, &amdx->numBones, true);

	return amdx;
}

//index normal into the mdx normals list
static int Model_MDX_IndexNormal(float *nrm)
{
	float bestDP = nrm[0]*g_q2Normals[0][0] + nrm[1]*g_q2Normals[0][1] + nrm[2]*g_q2Normals[0][2];
	int bestIdx = 0;
	for (int i = 1; i < 162; i++)
	{
		float dp = nrm[0]*g_q2Normals[i][0] + nrm[1]*g_q2Normals[i][1] + nrm[2]*g_q2Normals[i][2];
		if (dp > bestDP)
		{
			bestDP = dp;
			bestIdx = i;
		}
	}
	return bestIdx;
}

//bake a frame of vertex data
static void Model_MDX_MakeFrame(sharedModel_t *pmdl, modelMatrix_t *animMats, mdxFrame_t *frames, int frameNum, int frameSize, noeRAPI_t *rapi,
		mdxBBox_t *bboxFrames, int bboxSize, int objectID, BYTE *meshElementsIdxType) //hypov8 bbox, multipart option, PPM head/body/legs
{
	mdxFrame_t *frame = (mdxFrame_t *)((BYTE *)frames + frameSize*frameNum);
	mdxBBox_t  *bbox =  (mdxBBox_t *) ((BYTE *)bboxFrames + bboxSize*frameNum); //hypov8 bbox
	sprintf_s(frame->name, 16, "fr_%i", frameNum);
	float frameMins[3] = {9999.0f, 9999.0f, 9999.0f};
	float frameMaxs[3] = {-9999.0f, -9999.0f, -9999.0f};
	float bboxMins[3] = {9999.0f, 9999.0f, 9999.0f};
	float bboxMaxs[3] = {-9999.0f, -9999.0f, -9999.0f};

	if (frameNum > 0 && animMats)
	{ //if working with skeletal data, create/updated the transformed vertex arrays
		rapi->rpgTransformModel(pmdl, animMats, frameNum-1);
	}
	//allocate temporary buffers for the high-precision data
	modelVert_t *vpos = (modelVert_t *)rapi->Noesis_UnpooledAlloc(sizeof(modelVert_t)*pmdl->numAbsVerts);
	modelVert_t *vnrm = (modelVert_t *)rapi->Noesis_UnpooledAlloc(sizeof(modelVert_t)*pmdl->numAbsVerts);
	//fill out high-precision data arrays while calculating the frame bounds
	for (int i = 0; i < pmdl->numAbsVerts; i++)
	{
		sharedVMap_t *svm = pmdl->absVertMap + i;
		sharedMesh_t *mesh = pmdl->meshes + svm->meshIdx;
		modelVert_t *verts = NULL;
		modelVert_t *normals = NULL;
		if (frameNum == 0)
		{ //initial base-pose frame
			verts = mesh->verts + svm->vertIdx;
			normals = mesh->normals + svm->vertIdx;
		}
		else if (frameNum > 0 && animMats)
		{ //bake in skeletally-transformed verts
			verts = mesh->transVerts + svm->vertIdx;
			normals = mesh->transNormals + svm->vertIdx;
		}
		else if (frameNum > 0 && mesh->numMorphFrames > 0 && frameNum - 1 < mesh->numMorphFrames)
		{ //copy over vertex morph frames
			verts = mesh->morphFrames[frameNum - 1].pos + svm->vertIdx;
			normals = mesh->morphFrames[frameNum - 1].nrm + svm->vertIdx;
		}
		modelVert_t *framePos = vpos + i;
		modelVert_t *frameNrm = vnrm + i;
		if (!verts || !normals)
		{ //something went wrong with the transforms
			framePos->x = 0.0f;
			framePos->y = 0.0f;
			framePos->z = 0.0f;
			frameNrm->x = 0.0f;
			frameNrm->y = 0.0f;
			frameNrm->z = 1.0f;
		}
		else
		{
			framePos->x = verts->x;
			framePos->y = verts->y;
			framePos->z = verts->z;
			frameNrm->x = normals->x;
			frameNrm->y = normals->y;
			frameNrm->z = normals->z;
		}

#ifdef MDX_PLAYER_MODEL
		if (g_opts->exportPlayerModel)
		{
			sharedVMap_t *svm = pmdl->absVertMap + i;
			if ((int)meshElementsIdxType[svm->meshIdx] == (BYTE)objectID)
			{	//add to the frame bounds
				g_mfn->Math_ExpandBounds(frameMins, frameMaxs, (float *)framePos, (float *)framePos);
				g_mfn->Math_ExpandBounds(bboxMins, bboxMaxs, (float *)framePos, (float *)framePos); //hypov8 bbox
			}
		}
		else
#endif
		{	//add to the frame bounds
			g_mfn->Math_ExpandBounds(frameMins, frameMaxs, (float *)framePos, (float *)framePos);
			g_mfn->Math_ExpandBounds(bboxMins, bboxMaxs, (float *)framePos, (float *)framePos); //hypov8 bbox
		}
	}

	//hypov8 bbox
	bbox->min[0] = bboxMins[0];
	bbox->min[1] = bboxMins[1];
	bbox->min[2] = bboxMins[2];
	bbox->max[0] = bboxMaxs[0];
	bbox->max[1] = bboxMaxs[1];
	bbox->max[2] = bboxMaxs[2];


	//now convert the data to a mdx frame
	frame->trans[0] = frameMins[0];
	frame->trans[1] = frameMins[1];
	frame->trans[2] = frameMins[2];
	const float frange = 255.0f;
	const float invFRange = 1.0f/frange;
	frame->scale[0] = (frameMaxs[0]-frameMins[0])*invFRange;
	frame->scale[1] = (frameMaxs[1]-frameMins[1])*invFRange;
	frame->scale[2] = (frameMaxs[2]-frameMins[2])*invFRange;

	//start writing vertex data
	mdxVert_t *mdxVerts = (mdxVert_t *)(frame+1);
	int vCount = 0;
	for (int i = 0; i < pmdl->numAbsVerts; i++)
	{
#ifdef MDX_PLAYER_MODEL
		if (g_opts->exportPlayerModel)
		{
			sharedVMap_t *svm = pmdl->absVertMap + i;
			if ((int)meshElementsIdxType[svm->meshIdx] != (BYTE)objectID)
				continue;
		}
#endif
		mdxVert_t *lVert = mdxVerts + vCount;
		float *hvPos = (float *)(vpos+i);
		float *hvNrm = (float *)(vnrm+i);
		for (int j = 0; j < 3; j++)
		{
			float f = (((hvPos[j]-frameMins[j])/(frameMaxs[j]-frameMins[j])) * frange)+0.5f; //hypov8 added .5 to round better
			lVert->pos[j] = (BYTE)g_mfn->Math_Min2(f, frange);
		}
		lVert->nrmIdx = Model_MDX_IndexNormal(hvNrm);
		vCount++;
	}

	rapi->Noesis_UnpooledFree(vpos);
	rapi->Noesis_UnpooledFree(vnrm);
}

//put uv in 0-1 range. only used for software mode in Q2
static float Model_MDX_CrunchUV(float f)
{
	f = fmodf(f, 1.0f);

	while (f < 0.0f)
	{
		f += 1.0f;
	}

	return f;
}

/*
=======================
build uv. hi pricision. 
disregard image size. 
dont fit into 0-1 uv
=======================
*/
void Model_MDX_BuildUVs(sharedModel_t *pmdl, modelTexCoord_t *fuvs)
{
	//build uv. regardless of image found
	//if ppm load external image. get external texture size eg.. head_001
	//non ppl. load internl image #1?

	//build UV list
	for (int i = 0; i < pmdl->numAbsVerts; i++)
	{
		sharedVMap_t *svm = pmdl->absVertMap + i;
		sharedMesh_t *mesh = pmdl->meshes + svm->meshIdx;
		modelTexCoord_t *uv = fuvs + i;
		modelTexCoord_t *srcUV = mesh->uvs + svm->vertIdx;
		uv->u = srcUV->u;
		uv->v = srcUV->v;
	}
}

//hypov8 clean the end of the string with null
void Model_MDX_Null_SkinName(mdxSkin_t *skin, int skinCount)
{
	int i, j;
	bool nameEnd = false;

	for (i = 0; i < skinCount; i++)
	{
		mdxSkin_t *currSkin = skin + sizeof(mdxSkin_t)*i;
		nameEnd = false;
		for (j = 0; i < 64; i++)
		{
			if (nameEnd)
				currSkin->name[i] = '\0';

			if (currSkin->name[i] == '\0')
				nameEnd = true;
		}
	}
}

void StringConvertToBackSlash(char *inString); //from importer
void StringConvertToForwardSlash(char *inString); //from importer 
void StringToLower(char *inString); //from importer 

//check for correct folders in skin paths
bool Model_MDX_BakeSkinName(noeRAPI_t *rapi, char *in, char *outSkin)
{
	int sLen=0, fileEnd = false;
	bool outOK = true;
	char *playerDir, *modelDir, *texturesDir;

	StringConvertToForwardSlash(in);
	StringToLower(in);

	modelDir = strstr(in, "models/");
	playerDir = strstr(in, "players/");
	texturesDir = strstr(in, "textures/");

	if (modelDir)
		memcpy(outSkin, modelDir, 64);
	else if (playerDir)
		memcpy(outSkin, playerDir, 64);			
	else if (texturesDir)
		memcpy(outSkin, texturesDir, 64);			
	else
	{
		memcpy(outSkin, in, 64);	
		outOK = false; //try use model folder name instead of internal image name.
	}
	//chect file type. force .tga if missing
	if (!rapi->Noesis_CheckFileExt( outSkin, ".tga") && !rapi->Noesis_CheckFileExt(outSkin, ".pcx"))
	{
		rapi->Noesis_GetExtensionlessName(outSkin, outSkin);
		StringToLower(outSkin);
		strcat_s(outSkin, (rsize_t)64, ".tga");
	}

	return outOK;
}

bool Model_MDX_LoadExternalImageSize(noeRAPI_t *rapi, int &skinWidth, int &skinHeight, char * skinName)
{
	noesisTex_t	*loadedTexture;
	if (loadedTexture = rapi->Noesis_LoadExternalTex(skinName))
	{
		skinWidth = loadedTexture->w;
		skinHeight = loadedTexture->h;
		return true;
	}

	bool		foundFile = false;
	int			lenFile, i = 0;
	char		*fExt[5] = { ".tga", ".pcx", ".jpg", ".png", ".dds" };
	char		*playerIdx, *modelIdx, *textIdx, *kingIdx;
	char		*srcDir = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
	char		outSkin[64];

	rapi->Noesis_GetDirForFilePath(srcDir, rapi->Noesis_GetOutputName());
	StringToLower(srcDir);

	playerIdx = strstr(srcDir, "\\players\\");
	modelIdx = strstr(srcDir, "\\models\\");
	textIdx = strstr(srcDir, "\\textures\\");
	kingIdx = strstr(srcDir, "\\kingpin\\");

	//found correct folders, trim and append skin
	if (modelIdx || playerIdx || textIdx ||kingIdx)
	{
		lenFile = strlen(srcDir);
		if (modelIdx)
			srcDir[lenFile - strlen(modelIdx) + 1] = '\0';
		else if (playerIdx)
			srcDir[lenFile - strlen(playerIdx) + 1] = '\0';
		else if (textIdx)
			srcDir[lenFile - strlen(textIdx) + 1] = '\0';
		else //search main\ folder. cant find a valid folder
		{
			srcDir[lenFile - strlen(kingIdx) + 1] = '\0';
			strcat_s(srcDir, MAX_NOESIS_PATH, "kingpin\\main\\");
			kingIdx = NULL;
		}

		//append model skin to output path
		Model_MDX_BakeSkinName(rapi, skinName, outSkin);
		strcat_s(srcDir, MAX_NOESIS_PATH, outSkin);
		StringConvertToBackSlash(srcDir);

		while (i < 5 && !foundFile)
		{
			rapi->Noesis_GetExtensionlessName(srcDir, srcDir);
			strcat_s(srcDir, MAX_NOESIS_PATH, fExt[i]);

			if (loadedTexture = rapi->Noesis_LoadExternalTex(srcDir))
			{
				skinWidth = loadedTexture->w;
				skinHeight = loadedTexture->h;
				foundFile = true;
			}
			i++;
		}
		//search in main
		if (!foundFile && kingIdx)
		{
			i = 0;
			lenFile = strlen(srcDir);
			srcDir[lenFile - strlen(kingIdx) + 1] = '\0';
			strcat_s(srcDir, MAX_NOESIS_PATH, "kingpin\\main\\");
			strcat_s(srcDir, MAX_NOESIS_PATH, outSkin);
			StringConvertToBackSlash(srcDir);
			while (i < 5 && !foundFile)
			{
				rapi->Noesis_GetExtensionlessName(srcDir, srcDir);
				strcat_s(srcDir, MAX_NOESIS_PATH, fExt[i]);

				if (loadedTexture = rapi->Noesis_LoadExternalTex(srcDir))
				{
					skinWidth = loadedTexture->w;
					skinHeight = loadedTexture->h;
					foundFile = true;
				}
				i++;
			}
		}
	}

	//use output filename
	if (!foundFile)
	{
		i = 0;
		while (i < 5 && !foundFile)
		{
			rapi->Noesis_GetExtensionlessName(srcDir, rapi->Noesis_GetOutputName());
			strcat_s(srcDir, MAX_NOESIS_PATH, fExt[i]);

			if (loadedTexture = rapi->Noesis_LoadExternalTex(srcDir))
			{
				skinWidth = loadedTexture->w;
				skinHeight = loadedTexture->h;
				foundFile = true;
			}
			i++;
		}
	}

	rapi->Noesis_UnpooledFree(srcDir);
	return foundFile;
}


void Model_MDX_FindMateral(sharedModel_t *pmdl,noeRAPI_t *rapi,  int &skinWidth,int &skinHeight, int & skinCount, 
							mdxSkin_t *skins,char *skinTmp,int skinCounter, sharedMesh_t *mesh, int &foundTextureSize)
{
	if (mesh)
	{	//mesh has asigned a global material
		if (mesh->materialIdx > -1
			&& pmdl->matData->materials[mesh->materialIdx].extRefs
			&& pmdl->matData->materials[mesh->materialIdx].extRefs->diffuse)
		{
			rapi->Noesis_GetDirForFilePath(skinTmp, rapi->Noesis_GetInputName()); //input dir
			strcat_s(skinTmp, (size_t)MAX_NOESIS_PATH, pmdl->matData->materials[mesh->materialIdx].extRefs->diffuse);
			_fullpath(skinTmp, skinTmp, (size_t)MAX_NOESIS_PATH); //fix any ..\ paths

			noesisTex_t	*loadedTexture = rapi->Noesis_LoadExternalTex(skinTmp);
			if (loadedTexture)
			{
				if (!foundTextureSize)
				{
					skinWidth = loadedTexture->w;
					skinHeight = loadedTexture->h;
					foundTextureSize = true;
				}
			}
			else
				strcpy_s(skinTmp, MAX_NOESIS_PATH, mesh->skinName);
		}
		else
			strcpy_s(skinTmp, MAX_NOESIS_PATH, mesh->skinName);
	}


	//get texture size (width/height)
	if (!foundTextureSize)
		foundTextureSize = Model_MDX_LoadExternalImageSize(rapi, skinWidth, skinHeight, skinTmp);


	//setup model internal skin paths. look for models/ players/ etc..
	if (!Model_MDX_BakeSkinName(rapi, skinTmp, skins->name + (sizeof(mdxSkin_t)*skinCounter)))
	{
		//check output filename for correct folder paths
		strcpy_s(skinTmp, MAX_NOESIS_PATH, rapi->Noesis_GetOutputName());
		if (!Model_MDX_BakeSkinName(rapi, skinTmp, skins->name + (sizeof(mdxSkin_t)*skinCounter)))
		{
			//use outputFileName.tga
			rapi->Noesis_GetLocalFileName(skinTmp, rapi->Noesis_GetOutputName());
			Model_MDX_BakeSkinName(rapi, skinTmp, skins->name + (sizeof(mdxSkin_t)*skinCounter));
		}
	}
					
}

/*
/////////////////////
Model_MDX_CreateSkinLists

hypov8 update
stop joining all images into 1 page
leave uv untouched
////////////////////
*/
void Model_MDX_CreateSkinLists(sharedModel_t *pmdl, int &skinWidth,	int &skinHeight, int & skinCount, noeRAPI_t *rapi, int objectID, mdxSkin_t *skins)
{
	//set default skin size
	skinWidth = 128;
	skinHeight = 128;

	//3 part player model
	if (g_opts->exportPlayerModel && (objectID == MDX_PLAYER_HEAD|| objectID == MDX_PLAYER_BODY||objectID == MDX_PLAYER_HEAD))
	{
		char *filename = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
		rapi->Noesis_GetDirForFilePath(filename, rapi->Noesis_GetOutputName());

		if (objectID == MDX_PLAYER_HEAD)
			strcat_s(filename, MAX_NOESIS_PATH, "head_001.tga");
		else if (objectID == MDX_PLAYER_BODY)
			strcat_s(filename, MAX_NOESIS_PATH, "body_001.tga");
		else
			strcat_s(filename, MAX_NOESIS_PATH, "legs_001.tga");

		//get texture size
		Model_MDX_LoadExternalImageSize(rapi, skinWidth, skinHeight, filename);
		Model_MDX_BakeSkinName(rapi, filename, skins->name); //#1

		rapi->Noesis_UnpooledFree(filename);
	}
	else //1 complete model. embed all images into file if not ppm weapon.
	{			
		char *skinPathsArray[32];
		char *skinTmp = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
		int skinCounter = 0;
		int foundMatch;
		int foundTextureSize = 0;

		//assign materals to mesh
		for (int i = 0; i < pmdl->numMeshes; i++)
		{
			if (g_opts->exportPlayerModel && skinCounter == 1)
				break; //only add 1 gun skin

			foundMatch = 0;
			sharedMesh_t *mesh = pmdl->meshes + i;
			if (mesh->skinName[0])
			{
				for (int j = 0; j < skinCounter; j++)
				{
					if (!_stricmp(skinPathsArray[j], mesh->skinName))
					{
						foundMatch = 1;
						break;
					}
				} 

				//append a new skin
				if (!foundMatch)
				{	//store texture set. helps eliminate duplicates.
					skinPathsArray[skinCounter] = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
					strcpy_s(skinPathsArray[skinCounter], MAX_NOESIS_PATH, mesh->skinName);
					Model_MDX_FindMateral(pmdl, rapi,skinWidth, skinHeight, skinCount, skins, skinTmp,skinCounter, mesh, foundTextureSize);
					skinCounter++;
				}
			}
			else //use model path. no skin assigned to mesh
			{
				strcpy_s(skinTmp, MAX_NOESIS_PATH, rapi->Noesis_GetOutputName());
				Model_MDX_BakeSkinName(rapi, skinTmp, skins->name);//use "model_path/model_name.tga"
				break; //dont look for any more skins
			}
		}

		//append any materals in file but not assigned to a mesh
		if (!g_opts->exportPlayerModel&&pmdl->matData&&pmdl->matData->numMaterials)
		{
			for (int i = 0; i < pmdl->matData->numMaterials; i++)
			{
				foundMatch = 0;
				if (pmdl->matData->materials[i].name)
				{
					for (int j = 0; j < skinCounter; j++)
					{
						if (!_stricmp(skinPathsArray[j], pmdl->matData->materials[i].name))
						{
							foundMatch = 1;
							break;
						}
					}

					//append a new skin
					if (!foundMatch)
					{	//store texture set. helps eliminate duplicates.
						skinPathsArray[skinCounter] = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
						strcpy_s(skinPathsArray[skinCounter], MAX_NOESIS_PATH, pmdl->matData->materials[i].name);
						strcpy_s(skinTmp, MAX_NOESIS_PATH, pmdl->matData->materials[i].name);
						Model_MDX_FindMateral(pmdl, rapi, skinWidth, skinHeight, skinCount, skins, skinTmp, skinCounter, NULL, foundTextureSize);
						skinCounter++;
					}
				}
			}
		}

		//free temp skins
		rapi->Noesis_UnpooledFree(skinTmp);
		for (int i = 0; i < skinCounter; i++)
			rapi->Noesis_UnpooledFree(skinPathsArray[i]);

		if (skinCounter > 32)
			skinCounter = 32;
		if (skinCounter <= 0)
			skinCounter = 1;

		skinCount = skinCounter;	
	} //end complete model (non player mesh)

	//fill skin names with null
	Model_MDX_Null_SkinName(skins, skinCount);

}


//generate the gl command data																															 
BYTE *Model_MDX_GenerateGLCmds(sharedModel_t *pmdl, mdxTri_t *tris, modelTexCoord_t *fuvs, int &glcmdsSize, noeRAPI_t *rapi, 
		int objectID, BYTE *meshElementsIdxType, int numAbsTris, WORD *meshVertexOfs) //hypov8 bbox, multipart option, PPM head/body/legs
{
	rapi->LogOutput("Generating GL command lists...\n");
	RichBitStream bs;
	int triCount = 0;
	bool hasStrips = false;
	sharedStripList_t *slist = NULL;
	int numSList = 0;

	//convert the triangle indices to a flat short list for ingestion by the stripper
	WORD *triIdx = (WORD *)rapi->Noesis_UnpooledAlloc(sizeof(WORD)*numAbsTris*3);
	for (int i = 0; i < pmdl->numAbsTris; i++)
	{
		modelLongTri_t *triTmp = pmdl->absTris+i;
		sharedVMap_t *svm0 = pmdl->absVertMap + triTmp->idx[0];
		if (g_opts->exportPlayerModel)
		{
			if (svm0->meshIdx >= pmdl->numMeshes)
				continue;
			if (meshElementsIdxType[svm0->meshIdx] != (BYTE)objectID) //1,2,3 etc..
				continue;
			if (triCount > numAbsTris)
				continue;
		}

		mdxTri_t *tri = tris+i;
		WORD *dst = triIdx + triCount * 3;
		dst[0] = tri->vIdx[0];
		dst[1] = tri->vIdx[1];
		dst[2] = tri->vIdx[2];
		triCount++;
	}
	hasStrips = rapi->rpgGenerateStripLists(triIdx, numAbsTris*3, &slist, numSList, false);
	rapi->Noesis_UnpooledFree(triIdx);


	if (!hasStrips)
	{ //if there are no strip lists, plot a plain list
		triCount = 0;
		for (int i = 0; i < pmdl->numAbsTris; i++)
		{
			modelLongTri_t *triTmp = pmdl->absTris+i;
			sharedVMap_t *svm0 = pmdl->absVertMap + triTmp->idx[0];
			if (g_opts->exportPlayerModel)
			{
				if (svm0->meshIdx >= pmdl->numMeshes)
					continue;
				if (meshElementsIdxType[svm0->meshIdx] != (BYTE)objectID) //1,2 or 3
					continue;
				if (triCount > numAbsTris)
					continue;
			}

			mdxTri_t *tri = tris+i;
			bs.WriteInt(3); //TrisTypeNum
			bs.WriteInt(0); //SubObjectID //todo
			mdxGLCmd_t glcmds[3];
			for (int j = 0; j < 3; j++)
			{
				glcmds[j].vIdx = meshVertexOfs[tri->vIdx[j]];// hypov8 //tri->vIdx[j];
				glcmds[j].st[0] = fuvs[tri->vIdx[j]].u;
				glcmds[j].st[1] = fuvs[tri->vIdx[j]].v;
			}
			bs.WriteBytes(glcmds, sizeof(mdxGLCmd_t)*3);
			triCount++;
		}
	}
	else
	{ //plot the strips down
		for (int i = 0; i < numSList; i++)
		{
			sharedStripList_t *strip = slist+i;
			if (strip->type == SHAREDSTRIP_LIST)
			{ //plot it down as a list
				for (int j = 0; j < strip->numIdx; j += 3)
				{
					WORD *tri = strip->idx+j;
					bs.WriteInt(3); //TrisTypeNum //hypov8 todo: this only writes 1 triangle for fan!!
					bs.WriteInt(0); //SubObjectID
					mdxGLCmd_t glcmds[3];
					for (int j = 0; j < 3; j++)
					{
						glcmds[j].vIdx = meshVertexOfs[tri[j]];//hypo //tri[j];
						glcmds[j].st[0] = fuvs[tri[j]].u;
						glcmds[j].st[1] = fuvs[tri[j]].v;
					}
					bs.WriteBytes(glcmds, sizeof(mdxGLCmd_t)*3);
				}
			}
			else
			{ //plot a strip
				bs.WriteInt(strip->numIdx); //TrisTypeNum
				bs.WriteInt(0);				//SubObjectID
				for (int j = 0; j < strip->numIdx; j++)
				{
					mdxGLCmd_t glcmd;
					WORD idx = strip->idx[j];
					glcmd.vIdx = meshVertexOfs[idx];//hypo //idx; 
					glcmd.st[0] = fuvs[idx].u;
					glcmd.st[1] = fuvs[idx].v;
					bs.WriteBytes(&glcmd, sizeof(mdxGLCmd_t));
				}
			}
		}
	}
	bs.WriteInt(0); //finish of with zero..
	//bs.WriteString("\0hypov8\0");

	glcmdsSize = bs.GetSize();
	BYTE *tmp = (BYTE *)rapi->Noesis_UnpooledAlloc(glcmdsSize);
	memcpy(tmp, bs.GetBuffer(), glcmdsSize);
	rapi->LogOutput("Generated %i GL commands\n", glcmdsSize);
	return tmp;
}

#define SetPPM_Elements(mOfs,vOfs, cNunV, cNumT, eType, MDX_PPPM, mNumV, mNumT) ( \
mOfs = (vOfs-cNunV), \
eType = MDX_PPPM, \
cNunV+= mNumV, \
cNumT+= mNumT \
);

void Model_MDX_SetupMeshElemets(sharedModel_t *pmdl, int *meshElementsOffs, int *meshElementsCountVert,int * meshElementsCountTri, BYTE *meshElementsIdxType)
{
	int vertCountOfs = 0;
	for (int i = 0; i < pmdl->numMeshes; i++)
	{
		sharedMesh_t *mesh = pmdl->meshes + i;
		if (!_strnicmp(mesh->name,"head", sizeof("head")-1))	{
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_HEAD], meshElementsCountTri[MDX_PLAYER_HEAD], meshElementsIdxType[i], MDX_PLAYER_HEAD, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"body", sizeof("body")-1))	{
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_BODY], meshElementsCountTri[MDX_PLAYER_BODY], meshElementsIdxType[i], MDX_PLAYER_BODY, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"legs", sizeof("legs")-1))	{
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_LEGS], meshElementsCountTri[MDX_PLAYER_LEGS], meshElementsIdxType[i], MDX_PLAYER_LEGS, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_bazooka", sizeof("w_bazooka")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_RL], meshElementsCountTri[MDX_PLAYER_RL], meshElementsIdxType[i], MDX_PLAYER_RL, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_flamethrower", sizeof("w_flamethrower")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_FL], meshElementsCountTri[MDX_PLAYER_FL], meshElementsIdxType[i], MDX_PLAYER_FL, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_grenade", sizeof("w_grenade")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_GL], meshElementsCountTri[MDX_PLAYER_GL], meshElementsIdxType[i], MDX_PLAYER_GL, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_heavy", sizeof("w_heavy")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_HMG], meshElementsCountTri[MDX_PLAYER_HMG], meshElementsIdxType[i], MDX_PLAYER_HMG, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_pipe", sizeof("w_pipe")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_PIPE], meshElementsCountTri[MDX_PLAYER_PIPE], meshElementsIdxType[i], MDX_PLAYER_PIPE, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_pistol", sizeof("w_pistol")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_PISTOL], meshElementsCountTri[MDX_PLAYER_PISTOL], meshElementsIdxType[i], MDX_PLAYER_PISTOL, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_shotgun", sizeof("w_shotgun")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_SG], meshElementsCountTri[MDX_PLAYER_SG], meshElementsIdxType[i], MDX_PLAYER_SG, mesh->numVerts, mesh->numTris);
		}
		else if (!_strnicmp(mesh->name,"w_tommygun", sizeof("w_tommygun")-1)){
			SetPPM_Elements(meshElementsOffs[i], vertCountOfs, meshElementsCountVert[MDX_PLAYER_TG], meshElementsCountTri[MDX_PLAYER_TG], meshElementsIdxType[i], MDX_PLAYER_TG, mesh->numVerts, mesh->numTris);
		}
		else // no match. diacard //-mdxplayer
		{
			meshElementsIdxType[i] = MDX_PLAYER_NULL;
			meshElementsOffs[i] = -1;
		}

		vertCountOfs += mesh->numVerts;
	}
}



/*
====================
Model_MDX_Write

MAIN write function
====================
*/
bool Model_MDX_Write(noesisModel_t *mdl, RichBitStream *outStream, noeRAPI_t *rapi)
{
	sharedModel_t *pmdl = rapi->rpgGetSharedModel(mdl, NMSHAREDFL_WANTGLOBALARRAY|NMSHAREDFL_REVERSEWINDING);
	if (!pmdl)
	{
		return false;
	}

	if (pmdl->numAbsVerts > 65535)
	{ //this is a hard format limit
		rapi->LogOutput("ERROR: numVerts (%i) exceeds 65535!\n", pmdl->numAbsVerts);
		return false;
	}


	// build mesh groups (head/body/legs) MDX_PLAYER_MODEL
	// offset index numbers to vertex. compensate for ommited mesh
	BYTE *meshElementsIdxType = (BYTE *)rapi->Noesis_UnpooledAlloc(pmdl->numMeshes);
	int *meshElementsOffs = (int *)rapi->Noesis_UnpooledAlloc(sizeof(int)*pmdl->numMeshes);
	WORD*meshVertexOfs = (WORD*)rapi->Noesis_UnpooledAlloc(sizeof(WORD)*pmdl->numAbsVerts);
	int modelParts = 1;

	int *meshElementsCountVert= (int *)rapi->Noesis_UnpooledAlloc(sizeof(int)*MDX_PLAYER_MAX);
	int *meshElementsCountTri= (int *)rapi->Noesis_UnpooledAlloc(sizeof(int)*MDX_PLAYER_MAX);

	memset(meshElementsCountVert, 0, sizeof(int)*MDX_PLAYER_MAX);
	memset(meshElementsCountTri, 0, sizeof(int)*MDX_PLAYER_MAX);

	//default vertex offsets
	for (int i = 0; i < pmdl->numAbsVerts; i++)	{
		meshVertexOfs[i] = i;
	}

#ifdef MDX_PLAYER_MODEL
	// player model. export using mesh names
	if (g_opts->exportPlayerModel)
	{
		modelParts = MDX_PLAYER_MAX;

		Model_MDX_SetupMeshElemets(pmdl, meshElementsOffs, meshElementsCountVert, meshElementsCountTri, meshElementsIdxType);

		//redefine vertex indexID
		for (int i = 0; i < pmdl->numAbsVerts; i++)
		{
			sharedVMap_t *svm = pmdl->absVertMap + i;
			meshVertexOfs[i] = i - meshElementsOffs[svm->meshIdx];
		}

		for (int i = 0; i < MDX_PLAYER_MAX; i++)
		{
			//todo mesh name?
			if (meshElementsCountTri[i]>MAX_MDX_VERT)
				rapi->LogOutput("WARNING: numVerts (%i) exceeds 2048, model will not work in standard Quake 2 engines.\n", meshElementsCountTri[i]);
			if (meshElementsCountVert[i]>MAX_MDX_TRI)
				rapi->LogOutput("WARNING: numTris (%i) exceeds 4096, model will not work in standard Quake 2 engines.\n", meshElementsCountVert[i]);
		}
	}
	else
#endif
	{
		if (pmdl->numAbsVerts > MAX_MDX_VERT)
			rapi->LogOutput("WARNING: numVerts (%i) exceeds 2048, model will not work in standard Quake 2 engines.\n", pmdl->numAbsVerts);
		if (pmdl->numAbsTris > MAX_MDX_TRI)
			rapi->LogOutput("WARNING: numTris (%i) exceeds 4096, model will not work in standard Quake 2 engines.\n", pmdl->numAbsTris);
	}

	//first, let's see if the data being exported contains any morph frames. 
	//we'll prioritize those over skeletal animation data, although we could also combine them.
	int maxMorphFrames = 0;
	for (int i = 0; i < pmdl->numMeshes; i++)
	{
		sharedMesh_t *mesh = pmdl->meshes+i;
		if (!mesh->morphFrames || mesh->numMorphFrames <= 0)
		{
			continue;
		}
		if (mesh->numMorphFrames > maxMorphFrames)
		{
			maxMorphFrames = mesh->numMorphFrames;
		}
	}


	mdxAnimHold_t *amdx = NULL;
	if (pmdl->bones && pmdl->numBones > 0 && maxMorphFrames <= 0)
	{ //if it's a skeletal mesh and no morph frames were found, look for some skeletal animation data
		amdx = Model_MDX_GetAnimData(rapi);
		if (amdx && amdx->numBones != pmdl->numBones)
		{ //got some, but the bone count doesn't match!
			amdx = NULL;
		}
	}

	int mdlFrames = (amdx) ? 1 + amdx->numFrames : 1 + maxMorphFrames;
	if (mdlFrames > 512)
		rapi->LogOutput("WARNING: numFrames (%i) exceeds 512, not compatible with standard Quake 2 network protocol.\n", mdlFrames);

	modelMatrix_t *animMats = (amdx) ? amdx->mats : NULL;
	//now, bake all the frame data out
	rapi->LogOutput("Compressing and encoding frames...\n");


	//create triangle data for complete model
	mdxTri_t *tris = (mdxTri_t *)rapi->Noesis_UnpooledAlloc(pmdl->numAbsTris*sizeof(mdxTri_t));
	for (int i = 0; i < pmdl->numAbsTris; i++)
	{
		modelLongTri_t *src = pmdl->absTris + i; //hypov8 todo: 
		mdxTri_t *dst = tris + i; //hypo
		dst->vIdx[0] = src->idx[0];
		dst->vIdx[1] = src->idx[1];
		dst->vIdx[2] = src->idx[2];
		dst->uvIdx[0] = src->idx[0]; //textureIndices
		dst->uvIdx[1] = src->idx[1]; //textureIndices
		dst->uvIdx[2] = src->idx[2]; //textureIndices
	}


	/////////////////////////////////////
	// deal with multi part player model
	/////////////////////////////////////
	for (int objectID = 0; objectID < modelParts; objectID++)
	{
		//set defaults or a complete model
		int numAbsVerts = pmdl->numAbsVerts;
		int numAbsTris = pmdl->numAbsTris;
		int frameSize = sizeof(mdxFrame_t) + sizeof(mdxVert_t)*pmdl->numAbsVerts;

#ifdef MDX_PLAYER_MODEL
		if (modelParts == MDX_PLAYER_MAX)
		{
			if (meshElementsCountVert[objectID]<=0)
				continue; //skip object
			numAbsVerts = meshElementsCountVert[objectID];
			numAbsTris = meshElementsCountTri[objectID];
			frameSize = sizeof(mdxFrame_t) + sizeof(mdxVert_t)*meshElementsCountVert[objectID];
		}
#endif

		//build frame data
		int bboxSize = sizeof(mdxBBox_t);
		mdxFrame_t *frames = (mdxFrame_t *)rapi->Noesis_UnpooledAlloc(frameSize*mdlFrames);
		mdxBBox_t *bboxFrames = (mdxBBox_t *)rapi->Noesis_UnpooledAlloc(bboxSize*mdlFrames);
		for (int i = 0; i < mdlFrames; i++)
		{
			Model_MDX_MakeFrame(pmdl, animMats, frames, i, frameSize, rapi, 
				bboxFrames, bboxSize, objectID, meshElementsIdxType); //hypov8 add bbox
		}

		//VERTEX INFO
		mdxVertInfo_t *vertexInfo = (mdxVertInfo_t *)rapi->Noesis_UnpooledAlloc(sizeof(mdxVertInfo_t)*numAbsVerts);
		for (int i = 0; i < numAbsVerts; i++)		
		{	//hypov8 vert group id (always 1) //todo group num(hit boxes)
			vertexInfo[i].objectNum = 1;
		}




#ifdef MDX_PLAYER_MODEL
		//create triangle data
		mdxTri_t *trisPPM = (mdxTri_t *)rapi->Noesis_UnpooledAlloc(numAbsTris*sizeof(mdxTri_t)); //-mdxplayer
		int triIdex = 0;
		if (g_opts->exportPlayerModel)
		{
			for (int i = 0; i < pmdl->numAbsTris; i++)	
			{
				modelLongTri_t *src = pmdl->absTris+i;
				sharedVMap_t *svm0 = pmdl->absVertMap + src->idx[0];
				if (svm0->meshIdx >= pmdl->numMeshes)
					continue;
				if (meshElementsIdxType[svm0->meshIdx] != (BYTE)objectID) //1,2 or 3
					continue;
				if (triIdex > numAbsTris)
					continue;

				mdxTri_t *dstPPM = trisPPM + triIdex; //i; //hypo
				dstPPM->vIdx[0] = meshVertexOfs[src->idx[0]];
				dstPPM->vIdx[1] = meshVertexOfs[src->idx[1]];
				dstPPM->vIdx[2] = meshVertexOfs[src->idx[2]];
				dstPPM->uvIdx[0] = meshVertexOfs[src->idx[0]]; //textureIndices
				dstPPM->uvIdx[1] = meshVertexOfs[src->idx[1]]; //textureIndices
				dstPPM->uvIdx[2] = meshVertexOfs[src->idx[2]]; //textureIndices
				triIdex++;
			}
		}
#endif
		mdxHdr_t hdr;
		memset(&hdr, 0, sizeof(hdr));
		memcpy(hdr.id, "IDPX", 4);
		hdr.ver = 4;
		hdr.frameSize = frameSize;
		hdr.numSkins = 1;
		hdr.numVerts = numAbsVerts;
		hdr.numTris = numAbsTris;
		hdr.numFrames = mdlFrames;


		// Build float UV array
		modelTexCoord_t *fuvs = (modelTexCoord_t *)rapi->Noesis_UnpooledAlloc(sizeof(modelTexCoord_t)*pmdl->numAbsVerts);
		memset(fuvs, 0, sizeof(modelTexCoord_t)*pmdl->numAbsVerts);
		Model_MDX_BuildUVs(pmdl, fuvs);

		//Setup texture data
		rapi->LogOutput("Building Texture lists...\n");
		mdxSkin_t *skins= (mdxSkin_t *)rapi->Noesis_UnpooledAlloc(sizeof(mdxSkin_t)*32); //max 32 skins
		memset(skins, 0, sizeof(sizeof(mdxSkin_t)*32));
		Model_MDX_CreateSkinLists(pmdl, hdr.skinWidth, hdr.skinHeight, hdr.numSkins, rapi, objectID, skins);



		//Build glCommands
		int glcmdsSize;
		BYTE *glcmds = Model_MDX_GenerateGLCmds(pmdl, tris, fuvs, glcmdsSize, rapi, 
			objectID, meshElementsIdxType, numAbsTris, meshVertexOfs);
		hdr.numGLCmds = glcmdsSize/4;


		//set up all the offsets
		hdr.ofsSkins = sizeof(hdr);
		hdr.ofsTris = hdr.ofsSkins + sizeof(mdxSkin_t)*hdr.numSkins;
		hdr.ofsFrames = hdr.ofsTris + sizeof(mdxTri_t)*hdr.numTris;
		hdr.ofsGLCmds = hdr.ofsFrames + frameSize*hdr.numFrames;
		hdr.offsetVertexInfo = hdr.ofsGLCmds + glcmdsSize;
		hdr.offsetSfxDefines =hdr.offsetVertexInfo + sizeof(mdxVertInfo_t)*hdr.numVerts; //todo: numObjects
		hdr.offsetSfxEntries = hdr.offsetSfxDefines + 0; // hdr.num_SfxDefines no sfx def's
		hdr.offsetBBoxFrames = hdr.offsetSfxEntries + 0; // no sfx entries
		hdr.offsetDummyEnd = hdr.offsetBBoxFrames+ (sizeof(mdxBBox_t)*hdr.numFrames);
		hdr.ofsEnd = hdr.offsetDummyEnd;

		//-mdxplayer
		//hypov8 todo check output file name
		if (g_opts->exportPlayerModel)
		{
			char		*filename = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
			rapi->Noesis_GetDirForFilePath(filename, rapi->Noesis_GetOutputName());
			strcat_s(filename, MAX_NOESIS_PATH, exportFileNames[objectID]);		

			RichBitStream FileBuff; 
			FileBuff.WriteBytes(&hdr, sizeof(hdr));
			FileBuff.WriteBytes(skins, sizeof(mdxSkin_t)*hdr.numSkins);
			FileBuff.WriteBytes(trisPPM, sizeof(mdxTri_t)*hdr.numTris);
			FileBuff.WriteBytes(frames, frameSize*hdr.numFrames);
			FileBuff.WriteBytes(glcmds, glcmdsSize);
			FileBuff.WriteBytes(vertexInfo, sizeof(mdxVertInfo_t)*hdr.numVerts);
			FileBuff.WriteBytes(bboxFrames, sizeof(mdxBBox_t)* hdr.numFrames);

			rapi->Noesis_WriteFile(filename, FileBuff.GetBuffer(), hdr.ofsEnd);

			rapi->LogOutput("Write mdx Player file: \"%s\"\n", filename);
			rapi->Noesis_UnpooledFree(filename);
		}
		else
		{
			//now write it all to the output stream
			outStream->WriteBytes(&hdr, sizeof(hdr));
			outStream->WriteBytes(skins, sizeof(mdxSkin_t)*hdr.numSkins);
			outStream->WriteBytes(tris, sizeof(mdxTri_t)*hdr.numTris);
			outStream->WriteBytes(frames, frameSize*hdr.numFrames);
			outStream->WriteBytes(glcmds, glcmdsSize);
			outStream->WriteBytes(vertexInfo, sizeof(mdxVertInfo_t)*hdr.numVerts);
			// offsetSfxDefines null
			// offsetSfxEntries null
			outStream->WriteBytes(bboxFrames, sizeof(mdxBBox_t)* hdr.numFrames); //hypov8 bbox
			//end
		}

		rapi->Noesis_UnpooledFree(glcmds);
		rapi->Noesis_UnpooledFree(skins);
		rapi->Noesis_UnpooledFree(fuvs);
		rapi->Noesis_UnpooledFree(frames);
		rapi->Noesis_UnpooledFree(trisPPM);
		rapi->Noesis_UnpooledFree(vertexInfo); //hypov8 vertexinfo
		rapi->Noesis_UnpooledFree(bboxFrames); //hypov8 bbox
	}//end (player model)

	rapi->Noesis_UnpooledFree(tris);
	rapi->Noesis_UnpooledFree(meshElementsIdxType); 
	rapi->Noesis_UnpooledFree(meshElementsOffs); 
	rapi->Noesis_UnpooledFree(meshVertexOfs);

	rapi->Noesis_UnpooledFree(meshElementsCountTri);
	rapi->Noesis_UnpooledFree(meshElementsCountVert);

	return true;
}

//catch anim writes
//(note that this function would normally write converted data to a file at anim->filename, but for this format it instead saves the data to combine with the model output)
void Model_MDX_WriteAnim(noesisAnim_t *anim, noeRAPI_t *rapi)
{
	if (!rapi->Noesis_HasActiveGeometry() || rapi->Noesis_GetActiveType() != g_fmtHandle)
	{
		rapi->LogOutput("WARNING: Stand-alone animations cannot be converted to MDX.\nNothing will be written.\n");
		return;
	}

	rapi->Noesis_SetExtraAnimData(anim->data, anim->dataLen);
}
