//=========================================
// rain.cpp
// written by BUzer
//=========================================

#include <memory.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "parsemsg.h"
#include "entity_types.h"
#include "cdll_int.h"
#include "pm_defs.h"
#include "event_api.h"
#include "custom_alloc.h"
#include "triangleapi.h"
#include "gl_local.h"

#define DRIPSPEED			900	// скорость падения капель (пикс в сек)
#define SNOWSPEED			200	// скорость падения снежинок
#define SNOWFADEDIST		80

#define MAX_RAIN_VERTICES		65536	// snowflakes and waterrings draw as quads
#define MAX_RAIN_INDICES		MAX_RAIN_VERTICES * 4

#define MAXDRIPS			40000	// лимит капель (можно увеличить при необходимости)
#define MAXFX			20000	// лимит дополнительных частиц (круги по воде и т.п.)

#define MODE_RAIN			0
#define MODE_SNOW			1

#define DRIP_SPRITE_HALFHEIGHT	46
#define DRIP_SPRITE_HALFWIDTH		8
#define SNOW_SPRITE_HALFSIZE		3
#define MAX_RING_HALFSIZE		25	// "радиус" круга на воде, до которого он разрастается за секунду	

typedef struct
{
	int		dripsPerSecond;
	float		distFromPlayer;
	float		windX, windY;
	float		randX, randY;
	int		weatherMode;	// 0 - snow, 1 - rain
	float		globalHeight;
} rain_properties;

typedef struct cl_drip
{
	float		birthTime;
	float		minHeight;	// капля будет уничтожена на этой высоте.
	Vector		origin;
	float		alpha;
	float		xDelta;		// side speed
	float		yDelta;
	int		landInWater;
} cl_drip_t;

typedef struct cl_rainfx
{
	float		birthTime;
	float		life;
	Vector		origin;
	float		alpha;
} cl_rainfx_t;

void WaterLandingEffect(cl_drip *drip);


rain_properties     Rain;
MemBlock<cl_drip>	g_dripsArray( MAXDRIPS );
MemBlock<cl_rainfx>	g_fxArray( MAXFX );

cvar_t		*cl_draw_rain = NULL;
cvar_t		*cl_debug_rain = NULL;

double		rain_curtime;	// current time
double		rain_oldtime;	// last time we have updated drips
double		rain_timedelta;	// difference between old time and current time
double		rain_nextspawntime;	// when the next drip should be spawned
int		dripcounter = 0;
int		fxcounter = 0;

static Vector	rain_mins, rain_maxs; // for vis culling
static Vector	m_vertexarray[MAX_RAIN_VERTICES];
static byte	m_colorarray[MAX_RAIN_VERTICES][4];
static Vector2D	m_coordsarray[MAX_RAIN_VERTICES];
static word	m_indexarray[MAX_RAIN_INDICES];
static int	m_iNumVerts, m_iNumIndex;

