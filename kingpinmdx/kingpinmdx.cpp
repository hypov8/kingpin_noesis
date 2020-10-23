//General requirements and/or suggestions for Noesis plugins:
//
// -Use 1-byte struct alignment for all shared structures. (plugin MSVC project file defaults everything to 1-byte-aligned)
// -Always clean up after yourself. Your plugin stays in memory the entire time Noesis is loaded, so you don't want to crap up the process heaps.
// -Try to use reliable type-checking, to ensure your plugin doesn't conflict with other file types and create false-positive situations.
// -Really try not to write crash-prone logic in your data check function! This could lead to Noesis crashing from trivial things like the user browsing files.
// -When using the rpg begin/end interface, always make your Vertex call last, as it's the function which triggers a push of the vertex with its current attributes.
// -!!!! Check the web site and documentation for updated suggestions/info! !!!!

//hypov8
//v1.00
//  initial release. just importer working
//v1.01
//  fix: when a files is clicked->open with. noesis cant find the image path
//v1.02
//  exporter implimented
//v1.03
//  added player model suport. export all 3 body parts. enable using -mdxplayer
//  any mesh name that contains "_head_" will become head.mdx
//  any mesh name that contains "_body_" will become body.mdx
//  all other mesh names will become legs.mdx
//  export model search paths for textures updated. includes looking at diffuse image if it exists(fbx)
//  import model search paths look for kingpin dir. fix for missing textures that reside in main\
//  import model now allocates all images. even if not used on mesh (md2/mdx can only display 1 texture but can switch between 32)
//  better implimentation of memory use


#include "stdafx.h"
#include "kingpinmdx.h"
#include "q2nrm.h"
#include "q2pal.h"

extern bool Model_MDX_Write(noesisModel_t *mdl, RichBitStream *outStream, noeRAPI_t *rapi);
extern void Model_MDX_WriteAnim(noesisAnim_t *anim, noeRAPI_t *rapi);

const char *g_pPluginName = "kingpinmdx";
const char *g_pPluginDesc = "Kingpin MDX format handler, by Hypov8. v1.03"; //duplicate of md2
int g_fmtHandle = -1;

mdxOpts_t *g_opts = NULL;


//is it this format?
bool Model_MDX_Check(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	if (bufferLen < sizeof(mdxHdr_t))
	{
		return false;
	}
	mdxHdr_t *hdr = (mdxHdr_t *)fileBuffer;
	if (memcmp(hdr->id, "IDPX", 4))
	{
		return false;
	}
	if (hdr->ver != 4)
	{
		return false;
	}
	if (hdr->ofsEnd <= 0 || hdr->ofsEnd > bufferLen)
	{
		return false;
	}
	if (hdr->ofsSkins <= 0 || hdr->ofsSkins > bufferLen ||
		//hdr->ofsST <= 0 || hdr->ofsST > bufferLen || //not in mdx
		hdr->ofsTris <= 0 || hdr->ofsTris > bufferLen ||
		hdr->ofsFrames <= 0 || hdr->ofsFrames > bufferLen ||
		hdr->ofsGLCmds <= 0 || hdr->ofsGLCmds > bufferLen ||
	//mdx
		hdr->offsetVertexInfo <= 0 || hdr->offsetVertexInfo > bufferLen ||
		hdr->offsetSfxDefines <= 0 || hdr->offsetSfxDefines > bufferLen ||
		hdr->offsetSfxEntries <= 0 || hdr->offsetSfxEntries > bufferLen ||
		hdr->offsetBBoxFrames <= 0 || hdr->offsetBBoxFrames > bufferLen ||
		hdr->offsetDummyEnd <= 0 || hdr->offsetDummyEnd > bufferLen ||

		hdr->ofsEnd <= 0 || hdr->ofsEnd > bufferLen)
	{
		return false;
	}

	return true;
}

//decode a vert
static void Model_MDX_DecodeVert(mdxFrame_t *frame, mdxVert_t *vert, float *pos, float *nrm)
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

