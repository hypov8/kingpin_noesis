//General requirements and/or suggestions for Noesis plugins:
//
// -Use 1-byte struct alignment for all shared structures. (plugin MSVC project file defaults everything to 1-byte-aligned)
// -Always clean up after yourself. Your plugin stays in memory the entire time Noesis is loaded, so you don't want to crap up the process heaps.
// -Try to use reliable type-checking, to ensure your plugin doesn't conflict with other file types and create false-positive situations.
// -Really try not to write crash-prone logic in your data check function! This could lead to Noesis crashing from trivial things like the user browsing files.
// -When using the rpg begin/end interface, always make your Vertex call last, as it's the function which triggers a push of the vertex with its current attributes.
// -!!!! Check the web site and documentation for updated suggestions/info! !!!!

#include "stdafx.h"
#include "quakemd2.h"
#include "q2nrm.h"
#include "q2pal.h"

extern bool Model_MD2_Write(noesisModel_t *mdl, RichBitStream *outStream, noeRAPI_t *rapi);
extern void Model_MD2_WriteAnim(noesisAnim_t *anim, noeRAPI_t *rapi);

const char *g_pPluginName = "quakemd2";
const char *g_pPluginDesc = "Kingpin MD2 format handler, update by hypov8. v1.03.";
int g_fmtHandle = -1;

md2Opts_t *g_opts = NULL;

//is it this format?
bool Model_MD2_Check(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	if (bufferLen < sizeof(md2Hdr_t))
	{
		return false;
	}
	md2Hdr_t *hdr = (md2Hdr_t *)fileBuffer;
	if (memcmp(hdr->id, "IDP2", 4))
	{
		return false;
	}
	if (hdr->ver != 8)
	{
		return false;
	}
	if (hdr->ofsEnd <= 0 || hdr->ofsEnd > bufferLen)
	{
		return false;
	}
	if (hdr->ofsSkins <= 0 || hdr->ofsSkins > bufferLen ||
		hdr->ofsST <= 0 || hdr->ofsST > bufferLen ||
		hdr->ofsTris <= 0 || hdr->ofsTris > bufferLen ||
		hdr->ofsFrames <= 0 || hdr->ofsFrames > bufferLen ||
		hdr->ofsGLCmds <= 0 || hdr->ofsGLCmds > bufferLen ||
		hdr->ofsEnd <= 0 || hdr->ofsEnd > bufferLen)
	{
		return false;
	}

	return true;
}

//decode a vert
static void Model_MD2_DecodeVert(md2Frame_t *frame, md2Vert_t *vert, float *pos, float *nrm)
{
#ifndef HYPODEBUG
	assert(vert->nrmIdx < 162);
#else
	if (vert->nrmIdx >= 162)
		return; //breakpoint. catch errors in file
#endif
	nrm[0] = g_q2Normals[vert->nrmIdx][0];
	nrm[1] = g_q2Normals[vert->nrmIdx][1];
	nrm[2] = g_q2Normals[vert->nrmIdx][2];
	pos[0] = (float)vert->pos[0] * frame->scale[0] + frame->trans[0];
	pos[1] = (float)vert->pos[1] * frame->scale[1] + frame->trans[1];
	pos[2] = (float)vert->pos[2] * frame->scale[2] + frame->trans[2];
}

//decode st's
static void Model_MD2_DecodeST(md2Hdr_t *hdr, short *st, float *uv)
{
	uv[0] = (float)st[0] / (float)hdr->skinWidth;
	uv[1] = (float)st[1] / (float)hdr->skinHeight;
}

void StringConvertToBackSlash(char *inString)
{
	for (int i = 0; i < MAX_NOESIS_PATH; i++)
	{
		if (inString[i] == '/') 
			inString[i] = '\\';

		if (inString[i] == '\0')
			return;
	}
}
void StringConvertToForwardSlash(char *inString)
{
	for (int i = 0; i < MAX_NOESIS_PATH; i++)
	{
		if (inString[i] == '\\') 
			inString[i] = '/';

		if (inString[i] == '\0')
			return;
	}
}
void StringToLower(char *inString)
{
	for (int i = 0; i < MAX_NOESIS_PATH; i++)
	{
		if (inString[i] >= 'A' && inString[i] <= 'Z')
			inString[i] = tolower(inString[i]);

		if (inString[i] == '\0')
			return;
	}
}