/*
=================================
ProcessRain
Перемещает существующие объекты, удаляет их при надобности,
и, если дождь включен, создает новые.

Должна вызываться каждый кадр.
=================================
*/
void ProcessRain( void )
{
	rain_oldtime = rain_curtime; // save old time
	rain_curtime = GET_CLIENT_TIME();
	rain_timedelta = rain_curtime - rain_oldtime;

	// first frame
	if( rain_oldtime == 0 )
	{
		// fix first frame bug with nextspawntime
		rain_nextspawntime = rain_curtime;
		return;
	}

	if(( Rain.dripsPerSecond == 0 && g_dripsArray.IsClear( )) || rain_timedelta > 0.1f )
	{
		rain_timedelta = min( rain_timedelta, 0.1f );

		// keep nextspawntime correct
		rain_nextspawntime = rain_curtime;
		return;
	}

	if( rain_timedelta == 0 ) return; // not in pause

	double timeBetweenDrips = 1.0 / (double)Rain.dripsPerSecond;

	if( !g_dripsArray.StartPass( ))
	{
		Rain.dripsPerSecond = 0; // disable rain
		ALERT( at_error, "rain: failed to allocate memory block!\n" );
		return;
	}

	cl_drip *curDrip = g_dripsArray.GetCurrent();	

	// хранение отладочной информации
	float debug_lifetime = 0;
	int debug_howmany = 0;
	int debug_attempted = 0;
	int debug_dropped = 0;

	ClearBounds( rain_mins, rain_maxs );

	while( curDrip != NULL ) // go through list
	{
		if( Rain.weatherMode == MODE_RAIN )
			curDrip->origin.z -= rain_timedelta * DRIPSPEED;
		else if( Rain.weatherMode == MODE_SNOW )
			curDrip->origin.z -= rain_timedelta * SNOWSPEED;
		else return;

		curDrip->origin.x += rain_timedelta * curDrip->xDelta;
		curDrip->origin.y += rain_timedelta * curDrip->yDelta;
#if 1
		// unrolled version of AddPointToBounds (perf)
		if( curDrip->origin[0] < rain_mins[0] ) rain_mins[0] = curDrip->origin[0];
		if( curDrip->origin[0] > rain_maxs[0] ) rain_maxs[0] = curDrip->origin[0];
		if( curDrip->origin[1] < rain_mins[1] ) rain_mins[1] = curDrip->origin[1];
		if( curDrip->origin[1] > rain_maxs[1] ) rain_maxs[1] = curDrip->origin[1];
		if( curDrip->origin[2] < rain_mins[2] ) rain_mins[2] = curDrip->origin[2];
		if( curDrip->origin[2] > rain_maxs[2] ) rain_maxs[2] = curDrip->origin[2];
#else
		AddPointToBounds( curDrip->origin, rain_mins, rain_maxs );
#endif		
		// remove drip if its origin lower than minHeight
		if( curDrip->origin.z < curDrip->minHeight ) 
		{
			if( curDrip->landInWater )
				WaterLandingEffect( curDrip ); // create water rings

			if( cl_debug_rain->value )
			{
				debug_lifetime += (rain_curtime - curDrip->birthTime);
				debug_howmany++;
			}

			g_dripsArray.DeleteCurrent();
			dripcounter--;
		}
		else
			g_dripsArray.MoveNext();

		curDrip = g_dripsArray.GetCurrent();
	}

	int maxDelta; // maximum height randomize distance
	float falltime;

	if( Rain.weatherMode == MODE_RAIN )
	{
		maxDelta = DRIPSPEED * rain_timedelta; // for rain
		falltime = (Rain.globalHeight + 4096) / DRIPSPEED;
	}
	else
	{
		maxDelta = SNOWSPEED * rain_timedelta; // for snow
		falltime = (Rain.globalHeight + 4096) / SNOWSPEED;
	}

	while( rain_nextspawntime < rain_curtime )
	{
		rain_nextspawntime += timeBetweenDrips;		

		if( cl_debug_rain->value )
			debug_attempted++;

		// check for overflow
		if( dripcounter < MAXDRIPS )
		{
			float deathHeight;
			vec3_t vecStart, vecEnd;

			vecStart[0] = RANDOM_FLOAT( GetVieworg().x - Rain.distFromPlayer, GetVieworg().x + Rain.distFromPlayer );
			vecStart[1] = RANDOM_FLOAT( GetVieworg().y - Rain.distFromPlayer, GetVieworg().y + Rain.distFromPlayer );
			vecStart[2] = Rain.globalHeight;

			float xDelta = Rain.windX + RANDOM_FLOAT( Rain.randX * -1, Rain.randX );
			float yDelta = Rain.windY + RANDOM_FLOAT( Rain.randY * -1, Rain.randY );

			// find a point at bottom of map
			vecEnd[0] = falltime * xDelta;
			vecEnd[1] = falltime * yDelta;
			vecEnd[2] = -4096;

			pmtrace_t pmtrace;
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, vecStart + vecEnd, PM_STUDIO_IGNORE|PM_CUSTOM_IGNORE, -1, &pmtrace );

			if( pmtrace.startsolid || pmtrace.allsolid )
			{
				if( cl_debug_rain->value )
					debug_dropped++;
				continue; // drip cannot be placed
			}

			// falling to water?
			int contents = gEngfuncs.PM_PointContents( pmtrace.endpos, NULL );

			if( contents == CONTENTS_WATER )
			{
				int waterEntity = WATER_ENTITY( pmtrace.endpos );

				if( waterEntity > 0 )
				{
					cl_entity_t *pwater = gEngfuncs.GetEntityByIndex( waterEntity );

					if( pwater && ( pwater->model != NULL ))
					{
						deathHeight = pwater->curstate.maxs.z - 1.0f;

						if( !Mod_BoxVisible( pwater->curstate.mins, pwater->curstate.maxs, Mod_GetCurrentVis( )))
							contents = CONTENTS_EMPTY; // not error, just water out of PVS
					}
					else
					{
						ALERT( at_error, "rain: can't get water entity\n" );
						continue;
					}
				}
				else
				{
					ALERT( at_error, "rain: water is not func_water entity\n" );
					continue;
				}
			}
			else
			{
				deathHeight = pmtrace.endpos.z;
			}

			// just in case..
			if( deathHeight > vecStart.z )
			{
				ALERT( at_error, "rain: can't create drip in water\n");
				continue;
			}


			cl_drip *newClDrip = g_dripsArray.Allocate();

			if( !newClDrip )
			{
				Rain.dripsPerSecond = 0; // disable rain
				ALERT( at_error, "rain: failed to allocate object!\n" );
				return;
			}

			vecStart.z -= RANDOM_FLOAT( 0, maxDelta ); // randomize a bit

			newClDrip->alpha = RANDOM_FLOAT( 0.12f, 0.2f );
			newClDrip->origin = vecStart;

			newClDrip->xDelta = xDelta;
			newClDrip->yDelta = yDelta;

			newClDrip->birthTime = rain_curtime; // store time when it was spawned
			newClDrip->minHeight = deathHeight;

			if( contents == CONTENTS_WATER )
				newClDrip->landInWater = 1;
			else
				newClDrip->landInWater = 0;

			// add to first place in chain
			dripcounter++;
		}
		else
		{
			ALERT( at_error, "rain: Drip limit overflow!\n" );
			return;
		}
	}

	if( cl_debug_rain->value )
	{
		// print debug info
		gEngfuncs.Con_NPrintf( 1, "rain info: Drips exist: %i\n", dripcounter );
		gEngfuncs.Con_NPrintf( 2, "rain info: FX's exist: %i\n", fxcounter );
		gEngfuncs.Con_NPrintf( 3, "rain info: Attempted/Dropped: %i, %i\n", debug_attempted, debug_dropped );

		if( debug_howmany )
		{
			float ave = debug_lifetime / (float)debug_howmany;
			gEngfuncs.Con_NPrintf( 4, "rain info: Average drip life time: %f\n", ave );
		}
		else
			gEngfuncs.Con_NPrintf( 4, "rain info: Average drip life time: --\n" );
	}
}

