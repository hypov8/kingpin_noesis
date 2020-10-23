#include "../pluginshare.h"
#include <math.h>

//#define USE_GLCOMMANDS_VERSION //hypo use newer kp style glCmds
#define copyUVIdx(src, dst, vertCnt) (dst.st[0]=src.st[0],dst.st[1]=src.st[1],dst.idx=src.idx, vertCnt+=1)

#define MAX_MD2_FRAMES 512 //hypov8 was 1024??
#define MAX_MD2_VERT 2048
#define MAX_MD2_TRI 4096

enum
{
	MD2_PLAYER_HEAD,	//0
	MD2_PLAYER_BODY,	//1
	MD2_PLAYER_LEGS,	//2

	MD2_PLAYER_RL,		//3
	MD2_PLAYER_FL,		//4
	MD2_PLAYER_GL,		//5
	MD2_PLAYER_HMG,	//6
	MD2_PLAYER_PIPE,	//7
	MD2_PLAYER_PISTOL,	//8
	MD2_PLAYER_SG,		//9
	MD2_PLAYER_TG,		//10
	MD2_PLAYER_MAX,	//11

	MD2_PLAYER_NULL = 255
};


typedef struct md2Opts_s
{
	int					exportPlayerModel; // keepBoneSpaces;
} md2Opts_t;

typedef struct md2Hdr_s
{
	BYTE			id[4];
	int				ver;

	int				skinWidth;
	int				skinHeight;
	int				frameSize;
	int				numSkins;
	int				numVerts;
	int				numST;
	int				numTris;
	int				numGLCmds;
	int				numFrames;

	int				ofsSkins;
	int				ofsST;
	int				ofsTris;
	int				ofsFrames;
	int				ofsGLCmds;
	int				ofsEnd;
} md2Hdr_t;
typedef struct md2Skin_s
{
	char			name[64];
} md2Skin_t;
typedef struct md2ST_s
{
	short			st[2];
} md2ST_t;
typedef struct md2Tri_s
{
	WORD			vIdx[3];
	WORD			stIdx[3];
} md2Tri_t;
typedef struct md2Vert_s
{
	BYTE			pos[3];
	BYTE			nrmIdx;
} md2Vert_t;
typedef struct md2Frame_s
{
	float			scale[3];
	float			trans[3];
	char			name[16];
} md2Frame_t;
typedef struct md2GLCmd_s
{
	float			st[2]; //uv
	int				idx; //vertex index
} md2GLCmd_t;

//GL OBJECT HEADDER
typedef struct md2GLCmdHeader_s
{
	int TrisTypeNum;
} md2GLCmdHeader_t;


extern mathImpFn_t *g_mfn;
extern noePluginFn_t *g_nfn;
extern int g_fmtHandle;

extern float g_q2Normals[162][3];
extern BYTE g_q2Pal[256][4];

NPLUGIN_API bool NPAPI_Init(void);
NPLUGIN_API void NPAPI_Shutdown(void);
NPLUGIN_API int NPAPI_GetPluginVer(void);
