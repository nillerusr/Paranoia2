/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

Class Hierachy

CBaseEntity
	CPointEntity
	CBasePlayerAmmo
	CBaseDelay
		CBaseAnimating
			CBasePlayerItem
		CBaseToggle
				CBaseButton
				CBaseDoor
				CBaseTrigger
				CBasePlatTrain
			CBaseMonster
					CCycler
					CBasePlayer
					CCineMonster
*/

#define		MAX_PATH_SIZE	10 // max number of nodes available for a path.

// These are caps bits to indicate what an object's capabilities (currently used for save/restore and level transitions)
#define		FCAP_CUSTOMSAVE				0x00000001
#define		FCAP_ACROSS_TRANSITION		0x00000002		// should transfer between transitions
#define		FCAP_MUST_SPAWN				0x00000004		// Spawn after restore
#define		FCAP_DONT_SAVE				0x80000000		// Don't save this
#define		FCAP_IMPULSE_USE			0x00000008		// can be used by the player
#define		FCAP_CONTINUOUS_USE			0x00000010		// can be used by the player
#define		FCAP_ONOFF_USE				0x00000020		// can be used by the player
#define		FCAP_DIRECTIONAL_USE		0x00000040		// Player sends +/- 1 when using (currently only tracktrains)
#define		FCAP_MASTER					0x00000080		// Can be used to "master" other entities (like multisource)
														// LRC: no longer used
#define		FCAP_ONLYDIRECT_USE			0x00000100		//LRC - can't use this entity through a wall.
#define		FCAP_DISTANCE_USE			0x00000200		//buz - can be used without distance restrictions
#define		FCAP_USE_ONLY			0x00000800		//g-cont. for items & weapons

// UNDONE: This will ignore transition volumes (trigger_transition), but not the PVS!!!
#define		FCAP_FORCE_TRANSITION		0x00000080		// ALWAYS goes across transitions

// Wargon: Энтитя не будет показывать иконку юза, даже если она юзабельна.
#define		FCAP_HIDE_USE				0x00000400

#include "saverestore.h"
#include "schedule.h"

#ifndef MONSTEREVENT_H
#include "monsterevent.h"
#endif

#include "meshdesc.h"

// C functions for external declarations that call the appropriate C++ methods

#ifdef _WIN32
#define EXPORT	_declspec( dllexport )
#else
#define EXPORT	/* */
#endif

extern "C" EXPORT int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
extern "C" EXPORT int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
extern "C" EXPORT int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
extern "C" EXPORT int Server_GetPhysicsInterface( int iVersion, server_physics_api_t *pfuncsFromEngine, physics_interface_t *pFunctionTable );
extern "C" EXPORT int Server_GetBlendingInterface( int, struct sv_blending_interface_s**, struct server_studio_api_s*, float (*t)[3][4], float (*b)[128][3][4] );

extern int DispatchSpawn( edict_t *pent );
extern void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd );
extern void DispatchTouch( edict_t *pentTouched, edict_t *pentOther );
extern void DispatchUse( edict_t *pentUsed, edict_t *pentOther );
extern void DispatchThink( edict_t *pent );
extern void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther );
extern void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData );
extern int  DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
extern void	DispatchObjectCollsionBox( edict_t *pent );
extern void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveGlobalState( SAVERESTOREDATA *pSaveData );
extern void RestoreGlobalState( SAVERESTOREDATA *pSaveData );
extern void ResetGlobalState( void );

//extern CBaseEntity *g_pDesiredList; //LRC- handles DesiredVel, for movewith

//LRC- added USE_SAME, USE_NOT, and USE_KILL
typedef enum
{
	USE_OFF = 0,
	USE_ON = 1,
	USE_SET = 2,
	USE_TOGGLE = 3,
	USE_KILL = 4,
// special signals, never actually get sent:
	USE_SAME = 5,
	USE_NOT = 6,
} USE_TYPE;

extern char* GetStringForUseType( USE_TYPE useType );

extern void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

typedef void (CBaseEntity::*BASEPTR)(void);
typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther );
typedef void (CBaseEntity::*USEPTR)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

// For CLASSIFY
#define	CLASS_NONE				0
#define CLASS_MACHINE			1
#define CLASS_PLAYER			2
#define	CLASS_HUMAN_PASSIVE		3
#define CLASS_HUMAN_MILITARY	4
#define CLASS_ALIEN_MILITARY	5
#define CLASS_ALIEN_PASSIVE		6
#define CLASS_ALIEN_MONSTER		7
#define CLASS_ALIEN_PREY		8
#define CLASS_ALIEN_PREDATOR	9
#define CLASS_INSECT			10
#define CLASS_PLAYER_ALLY		11
#define CLASS_PLAYER_BIOWEAPON	12 // hornets and snarks.launched by players
#define CLASS_ALIEN_BIOWEAPON	13 // hornets and snarks.launched by the alien menace
#define CLASS_FACTION_A			14 //LRC - very simple new classes, for use with Behaves As
#define CLASS_FACTION_B			15
#define CLASS_FACTION_C			16
#define CLASS_TERROR			17 // buz
#define	CLASS_BARNACLE			99 // special because no one pays attention to it, and it eats a wide cross-section of creatures.