void Model_MDX_SearchSkins(noeRAPI_t *rapi, char *skinPath, char *origSkinName)
{
	char *playerIdx, *modelIdx, *textIdx, *kingIdx;
	int  lenFile, skinIdx = 0;

	StringToLower(skinPath);
	playerIdx = strstr(skinPath, "\\players\\");
	modelIdx = strstr(skinPath, "\\models\\");
	textIdx = strstr(skinPath, "\\textures\\");
	kingIdx = strstr(skinPath, "\\kingpin\\"); //search in main\. model might be in mod folder


	if (modelIdx || playerIdx || textIdx || kingIdx)
	{
		bool foundFile = false;
		int i = 0;
		char *fExt[5] = { ".tga", ".pcx", ".jpg", ".png", ".dds" };

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
			while (i < 5)
			{
				rapi->Noesis_GetExtensionlessName(skinPath, skinPath);
				strcat_s(skinPath, MAX_NOESIS_PATH, fExt[i]);
				if (rapi->Noesis_FileExists(skinPath))
					break;
				i++;
			}
			if (i == 5)
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
					if (i == 5)
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
void Model_MDX_SetupSkins(noeRAPI_t *rapi, mdxSkin_t *skins, int skinCount)
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
		Model_MDX_SearchSkins(rapi, srcDir, skins->name+sizeof(mdxSkin_t)*i);
		rapi->rpgSetMaterial(srcDir);
		if (i== 0) //set mesh name to first skin
			rapi->rpgSetName(skins->name);
	}
	//hypov8 use first image as display
	rapi->rpgSetMaterialIndex(0);

	rapi->Noesis_UnpooledFree(srcDir);
	rapi->Noesis_UnpooledFree(noesisFileDir);

}