/*
=================================
WaterLandingEffect
создает круг на водной поверхности
=================================
*/
void WaterLandingEffect( cl_drip *drip )
{
	if( fxcounter >= MAXFX )
	{
		ALERT( at_error, "rain: FX limit overflow!\n" );
		return;
	}
	
	cl_rainfx *newFX = g_fxArray.Allocate();
	if( !newFX )
	{
		ALERT( at_error, "rain: failed to allocate FX object!\n" );
		return;
	}

	newFX->alpha = RANDOM_FLOAT( 0.6f, 0.9f );
	newFX->origin = drip->origin; 
	newFX->origin.z = drip->minHeight - 1; // correct position

	newFX->birthTime = GET_CLIENT_TIME();
	newFX->life = RANDOM_FLOAT( 0.7f, 1.0f );

	// add to first place in chain
	fxcounter++;
}

/*
=================================
ProcessFXObjects
удаляет FX объекты, у которых вышел срок жизни

Каждый кадр вызывается перед ProcessRain
=================================
*/
void ProcessFXObjects( void )
{
	if( !g_fxArray.StartPass( ))
	{
		Rain.dripsPerSecond = 0; // disable rain
		ALERT( at_error, "rain: failed to allocate FX object!\n" );
		return;
	}

	cl_rainfx* curFX = g_fxArray.GetCurrent();

	while( curFX != NULL ) // go through FX objects list
	{
		// delete current?
		if(( curFX->birthTime + curFX->life ) < rain_curtime )
		{
			g_fxArray.DeleteCurrent();
			fxcounter--;
		}
		else
			g_fxArray.MoveNext();

		curFX = g_fxArray.GetCurrent();
	}
}

/*
=================================
ResetRain
очищает память, удаляя все объекты.
=================================
*/
void ResetRain( void )
{
	// delete all drips
	g_dripsArray.Clear();
	g_fxArray.Clear();
	
	dripcounter = 0;
	fxcounter = 0;

	InitRain();
}

