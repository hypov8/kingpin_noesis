#include "../pluginshare.h"
#include <math.h>

//#define USE_GLCOMMANDS_VERSION //hypo use newer kp style glCmds
#define copyUVIdx(src, dst, vertCnt) (dst.st[0]=src.st[0],dst.st[1]=src.st[1],dst.vIdx=src.vIdx, vertCnt+=1)

#define MAX_MDX_FRAMES 512 //hypov8 was 1024??
#define MAX_MDX_OBJECTS 64
#define MAX_MDX_VERT 2048
#define MAX_MDX_TRI 4096

enum
{
	MDX_PLAYER_HEAD,	//0
	MDX_PLAYER_BODY,	//1
	MDX_PLAYER_LEGS,	//2

	MDX_PLAYER_RL,		//3
	MDX_PLAYER_FL,		//4
	MDX_PLAYER_GL,		//5
	MDX_PLAYER_HMG,	//6
	MDX_PLAYER_PIPE,	//7
	MDX_PLAYER_PISTOL,	//8
	MDX_PLAYER_SG,		//9
	MDX_PLAYER_TG,		//10
	MDX_PLAYER_MAX,	//11

	MDX_PLAYER_NULL = 255
};


typedef struct mdxOpts_s
{
	int					exportPlayerModel; // keepBoneSpaces;
} mdxOpts_t;

typedef struct mdxHdr_s
{
	BYTE			id[4];
	int				ver; //kp ver = 4

	int				skinWidth;
	int				skinHeight;
	int				frameSize;

	int				numSkins;				// number of textures
	int				numVerts;				// number of vertices
	int				numTris;				// number of triangles
	int				numGLCmds;				// number of gl commands
	int				numFrames;				// number of frames
	int				num_SfxDefines; //mdx	// number of sfx definitions
	int				num_SfxEntries; //mdx	// number of sfx entries
	int				num_SubObjects; //mdx	// number of subobjects

	int				ofsSkins;			/*mdxSkin_t*/	//name[64];
	int				ofsTris;			/*mdxTri_t*/	//vertIDX[3], nornalIdx[3].
	int				ofsFrames;			//vertex pos, vertex normalIDX
	int				ofsGLCmds;			//triCount(-/+ is type), objectNum, (tri1)s,t, vertIdx... (tri2)s,t,vertIdx...
	int				offsetVertexInfo;	//objectID //mdx
	int				offsetSfxDefines;	//mdx 
	int				offsetSfxEntries;	//mdx 
	int				offsetBBoxFrames;	//mdx 
	int				offsetDummyEnd;

	int				ofsEnd;
} mdxHdr_t;


typedef struct mdxSkin_s
{
	char			name[64];
} mdxSkin_t;

 //vertPos[3], normalIndex
typedef struct mdxVert_s 
{
	BYTE			pos[3]; //vertex position
	BYTE			nrmIdx; //vertex normal index. shared normals
} mdxVert_t;

typedef struct mdxVertInfo_s 
{
	int				objectNum; //allways 1 (first object)
} mdxVertInfo_t;

//vertIDX[3], normalIDX[3]
typedef struct mdxTri_s
{
	WORD			vIdx[3]; //3 vertex index to make a tri
	WORD			uvIdx[3]; //vertexnormal[3] nornalIdx
} mdxTri_t;

//scale[3], trans[3], name[16]
typedef struct mdxFrame_s
{
	float			scale[3];
	float			trans[3];
	char			name[16]; //frame name
} mdxFrame_t;


typedef struct mdxGLCmd_s
{
	float			st[2];
	int				vIdx;
} mdxGLCmd_t;


//GL OBJECT HEADDER
typedef struct mdxGLCmdHeader_s
{
	int TrisTypeNum;
	int SubObjectID;
} mdxGLCmdHeader_t;


typedef struct mdxBBox_s
{
	float min[3];
	float max[3];
} mdxBBox_t;






extern mathImpFn_t *g_mfn;
extern noePluginFn_t *g_nfn;
extern int g_fmtHandle;

extern float g_q2Normals[162][3];
extern BYTE g_q2Pal[256][4];

NPLUGIN_API bool NPAPI_Init(void);
NPLUGIN_API void NPAPI_Shutdown(void);
NPLUGIN_API int NPAPI_GetPluginVer(void);
