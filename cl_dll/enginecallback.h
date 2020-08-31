/*
enginecallback.h - actual engine callbacks
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef ENGINECALLBACK_H
#define ENGINECALLBACK_H

extern cl_enginefunc_t gEngfuncs;
extern render_api_t gRenderfuncs;

#define GET_CLIENT_TIME	(*gEngfuncs.GetClientTime)
#define GET_CLIENT_OLDTIME	(*gEngfuncs.GetClientOldTime)
#define CVAR_REGISTER	(*gEngfuncs.pfnRegisterVariable)
#define CVAR_GET_FLOAT	(*gEngfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*gEngfuncs.pfnGetCvarString)
#define CVAR_SET_FLOAT	(*gEngfuncs.Cvar_SetValue)
//#define CVAR_SET_STRING	(*gEngfuncs.pfnCVarSetString)		// not implemented
#define CVAR_GET_POINTER	(*gEngfuncs.pfnGetCvarPointer)
#define ADD_COMMAND		(*gEngfuncs.pfnAddCommand)
#define CMD_ARGC		(*gEngfuncs.Cmd_Argc)
#define CMD_ARGV		(*gEngfuncs.Cmd_Argv)
#define Msg		(*gEngfuncs.Con_Printf)

#define GET_LOCAL_PLAYER	(*gEngfuncs.GetLocalPlayer)
#define GET_VIEWMODEL	(*gEngfuncs.GetViewModel)
#define GET_ENTITY		(*gEngfuncs.GetEntityByIndex)

#define POINT_CONTENTS( p )	(*gEngfuncs.PM_PointContents)( p, NULL )
#define WATER_ENTITY	(*gEngfuncs.PM_WaterEntity)
#define RANDOM_LONG		(*gEngfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*gEngfuncs.pfnRandomFloat)

#define GetScreenInfo	(*gEngfuncs.pfnGetScreenInfo)
#define ServerCmd		(*gEngfuncs.pfnServerCmd)
#define ClientCmd		(*gEngfuncs.pfnClientCmd)
#define SetCrosshair	(*gEngfuncs.pfnSetCrosshair)
#define AngleVectors	(*gEngfuncs.pfnAngleVectors)
#define GetPlayerInfo	(*gEngfuncs.pfnGetPlayerInfo)
	
#define SPR_Load		(*gEngfuncs.pfnSPR_Load)
#define SPR_Set		(*gEngfuncs.pfnSPR_Set)
#define SPR_Frames		(*gEngfuncs.pfnSPR_Frames)
#define SPR_Draw		(*gEngfuncs.pfnSPR_Draw)
#define SPR_DrawHoles	(*gEngfuncs.pfnSPR_DrawHoles)
#define SPR_DrawAdditive	(*gEngfuncs.pfnSPR_DrawAdditive)
#define SPR_EnableScissor	(*gEngfuncs.pfnSPR_EnableScissor)
#define SPR_DisableScissor	(*gEngfuncs.pfnSPR_DisableScissor)
#define FillRGBA		(*gEngfuncs.pfnFillRGBA)
#define SPR_Height		(*gEngfuncs.pfnSPR_Height)
#define SPR_Width		(*gEngfuncs.pfnSPR_Width)
#define SPR_GetList		(*gEngfuncs.pfnSPR_GetList)
#define SPR_LoadEx		(*gRenderfuncs.SPR_LoadExt)

#define ConsolePrint	(*gEngfuncs.pfnConsolePrint)
#define CenterPrint		(*gEngfuncs.pfnCenterPrint)
#define TextMessageGet	(*gEngfuncs.pfnTextMessageGet)
#define TextMessageDrawChar	(*gEngfuncs.pfnDrawCharacter)
#define DrawConsoleString	(*gEngfuncs.pfnDrawConsoleString)
#define GetConsoleStringSize	(*gEngfuncs.pfnDrawConsoleStringLen)
#define DrawSetTextColor	(*gEngfuncs.pfnDrawSetTextColor)

#define LOAD_FILE( x, y )	(*gEngfuncs.COM_LoadFile)( x, 5, y )
#define FREE_FILE		(*gEngfuncs.COM_FreeFile)
#define SAVE_FILE		(*gRenderfuncs.pfnSaveFile)

#define Sys_DoubleTime	(*gRenderfuncs.pfnTime)

// sound functions (we can't use macroses - this names is collide with standard windows methods)
inline void PlaySound( char *szSound, float vol ) { gEngfuncs.pfnPlaySoundByName( szSound, vol ); }
inline void PlaySound( int iSound, float vol ) { gEngfuncs.pfnPlaySoundByIndex( iSound, vol ); }

// render api callbacks
#define RENDER_GET_PARM		(*gRenderfuncs.RenderGetParm)
#define SET_CURRENT_ENTITY		(*gRenderfuncs.R_SetCurrentEntity)
#define SET_CURRENT_MODEL		(*gRenderfuncs.R_SetCurrentModel)
#define ENGINE_SET_PVS		(*gRenderfuncs.R_FatPVS)
#define HOST_ERROR			(*gRenderfuncs.Host_Error)
#define GET_LIGHTSTYLE		(*gRenderfuncs.GetLightStyle)
#define GET_DYNAMIC_LIGHT		(*gRenderfuncs.GetDynamicLight)
#define GET_ENTITY_LIGHT		(*gRenderfuncs.GetEntityLight)
#define TEXTURE_TO_TEXGAMMA		(*gRenderfuncs.LightToTexGamma)
#define GET_FRAMETIME		(*gRenderfuncs.GetFrameTime)
#define DRAW_SINGLE_DECAL		(*gRenderfuncs.DrawSingleDecal)
#define DECAL_SETUP_VERTS		(*gRenderfuncs.R_DecalSetupVerts)
#define GET_DETAIL_SCALE		(*gRenderfuncs.GetDetailScaleForTexture)
#define GET_EXTRA_PARAMS		(*gRenderfuncs.GetExtraParmsForTexture)
#define CREATE_TEXTURE		(*gRenderfuncs.GL_CreateTexture)
#define FIND_TEXTURE		(*gRenderfuncs.GL_FindTexture)
#define FREE_TEXTURE		(*gRenderfuncs.GL_FreeTexture)
#define CREATE_TEXTURE_ARRAY		(*gRenderfuncs.GL_CreateTextureArray)
#define STORE_EFRAGS		(*gRenderfuncs.R_StoreEfrags)
#define INIT_BEAMCHAINS		(*gRenderfuncs.GetBeamChains)
#define DRAW_PARTICLES		(*gRenderfuncs.GL_DrawParticles)
#define SET_ENGINE_WORLDVIEW_MATRIX	(*gRenderfuncs.GL_SetWorldviewProjectionMatrix)
#define GET_FOG_PARAMS		(*gRenderfuncs.GetExtraParmsForTexture)
#define GET_TEXTURE_NAME		(*gRenderfuncs.GL_TextureName)
#define GET_TEXTURE_DATA		(*gRenderfuncs.GL_TextureData)
#define COMPARE_FILE_TIME		(*gRenderfuncs.COM_CompareFileTime)
#define REMOVE_BSP_DECALS		(*gRenderfuncs.R_EntityRemoveDecals)
#define STUDIO_GET_TEXTURE		(*gRenderfuncs.StudioGetTexture)
#define GET_OVERVIEW_PARMS		(*gRenderfuncs.GetOverviewParms)
#define FS_SEARCH			(*gRenderfuncs.pfnGetFilesList)
#define ENV_SHOT			(*gRenderfuncs.EnvShot)

#define LOAD_TEXTURE		(*gRenderfuncs.GL_LoadTexture)
#define LOAD_TEXTURE_ARRAY		(*gRenderfuncs.GL_LoadTextureArray)

// AVIKit interface
#define OPEN_CINEMATIC		(*gRenderfuncs.AVI_LoadVideo)
#define FREE_CINEMATIC		(*gRenderfuncs.AVI_FreeVideo)
#define CIN_IS_ACTIVE		(*gRenderfuncs.AVI_IsActive)
#define CIN_GET_VIDEO_INFO		(*gRenderfuncs.AVI_GetVideoInfo)
#define CIN_GET_FRAME_NUMBER		(*gRenderfuncs.AVI_GetVideoFrameNumber)
#define CIN_GET_FRAMEDATA		(*gRenderfuncs.AVI_GetVideoFrame)
#define CIN_UPLOAD_FRAME		(*gRenderfuncs.AVI_UploadRawFrame)
#define CIN_UPDATE_SOUND		(*gRenderfuncs.AVI_StreamSound)

// glcommands
#define GL_BindTexture		(*gRenderfuncs.GL_Bind)
#define GL_SelectTexture		(*gRenderfuncs.GL_SelectTexture)
#define GL_TexGen			(*gRenderfuncs.GL_TexGen)
#define GL_LoadTextureMatrix		(*gRenderfuncs.GL_LoadTextureMatrix)
#define GL_LoadIdentityTexMatrix	(*gRenderfuncs.GL_TexMatrixIdentity)
#define GL_CleanUpTextureUnits	(*gRenderfuncs.GL_CleanUpTextureUnits)
#define GL_TexCoordArrayMode		(*gRenderfuncs.GL_TexCoordArrayMode)
#define GL_TextureTarget		(*gRenderfuncs.GL_TextureTarget)
#define GL_UpdateTexSize		(*gRenderfuncs.GL_UpdateTexSize)

#define RANDOM_SEED			(*gRenderfuncs.SetRandomSeed)
#define MUSIC_FADE_VOLUME		(*gRenderfuncs.S_FadeMusicVolume)

#define GL_GetProcAddress		(*gRenderfuncs.GL_GetProcAddress)

#define MODEL_HANDLE		(*gRenderfuncs.pfnGetModel)

// built-in memory manager
#define Mem_Alloc( x )		(*gRenderfuncs.pfnMemAlloc)( x, __FILE__, __LINE__ )
#define Mem_Free( x )		(*gRenderfuncs.pfnMemFree)( x, __FILE__, __LINE__ )
#define _Mem_Alloc			(*gRenderfuncs.pfnMemAlloc)
#define _Mem_Free			(*gRenderfuncs.pfnMemFree)

#define ASSERT( exp )		if(!( exp )) HOST_ERROR( "assert failed at %s:%i\n", __FILE__, __LINE__ )

#define IMAGE_EXISTS( path )		( FILE_EXISTS( va( "%s.tga", path )) || FILE_EXISTS( va( "%s.dds", path )))

extern void ALERT( ALERT_TYPE level, char *szFmt, ... );
	
inline bool FILE_EXISTS( const char *filename )
{
	int iCompare;

	// verify file exists
	// g-cont. idea! use COMPARE_FILE_TIME instead of COM_LoadFile
	if( COMPARE_FILE_TIME( filename, filename, &iCompare ))
		return true;
	return false;
}

#define FILE_CRC32			(*gRenderfuncs.pfnFileBufferCRC32)
#define GET_MAX_CLIENTS		(*gEngfuncs.GetMaxClients)

#endif//ENGINECALLBACK_H