/*
=================================
DrawRain

Рисование капель и снежинок.
=================================
*/
void DrawRain( void )
{
	if( g_dripsArray.IsClear( ))
		return; // no drips to draw

	if( !Mod_BoxVisible( rain_mins, rain_maxs, Mod_GetCurrentVis( )))
		return; // rain volume is invisible

	HSPRITE hsprTexture;

	if( Rain.weatherMode == MODE_RAIN )
		hsprTexture = LoadSprite("sprites/hi_rain.spr" ); // load rain sprite
	else if( Rain.weatherMode == MODE_SNOW )
		hsprTexture = LoadSprite( "sprites/snowflake.spr" ); // load snow sprite
	else hsprTexture = 0;

	if( !hsprTexture ) return;

	GL_SelectTexture( GL_TEXTURE0 ); // keep texcoords at 0-th unit

	// usual triapi stuff
	const model_s *pTexture = gEngfuncs.GetSpritePointer( hsprTexture );
	if( !gEngfuncs.pTriAPI->SpriteTexture(( struct model_s *)pTexture, 0 ))
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );

	float visibleHeight = Rain.globalHeight - SNOWFADEDIST;

	m_iNumVerts = m_iNumIndex = 0;
	
	// go through drips list
	g_dripsArray.StartPass();
	cl_drip *Drip = g_dripsArray.GetCurrent();

	if( Rain.weatherMode == MODE_RAIN )
	{
		while( Drip != NULL )
		{
			// cull invisible drips
			if( R_CullSphere( Drip->origin, SNOW_SPRITE_HALFSIZE + 1 ))
			{
				g_dripsArray.MoveNext();
				Drip = g_dripsArray.GetCurrent();
				continue;
			}

			if(( m_iNumVerts + 3 ) >= MAX_RAIN_VERTICES )
			{
				// this should never happens because we used vertex array only at 75%
				ALERT( at_error, "Too many drips specified\n" );
				break;
			}

			Vector2D toPlayer; 
			toPlayer.x = GetVieworg().x - Drip->origin[0];
			toPlayer.y = GetVieworg().y - Drip->origin[1];
			toPlayer = toPlayer.Normalize();
	
			toPlayer.x *= DRIP_SPRITE_HALFWIDTH;
			toPlayer.y *= DRIP_SPRITE_HALFWIDTH;

			float shiftX = (Drip->xDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;
			float shiftY = (Drip->yDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;

			byte alpha = Drip->alpha * 255;
			
			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Vector( Drip->origin.x - toPlayer.y - shiftX, Drip->origin.y + toPlayer.x - shiftY, Drip->origin.z + DRIP_SPRITE_HALFHEIGHT );
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			m_coordsarray[m_iNumVerts].x = 0.5f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Vector( Drip->origin.x + shiftX, Drip->origin.y + shiftY, Drip->origin.z - DRIP_SPRITE_HALFHEIGHT );
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set right top corner
			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Vector( Drip->origin.x + toPlayer.y - shiftX, Drip->origin.y - toPlayer.x - shiftY, Drip->origin.z + DRIP_SPRITE_HALFHEIGHT );
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}

		pglEnableClientState( GL_VERTEX_ARRAY );
		pglVertexPointer( 3, GL_FLOAT, 0, m_vertexarray );

		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		pglTexCoordPointer( 2, GL_FLOAT, 0, m_coordsarray );

		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_colorarray );

		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_TRIANGLES, 0, m_iNumVerts - 1, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
		else pglDrawElements( GL_TRIANGLES, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );

		r_stats.c_total_tris += (m_iNumIndex / 3);
		pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		pglDisableClientState( GL_VERTEX_ARRAY );
		pglDisableClientState( GL_COLOR_ARRAY );
	}
	else if( Rain.weatherMode == MODE_SNOW )
	{
		while( Drip != NULL )
		{
			// cull invisible flakes
			if( R_CullSphere( Drip->origin, SNOW_SPRITE_HALFSIZE + 1 ))
			{
				g_dripsArray.MoveNext();
				Drip = g_dripsArray.GetCurrent();
				continue;
			}

			if(( m_iNumVerts + 4 ) >= MAX_RAIN_VERTICES )
			{
				ALERT( at_error, "Too many snowflakes specified\n" );
				break;
			}

			// apply start fading effect
			byte alpha;

			if( Drip->origin.z <= visibleHeight )
				alpha = Drip->alpha * 255;
			else alpha = ((( Rain.globalHeight - Drip->origin.z ) / (float)SNOWFADEDIST ) * Drip->alpha) * 255;

			// set left bottom corner
			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + GetVLeft() * -SNOW_SPRITE_HALFSIZE + GetVUp() * -SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set left top corner
			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + GetVLeft() * -SNOW_SPRITE_HALFSIZE + GetVUp() * SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set right top corner
			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + GetVLeft() * SNOW_SPRITE_HALFSIZE + GetVUp() * SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set right bottom corner
			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + GetVLeft() * SNOW_SPRITE_HALFSIZE + GetVUp() * -SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}

		pglEnableClientState( GL_VERTEX_ARRAY );
		pglVertexPointer( 3, GL_FLOAT, 0, m_vertexarray );

		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		pglTexCoordPointer( 2, GL_FLOAT, 0, m_coordsarray );

		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_colorarray );

		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_QUADS, 0, m_iNumVerts - 1, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
		else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );

		r_stats.c_total_tris += (m_iNumIndex / 4); // FIXME: this is correct?
		pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		pglDisableClientState( GL_VERTEX_ARRAY );
		pglDisableClientState( GL_COLOR_ARRAY );
	}
}