class CBaseEntity;
class CBaseMonster;
class CBasePlayerItem;
class CSquadMonster;
class CStudioBoneSetup;

#define	SF_NORESPAWN	( 1 << 30 )// !!!set this bit on guns and stuff that should never respawn.

//
// EHANDLE. Safe way to point to CBaseEntities who may die between frames
//
class EHANDLE
{
private:
	edict_t *m_pent;
	int		m_serialnumber;
public:
	edict_t *Get( void );
	edict_t *Set( edict_t *pent );

	operator int ();

	operator CBaseEntity *();

	CBaseEntity * operator = (CBaseEntity *pEntity);
	CBaseEntity * operator ->();
};


//
// Base Entity.  All entity types derive from this
//
class CBaseEntity 
{
public:
	// Constructor.  Set engine to use C/C++ callback functions
	// pointers to engine data
	entvars_t *pev;		// Don't need to save/restore this pointer, the engine resets it

	// path corners
	CBaseEntity			*m_pGoalEnt;// path corner we are heading towards
	CBaseEntity			*m_pLink;// used for temporary link-list operations. 

	CBaseEntity			*m_pMoveWith; // LRC- the entity I move with.
	int					m_MoveWith;	//LRC- Name of that entity
	CBaseEntity			*m_pChildMoveWith;	//LRC- one of the entities that's moving with me.
	CBaseEntity			*m_pSiblingMoveWith; //LRC- another entity that's Moving With the same ent as me. (linked list.)
	Vector				m_vecMoveWithOffset; // LRC- Position I should be in relative to m_pMoveWith->pev->origin.
	Vector				m_vecRotWithOffset; // LRC- Angles I should be facing relative to m_pMoveWith->pev->angles.
	CBaseEntity			*m_pAssistLink; // LRC- link to the next entity which needs to be Assisted before physics are applied.
	Vector				m_vecPostAssistVel; // LRC
	Vector				m_vecPostAssistAVel; // LRC
	float				m_fNextThink; // LRC - for SetNextThink and SetPhysThink. Marks the time when a think will be performed - not necessarily the same as pev->nextthink!
	float				m_fPevNextThink; // LRC - always set equal to pev->nextthink, so that we can tell when the latter gets changed by the @#$^В¬! engine.
	int					m_iLFlags; // LRC- a new set of flags. (pev->spawnflags and pev->flags are full...)
	virtual void		DesiredAction( void ) {}; // LRC - for postponing stuff until PostThink time, not as a think.
	int					m_iStyle; // LRC - almost anything can have a lightstyle these days...

	Vector				m_vecSpawnOffset; // LRC- To fix things which (for example) MoveWith a door which Starts Open.
	BOOL				m_activated;	// LRC- moved here from func_train. Signifies that an entity has already been
										// activated. (and hence doesn't need reactivating.)
	float				m_flGaitYaw;	// player stuff

	float		m_flPoseParameter[MAXSTUDIOPOSEPARAM];

	//LRC - decent mechanisms for setting think times!
	// this should have been done a long time ago, but MoveWith finally forced me.
	virtual void		SetNextThink( float delay ) { SetNextThink(delay, FALSE); }
	virtual void		SetNextThink( float delay, BOOL correctSpeed );
	virtual void		AbsoluteNextThink( float time ) { AbsoluteNextThink(time, FALSE); }
	virtual void		AbsoluteNextThink( float time, BOOL correctSpeed );
	void				SetEternalThink( );
	// this is called by an entity which is starting to move, and will reach
	// its destination after the given wait.
	// Its think function should be called at that time, to make it stop moving.
//	void				SetPhysThink( float delay );
	// this is called by an entity which is movingWith another entity.
	// it signifies that the other entity is starting to move and will reach its
	// destination after the given wait.
	// This entity will need to think at that time (so that physics gets
	// processed correctly), but the Think function shouldn't actually get
	// called; the parent will intervene to make it stop.
//	void				SetMWPhysThink( float delay );
	// this is called by an entity which is starting to move, and wants its children
	// to follow it.
//	void				SetChildrenThink( float delay );

	//LRC use this instead of "SetThink( NULL )" or "pev->nextthink = -1".
	void	DontThink( void );
	//LRC similar, but called by the parent when a think needs to be aborted.
//	void	DontMWThink( void );

	virtual void ThinkCorrection( void );

	virtual BOOL	AllowDecal( entvars_t *pevAttacker ) { return TRUE; }

	//LRC - loci
	virtual Vector	CalcPosition( CBaseEntity *pLocus )	{ return pev->origin; }
	virtual Vector	CalcVelocity( CBaseEntity *pLocus )	{ return pev->velocity; }
	virtual float	CalcRatio( CBaseEntity *pLocus )	{ return 0; }

