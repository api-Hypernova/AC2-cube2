#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"

// console message types

enum
{
    CON_CHAT       = 1<<8,
    CON_TEAMCHAT   = 1<<9,
    CON_GAMEINFO   = 1<<10,
    CON_FRAG_SELF  = 1<<11,
    CON_FRAG_OTHER = 1<<12,
    CON_TEAMKILL   = 1<<13
};

// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

enum                            // static entity types
{
    NOTUSED = ET_EMPTY,         // entity slot not in use in map
    LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
    MAPMODEL = ET_MAPMODEL,     // attr1 = angle, attr2 = idx
    PLAYERSTART,                // attr1 = angle, attr2 = team
    ENVMAP = ET_ENVMAP,         // attr1 = radius
    PARTICLES = ET_PARTICLES,
    MAPSOUND = ET_SOUND,
    SPOTLIGHT = ET_SPOTLIGHT,
    I_ELECTRO, I_MINIGUN, I_RPG, I_MAGNUM, I_CROSSBOW, I_GRENADE, I_PISTOLAMMO, I_SMGAMMO, I_SMGNADE, I_SHELLS, I_ELECTROAMMO, I_ELECTROORBS, I_MINIGUNAMMO, I_TESLAORB, I_XBOWAMMO, I_RPGAMMO, I_SNIPERAMMO,
    I_SHOTGUN, //attr1 = yaw
    I_HEALTH, I_BOOST, I_SMALLHEALTH, I_SHIELDBATTERY,
    I_GREENARMOUR, I_YELLOWARMOUR,
    I_QUAD,
    TELEPORT,                   // attr1 = idx, attr2 = model, attr3 = tag
    TELEDEST,                   // attr1 = angle, attr2 = idx
    MONSTER,                    // attr1 = angle, attr2 = monstertype
    CARROT,                     // attr1 = tag, attr2 = type
    JUMPPAD,                    // attr1 = zpush, attr2 = ypush, attr3 = xpush
    BASE,
    RESPAWNPOINT,
    BOX,                        // attr1 = angle, attr2 = idx, attr3 = weight
    BARREL,                     // attr1 = angle, attr2 = idx, attr3 = weight, attr4 = health
    PLATFORM,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
    ELEVATOR,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
    FLAG,                       // attr1 = angle, attr2 = team
    MUTER,      //31
    MAXENTTYPES
};

enum
{
    TRIGGER_RESET = 0,
    TRIGGERING,
    TRIGGERED,
    TRIGGER_RESETTING,
    TRIGGER_DISAPPEARED
};

struct fpsentity : extentity
{
    int triggerstate, lasttrigger;

    fpsentity() : triggerstate(TRIGGER_RESET), lasttrigger(0) {}
};
//       chainsaw    shotty1 pulser  rpg     magnum     handgrenade
enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_MAGNUM, GUN_HANDGRENADE, GUN_ELECTRO, GUN_ELECTRO2, GUN_CG2, GUN_SHOTGUN2, GUN_SMG, GUN_SMG2, GUN_CROSSBOW, GUN_TELEKENESIS, GUN_TELEKENESIS2, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };
enum { A_BLUE, A_GREEN, A_YELLOW };     // armour types... take 20/40/60 % off
enum { M_NONE = 0, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };  // monster states

enum
{
    M_TEAM       = 1<<0,
    M_NOITEMS    = 1<<1,
    M_NOAMMO     = 1<<2,
    M_INSTA      = 1<<3,
    M_EFFICIENCY = 1<<4,
    M_TACTICS    = 1<<5,
    M_CAPTURE    = 1<<6,
    M_REGEN      = 1<<7,
    M_CTF        = 1<<8,
    M_PROTECT    = 1<<9,
    M_HOLD       = 1<<10,
    M_OVERTIME   = 1<<11,
    M_EDIT       = 1<<12,
    M_DEMO       = 1<<13,
    M_LOCAL      = 1<<14,
    M_LOBBY      = 1<<15,
    M_DMSP       = 1<<16,
    M_CLASSICSP  = 1<<17,
    M_SLOWMO     = 1<<18
};