/* rain
=================================
DrawFXObjects

Рисование водяных кругов
=================================
*/
void DrawFXObjects( void )
{
	if( g_fxArray.IsClear( ))
		return; // no objects to draw

	HSPRITE hsprTexture;

	hsprTexture = LoadSprite( "sprites/waterring.spr" ); // load water ring sprite
	if( !hsprTexture ) return;

	const model_s *pTexture = gEngfuncs.GetSpritePointer( hsprTexture );
	if( !gEngfuncs.pTriAPI->SpriteTexture(( struct model_s *)pTexture, 0 ))
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE ); // because we also want to view water rings underwater

	// go through objects list
	g_fxArray.StartPass();
	cl_rainfx *curFX = g_fxArray.GetCurrent();

	m_iNumVerts = m_iNumIndex = 0;

	while( curFX != NULL )
	{
		if(( m_iNumVerts + 4 ) >= MAX_RAIN_VERTICES )
		{
			ALERT( at_error, "Too many water rings\n" );
			break;
		}

		// cull invisible rings
		if( R_CullSphere( curFX->origin, MAX_RING_HALFSIZE + 1 ))
		{
			g_fxArray.MoveNext();
			curFX = g_fxArray.GetCurrent();
			continue;
		}

		// fadeout
		byte alpha = (((curFX->birthTime + curFX->life - rain_curtime) / curFX->life) * curFX->alpha) * 255;
		float size = (rain_curtime - curFX->birthTime) * MAX_RING_HALFSIZE;

		m_coordsarray[m_iNumVerts].x = 0.0f;
		m_coordsarray[m_iNumVerts].y = 0.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x - size, curFX->origin.y - size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		m_coordsarray[m_iNumVerts].x = 0.0f;
		m_coordsarray[m_iNumVerts].y = 1.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x - size, curFX->origin.y + size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		m_coordsarray[m_iNumVerts].x = 1.0f;
		m_coordsarray[m_iNumVerts].y = 1.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x + size, curFX->origin.y + size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		m_coordsarray[m_iNumVerts].x = 1.0f;
		m_coordsarray[m_iNumVerts].y = 0.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x + size, curFX->origin.y - size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		g_fxArray.MoveNext();
		curFX = g_fxArray.GetCurrent();
	}

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, m_vertexarray );

	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, 0, m_coordsarray );

	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_colorarray );

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_QUADS, 0, m_iNumVerts - 1, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
	else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );

	r_stats.c_total_tris += (m_iNumIndex / 4); // FIXME: this is correct?
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );

	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
}

/*
=================================
InitRain
Инициализирует все переменные нулевыми значениями.
=================================
*/
void InitRain( void )
{
	cl_draw_rain = CVAR_REGISTER( "cl_draw_rain", "1", FCVAR_ARCHIVE );
	cl_debug_rain = CVAR_REGISTER( "cl_debug_rain", "0", 0 ); 
	memset( &Rain, 0, sizeof( Rain ));

	rain_oldtime = 0;
	rain_curtime = 0;
	rain_nextspawntime = 0;
}

/*
=================================
ParseRain
=================================
*/
void ParseRain( void )
{
	Rain.dripsPerSecond =	READ_SHORT();
	Rain.distFromPlayer =	READ_COORD();
	Rain.windX =		READ_COORD();
	Rain.windY =		READ_COORD();
	Rain.randX =		READ_COORD();
	Rain.randY =		READ_COORD();
	Rain.weatherMode =		READ_SHORT();
	Rain.globalHeight =		READ_COORD();
}

/*
=================================
R_DrawWeather
=================================
*/
void R_DrawWeather( void )
{
	if( !CVAR_TO_BOOL( cl_draw_rain ))
		return;

	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	GL_CleanupDrawState();
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );

	ProcessRain();
	ProcessFXObjects();

	DrawRain();
	DrawFXObjects();
}