	//LRC - aliases
	virtual BOOL IsAlias( void ) { return FALSE; }

	// buz - gas reactions (should be used by soldiers to turn gas mask on and off)
	virtual void GasWarning( int warnlevel ) {};

	void		ResetPoseParameters( void );
	int		LookupPoseParameter( const char *szName );
	void		SetPoseParameter( int iParameter, float flValue );
	float		GetPoseParameter( int iParameter );
	void		SetPoseParameter( const char *szName, float flValue );
	float		GetPoseParameter( const char *szName );
	bool		HasPoseParameter( int iSequence, const char *szName );
	bool		HasPoseParameter( int iSequence, int iParameter );

	// initialization functions
	virtual void	Spawn( void ) { return; }
	virtual void	Precache( void ) { return; }
	virtual void	KeyValue( KeyValueData* pkvd)
	{
		//LRC - MoveWith for all!
		if (FStrEq(pkvd->szKeyName, "movewith"))
		{
			m_MoveWith = ALLOC_STRING(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "skill"))
		{
			m_iLFlags = atoi(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "style"))
		{
			m_iStyle = atoi(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq( pkvd->szKeyName, "reflection" ))
		{
			switch( atoi( pkvd->szValue ))
			{
			case 1: pev->effects |= EF_NOREFLECT; break;
			case 2: pev->effects |= EF_REFLECTONLY; break;
			}
		}
		else pkvd->fHandled = FALSE;
	}
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	//LRC - if I MoveWith something, then only cross transitions if the MoveWith entity does too.
	virtual int		ObjectCaps( void ) { return m_pMoveWith?m_pMoveWith->ObjectCaps()&FCAP_ACROSS_TRANSITION:FCAP_ACROSS_TRANSITION; }
	virtual void	Activate( void ); //LRC
	void			InitMoveWith( void ); //LRC - called by Activate() to set up moveWith values
	virtual void	PostSpawn( void ) {} //LRC - called by Activate() to handle entity-specific initialisation.
										 // (mostly setting positions, for MoveWith support)

	void	FireBulletsWater( Vector vecEnd, Vector vecSrc, float ScaleSplash1, float ScaleSplash2 );
	
	// Setup the object->object collision box (pev->mins / pev->maxs is the object->world collision box)
	virtual void	SetObjectCollisionBox( void );

// Classify - returns the type of group (e.g., "alien monster", or "human military" so that monsters
// on the same side won't attack each other, even if they have different classnames.
	virtual int Classify ( void ) { return CLASS_NONE; };
	virtual void DeathNotice ( entvars_t *pevChild ) {}// monster maker children use this to tell the monster maker that they have died.


// LRC- this supports a global concept of "entities with states", so that state_watchers and
// mastership (mastery? masterhood?) can work universally.
	virtual STATE GetState ( void ) { return STATE_OFF; };

// For team-specific doors in multiplayer, etc: a master's state depends on who wants to know.
	virtual STATE GetState ( CBaseEntity* pEnt ) { return GetState(); };

	static	TYPEDESCRIPTION m_SaveData[];

	virtual void	TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int		TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual int		TakeHealth( float flHealth, int bitsDamageType );
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual int		BloodColor( void ) { return DONT_BLEED; }
	virtual void	TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
//LRC- superceded by GetState ( pActivator ).
//	virtual BOOL    IsTriggered( CBaseEntity *pActivator ) {return TRUE;}
	virtual CBaseMonster *MyMonsterPointer( void ) { return NULL;}
	virtual CSquadMonster *MySquadMonsterPointer( void ) { return NULL;}
	virtual	int		GetToggleState( void ) { return TS_AT_TOP; }
	virtual void	AddPoints( int score, BOOL bAllowNegativeScore ) {}
	virtual void	AddPointsToTeam( int score, BOOL bAllowNegativeScore ) {}
	virtual BOOL	AddPlayerItem( CBasePlayerItem *pItem ) { return 0; }
	virtual BOOL	RemovePlayerItem( CBasePlayerItem *pItem ) { return 0; }
	virtual int 	GiveAmmo( int iAmount, const char *szName ) { return -1; };
	virtual float	GetDelay( void ) { return 0; }
	virtual int		IsMoving( void ) { return pev->velocity != g_vecZero; }
	virtual void	OverrideReset( void ) {}
	virtual const char*	DamageDecal( int bitsDamageType );
	// This is ONLY used by the node graph to test movement through a door
	virtual void	SetToggleState( int state ) {}
	virtual void    StartSneaking( void ) {}
	virtual void    StopSneaking( void ) {}
	virtual BOOL	OnControls( entvars_t *pev ) { return FALSE; }
	virtual BOOL    IsSneaking( void ) { return FALSE; }
	virtual BOOL	IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL	IsBSPModel( void ) { return pev->solid == SOLID_BSP || pev->movetype == MOVETYPE_PUSHSTEP; }
	virtual BOOL	HasTarget( string_t targetname ) { return FStrEq(STRING(targetname), STRING(pev->targetname) ); }
	virtual BOOL    IsInWorld( void );
	virtual	BOOL	IsPlayer( void ) { return FALSE; }
	virtual BOOL	IsNetClient( void ) { return FALSE; }
	virtual const char *TeamID( void ) { return ""; }

//	virtual void	SetActivator( CBaseEntity *pActivator ) {}
	virtual CBaseEntity *GetNextTarget( void );
	
	// fundamental callbacks
	void (CBaseEntity ::*m_pfnThink)(void);
	void (CBaseEntity ::*m_pfnTouch)( CBaseEntity *pOther );
	void (CBaseEntity ::*m_pfnUse)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void (CBaseEntity ::*m_pfnBlocked)( CBaseEntity *pOther );

	virtual void Think( void ) { if (m_pfnThink) (this->*m_pfnThink)(); };
	virtual void Touch( CBaseEntity *pOther ) { if (m_pfnTouch) (this->*m_pfnTouch)( pOther ); };
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) 
	{ 
		if (m_pfnUse) 
			(this->*m_pfnUse)( pActivator, pCaller, useType, value );
	}
	virtual void Blocked( CBaseEntity *pOther ) { if (m_pfnBlocked) (this->*m_pfnBlocked)( pOther ); };