static struct gamemodeinfo
{
    const char *name;
    int flags;
    const char *info;
} gamemodes[] =
{
{ "SP", M_LOCAL | M_CLASSICSP, NULL },
{ "DMSP", M_LOCAL | M_DMSP, NULL },
{ "demo", M_DEMO | M_LOCAL, NULL},
{ "ffa", M_LOBBY, "Free For All: Collect items for ammo. Frag everyone to score points." },
{ "coop edit", M_EDIT, "Cooperative Editing: Edit maps with multiple players simultaneously." },
{ "teamplay", M_TEAM | M_OVERTIME, "Teamplay: Collect items for ammo. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
{ "instagib", M_NOITEMS | M_INSTA, "Instagib: You spawn with full rifle ammo and die instantly from one shot. There are no items. Frag everyone to score points." },
{ "instagib team", M_NOITEMS | M_INSTA | M_TEAM | M_OVERTIME, "Instagib Team: You spawn with full rifle ammo and die instantly from one shot. There are no items. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
{ "efficiency", M_NOITEMS | M_EFFICIENCY, "Efficiency: You spawn with all weapons and armour. There are no items. Frag everyone to score points." },
{ "efficiency team", M_NOITEMS | M_EFFICIENCY | M_TEAM | M_OVERTIME, "Efficiency Team: You spawn with all weapons and armour. There are no items. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
{ "tactics", M_NOITEMS | M_TACTICS, "Tactics: You spawn with two random weapons and armour. There are no items. Frag everyone to score points." },
{ "tactics team", M_NOITEMS | M_TACTICS | M_TEAM | M_OVERTIME, "Tactics Team: You spawn with two random weapons and armour. There are no items. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
{ "capture", M_NOAMMO | M_TACTICS | M_CAPTURE | M_TEAM | M_OVERTIME, "Capture: Capture neutral bases or steal \fs\f3enemy bases\fr by standing next to them.  \fs\f1Your team\fr scores points for every 10 seconds it holds a base. You spawn with two random weapons and armour. Collect extra ammo that spawns at \fs\f1your bases\fr. There are no ammo items." },
{ "regen capture", M_NOITEMS | M_CAPTURE | M_REGEN | M_TEAM | M_OVERTIME, "Regen Capture: Capture neutral bases or steal \fs\f3enemy bases\fr by standing next to them. \fs\f1Your team\fr scores points for every 10 seconds it holds a base. Regenerate health and ammo by standing next to \fs\f1your bases\fr. There are no items." },
{ "ctf", M_CTF | M_TEAM, "Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. Collect items for ammo." },
{ "insta ctf", M_NOITEMS | M_INSTA | M_CTF | M_TEAM, "Instagib Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
{ "protect", M_CTF | M_PROTECT | M_TEAM, "Protect The Flag: Touch \fs\f3the enemy flag\fr to score points for \fs\f1your team\fr. Pick up \fs\f1your flag\fr to protect it. \fs\f1Your team\fr loses points if a dropped flag resets. Collect items for ammo." },
{ "insta protect", M_NOITEMS | M_INSTA | M_CTF | M_PROTECT | M_TEAM, "Instagib Protect The Flag: Touch \fs\f3the enemy flag\fr to score points for \fs\f1your team\fr. Pick up \fs\f1your flag\fr to protect it. \fs\f1Your team\fr loses points if a dropped flag resets. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
{ "hold", M_CTF | M_HOLD | M_TEAM, "Hold The Flag: Hold \fs\f7the flag\fr for 20 seconds to score points for \fs\f1your team\fr. Collect items for ammo." },
{ "insta hold", M_NOITEMS | M_INSTA | M_CTF | M_HOLD | M_TEAM, "Instagib Hold The Flag: Hold \fs\f7the flag\fr for 20 seconds to score points for \fs\f1your team\fr. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
{ "efficiency ctf", M_NOITEMS | M_EFFICIENCY | M_CTF | M_TEAM, "Efficiency Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. You spawn with all weapons and armour. There are no items." },
{ "efficiency protect", M_NOITEMS | M_EFFICIENCY | M_CTF | M_PROTECT | M_TEAM, "Efficiency Protect The Flag: Touch \fs\f3the enemy flag\fr to score points for \fs\f1your team\fr. Pick up \fs\f1your flag\fr to protect it. \fs\f1Your team\fr loses points if a dropped flag resets. You spawn with all weapons and armour. There are no items." },
{ "efficiency hold", M_NOITEMS | M_EFFICIENCY | M_CTF | M_HOLD | M_TEAM, "Efficiency Hold The Flag: Hold \fs\f7the flag\fr for 20 seconds to score points for \fs\f1your team\fr. You spawn with all weapons and armour. There are no items." }
};

#define STARTGAMEMODE (-3)
#define NUMGAMEMODES ((int)(sizeof(gamemodes)/sizeof(gamemodes[0])))

#define m_valid(mode)          ((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES)
#define m_check(mode, flag)    (m_valid(mode) && gamemodes[(mode) - STARTGAMEMODE].flags&(flag))
#define m_checknot(mode, flag) (m_valid(mode) && !(gamemodes[(mode) - STARTGAMEMODE].flags&(flag)))
#define m_checkall(mode, flag) (m_valid(mode) && (gamemodes[(mode) - STARTGAMEMODE].flags&(flag)) == (flag))

#define m_noitems      (m_check(gamemode, M_NOITEMS))
#define m_noammo       (m_check(gamemode, M_NOAMMO|M_NOITEMS))
#define m_insta        (m_check(gamemode, M_INSTA))
#define m_tactics      (m_check(gamemode, M_TACTICS))
#define m_efficiency   (m_check(gamemode, M_EFFICIENCY))
#define m_capture      (m_check(gamemode, M_CAPTURE))
#define m_regencapture (m_checkall(gamemode, M_CAPTURE | M_REGEN))
#define m_ctf          (m_check(gamemode, M_CTF))
#define m_protect      (m_checkall(gamemode, M_CTF | M_PROTECT))
#define m_hold         (m_checkall(gamemode, M_CTF | M_HOLD))
#define m_teammode     (m_check(gamemode, M_TEAM))
#define m_overtime     (m_check(gamemode, M_OVERTIME))
#define isteam(a,b)    (m_teammode && strcmp(a, b)==0)

#define m_demo         (m_check(gamemode, M_DEMO))
#define m_edit         (m_check(gamemode, M_EDIT))
#define m_lobby        (m_check(gamemode, M_LOBBY))
#define m_timed        (m_checknot(gamemode, M_DEMO|M_EDIT|M_LOCAL))
#define m_botmode      (m_checknot(gamemode, M_DEMO|M_LOCAL))
#define m_mp(mode)     (m_checknot(mode, M_LOCAL))

#define m_sp           (m_check(gamemode, M_DMSP | M_CLASSICSP))
#define m_dmsp         (m_check(gamemode, M_DMSP))
#define m_classicsp    (m_check(gamemode, M_CLASSICSP))

enum { MM_AUTH = -1, MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD, MM_START = MM_AUTH };

static const char * const mastermodenames[] =  { "auth",   "open",   "veto",       "locked",     "private",    "password" };
static const char * const mastermodecolors[] = { "",       "\f0",    "\f2",        "\f2",        "\f3",        "\f3" };
static const char * const mastermodeicons[] =  { "server", "server", "serverlock", "serverlock", "serverpriv", "serverpriv" };

// hardcoded sounds, defined in sounds.cfg
enum
{
    S_JUMP = 0, S_LAND, S_RIFLE, S_PUNCH1, S_SG, S_CG,
    S_RLFIRE, S_RLHIT, S_WEAPLOAD, S_ITEMAMMO, S_ITEMHEALTH,
    S_ITEMARMOUR, S_ITEMPUP, S_ITEMSPAWN, S_TELEPORT, S_NOAMMO, S_PUPOUT,
    S_PAIN1, S_PAIN2, S_PAIN3, S_PAIN4, S_PAIN5, S_PAIN6,
    S_DIE1, S_DIE2,
    S_FLAUNCH, S_FEXPLODE,
    S_SPLASH1, S_SPLASH2,
    S_GRUNT1, S_GRUNT2, S_RUMBLE,
    S_PAINO,
    S_PAINR, S_DEATHR,
    S_PAINE, S_DEATHE,
    S_PAINS, S_DEATHS,
    S_PAINB, S_DEATHB,
    S_PAINP, S_PIGGR2,
    S_PAINH, S_DEATHH,
    S_PAIND, S_DEATHD,
    S_PIGR1, S_ICEBALL, S_SLIMEBALL,
    S_JUMPPAD, S_PL,

    S_V_BASECAP, S_V_BASELOST,
    S_V_FIGHT,
    S_V_BOOST, S_V_BOOST10,
    S_V_QUAD, S_V_QUAD10,
    S_V_RESPAWNPOINT,

    S_FLAGPICKUP,
    S_FLAGDROP,
    S_FLAGRETURN,
    S_FLAGSCORE,
    S_FLAGRESET,

    S_BURN,
    S_CHAINSAW_ATTACK,
    S_CHAINSAW_IDLE,

    S_HIT,

    S_BHIT1, S_BHIT2, S_BHIT3,

    S_RIFLELOAD, S_SGLOAD, S_GLLOAD,

    S_QHIT1, S_QHIT2, S_QHIT3,
    S_RLEX,

    S_RIFLE1, S_RIFLE2, S_RIFLE3,
    S_SG1, S_SG2, S_SG3,
    S_CG1, S_CG2, S_CG3,
    S_FLAUNCH1, S_FLAUNCH2, S_FLAUNCH3,
    S_PL1, S_PL2, S_PL3,
    S_RLHIT1, S_RLHIT2, S_RLHIT3,
    S_RLEX1, S_RLEX2, S_RLEX3,

    S_CHAT,

    S_SCH1, S_SCH2, S_SCH3,
    S_ROCKET, S_ROCKETUW,

    S_GIB1, S_GIB2, S_GIB3,

    S_UWEN, S_UWST1, S_UWST2, S_UWST3, S_UW,

    S_UWPN4, S_UWPN5, S_UWPN6,
    S_KILL, S_TEAMKILL, S_GREATSHOT,

    S_NEARMISS1, S_NEARMISS2, S_NEARMISS3,

    S_HEARTBEAT,

    S_GUI_OPEN, S_GUI_CLOSE, S_GUI_BUTTON,

    S_BNC_GIB1, S_BNC_GIB2, S_BNC_GIB3,

    S_AIRSHOT,
    S_AMAZING,
    S_AWESOME,
    S_BOTLIKE,
    S_TAUNT1,
    S_TAUNT2,
    S_TAUNT3,
    S_TEAMSHOOT,
    S_TEAMSHOOT2,
    S_TEAMSHOOT3,
    S_TEAMSHOOT4,


    S_BLUE_CAPTURE,
    S_BLUE_DROPPED,
    S_BLUE_RETURNED,
    S_BLUE_TAKEN,

    S_RED_CAPTURE,
    S_RED_DROPPED,
    S_RED_TAKEN,
    S_RED_RETURNED,

    S_IMPACT,
    S_IMPACT2,

    S_BZAP,
    S_CELLPICKUP,
    S_CELLSWITCH,
    S_CHARGER,
    S_ELECTRO,
    S_GIB,
    S_NADEBOUNCE,
    S_NADEBOUNCE2,
    S_NADEBOUNCE3,
    S_LASERIMPACT,
    S_LGPICKUP,
    S_MINSTANEX,
    S_NEXIMPACT,
    S_ROCKETPICKUP,
    S_ROCKETSWITCH,
    S_RPG,
    S_SHIELDBROKEN,
    S_SHOTGUN,
    S_SHOTGUNPICKUP,
    S_SHOTGUNSWITCH,
    S_SMGNADE,
    S_SMGSWITCH,
    S_UZI,
    S_LG,
    S_REMOTEDETONATE,
    S_SILENCE,
    S_LASER,
    S_ORB,
    S_BOUNCE1,
    S_IMPRESSIVE,
    S_ORBCLOSE,
    S_LIGHTNING,
    S_CROSSBOWFIRE,
    S_CROSSBOWRELOAD,
    S_PULSERIFLE,
    S_SHOTGUN2,
    S_MAGNUM,
    S_ORBBOUNCE,
    S_TELE,
    S_JUMP2,
    S_JUMP3,
    S_JUMP4,
    S_HEADSHOT,
    S_TRYGRAB,
    S_PUNT,
    S_WALK1,
    S_WALK2,
    S_WALK3,
    S_WALK4,
    S_WALK5,
    S_WALK6,
    S_WALK7,
    S_WALK8,
    S_LAND1,
    S_LAND2,
    S_LAND3,
    S_SHOTGUNBURST,
    S_PISTOL,
    S_HANDNADE,
    S_NADEBEEP,
    S_RELOAD,
    S_HEADSHOT2,
    S_CRYLINK,
    S_SPREE1,
    S_SPREE2,
    S_SPREE3,
    S_SPREE4,
    S_SNIPER,
    S_SKEWER,
    S_D1,
    S_D2,
    S_D3

};

// network messages codes, c2s, c2c, s2c

enum { PRIV_NONE = 0, PRIV_MASTER, PRIV_ADMIN };

enum
{
    N_CONNECT = 0, N_SERVINFO, N_WELCOME, N_INITCLIENT, N_POS, N_TEXT, N_SOUND, N_CDIS,
    N_SHOOT, N_EXPLODE, N_SUICIDE,
    N_DIED, N_DAMAGE, N_HITPUSH, N_SHOTFX, N_EXPLODEFX,
    N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_FORCEDEATH,
    N_GUNSELECT, N_TAUNT,
    N_MAPCHANGE, N_MAPVOTE, N_ITEMSPAWN, N_ITEMPICKUP, N_ITEMACC, N_TELEPORT, N_JUMPPAD,
    N_PING, N_PONG, N_CLIENTPING,
    N_TIMEUP, N_MAPRELOAD, N_FORCEINTERMISSION,
    N_SERVMSG, N_ITEMLIST, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_REMIP, N_NEWMAP, N_GETMAP, N_SENDMAP, N_CLIPBOARD, N_EDITVAR,
    N_MASTERMODE, N_KICK, N_CLEARBANS, N_CURRENTMASTER, N_SPECTATOR, N_SETMASTER, N_SETTEAM,
    N_BASES, N_BASEINFO, N_BASESCORE, N_REPAMMO, N_BASEREGEN, N_ANNOUNCE,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO,
    N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_TAKEFLAG, N_RETURNFLAG, N_RESETFLAG, N_INVISFLAG, N_TRYDROPFLAG, N_DROPFLAG, N_SCOREFLAG, N_INITFLAGS,
    N_SAYTEAM,
    N_CLIENT,
    N_AUTHTRY, N_AUTHCHAL, N_AUTHANS, N_REQAUTH,
    N_PAUSEGAME, N_GAMESPEED,
    N_ADDBOT, N_DELBOT, N_INITAI, N_FROMAI, N_BOTLIMIT, N_BOTBALANCE,
    N_MAPCRC, N_CHECKMAPS,
    N_SWITCHNAME, N_SWITCHMODEL, N_SWITCHTEAM, N_CROUCH, N_CATCH, N_DELMOVABLE,
    NUMSV
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
        N_CONNECT, 0, N_SERVINFO, 0, N_WELCOME, 2, N_INITCLIENT, 0, N_POS, 0, N_TEXT, 0, N_SOUND, 2, N_CDIS, 2,
        N_SHOOT, 0, N_EXPLODE, 0, N_SUICIDE, 1,
        N_DIED, 4, N_DAMAGE, 6, N_HITPUSH, 7, N_SHOTFX, 10, N_EXPLODEFX, 4, //shotfx by def is 10
        N_TRYSPAWN, 1, N_SPAWNSTATE, 14, N_SPAWN, 3, N_FORCEDEATH, 2,
        N_GUNSELECT, 2, N_TAUNT, 1,
        N_MAPCHANGE, 0, N_MAPVOTE, 0, N_ITEMSPAWN, 2, N_ITEMPICKUP, 2, N_ITEMACC, 3,
        N_PING, 2, N_PONG, 2, N_CLIENTPING, 2,
        N_TIMEUP, 2, N_MAPRELOAD, 1, N_FORCEINTERMISSION, 1,
        N_SERVMSG, 0, N_ITEMLIST, 0, N_RESUME, 0,
        N_EDITMODE, 2, N_EDITENT, 11, N_EDITF, 16, N_EDITT, 16, N_EDITM, 16, N_FLIP, 14, N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14, N_REMIP, 1, N_NEWMAP, 2, N_GETMAP, 1, N_SENDMAP, 0, N_EDITVAR, 0,
        N_MASTERMODE, 2, N_KICK, 2, N_CLEARBANS, 1, N_CURRENTMASTER, 4, N_SPECTATOR, 3, N_SETMASTER, 0, N_SETTEAM, 0,
        N_BASES, 0, N_BASEINFO, 0, N_BASESCORE, 0, N_REPAMMO, 1, N_BASEREGEN, 6, N_ANNOUNCE, 2,
        N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 2, N_SENDDEMO, 0,
        N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
        N_TAKEFLAG, 3, N_RETURNFLAG, 4, N_RESETFLAG, 6, N_INVISFLAG, 3, N_TRYDROPFLAG, 1, N_DROPFLAG, 7, N_SCOREFLAG, 10, N_INITFLAGS, 0,
        N_SAYTEAM, 0,
        N_CLIENT, 0,
        N_AUTHTRY, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_REQAUTH, 0,
        N_PAUSEGAME, 0, N_GAMESPEED, 0,
        N_ADDBOT, 2, N_DELBOT, 1, N_INITAI, 0, N_FROMAI, 2, N_BOTLIMIT, 2, N_BOTBALANCE, 2,
        N_MAPCRC, 0, N_CHECKMAPS, 1,
        N_SWITCHNAME, 0, N_SWITCHMODEL, 2, N_SWITCHTEAM, 0, N_CROUCH, 2, N_CATCH, 6, N_DELMOVABLE, 2,
        -1
        };

#define SAUERBRATEN_LANINFO_PORT 28784
#define SAUERBRATEN_SERVER_PORT 28785
#define SAUERBRATEN_SERVINFO_PORT 28786
#define SAUERBRATEN_MASTER_PORT 28787
#define PROTOCOL_VERSION 1212            // bump when protocol changes
#define DEMO_VERSION 5                  // bump when demo format changes
#define DEMO_MAGIC "SAUERBRATEN_DEMO"

struct demoheader
{
    char magic[16];
    int version, protocol;
};

#define MAXNAMELEN 25
#define MAXTEAMLEN 10

enum
{
    HICON_BLUE_ARMOUR = 0,
    HICON_GREEN_ARMOUR,
    HICON_YELLOW_ARMOUR,

    HICON_HEALTH,

    HICON_FIST,
    HICON_SG,
    HICON_CG,
    HICON_RL,
    HICON_RIFLE,
    HICON_GL,
    HICON_PISTOL,

    HICON_QUAD,

    HICON_RED_FLAG,
    HICON_BLUE_FLAG,
    HICON_NEUTRAL_FLAG,

    HICON_X       = 20,
    HICON_Y       = 1650,
    HICON_TEXTY   = 1644,
    HICON_STEP    = 490,
    HICON_SIZE    = 120,
    HICON_SPACE   = 40
};

static struct itemstat { int add, max, sound; const char *name; int icon, info, gunneeded; } itemstats[] =
{
    //{40,    200,    S_ITEMAMMO,   "PI", HICON_PISTOL, GUN_SHOTGUN},
{10,   30,    S_ROCKETPICKUP,   "PI", HICON_GL, GUN_ELECTRO, 0},
{30,   120,    S_ROCKETPICKUP,   "CG", HICON_CG, GUN_CG, 0},
//{10,     40,     S_RLPICKUP,   "RL", HICON_RL, GUN_RL},
{3,     3,     S_ROCKETPICKUP,   "RL", HICON_RL, GUN_RL, 0},
{12,    24,     S_ROCKETPICKUP,   "RI", HICON_GL, GUN_MAGNUM, 0},
{5,     10,     S_ROCKETPICKUP,   "GL", HICON_RL, GUN_CROSSBOW, 0},
//{20,    100,    S_SGPICKUP,   "SG", HICON_SG, GUN_SG},
{1, 5, S_ITEMAMMO, "", -1, GUN_HANDGRENADE, 0},
{18,150, S_ITEMAMMO, "", -1, GUN_PISTOL, 0},
{45,225, S_ITEMAMMO, "", -1, GUN_SMG, 0},
{1, 3, S_ITEMAMMO, "", -1, GUN_SMG2, 0},
{15, 30, S_ITEMAMMO, "", -1, GUN_SG, GUN_SG},
{10, 30, S_ITEMAMMO, "", -1, GUN_ELECTRO, GUN_ELECTRO},
{10, 30, S_ITEMAMMO, "", -1, GUN_ELECTRO2, GUN_ELECTRO},
{30, 120, S_ITEMAMMO, "", -1, GUN_CG, GUN_CG},
{1, 3, S_ITEMAMMO, "", -1, GUN_CG2, GUN_CG},
{5, 10, S_ITEMAMMO, "", -1, GUN_CROSSBOW, GUN_CROSSBOW},
{1, 3, S_ITEMAMMO, "", -1, GUN_RL, GUN_RL},
{12,24, S_ITEMAMMO, "", -1, GUN_MAGNUM, GUN_MAGNUM},
{15,    30,    S_ROCKETPICKUP,   "SG", HICON_SG, GUN_SG, 0},
{25,    100,    S_ITEMHEALTH, "H",  HICON_HEALTH, -1},
{4,     100,   S_ITEMARMOUR, "MH", HICON_HEALTH, -1},
{15,    100,    S_ITEMHEALTH},
{15,    100,    S_LGPICKUP, "", HICON_BLUE_ARMOUR, A_YELLOW},
{2,    60,    S_ITEMARMOUR, "GA", HICON_GREEN_ARMOUR, A_YELLOW},
{2,   200,    S_ITEMARMOUR, "YA", HICON_YELLOW_ARMOUR, A_YELLOW},
{40000, INT_MAX-1, S_ITEMPUP,    "Q",  HICON_QUAD, -1},
};

#define MAXRAYS 12
#define SGSPREAD 2
#define RL_DAMRAD 40
#define RL_SELFDAMDIV 2
#define RL_DISTSCALE 1.5f

//enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_MAGNUM, GUN_HANDGRENADE, GUN_ELECTRO, GUN_ELECTRO2, GUN_CG2, GUN_SHOTGUN2, GUN_SMG, GUN_SMG2, GUN_TELEKENESIS, GUN_TELEKENESIS2, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };
//guns change: game, weapon, render, animsthing, server, fps, playeranim files, autocrosshair, weapon binds, bouncers, gun icons
static const struct guninfo { short sound, reloadsound, magsize, attackdelay, damage, spread, projspeed, part, kickamount, rays, range, hitpush, splash, ttl, burstlen, guided, sub; const char *name, *file; } guns[NUMGUNS] =
{
    { S_LASER,   -1, 0,  500,  45, 0,   0,   0, 75,  1, 1024,  200, 0, 0,    0, 0, 0, "blaster", "laser"  },
    { S_SHOTGUN2,  -1, 6, 900,  9, 150, 0,   0, 20, 12,1024, 200, 0, 0,    0, 0, 2, "*",         "shotgdefault" }, //shotgdefault
    { S_PULSERIFLE, -1, 30, 100,  26, 0,  0,   0, 7,  1, 1024, 150, 0, 0,    0, 0, 1,  "}",        "pulse_rifle"}, //chaing
    { S_RPG,    -1, 0, 1500, 150, 0,   400,  0, 10, 1, 1024, 200, 40,0,    0, 1, 1,  "Z",  "rocket"}, //rocket
    { S_MAGNUM,   -1, 6, 300, 75, 0,   0,   0, 30, 1, 2048, 200, 0, 0,    0, 0, 1, "#",           "revolver" },
    { S_HANDNADE,  -1, 0, 1000, 150, 0,  300,  0,  5, 1, 1024, 200, 55, 1500, 0, 0, 1,  "@", "gl" },
    //{ S_LASER,     -1, 0, 200,  60, 0,  250,  0,  5, 1, 1024, 200, 30,5000, 3, 0, 1, "<", "pyccna_railgun"},
    { S_MINSTANEX,  -1, 0, 1300,  90, 0,   0,   0, 30, 1, 1024, 300, 25, 0,  0,  0,  1,  "<", "pyccna_railgun"}, //rocketold
    //{ S_CRYLINK,   -1, 0, 100,  30, 0,  1200,  0, 10, 1, 1024, 200, 20,  0,  0, 0, 1, "<", "pyccna_svd"},
    { S_LIGHTNING,  -1, 0, 50, 10,  0,  0,  0,  5,  1,  300,  0,  0,  0,  0,  0,  1, "<",  "pyccna_railgun"}, //pyccna_railgun
    { S_ORB,      -1, 0, 1000, 8000, 0,  350,  0, 30, 1, 1024, 10, 40, 5000, 0, 0, 1, ";", "pulse_rifle"}, //chaing
    { S_SHOTGUNBURST,  -1, 6, 500,  9, 90, 0,   0, 10, 6, 1024, 200, 0, 0,    3, 0, 1, "*", "shotgdefault"}, //shotgdefault
    //{ S_SHOTGUN,   1000,  9, 220, 0,   0, 20, 12,1024, 200, 0, 0,    0, 0, 2, "Shotgun",         "shotgdefault" },
    { S_UZI,       -1, 30, 100,   23,  0,  0,   0, 3,  1, 1024, 150, 0, 0,    0, 0, 1, "&",  "pyccna_m4a1"},
    { S_SMGNADE,   -1, 0, 800,  120, 0,  300,  0, 10, 1, 1024, 200, 40,5000, 0, 0, 1, "@",  "m4a1_grenade"}, //"pistol"
    { S_CROSSBOWFIRE, -1, 0, 1000,300,0, 1000,  0, 30, 1, 1024, 50, 20,10000, 0, 1, 1, "{",       "crossbow"},
    { S_PUNT,     -1, 0, 200,   0, 0,   0,   0, 0,  1,  50, 0,  0,  0,  0,  0,  0, "$",     "electrodriver"},
    { S_TRYGRAB,   -1, 0, 50,   0, 0,   0,   0, 0,  1, 75, 0,  0,  0,  0,  0,  0, "$",     "electrodriver2"},
    { S_PISTOL,    -1, 18, 100, 20, 0,  0,   0, 8,  1, 1024, 150, 0, 0,    0, 0, 1, "%",          "pyccna_fn57" },
    { S_FLAUNCH,   -1, 0, 200,  20, 0,   50,PART_FIREBALL1,1, 1, 1024, 0, 0, 0, 0, 0, 0, "fireball",  NULL },
    { S_ICEBALL,   -1, 0, 200,  40, 0,   30,PART_FIREBALL2,1, 1, 1024, 0, 0, 0, 0, 0, 0,  "iceball",   NULL },
    { S_SLIMEBALL, -1, 0, 200,  30, 0, 160, PART_FIREBALL3,1, 1, 1024, 0, 0, 0, 0, 0, 0, "slimeball", NULL },
    { S_PIGR1,     -1, 0, 250,  150, 0,    0,  0,  1,  1, 1024, 300, 40, 0, 0, 0, 0, "bite",            NULL }, //prop
    { -1,          -1, 0,  0, 150, 0,    0,  0,  0,  1, 1024, 300, 50, 0, 0, 0, 0, "barrel",          NULL } //barrel
};

#include "ai.h"

// inherited by fpsent and server clients
struct fpsstate
{
    int health, maxhealth;
    int armour, armourtype;
    int quadmillis;
    int caughttime;
    int spreelength;
    int dropitem;
    int doshake, lastshake, lastrecoil, lastattackmillis; //amount of spread our weapon had on last fire
    int headshots;
    int detonateelectro;
    int gunselect, gunwait;
    int burstprogress;
    int ammo[NUMGUNS];
    int hasgun[NUMGUNS];
    int diedgun;
    int dropgun;
    int uncrouchtime;
    int sprintleft;
    int strafeleft;
    int flareleft;
    int holdingweapon;
    int beepleft;
    int finaltimer;
    int lastflash;
    int lastreload;
    int lastswitch;
    vec aicatch; //stuff related to ai catching
    bool aicancatch;
    int ailastcatch;
    int walkleft;
    int aitype, skill;
    vec aimpos;
    vec flarepos;
    int isholdingorb;
    //long movable;
    int isholdingnade;
    int isholdingprop;
    int isholdingbarrel;
    int propmodeldir;
    int magprogress[NUMGUNS];



    fpsstate() : maxhealth(100), aitype(AI_NONE), skill(0) {}

    void baseammo(int gun, int k = 2, int scale = 1)
    {
        ammo[gun] = (itemstats[gun-GUN_SG].add*k)/scale;
    }

    void addammo(int gun, int k = 1, int scale = 1)
    {
        itemstat &is = itemstats[gun-GUN_SG];
        ammo[gun] = min(ammo[gun] + (is.add*k)/scale, is.max);
    }

    bool hasmaxammo(int type)
    {
        const itemstat &is = itemstats[type-I_ELECTRO];
        return ammo[type-I_ELECTRO+GUN_SG]>=is.max;
    }

    bool canpickup(int type)
    {
        //        if(type<I_ELECTRO || type>I_QUAD) return false;
        //        itemstat &is = itemstats[type-I_ELECTRO];
        //        switch(type)
        //        {
        //            case I_BOOST: //return maxhealth<is.max;
        //            case I_HEALTH: return health<maxhealth;
        //            case I_GREENARMOUR:
        //                // (100h/100g only absorbs 200 damage)
        //                if(armourtype==A_YELLOW && armour>=100) return false;
        //            case I_YELLOWARMOUR: return !armourtype || armour<is.max;
        //            case I_QUAD: return quadmillis<is.max;
        //            if(!hasgun[is.gunneeded]) return false;//return hasgun[is.info] && ammo[is.info]<is.max;
        //            else return ammo[is.info]<is.max;
        //        }
        if(type<I_ELECTRO || type>I_QUAD) return false;
        itemstat &is = itemstats[type-I_ELECTRO];
        switch(type)
        {
        case I_SMALLHEALTH:
        case I_HEALTH: return health<maxhealth;break;
        case I_BOOST: return health<maxhealth||maxhealth<200; break;
        case I_GREENARMOUR:return armour<60;break;
        case I_SHIELDBATTERY: return armour<100; break;
            // (100h/100g only absorbs 200 damage)
            //if(armourtype==A_YELLOW && armour>=100) return false;break;
        case I_YELLOWARMOUR: return health<maxhealth || armour<200; break;
        case I_QUAD: return quadmillis<is.max; break;
        case I_MAGNUM: return ammo[GUN_MAGNUM]<24; break;
        case I_CROSSBOW: return ammo[GUN_CROSSBOW]<10;break;
        case I_RPG: return ammo[GUN_RL]<15;break;
        case I_ELECTRO: return ammo[GUN_ELECTRO]<15; break;
        case I_MINIGUN: return ammo[GUN_CG]<120;break;
        case I_SHOTGUN: return ammo[GUN_SG]<30;break;
        case I_GRENADE:return ammo[GUN_HANDGRENADE]<5; break;
        case I_PISTOLAMMO:return ammo[GUN_SMG]<150; break;
        case I_SMGAMMO: return ammo[GUN_SMG]<140; break;
        case I_SMGNADE: return ammo[GUN_SMG2]<10;break;
        case I_SHELLS: return ammo[GUN_SG]<30 && hasgun[GUN_SG];break;
        case I_ELECTROAMMO: return ammo[GUN_ELECTRO]<15&& hasgun[GUN_ELECTRO];break;
        case I_ELECTROORBS: return ammo[GUN_ELECTRO2]<80&& hasgun[GUN_ELECTRO];break;
        case I_MINIGUNAMMO: return ammo[GUN_CG]<120&& hasgun[GUN_CG];break;
        case I_TESLAORB: return ammo[GUN_CG2]<3&& hasgun[GUN_CG];break;
        case I_XBOWAMMO: return ammo[GUN_CROSSBOW]<10&& hasgun[GUN_CROSSBOW];break;
        case I_RPGAMMO: return ammo[GUN_RL]<15&& hasgun[GUN_RL];break;
        case I_SNIPERAMMO: return ammo[GUN_MAGNUM]<24&& hasgun[GUN_MAGNUM]; break;

        default: return false; //ammo[is.info]<is.max;
        }
    }

    void pickup(int type)
    {
        if(type<I_ELECTRO || type>I_QUAD) return;
        itemstat &is = itemstats[type-I_ELECTRO];
        switch(type)
        {
        //            case I_BOOST:
        //                //maxhealth = min(maxhealth+is.add, is.max);
        //            case I_HEALTH: // boost also adds to health
        //                health = min(health+is.add, maxhealth);
        //                break;
        //            case I_GREENARMOUR:
        //            case I_YELLOWARMOUR:
        //                armour = min(armour+is.add, is.max);
        //                armourtype = is.info;
        //                break;
        //            case I_QUAD:
        //                quadmillis = min(quadmillis+is.add, is.max);
        //                break;
        //            case I_SHELLS:
        //        case I_SHOTGUN:
        //                ammo[is.info] = min(ammo[GUN_SHOTGUN2]+is.add, is.max); //make needsgun->gunneeded and when you pick up for primary gives you hasgun for secondary
        //                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
        //            default://don't forget special case for shotgun
        //                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
        //                break;
        //            if(is.gunneeded==0)hasgun[is.info]=1;
        case I_BOOST:
            //maxhealth = min(maxhealth+is.add, is.max); //this line raises your maxhealth for each healthboost
            maxhealth = min(maxhealth+10, 200);
            health = maxhealth;
            break;
        case I_HEALTH:
        case I_SMALLHEALTH:
            health = min(health+is.add, maxhealth);
            break;
        case I_GREENARMOUR:
            //case I_YELLOWARMOUR:
        case I_SHIELDBATTERY:
            armourtype = A_YELLOW;
            armour = min(armour+is.add, is.max);
            break;
        case I_YELLOWARMOUR:
            armourtype = A_YELLOW;
            armour = min(armour+2, 200);
            health = min(health+2, maxhealth);
            break;
        case I_QUAD:
            quadmillis = min(quadmillis+is.add, is.max);
            break;
        case I_MAGNUM:
            ammo[GUN_MAGNUM]=min(ammo[GUN_MAGNUM]+12, 24); //check maxammo when changing full ammo cap
            hasgun[GUN_MAGNUM]=1;

            break;
        case I_ELECTRO:
            //ammo[GUN_ELECTRO]=min(ammo[GUN_ELECTRO]+40, 80);
            ammo[GUN_ELECTRO]=min(ammo[GUN_ELECTRO]+8, 15);
            hasgun[GUN_ELECTRO]=1;
            break;
        case I_RPG:
            ammo[GUN_RL]=min(ammo[GUN_RL]+5, 15);
            //ammo[GUN_RL2]=min(ammo[GUN_RL2]+3, 3);
            hasgun[GUN_RL]=1;
            break;
        case I_CROSSBOW:
            ammo[GUN_CROSSBOW]=min(ammo[GUN_CROSSBOW]+5, 10);
            hasgun[GUN_CROSSBOW]=1;
            break;

        case I_SHOTGUN:
            hasgun[GUN_SG]=1;

            ammo[GUN_SG]=min(ammo[GUN_SG]+12, 30);
            ammo[GUN_SHOTGUN2]=min(ammo[GUN_SHOTGUN2]+12, 30);
            break;

        case I_MINIGUN:
            hasgun[GUN_CG]=1;
            ammo[GUN_CG]=min(ammo[GUN_CG]+60, 120);
            //ammo[GUN_CG2]=min(ammo[GUN_CG2]+60, 150);
            break;

        case I_GRENADE:
            ammo[GUN_HANDGRENADE]=min(ammo[GUN_HANDGRENADE]+1, 5);
            break;
        case I_PISTOLAMMO:
            ammo[GUN_PISTOL]=min(ammo[GUN_PISTOL]+24, 150);
            break;
        case I_SMGAMMO:
            ammo[GUN_SMG]=min(ammo[GUN_SMG]+45, 140);
            break;
        case I_SMGNADE:
            ammo[GUN_SMG2]=min(ammo[GUN_SMG2]+1, 10);
            break;
        case I_SHELLS:
            ammo[GUN_SG]=min(ammo[GUN_SG]+12, 30);
            ammo[GUN_SHOTGUN2]=min(ammo[GUN_SHOTGUN2]+12, 30);
            break;
        case I_ELECTROAMMO:
            ammo[GUN_ELECTRO]=min(ammo[GUN_ELECTRO]+10, 15);
            break;
        case I_ELECTROORBS:
            ammo[GUN_ELECTRO2]=min(ammo[GUN_ELECTRO2]+40, 80);
            break;
        case I_MINIGUNAMMO:
            ammo[GUN_CG]=min(ammo[GUN_CG]+60, 120);
            break;
        case I_TESLAORB:
            ammo[GUN_CG2]=min(ammo[GUN_CG2]+1, 3);
            break;
        case I_XBOWAMMO:
            ammo[GUN_CROSSBOW]=min(ammo[GUN_CROSSBOW]+5, 10);
            break;
        case I_RPGAMMO:
            ammo[GUN_RL]=min(ammo[GUN_RL]+4, 15);
            break;
        case I_SNIPERAMMO:
            ammo[GUN_MAGNUM]=min(ammo[GUN_MAGNUM]+6, 24);
            break;


        default:
            return;
            break;

        }
    }

    void respawn()
    {
        health = maxhealth;
        armour = 0;
        spreelength=0;
        armourtype = A_BLUE;
        quadmillis = 0;
        gunselect = GUN_SMG;
        gunwait = 0;
        lastattackmillis=0;
        dropitem=0;
        lastflash=0;
        loopi(NUMGUNS) ammo[i] = 0;
        loopi(NUMGUNS) hasgun[i] = 0;
        ammo[GUN_FIST] = 1;
        ammo[GUN_TELEKENESIS] = 1;
        ammo[GUN_TELEKENESIS2] = 1;
        hasgun[GUN_FIST] = 1;
        hasgun[GUN_TELEKENESIS] = 1;
        hasgun[GUN_TELEKENESIS2] = 1;
        hasgun[GUN_SMG] = 1;
        hasgun[GUN_PISTOL] = 1;
        hasgun[GUN_HANDGRENADE] = 1;
        isholdingorb = 0;
        holdingweapon=1;
        flareleft = 5;
        lastswitch=0;
        sprintleft = 400;
        strafeleft = 150;
        walkleft = 50;
        beepleft = 50;
        isholdingnade = 0;
        isholdingprop = 0;
        isholdingbarrel=0;
        loopi(NUMGUNS)magprogress[i]=0;
        headshots = 0;
    }

    void spawnstate(int gamemode)
    {

        if(m_demo)
        {
            gunselect = GUN_FIST;
        }
        else if(m_insta)
        {
            armour = 0;
            health = 1;
            gunselect = GUN_MAGNUM;
            ammo[GUN_MAGNUM] = 100;
        }
        else if(m_regencapture)
        {
            armourtype = A_GREEN;
            armour = 0;
            gunselect = GUN_PISTOL;
            ammo[GUN_PISTOL] = 36;
            ammo[GUN_HANDGRENADE] = 2;
        }
        else if(m_tactics)
        {
            armourtype = A_GREEN;
            armour = 100;
            ammo[GUN_PISTOL] = 36;
            int spawngun1 = rnd(5)+1, spawngun2;
            gunselect = spawngun1;
            baseammo(spawngun1, m_noitems ? 2 : 1);
            do spawngun2 = rnd(5)+1; while(spawngun1==spawngun2);
            baseammo(spawngun2, m_noitems ? 2 : 1);
            if(m_noitems) ammo[GUN_HANDGRENADE] += 2;
        }
        else if(m_efficiency)
        {
            //armourtype = A_YELLOW;
            //armour = 100;
            //loopi(NUMGUNS) baseammo(i+1);
            gunselect = GUN_SG;
            loopi(NUMGUNS) {
                switch(i) {
                case GUN_FIST:
                case GUN_TELEKENESIS:
                case GUN_TELEKENESIS2:
                    ammo[i] = 1;
                    break;
                case GUN_SMG:
                    ammo[i] = 140;
                    break;
                case GUN_RL:
                    ammo[i] = 15;
                    break;
                case GUN_CG2:
                    ammo[i]=3;
                    break;
                case GUN_SMG2:
                    ammo[i] = 10;
                    break;
                case GUN_MAGNUM:
                    ammo[i] = 24;
                    break;
                case GUN_SG:
                case GUN_SHOTGUN2:
                    ammo[i] = 30;
                    break;
                case GUN_HANDGRENADE:
                    ammo[i] = 5;
                    break;
                case GUN_CG:
                    ammo[i] = 120;
                    break;
                case GUN_ELECTRO2:
                    ammo[i] = 80;
                    break;
                case GUN_ELECTRO:
                    ammo[i] = 15;
                    break;
                case GUN_CROSSBOW:
                    ammo[i] = 10;
                    break;

                case GUN_PISTOL:
                    ammo[i] = 150;
                    break;
                }
            }
        }
        else if(m_ctf)
        {
            armourtype = A_BLUE;
            armour = 50;
            health=maxhealth;
            ammo[GUN_PISTOL] = 36;
            ammo[GUN_SMG]=60;
            ammo[GUN_HANDGRENADE] = 2;
        }
        else
        {
            ammo[GUN_SMG] = 60;
            ammo[GUN_PISTOL] = m_sp ? 80 : 36;
            ammo[GUN_HANDGRENADE] = 2;
        }
    }

    // just subtract damage here, can set death, etc. later in code calling this
    int dodamage(int damage)
    {
        int ad = damage; //*(armourtype+1)*25/100; // let armour absorb when possible //make any armor 100% effective ^_^
        if(ad>armour) ad = armour;
        armour -= ad;
        damage -= ad;
        health -= damage;
        return damage;
    }

    int hasammo(int gun, int exclude = -1)
    {
        return gun >= 0 && gun <= NUMGUNS && gun != exclude && ammo[gun] > 0;
    }
};

struct fpsent : dynent, fpsstate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain;
    int lastaction, lastattackgun;
    bool attacking;
    bool altattacking;
    bool sprinting;
    bool crouching;
    bool didcrouchjump;
    bool reloading;
    int lastyaw;
    int attacksound, attackchan, idlesound, idlechan, basechan, hurtchan;
    int lasttaunt;
    int lastpickup, lastpickupmillis, lastbase, lastrepammo, flagpickup;
    int frags, flags, deaths, totaldamage, totalshots;
    editinfo *edit;
    float deltayaw, deltapitch, newyaw, newpitch;
    int smoothmillis;
    int screenjumpheight;


    string name, team, info;
    int playermodel;
    ai::aiinfo *ai;
    int ownernum, lastnode;

    vec muzzle;

    fpsent() : weight(100), clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0), lifesequence(0), respawned(-1), suicided(-1), lastpain(0), attacksound(-1), attackchan(-1), idlesound(-1), idlechan(-1), frags(0), flags(0), deaths(0), totaldamage(0), totalshots(0), edit(NULL), smoothmillis(-1), playermodel(-1), ai(NULL), ownernum(-1), muzzle(-1, -1, -1)
    {
        name[0] = team[0] = info[0] = 0;
        respawn();
    }
    ~fpsent()
    {
        freeeditinfo(edit);
        if(attackchan >= 0) stopsound(attacksound, attackchan);
        if(idlechan >= 0) stopsound(idlesound, idlechan);
        if(hurtchan >= 0) stopsound(S_HEARTBEAT, hurtchan);
        if(ai) delete ai;
    }

    void hitpush(int damage, const vec &dir, fpsent *actor, int gun)
    {
        vec push(dir);
        push.mul(guns[gun].hitpush*damage/weight);
        //if(gun==GUN_RL || gun==GUN_HANDGRENADE) push.mul(actor==this ? 5 : (type==ENT_AI ? 3 : 2));
        //if(gun==GUN_TELEKENESIS2)push.mul(-500);
        vel.add(push);
    }

    void stopattacksound()
    {
        if(attackchan >= 0) stopsound(attacksound, attackchan, 250);
        attacksound = attackchan = -1;
    }

    void stopidlesound()
    {
        if(idlechan >= 0) stopsound(idlesound, idlechan, 100);
        idlesound = idlechan = -1;
    }

    void stopheartbeat()
    {
        if(hurtchan >= 0) stopsound(S_HEARTBEAT, hurtchan, 2000);
        hurtchan = -1;
    }

    void respawn()
    {
        dynent::reset();
        fpsstate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattackgun = gunselect;
        attacking = false;
        altattacking = false;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        lastbase = lastrepammo = -1;
        flagpickup = 0;
        stopattacksound();
        crouching=0;
        lastnode = -1;
    }
};

struct teamscore
{
    const char *team;
    int score;
    teamscore() {}
    teamscore(const char *s, int n) : team(s), score(n) {}

    static int compare(const teamscore *x, const teamscore *y)
    {
        if(x->score > y->score) return -1;
        if(x->score < y->score) return 1;
        return strcmp(x->team, y->team);
    }
};

namespace entities
{
extern vector<extentity *> ents;

extern const char *entmdlname(int type);
extern const char *itemname(int i);
extern int itemicon(int i);

extern void preloadentities();
extern void renderentities();
extern void resettriggers();
extern void checktriggers();
extern void checkitems(fpsent *d);
extern void checkquad(int time, fpsent *d);
extern void trypickup(int n, fpsent *d);
extern void resetspawns();
extern void spawnitems(bool force = false);
extern void putitems(packetbuf &p);
extern void setspawn(int i, bool on);
extern void teleport(int n, fpsent *d);
extern void pickupeffects(int n, fpsent *d);
extern void teleporteffects(fpsent *d, int tp, int td, bool local = true);
extern void jumppadeffects(fpsent *d, int jp, bool local = true);

extern void repammo(fpsent *d, int type, bool local = true);
}

namespace game
{
struct clientmode
{
    virtual ~clientmode() {}

    virtual void preload() {}
    virtual int clipconsole(int w, int h) { return 0; }
    virtual void drawhud(fpsent *d, int w, int h) {}
    virtual void rendergame() {}
    virtual void respawned(fpsent *d) {}
    virtual void setup() {}
    virtual void checkitems(fpsent *d) {}
    virtual int respawnwait(fpsent *d) { return 0; }
    virtual void pickspawn(fpsent *d) { findplayerspawn(d); }
    virtual void senditems(packetbuf &p) {}
    virtual const char *prefixnextmap() { return ""; }
    virtual void removeplayer(fpsent *d) {}
    virtual void gameover() {}
    virtual bool hidefrags() { return false; }
    virtual int getteamscore(const char *team) { return 0; }
    virtual void getteamscores(vector<teamscore> &scores) {}
    virtual void aifind(fpsent *d, ai::aistate &b, vector<ai::interest> &interests) {}
    virtual bool aicheck(fpsent *d, ai::aistate &b) { return false; }
    virtual bool aidefend(fpsent *d, ai::aistate &b) { return false; }
    virtual bool aipursue(fpsent *d, ai::aistate &b) { return false; }
};

extern clientmode *cmode;
extern void setclientmode();

// fps
extern int gamemode, nextmode;
extern string clientmap;
extern bool intermission;
extern int maptime, maprealtime, maplimit;
extern fpsent *player1;
extern vector<fpsent *> players, clients;
extern int lastspawnattempt;
extern int lasthit;
extern int respawnent;
extern int following;
extern int smoothmove, smoothdist;

extern bool clientoption(const char *arg);
extern fpsent *getclient(int cn);
extern fpsent *newclient(int cn);
extern const char *colorname(fpsent *d, const char *name = NULL, const char *prefix = "");
extern fpsent *pointatplayer();
extern fpsent *hudplayer();
extern fpsent *followingplayer();
extern void stopfollowing();
extern void clientdisconnected(int cn, bool notify = true);
extern void clearclients(bool notify = true);
extern void startgame();
extern void spawnplayer(fpsent *);
extern void deathstate(fpsent *d, bool restore = false);
extern void damaged(int damage, fpsent *d, fpsent *actor, bool local = true);
extern void killed(fpsent *d, fpsent *actor);
extern void timeupdate(int timeremain);
extern void msgsound(int n, physent *d = NULL);
extern void drawicon(int icon, float x, float y, float sz = 120, int w = 0, int h = 0);
const char *mastermodecolor(int n, const char *unknown);
const char *mastermodeicon(int n, const char *unknown);

// client
extern bool connected, remote, demoplayback;
extern string servinfo;

extern int parseplayer(const char *arg);
extern void addmsg(int type, const char *fmt = NULL, ...);
extern void switchname(const char *name);
extern void switchteam(const char *name);
extern void switchplayermodel(int playermodel);
extern void sendmapinfo();
extern void stopdemo();
extern void changemap(const char *name, int mode);
extern void c2sinfo(bool force = false);
extern void sendposition(fpsent *d, bool reliable = false);

// monster
struct monster;
extern vector<monster *> monsters;

extern void clearmonsters();
extern void preloadmonsters();
extern void stackmonster(monster *d, physent *o);
extern void updatemonsters(int curtime);
extern void rendermonsters();
extern void suicidemonster(monster *m);
extern void hitmonster(int damage, monster *m, fpsent *at, const vec &vel, int gun);
extern void monsterkilled();
extern void endsp(bool allkilled);
extern void spsummary(int accuracy);

// movable
struct movable;
extern vector<movable *> movables;

extern void clearmovables();
extern void stackmovable(movable *d, physent *o);
extern void updatemovables(int curtime);
extern void rendermovables();
extern void suicidemovable(movable *m);
extern void hitmovable(int damage, movable *m, fpsent *at, const vec &vel, int gun);

// weapon
extern void shoot(fpsent *d, const vec &targ);
extern void shoteffects(int gun, const vec &from, const vec &to, fpsent *d, bool local, int id, int prevaction);
extern void explode(bool local, fpsent *owner, const vec &v, dynent *safe, int dam, int gun);
extern void explodeeffects(int gun, fpsent *d, bool local, int id = 0);
extern void damageeffect(int damage, fpsent *d, bool thirdperson = true);
extern void gibeffect(int damage, const vec &vel, fpsent *d);
extern void dropweapon(int damage, const vec & vel, fpsent *d);
extern float intersectdist;
extern bool intersect(dynent *d, const vec &from, const vec &to, float &dist = intersectdist);
extern dynent *intersectclosest(const vec &from, const vec &to, fpsent *at, float &dist = intersectdist);
extern void clearbouncers();
extern void updatebouncers(int curtime);
extern void removebouncers(fpsent *owner);
extern void renderbouncers();
extern void clearprojectiles();
extern void updateprojectiles(int curtime, fpsent *d);
extern void removeprojectiles(fpsent *owner);
extern void renderprojectiles();
extern void rendercaughtitems();
extern void preloadbouncers();
extern void removeweapons(fpsent *owner);
extern void updateweapons(int curtime);
extern void gunselect(int gun, fpsent *d);
extern void weaponswitch(fpsent *d);
extern void avoidweapons(ai::avoidset &obstacles, float radius);
extern bool isheadshot(dynent *d, vec to);

// scoreboard
extern void showscores(bool on);
extern void getbestplayers(vector<fpsent *> &best);
extern void getbestteams(vector<const char *> &best);

// render
struct playermodelinfo
{
    const char *ffa, *blueteam, *redteam, *hudguns,
    *vwep, *quad, *armour[3],
    *ffaicon, *blueicon, *redicon;
    bool ragdoll, selectable;
};

extern int playermodel, teamskins, testteam;

extern void saveragdoll(fpsent *d);
extern void clearragdolls();
extern void moveragdolls();
extern void changedplayermodel();
extern const playermodelinfo &getplayermodelinfo(fpsent *d);
extern int chooserandomplayermodel(int seed);
extern void swayhudgun(int curtime);
extern vec hudgunorigin(int gun, const vec &from, const vec &to, fpsent *d);
}

namespace server
{
extern const char *modename(int n, const char *unknown = "unknown");
extern const char *mastermodename(int n, const char *unknown = "unknown");
extern void startintermission();
extern void stopdemo();
extern void forcemap(const char *map, int mode);
extern void forcepaused(bool paused);
extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
extern int msgsizelookup(int msg);
extern bool serveroption(const char *arg);
extern bool delayspawn(int type);
}

#endif