void Model_MD2_SearchSkins(noeRAPI_t *rapi, char *skinPath, char *origSkinName)
{
	char *playerIdx, *modelIdx, *textIdx, *kingIdx;
	int  lenFile, skinIdx = 0;

	StringToLower(skinPath);
	playerIdx = strstr(skinPath, "\\players\\");
	modelIdx = strstr(skinPath, "\\models\\");
	textIdx = strstr(skinPath, "\\textures\\");
	kingIdx = strstr(skinPath, "\\kingpin\\"); //search in main\. model might be in mod folder


	if (modelIdx||playerIdx||textIdx||kingIdx)
	{
		bool foundFile = false;
		int i = 0;
		char *fExt[5]= { ".tga", ".pcx", ".jpg", ".png", ".dds" };

		lenFile = strlen(skinPath);
		if (modelIdx) 
			skinPath[lenFile - strlen(modelIdx) + 1] = '\0';
		else if (playerIdx)
			skinPath[lenFile - strlen(playerIdx) + 1] = '\0';
		else if (textIdx)
			skinPath[lenFile - strlen(textIdx) + 1] = '\0';
		else //search main\ folder. cant find a valid folder
		{
			skinPath[lenFile - strlen(kingIdx) + 1] = '\0';
			strcat_s(skinPath, MAX_NOESIS_PATH, "kingpin\\main\\");
			kingIdx = NULL;
		}

		//combine and clean string
		strcat_s(skinPath, MAX_NOESIS_PATH, origSkinName);
		StringConvertToBackSlash(skinPath);

		if (!rapi->Noesis_FileExists(skinPath))
		{
			while (i <5)
			{
				rapi->Noesis_GetExtensionlessName(skinPath, skinPath);
				strcat_s(skinPath, MAX_NOESIS_PATH, fExt[i]);
				if (rapi->Noesis_FileExists(skinPath))
					break;
				i++;
			}
			if (i == 5 )
			{
				i = 0;
				if (kingIdx)
				{	
					lenFile = strlen(skinPath);
					skinPath[lenFile - strlen(kingIdx) + 1] = '\0';
					strcat_s(skinPath, MAX_NOESIS_PATH, "kingpin\\main\\");
					strcat_s(skinPath, MAX_NOESIS_PATH, origSkinName);		
					StringConvertToBackSlash(skinPath);
					if (!rapi->Noesis_FileExists(skinPath))
					{
						while (i < 5)
						{
							rapi->Noesis_GetExtensionlessName(skinPath, skinPath);
							strcat_s(skinPath, MAX_NOESIS_PATH, fExt[i]);
							if (rapi->Noesis_FileExists(skinPath))
								break;
							i++;
						}
					}
					if (i == 5 )
						strcpy_s(skinPath, MAX_NOESIS_PATH, origSkinName); //fail
				}
				else
					strcpy_s(skinPath, MAX_NOESIS_PATH, origSkinName); //fail
			}
		}
	}
	else	//set the default material name (use in model)
		strcpy_s(skinPath, MAX_NOESIS_PATH, origSkinName); //fail
}

/*
///////////////////////////////
Model_MDX_SetupSkins

build skin. resolve full path
todo: use main/baseq2?
todo: load all skins. into mat?
///////////////////////////////
*/
void Model_MD2_SetupSkins(noeRAPI_t *rapi, md2Skin_t *skins, int skinCount)
{
	size_t cnt;
	char		*srcDir = (char*)rapi->Noesis_UnpooledAlloc(MAX_NOESIS_PATH);
	wchar_t		*noesisFileDir = (wchar_t*)rapi->Noesis_UnpooledAlloc(sizeof(wchar_t)*MAX_NOESIS_PATH);

	g_nfn->NPAPI_GetSelectedFile(noesisFileDir);

	//hypov8 command line lunched
	if (!noesisFileDir[0])
		g_nfn->NPAPI_GetOpenPreviewFile(noesisFileDir);

	//hypov8 add all skins in model.
	for (int i = 0; i < skinCount; i++)
	{
		wcstombs_s(&cnt, srcDir,(size_t)MAX_NOESIS_PATH, noesisFileDir, (size_t)(MAX_NOESIS_PATH*sizeof(wchar_t)));
		Model_MD2_SearchSkins(rapi, srcDir, skins->name+sizeof(md2Skin_t)*i);
		rapi->rpgSetMaterial(srcDir);
		if (i== 0) //set mesh name to first skin
			rapi->rpgSetName(skins->name);
	}
	//hypov8 use first image as display
	rapi->rpgSetMaterialIndex(0);

	rapi->Noesis_UnpooledFree(srcDir);
	rapi->Noesis_UnpooledFree(noesisFileDir);

}