	// allow engine to allocate instance data
    void *operator new( size_t stAllocateBlock, entvars_t *pev )
	{
		return (void *)ALLOC_PRIVATE(ENT(pev), stAllocateBlock);
	};

	// don't use this.
#if _MSC_VER >= 1200 // only build this code if MSVC++ 6.0 or higher
	void operator delete(void *pMem, entvars_t *pev)
	{
		pev->flags |= FL_KILLME;
	};
#endif

	void UpdateOnRemove( void );

	// common member functions
	void EXPORT SUB_Remove( void );
	void EXPORT SUB_DoNothing( void );
	void EXPORT SUB_StartFadeOut ( void );
	void EXPORT SUB_FadeOut ( void );
	void EXPORT SUB_CallUseToggle( void ) { this->Use( this, this, USE_TOGGLE, 0 ); } // a think function used at spawn time. Don't apply the moveWith fix to it.
	int	ShouldToggle( USE_TYPE useType, BOOL currentState );
	int	ShouldToggle( USE_TYPE useType ); //LRC this version uses GetState()
	void	FireBullets( ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, float flDamage = 0.0f, entvars_t *pevAttacker = NULL  );
	Vector	FireBulletsPlayer( ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, float flDamage, entvars_t *pevAttacker = NULL, int shared_rand = 0 );

	virtual CBaseEntity *Respawn( void ) { return NULL; }

	void SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value );
	// Do the bounding boxes of these two intersect?
	int	Intersects( CBaseEntity *pOther );
	void	MakeDormant( void );
	int	IsDormant( void );
	BOOL    IsLockedByMaster( void ) { return FALSE; }

	static CBaseEntity *Instance( edict_t *pent )
	{ 
		if ( !pent )
			pent = ENT(0);
		CBaseEntity *pEnt = (CBaseEntity *)GET_PRIVATE(pent); 
		return pEnt; 
	}

	static CBaseEntity *Instance( entvars_t *pev ) { return Instance( ENT( pev ) ); }
	static CBaseEntity *Instance( int eoffset) { return Instance( ENT( eoffset) ); }

	CBaseMonster *GetMonsterPointer( entvars_t *pevMonster ) 
	{ 
		CBaseEntity *pEntity = Instance( pevMonster );
		if ( pEntity )
			return pEntity->MyMonsterPointer();
		return NULL;
	}
	CBaseMonster *GetMonsterPointer( edict_t *pentMonster ) 
	{ 
		CBaseEntity *pEntity = Instance( pentMonster );
		if ( pEntity )
			return pEntity->MyMonsterPointer();
		return NULL;
	}

	struct model_s *GetModel( void )
	{
		if( g_fPhysicInitialized )
			return (struct model_s *)MODEL_HANDLE( pev->modelindex );
		return NULL;
	}

	// Ugly code to lookup all functions to make sure they are exported when set.
#ifdef _DEBUG
	void FunctionCheck( void *pFunction, char *name ) 
	{ 
		if (pFunction && !NAME_FOR_FUNCTION((unsigned long)(pFunction)) )
			ALERT( at_error, "No EXPORT: %s:%s (%08lx)\n", STRING(pev->classname), name, (unsigned long)pFunction );
	}

	BASEPTR	ThinkSet( BASEPTR func, char *name ) 
	{ 
		m_pfnThink = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnThink)))), name ); 
		return func;
	}
	ENTITYFUNCPTR TouchSet( ENTITYFUNCPTR func, char *name ) 
	{ 
		m_pfnTouch = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnTouch)))), name ); 
		return func;
	}
	USEPTR	UseSet( USEPTR func, char *name ) 
	{ 
		m_pfnUse = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnUse)))), name ); 
		return func;
	}
	ENTITYFUNCPTR	BlockedSet( ENTITYFUNCPTR func, char *name ) 
	{ 
		m_pfnBlocked = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnBlocked)))), name ); 
		return func;
	}