//mdx todo: object number..
//load it (note that you don't need to worry about validation here, if it was done in the Check function)
noesisModel_t *Model_MDX_Load(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	int			mdxGlVertCount = 0;
	mdxHdr_t	*hdr = (mdxHdr_t *)fileBuffer;
	mdxSkin_t	*skins = (mdxSkin_t *)(fileBuffer+hdr->ofsSkins);
	int			frameSize = hdr->frameSize;
	mdxGLCmd_t *mdxGlVertArray = (mdxGLCmd_t*)rapi->Noesis_UnpooledAlloc(hdr->numGLCmds*sizeof(mdxGLCmd_t));
	void		*pgctx = rapi->rpgCreateContext();

	for (int j = 1; j < hdr->numFrames; j++)
	{ //commit extra frames as morph frames
		int frameNum = 0;
		mdxFrame_t *frame = (mdxFrame_t *)(fileBuffer+hdr->ofsFrames + frameSize*j);
		mdxVert_t *vertData = (mdxVert_t *)(frame+1);
		float *xyz = (float *)rapi->Noesis_PooledAlloc(sizeof(float)*3*hdr->numVerts);
		float *nrm = (float *)rapi->Noesis_PooledAlloc(sizeof(float)*3*hdr->numVerts);
		for (int k = 0; k < hdr->numVerts; k++)
		{
			Model_MDX_DecodeVert(frame, vertData+k, xyz+k*3, nrm+k*3);
		}
		rapi->rpgFeedMorphTargetPositions(xyz, RPGEODATA_FLOAT, sizeof(float)*3);
		rapi->rpgFeedMorphTargetNormals(nrm, RPGEODATA_FLOAT, sizeof(float)*3);
		rapi->rpgCommitMorphFrame(hdr->numVerts);
	}
	rapi->rpgCommitMorphFrameSet();

	//import all skins
	Model_MDX_SetupSkins(rapi, skins, hdr->numSkins);	


	//hypov8 retreve mdx gl commands data
	{
		int countGLHeader = 0;	//# glcommands read from buffer
		int countGLvertex = 0;	//# glvertex read from buffer

		bool glData = true;
		while (glData)
		{
			mdxGLCmdHeader_t *glInfo = (mdxGLCmdHeader_t *)(fileBuffer + hdr->ofsGLCmds + (sizeof(mdxGLCmdHeader_t)*countGLHeader) + (sizeof(mdxGLCmd_t)* countGLvertex));
			int numGlVerts = glInfo->TrisTypeNum;
			int objID = glInfo->SubObjectID; //hypov8 todo: asign sub objects
			int glVertCnt;
			countGLHeader += 1;
			mdxGLCmd_t *glVert = (mdxGLCmd_t *)(fileBuffer + hdr->ofsGLCmds + (sizeof(mdxGLCmdHeader_t)*countGLHeader) + (sizeof(mdxGLCmd_t)*countGLvertex));

			//Triangle strip		
			if (numGlVerts > 0 && numGlVerts >= 3 && numGlVerts < MAX_MDX_VERT)		
			{
				bool flip = false;
				glVertCnt = numGlVerts;

				//set first tri.
				copyUVIdx(glVert[0], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
				copyUVIdx(glVert[1], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
				copyUVIdx(glVert[2], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
				countGLvertex += 3;

				for (int j = 3; j < glVertCnt;j++)
				{
					if (!flip)
					{
						copyUVIdx(glVert[j-1], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
						copyUVIdx(glVert[j-2], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
						copyUVIdx(glVert[j], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
					}
					else
					{
						copyUVIdx(glVert[j-2], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
						copyUVIdx(glVert[j-1], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
						copyUVIdx(glVert[j], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
					}

					countGLvertex += 1;
					flip = flip ? false : true;
				}
			}
			//Triangle fan
			else if (numGlVerts < 0 && numGlVerts <= -3 && numGlVerts > -MAX_MDX_VERT)
			{
				glVertCnt = -numGlVerts;

				//store vert[0] (centre)
				copyUVIdx(glVert[0], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
				copyUVIdx(glVert[1], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
				copyUVIdx(glVert[2], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
				countGLvertex += 3;

				for (int j = 3; j < glVertCnt;j++)
				{
					copyUVIdx(glVert[j], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);
					copyUVIdx(glVert[0], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);//vert[0] (centre)
					copyUVIdx(glVert[j-1], mdxGlVertArray[mdxGlVertCount], mdxGlVertCount);//previous vertex
					countGLvertex += 1;
				}
			}
			else
				glData = false; //done. This will also catch some vertex count errors
		}
	}

	//now render out the base frame geometry
	mdxFrame_t *frame = (mdxFrame_t *)(fileBuffer + hdr->ofsFrames);
	mdxVert_t *vertData = (mdxVert_t *)(frame + 1);
	rapi->rpgBegin(RPGEO_TRIANGLE);

	for (int i = 0; i < mdxGlVertCount; i+=3)
	{
		for (int j = 2; j >= 0; j--)
		{
			int order = i+j;
			int vIdx = mdxGlVertArray[order].vIdx;
			float pos[3], nrm[3], uv[2];

			Model_MDX_DecodeVert(frame, &vertData[vIdx], pos, nrm);

			uv[0] = mdxGlVertArray[order].st[0];
			uv[1] = mdxGlVertArray[order].st[1];

			rapi->rpgVertUV2f(uv, 0);
			rapi->rpgVertNormal3f(nrm);
			rapi->rpgVertMorphIndex(vIdx); //this is important to tie this vertex to the pre-provided morph arrays
			rapi->rpgVertex3f(pos);
		}
	}
	rapi->rpgEnd();
	rapi->rpgOptimize();

	rapi->Noesis_UnpooledFree(mdxGlVertArray);

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
static bool Model_MDX_OptHandlerA(const char *arg, unsigned char *store, int storeSize)
{
	mdxOpts_t *lopts = (mdxOpts_t *)store;
	assert(storeSize == sizeof(mdxOpts_t));
	lopts->exportPlayerModel = 1;
	return true;
}

//called by Noesis to init the plugin
bool NPAPI_InitLocal(void)
{
	g_fmtHandle = g_nfn->NPAPI_Register("Kingpin MDX", ".mdx");
	if (g_fmtHandle < 0)
	{
		return false;
	}

	//set the data handlers for this format
	g_nfn->NPAPI_SetTypeHandler_TypeCheck(g_fmtHandle, Model_MDX_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(g_fmtHandle, Model_MDX_Load);
	//export functions
	g_nfn->NPAPI_SetTypeHandler_WriteModel(g_fmtHandle, Model_MDX_Write);
	g_nfn->NPAPI_SetTypeHandler_WriteAnim(g_fmtHandle, Model_MDX_WriteAnim);

	//add first parm
	addOptParms_t optParms;
	memset(&optParms, 0, sizeof(optParms));
	optParms.optName = "-mdxplayer"; 
	optParms.optDescr = "Export MDX into 3 parts. Mesh names _head_ and _body_ should exist";
	optParms.storeSize = sizeof(mdxOpts_t);
	optParms.handler = Model_MDX_OptHandlerA;
	g_opts = (mdxOpts_t *)g_nfn->NPAPI_AddTypeOption(g_fmtHandle, &optParms);
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