//load it (note that you don't need to worry about validation here, if it was done in the Check function)
noesisModel_t *Model_MD2_Load(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	int			md2GlVertCount = 0;
	md2Hdr_t	*hdr = (md2Hdr_t *)fileBuffer;
	md2Skin_t	*skins = (md2Skin_t *)(fileBuffer+hdr->ofsSkins);
	int			frameSize = hdr->frameSize;
	md2GLCmd_t *md2GlVertArray = (md2GLCmd_t*)rapi->Noesis_UnpooledAlloc(hdr->numGLCmds*sizeof(md2GLCmd_t));

	void		*pgctx = rapi->rpgCreateContext();
	//md2 specific
	md2ST_t		*sts = (md2ST_t *)(fileBuffer+hdr->ofsST);
	md2Tri_t	*tris = (md2Tri_t *)(fileBuffer+hdr->ofsTris);
	int			hasGLCcmds = (hdr->numGLCmds > 3) ? true : false;

	for (int j = 1; j < hdr->numFrames; j++)
	{ //commit extra frames as morph frames
		int frameNum = 0;
		md2Frame_t *frame = (md2Frame_t *)(fileBuffer+hdr->ofsFrames + frameSize*j);
		md2Vert_t *vertData = (md2Vert_t *)(frame+1);
		float *xyz = (float *)rapi->Noesis_PooledAlloc(sizeof(float)*3*hdr->numVerts);
		float *nrm = (float *)rapi->Noesis_PooledAlloc(sizeof(float)*3*hdr->numVerts);
		for (int k = 0; k < hdr->numVerts; k++)
		{
			Model_MD2_DecodeVert(frame, vertData+k, xyz+k*3, nrm+k*3);;
		}
		rapi->rpgFeedMorphTargetPositions(xyz, RPGEODATA_FLOAT, sizeof(float)*3);
		rapi->rpgFeedMorphTargetNormals(nrm, RPGEODATA_FLOAT, sizeof(float)*3);
		rapi->rpgCommitMorphFrame(hdr->numVerts);
	}
	rapi->rpgCommitMorphFrameSet();

	//import all skins
	Model_MD2_SetupSkins(rapi, skins, hdr->numSkins);


	//hypov8 retreve mdx gl commands data
	if (hasGLCcmds)
	{
		int countGLHeader = 0;	//# glcommands read from buffer
		int countGLvertex = 0;	//# glvertex read from buffer

		bool glData = true;
		while (glData)
		{
			md2GLCmdHeader_t *glInfo = (md2GLCmdHeader_t *)(fileBuffer + hdr->ofsGLCmds + (sizeof(md2GLCmdHeader_t)*countGLHeader) + (sizeof(md2GLCmd_t)* countGLvertex));
			int numGlVerts = glInfo->TrisTypeNum;
			int glVertCnt;
			countGLHeader += 1;
			md2GLCmd_t *glVert = (md2GLCmd_t *)(fileBuffer + hdr->ofsGLCmds + (sizeof(md2GLCmdHeader_t)*countGLHeader) + (sizeof(md2GLCmd_t)*countGLvertex));

			//Triangle strip		
			if (numGlVerts > 0 && numGlVerts >= 3 && numGlVerts < MAX_MD2_VERT)
			{
				bool flip = false;
				glVertCnt = numGlVerts;

				//set first tri.
				copyUVIdx(glVert[0], md2GlVertArray[md2GlVertCount], md2GlVertCount);
				copyUVIdx(glVert[1], md2GlVertArray[md2GlVertCount], md2GlVertCount);
				copyUVIdx(glVert[2], md2GlVertArray[md2GlVertCount], md2GlVertCount);
				countGLvertex += 3;

				for (int j = 3; j < glVertCnt; j++)
				{
					if (!flip)
					{
						copyUVIdx(glVert[j - 1], md2GlVertArray[md2GlVertCount], md2GlVertCount);
						copyUVIdx(glVert[j - 2], md2GlVertArray[md2GlVertCount], md2GlVertCount);
						copyUVIdx(glVert[j], md2GlVertArray[md2GlVertCount], md2GlVertCount);
					}
					else
					{
						copyUVIdx(glVert[j - 2], md2GlVertArray[md2GlVertCount], md2GlVertCount);
						copyUVIdx(glVert[j - 1], md2GlVertArray[md2GlVertCount], md2GlVertCount);
						copyUVIdx(glVert[j], md2GlVertArray[md2GlVertCount], md2GlVertCount);
					}

					countGLvertex += 1;
					flip = flip ? false : true;
				}
			}
			//Triangle fan
			else if (numGlVerts < 0 && numGlVerts <= -3 && numGlVerts > -MAX_MD2_VERT)
			{
				glVertCnt = -numGlVerts;

				//store vert[0] (centre)
				copyUVIdx(glVert[0], md2GlVertArray[md2GlVertCount], md2GlVertCount);
				copyUVIdx(glVert[1], md2GlVertArray[md2GlVertCount], md2GlVertCount);
				copyUVIdx(glVert[2], md2GlVertArray[md2GlVertCount], md2GlVertCount);
				countGLvertex += 3;

				for (int j = 3; j < glVertCnt; j++)
				{
					copyUVIdx(glVert[j], md2GlVertArray[md2GlVertCount], md2GlVertCount);
					copyUVIdx(glVert[0], md2GlVertArray[md2GlVertCount], md2GlVertCount);//vert[0] (centre)
					copyUVIdx(glVert[j - 1], md2GlVertArray[md2GlVertCount], md2GlVertCount);//previour vertex
					countGLvertex += 1;
				}
			}
			else
				glData = false; //done. This will also catch some vertex count errors
		}
	}



	//now render out the base frame geometry
	md2Frame_t *frame = (md2Frame_t *)(fileBuffer+hdr->ofsFrames);
	md2Vert_t *vertData = (md2Vert_t *)(frame+1);
	rapi->rpgBegin(RPGEO_TRIANGLE);

	if (hasGLCcmds)
	{
		for (int i = 0; i < md2GlVertCount; i+=3)
		{
			for (int j = 2; j >= 0; j--)
			{
				int order = i+j;
				int vIdx = md2GlVertArray[order].idx;
				float pos[3], nrm[3], uv[2];

				Model_MD2_DecodeVert(frame, &vertData[vIdx], pos, nrm);

				uv[0] = md2GlVertArray[order].st[0];
				uv[1] = md2GlVertArray[order].st[1];

				rapi->rpgVertUV2f(uv, 0);
				rapi->rpgVertNormal3f(nrm);
				rapi->rpgVertMorphIndex(vIdx); //this is important to tie this vertex to the pre-provided morph arrays
				rapi->rpgVertex3f(pos);
			}
		}
	}
	else
	{
		for (int i = 0; i < hdr->numTris; i++)
		{
			md2Tri_t *tri = tris + i;
			for (int j = 2; j >= 0; j--) //loop backwards because q2 had reverse face windings
			{
				int vIdx = tri->vIdx[j];
				float pos[3], nrm[3], uv[2];
				Model_MD2_DecodeVert(frame, &vertData[vIdx], pos, nrm);
				Model_MD2_DecodeST(hdr, sts[tri->stIdx[j]].st, uv);

				rapi->rpgVertUV2f(uv, 0);
				rapi->rpgVertNormal3f(nrm);
				rapi->rpgVertMorphIndex(vIdx); //this is important to tie this vertex to the pre-provided morph arrays
				rapi->rpgVertex3f(pos);
			}
		}
	}
	rapi->rpgEnd();
	rapi->rpgOptimize();

	rapi->Noesis_UnpooledFree(md2GlVertArray);

	noesisModel_t *mdl = rapi->rpgConstructModel();
	if (mdl)
	{
		numMdl = 1; //it's important to set this on success! you can set it to > 1 if you have more than 1 contiguous model in memory
		rapi->SetPreviewAnimSpeed(10.0f);
		//this'll rotate the model (only in preview mode) into quake-friendly coordinates
		static float mdlAngOfs[3] = {0.0f, 180.0f, 0.0f};
		rapi->SetPreviewAngOfs(mdlAngOfs);
	}

	rapi->rpgDestroyContext(pgctx);
	return mdl;
}