#endif


	// virtual functions used by a few classes
	
	// used by monsters that are created by the MonsterMaker
	virtual	void UpdateOwner( void ) { return; };


	//
	static CBaseEntity *Create( const char *szName, const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner = NULL );

	virtual BOOL FBecomeProne( void ) {return FALSE;};
	edict_t *edict() { return ENT( pev ); };
	EOFFSET eoffset( ) { return OFFSET( pev ); };
	int	  entindex( ) { return ENTINDEX( edict() ); };

	virtual Vector Center( ) { return (pev->absmin + pev->absmax) * 0.5; }; // center point of entity
	virtual Vector EyePosition( ) { return pev->origin + pev->view_ofs; };			// position of eyes
	virtual Vector EarPosition( ) { return pev->origin + pev->view_ofs; };			// position of ears
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ); };		// position to shoot at

	virtual int Illumination( ) { return GETENTITYILLUM( ENT( pev ) ); };

	virtual	BOOL FVisible ( CBaseEntity *pEntity );
	virtual	BOOL FVisible ( const Vector &vecOrigin );
	virtual   BOOL ShouldCollide( CBaseEntity *pOther );

	virtual void SendInitMessage( CBasePlayer *player ) {}; // buz - does nothing by default
};

//LRC- moved here from player.cpp. I'd put it in util.h with its friends, but it needs CBaseEntity to be declared.
inline BOOL FNullEnt( CBaseEntity *ent )	{ return ent == NULL || FNullEnt( ent->edict() ); }

// Ugly technique to override base member functions
// Normally it's illegal to cast a pointer to a member function of a derived class to a pointer to a 
// member function of a base class.  static_cast is a sleezy way around that problem.

#ifdef _DEBUG

#define SetThink( a ) ThinkSet( static_cast <void (CBaseEntity::*)(void)> (a), #a )
#define SetTouch( a ) TouchSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )
#define SetUse( a ) UseSet( static_cast <void (CBaseEntity::*)(	CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a), #a )
#define SetBlocked( a ) BlockedSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )

#else

#define SetThink( a ) m_pfnThink = static_cast <void (CBaseEntity::*)(void)> (a)
#define SetTouch( a ) m_pfnTouch = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)
#define SetUse( a ) m_pfnUse = static_cast <void (CBaseEntity::*)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a)
#define SetBlocked( a ) m_pfnBlocked = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)

#endif


class CPointEntity : public CBaseEntity
{
public:
	void	Spawn( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
private:
};


typedef struct locksounds			// sounds that doors and buttons make when locked/unlocked
{
	string_t	sLockedSound;		// sound a door makes when it's locked
	string_t	sLockedSentence;	// sentence group played when door is locked
	string_t	sUnlockedSound;		// sound a door makes when it's unlocked
	string_t	sUnlockedSentence;	// sentence group played when door is unlocked

	int		iLockedSentence;		// which sentence in sentence group to play next
	int		iUnlockedSentence;		// which sentence in sentence group to play next

	float	flwaitSound;			// time delay between playing consecutive 'locked/unlocked' sounds
	float	flwaitSentence;			// time delay between playing consecutive sentences
	BYTE	bEOFLocked;				// true if hit end of list of locked sentences
	BYTE	bEOFUnlocked;			// true if hit end of list of unlocked sentences
} locksound_t;

void PlayLockSounds(entvars_t *pev, locksound_t *pls, int flocked, int fbutton);

//
// MultiSouce
//

#define MAX_MULTI_TARGETS	16 // maximum number of targets a single multi_manager entity may be assigned.
#define MS_MAX_TARGETS 32

class CMultiSource : public CPointEntity
{
public:
	void Spawn( );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void );
	void EXPORT Register( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	EHANDLE		m_rgEntities[MS_MAX_TARGETS];
	int			m_rgTriggered[MS_MAX_TARGETS];

	int			m_iTotal;
	string_t	m_globalstate;
};


//
// generic Delay entity.
//
class CBaseDelay : public CBaseEntity
{
public:
	float		m_flDelay;
	int			m_iszKillTarget;
	EHANDLE		m_hActivator; //LRC - moved here from CBaseToggle

	virtual void	KeyValue( KeyValueData* pkvd);
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
	// common member functions
	void SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value );
	void EXPORT DelayThink( void );
};


class CBaseAnimating : public CBaseDelay
{
public:
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	// Basic Monster Animation functions
	float StudioFrameAdvance( float flInterval = 0.0 ); // accumulate animation frame time from last time called until now
	float StudioGaitFrameAdvance( void );	// accumulate gait frame time from last time called until now
	int GetSequenceFlags( void );
	int LookupActivity ( int activity );
	int LookupActivityHeaviest ( int activity );
	int LookupSequence ( const char *label );
	static void *GetModelPtr( int modelindex );
	void *GetModelPtr( void );
	void ResetSequenceInfo ( );
	float SequenceDuration( int iSequence );
	void DispatchAnimEvents ( float flFutureInterval = 0.1 ); // Handle events that have happend since last time called up until X seconds into the future
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent ) { return; };
	float SetBoneController ( int iController, float flValue );
	void InitBoneControllers ( void );
	float SetBlending ( int iBlender, float flValue );
	void GetBonePosition ( int iBone, Vector &origin, Vector &angles );
	void GetAutomovement( Vector &origin, Vector &angles, float flInterval = 0.1 );
	int  FindTransition( int iEndingSequence, int iGoalSequence, int *piDir );
	void GetAttachment ( int iAttachment, Vector &origin, Vector &angles );
	int GetAttachment ( const char *pszAttachment, Vector &origin, Vector &angles );
	void SetBodygroup( int iGroup, int iValue );
	int GetBodygroup( int iGroup );
	int ExtractBbox( int sequence, Vector &mins, Vector &maxs );
	int GetHitboxSetByName( const char *szName );
	void SetSequenceBox( void );
	CStudioBoneSetup *GetBoneSetup();
	int GetSequenceCount( void );
	float GetSequenceMoveYaw( int iSequence );
	float GetSequenceMoveDist( int iSequence );
	float GetSequenceGroundSpeed( int iSequence );

	// buz
	float GetControllerBound( int iController );

	const char *GetNameForActivity( int activity );

	// animation needs
	float	m_flFrameRate;		// computed FPS for current sequence
	float	m_flGroundSpeed;	// computed linear movement rate for current sequence
	float	m_flLastEventCheck;	// last time the event list was checked
	BOOL	m_fSequenceFinished;// flag set when StudioAdvanceFrame moves across a frame boundry
	BOOL	m_fSequenceLoops;	// true if the sequence loops
	// gaitsequence needs
	float	m_flGaitMovement;
};


//
// generic Toggle entity.
//
#define	SF_ITEM_USE_ONLY	256 //  ITEM_USE_ONLY = BUTTON_USE_ONLY = DOOR_USE_ONLY!!! 

class CBaseToggle : public CBaseAnimating
{
public:
	void				KeyValue( KeyValueData *pkvd );

	TOGGLE_STATE		m_toggle_state;
	float				m_flActivateFinished;//like attack_finished, but for doors
	float				m_flMoveDistance;// how far a door should slide or rotate
	float				m_flWait;
	float				m_flLip;
	float				m_flTWidth;// for plats
	float				m_flTLength;// for plats

	Vector				m_vecPosition1;
	Vector				m_vecPosition2;
	Vector				m_vecAngle1;
	Vector				m_vecAngle2;

	int					m_cTriggersLeft;		// trigger_counter only, # of activations remaining
	float				m_flHeight;
	void (CBaseToggle::*m_pfnCallWhenMoveDone)(void);
	Vector				m_vecFinalDest;
	float				m_flLinearMoveSpeed;	// LRC- allows a LinearMove to be delayed until a think.
	float				m_flAngularMoveSpeed;	// LRC
	Vector				m_vecFinalAngle;

	int					m_bitsDamageInflict;	// DMG_ damage type that the door or tigger does

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	virtual int		GetToggleState( void ) { return m_toggle_state; }

	// LRC- overridden because toggling entities have general rules governing their states.
	virtual STATE GetState( void );

	virtual float	GetDelay( void ) { return m_flWait; }

	// common member functions
	void LinearMove( Vector	vecInput, float flSpeed );
	//void LinearMove( Vector	vecInput, float flSpeed, BOOL bNow );
	void EXPORT LinearMoveNow( void ); //LRC- think function that lets us guarantee a LinearMove gets done as a think.
	void EXPORT LinearMoveDone( void );
	void EXPORT LinearMoveDoneNow( void ); //LRC
//	void EXPORT LinearMoveFinalDone( void );
	void AngularMove( Vector vecDestAngle, float flSpeed );
	void EXPORT AngularMoveNow( void ); //LRC- think function that lets us guarantee an AngularMove gets done as a think.
	void EXPORT AngularMoveDone( void );
	void EXPORT AngularMoveDoneNow( void );
	BOOL IsLockedByMaster( void );

	static float		AxisValue( int flags, const Vector &angles );
	static void			AxisDir( entvars_t *pev );
	static float		AxisDelta( int flags, const Vector &angle1, const Vector &angle2 );

	string_t m_sMaster;		// If this button has a master switch, this is the targetname.
							// A master switch must be of the multisource type. If all 
							// of the switches in the multisource have been triggered, then
							// the button will be allowed to operate. Otherwise, it will be
							// deactivated.
};
#define SetMoveDone( a ) m_pfnCallWhenMoveDone = static_cast <void (CBaseToggle::*)(void)> (a)


// people gib if their health is <= this at the time of death
#define	GIB_HEALTH_VALUE	-30

#define	ROUTE_SIZE			8 // how many waypoints a monster can store at one time
#define MAX_OLD_ENEMIES		4 // how many old enemies to remember