//handle -pskkeepspace
static bool Model_MD2_OptHandlerA(const char *arg, unsigned char *store, int storeSize)
{
	md2Opts_t *lopts = (md2Opts_t *)store;
	assert(storeSize == sizeof(md2Opts_t));
	lopts->exportPlayerModel = 1;
	return true;
}

//called by Noesis to init the plugin
bool NPAPI_InitLocal(void)
{
	g_fmtHandle = g_nfn->NPAPI_Register("Kingpin MD2", ".md2");
	if (g_fmtHandle < 0)
	{
		return false;
	}

	//set the data handlers for this format
	g_nfn->NPAPI_SetTypeHandler_TypeCheck(g_fmtHandle, Model_MD2_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(g_fmtHandle, Model_MD2_Load);
	//export functions
	g_nfn->NPAPI_SetTypeHandler_WriteModel(g_fmtHandle, Model_MD2_Write);
	g_nfn->NPAPI_SetTypeHandler_WriteAnim(g_fmtHandle, Model_MD2_WriteAnim);

	//add first parm
	addOptParms_t optParms;
	memset(&optParms, 0, sizeof(optParms));
	optParms.optName = "-md2player"; 
	optParms.optDescr = "Export MD2 into 3 parts. Mesh names head, body and legs should exist";
	optParms.storeSize = sizeof(md2Opts_t);
	optParms.handler = Model_MD2_OptHandlerA;
	g_opts = (md2Opts_t *)g_nfn->NPAPI_AddTypeOption(g_fmtHandle, &optParms);
	assert(g_opts);
	optParms.shareStore = (unsigned char *)g_opts;

	return true;
}

//called by Noesis before the plugin is freed
void NPAPI_ShutdownLocal(void)
{
	//nothing to do in this plugin
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}