#define	bits_CAP_DUCK			( 1 << 0 )// crouch
#define	bits_CAP_JUMP			( 1 << 1 )// jump/leap
#define bits_CAP_STRAFE			( 1 << 2 )// strafe ( walk/run sideways)
#define bits_CAP_SQUAD			( 1 << 3 )// can form squads
#define	bits_CAP_SWIM			( 1 << 4 )// proficiently navigate in water
#define bits_CAP_CLIMB			( 1 << 5 )// climb ladders/ropes
#define bits_CAP_USE			( 1 << 6 )// open doors/push buttons/pull levers
#define bits_CAP_HEAR			( 1 << 7 )// can hear forced sounds
#define bits_CAP_AUTO_DOORS		( 1 << 8 )// can trigger auto doors
#define bits_CAP_OPEN_DOORS		( 1 << 9 )// can open manual doors
#define bits_CAP_TURN_HEAD		( 1 << 10)// can turn head, always bone controller 0

#define bits_CAP_RANGE_ATTACK1	( 1 << 11)// can do a range attack 1
#define bits_CAP_RANGE_ATTACK2	( 1 << 12)// can do a range attack 2
#define bits_CAP_MELEE_ATTACK1	( 1 << 13)// can do a melee attack 1
#define bits_CAP_MELEE_ATTACK2	( 1 << 14)// can do a melee attack 2

#define bits_CAP_CROUCH_COVER	( 1 << 16)// buz

#define bits_CAP_FLY			( 1 << 15)// can fly, move all around

#define bits_CAP_DOORS_GROUP    (bits_CAP_USE | bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS)

// used by suit voice to indicate damage sustained and repaired type to player

// instant damage

#define DMG_GENERIC			0			// generic damage was done
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object
#define DMG_BULLET			(1 << 1)	// shot
#define DMG_SLASH			(1 << 2)	// cut, clawed, stabbed
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_FREEZE			(1 << 4)	// frozen
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_BLAST			(1 << 6)	// explosive blast damage
#define DMG_CLUB			(1 << 7)	// crowbar, punch, headbutt
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_SONIC			(1 << 9)	// sound pulse shockwave
#define DMG_ENERGYBEAM		(1 << 10)	// laser or other high energy beam 
#define DMG_NEVERGIB		(1 << 12)	// with this bit OR'd in, no damage type will be able to gib victims upon death
#define DMG_ALWAYSGIB		(1 << 13)	// with this bit OR'd in, any damage type can be made to gib victims upon death.
#define DMG_DROWN			(1 << 14)	// Drowning
// time-based damage
#define DMG_TIMEBASED		(~(0x3fff))	// mask for time-based damage

#define DMG_PARALYZE		(1 << 15)	// slows affected creature down
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_POISON			(1 << 17)	// blood poisioning
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven
#define DMG_SLOWFREEZE		(1 << 22)	// in a subzero freezer
#define DMG_MORTAR			(1 << 23)	// Hit by air raid (done to distinguish grenade from mortar)

// these are the damage types that are allowed to gib corpses
#define DMG_GIB_CORPSE		( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC /* | DMG_CLUB */ ) // Wargon: DMG_CLUB не может гибать трупы.

// these are the damage types that have client hud art
#define DMG_SHOWNHUD		(DMG_POISON | DMG_ACID | DMG_FREEZE | DMG_SLOWFREEZE | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK)

// NOTE: tweak these values based on gameplay feedback:

#define PARALYZE_DURATION	2		// number of 2 second intervals to take damage
#define PARALYZE_DAMAGE		1.0		// damage to take each 2 second interval

#define NERVEGAS_DURATION	2
#define NERVEGAS_DAMAGE		5.0

#define POISON_DURATION		5
#define POISON_DAMAGE		2.0

#define RADIATION_DURATION	2
#define RADIATION_DAMAGE	1.0

#define ACID_DURATION		2
#define ACID_DAMAGE			5.0

#define SLOWBURN_DURATION	2
#define SLOWBURN_DAMAGE		1.0

#define SLOWFREEZE_DURATION	2
#define SLOWFREEZE_DAMAGE	1.0


#define	itbd_Paralyze		0		
#define	itbd_NerveGas		1
#define	itbd_Poison			2
#define	itbd_Radiation		3
#define	itbd_DrownRecover	4
#define	itbd_Acid			5
#define	itbd_SlowBurn		6
#define	itbd_SlowFreeze		7
#define CDMG_TIMEBASED		8

// when calling KILLED(), a value that governs gib behavior is expected to be 
// one of these three values
#define GIB_NORMAL			0// gib if entity was overkilled
#define GIB_NEVER			1// never gib, no matter how much death damage is done ( freezing, etc )
#define GIB_ALWAYS			2// always gib ( Houndeye Shock, Barnacle Bite )

class CBaseMonster;
class CCineMonster;
class CSound;

#include "basemonster.h"


char *ButtonSound( int sound );				// get string of button sound number


//
// Generic Button
//
class CBaseButton : public CBaseToggle
{
public:
	void Spawn( void );
	virtual void PostSpawn( void ); //LRC
	virtual void Precache( void );
	void RotSpawn( void );
	virtual void KeyValue( KeyValueData* pkvd);

	void ButtonActivate( );
	void SparkSoundCache( void );

	void EXPORT ButtonShot( void );
	void EXPORT ButtonTouch( CBaseEntity *pOther );
	void EXPORT ButtonSpark ( void );
	void EXPORT TriggerAndWait( void );
	void EXPORT ButtonReturn( void );
	void EXPORT ButtonBackHome( void );
	void EXPORT ButtonUse_IgnorePlayer( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ButtonUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int		TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN };
	BUTTON_CODE	ButtonResponseToTouch( void );
	
	static	TYPEDESCRIPTION m_SaveData[];
	virtual int	ObjectCaps( void );

	BOOL	m_fStayPushed;	// button stays pushed in until touched again?
	BOOL	m_fRotating;		// a rotating button?  default is a sliding button.

	string_t m_strChangeTarget;	// if this field is not null, this is an index into the engine string array.
							// when this button is touched, it's target entity's TARGET field will be set
							// to the button's ChangeTarget. This allows you to make a func_train switch paths, etc.

	locksound_t m_ls;			// door lock sounds
	
	BYTE	m_bLockedSound;		// ordinals from entity selection
	BYTE	m_bLockedSentence;	
	BYTE	m_bUnlockedSound;	
	BYTE	m_bUnlockedSentence;
	int		m_sounds;

	// Wargon: Переменная для скрытия иконки юза.
	int m_hide_use;
};

//
// Weapons 
//

#define	BAD_WEAPON 0x00007FFF

//
// Converts a entvars_t * to a class pointer
// It will allocate the class and entity if necessary
//
template <class T> T * GetClassPtr( T *a )
{
	entvars_t *pev = (entvars_t *)a;

	// allocate entity if necessary
	if (pev == NULL)
		pev = VARS(CREATE_ENTITY());

	// get the private data
	a = (T *)GET_PRIVATE(ENT(pev));

	if (a == NULL) 
	{
		// allocate private data 
		a = new(pev) T;
		a->pev = pev;
	}
	return a;
}


/*
bit_PUSHBRUSH_DATA | bit_TOGGLE_DATA
bit_MONSTER_DATA
bit_DELAY_DATA
bit_TOGGLE_DATA | bit_DELAY_DATA | bit_MONSTER_DATA
bit_PLAYER_DATA | bit_MONSTER_DATA
bit_MONSTER_DATA | CYCLER_DATA
bit_LIGHT_DATA
path_corner_data
bit_MONSTER_DATA | wildcard_data
bit_MONSTER_DATA | bit_GROUP_DATA
boid_flock_data
boid_data
CYCLER_DATA
bit_ITEM_DATA
bit_ITEM_DATA | func_hud_data
bit_TOGGLE_DATA | bit_ITEM_DATA
EOFFSET
env_sound_data
env_sound_data
push_trigger_data
*/

typedef struct _SelAmmo
{
	BYTE	Ammo1Type;
	BYTE	Ammo1;
	BYTE	Ammo2Type;
	BYTE	Ammo2;
} SelAmmo;

//LRC- much as I hate to add new globals, I can't see how to read data from the World entity.
//extern BOOL g_startSuit;

//LRC- moved here from alias.cpp so that util functions can use these defs.
class CBaseAlias : public CPointEntity
{
public:
	BOOL IsAlias( void ) { return TRUE; };
	virtual CBaseEntity *FollowAlias( CBaseEntity *pFrom ) { return NULL; };
	virtual void ChangeValue( int iszValue ) { ALERT(at_error, "%s entities cannot change value!", STRING(pev->classname)); }
	virtual void ChangeValue( CBaseEntity *pValue ) { ChangeValue(pValue->pev->targetname); }
	virtual void FlushChanges( void ) {};

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBaseAlias *m_pNextAlias;
};

class CInfoGroup : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int GetMember( const char* szMemberName );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	int		m_cMembers;
	int		m_iszMemberName	[ MAX_MULTI_TARGETS ];
	int		m_iszMemberValue [ MAX_MULTI_TARGETS ];
	int		m_iszDefaultMember;
};

class CMultiAlias : public CBaseAlias
{
public:
	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	CBaseEntity *FollowAlias( CBaseEntity *pFrom );

	int		m_cTargets;
	int		m_iszTargets	[ MAX_MULTI_TARGETS ];
	int		m_iTotalValue;
	int		m_iValues	[ MAX_MULTI_TARGETS ];
	int		m_iMode;
};


// this moved here from world.cpp, to allow classes to be derived from it
//=======================
// CWorld
//
// This spawns first when each level begins.
//=======================
class CWorld : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

	CBaseAlias *m_pFirstAlias;
	Vector m_vecTime;
};

extern CWorld *g_pWorld;