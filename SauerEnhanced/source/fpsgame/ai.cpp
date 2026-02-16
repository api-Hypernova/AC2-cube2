#include "game.h"

extern int fog;

namespace ai
{
using namespace game;

avoidset obstacles;
int updatemillis = 0, updateiteration = 0, forcegun = -1;
vec aitarget(0, 0, 0);

VAR(aidebug, 0, 0, 6);
VAR(aiforcegun, -1, -1, NUMGUNS-1);

ICOMMAND(addbot, "s", (char *s), addmsg(N_ADDBOT, "ri", *s ? clamp(parseint(s), 1, 101) : -1));
ICOMMAND(delbot, "", (), addmsg(N_DELBOT, "r"));
ICOMMAND(botlimit, "i", (int *n), addmsg(N_BOTLIMIT, "ri", *n));
ICOMMAND(botbalance, "i", (int *n), addmsg(N_BOTBALANCE, "ri", *n));

// Forward declarations for new AI functions
void updateenemytracking(fpsent *d);
fpsent *selectbesttarget(fpsent *d);
bool findcover(fpsent *d, aistate &b, const vec &threat);
vec predictposition(fpsent *e, float projspeed);
bool isreachable(fpsent *d, const vec &pos, int timeout);
void updatecombatmovement(fpsent *d, aistate &b);
bool shouldretreat(fpsent *d);
void initpersonality(fpsent *d);
float getweaponrange(int gun, bool optimal);
bool shouldburstfire(fpsent *d, int gun);
void findteammates(fpsent *d, vector<fpsent *> &teammates, float maxdist);
void updatesquad(fpsent *d);
void coordinatesquad(fpsent *d);
void maintainspacing(fpsent *d, aistate &b);
void applyweapontactics(fpsent *d, fpsent *e, aistate &b);
bool manageengagementrange(fpsent *d, fpsent *e, aistate &b);
void updateenvironmentalawareness(fpsent *d);
bool useenvironmentknowledge(fpsent *d, aistate &b);

float viewdist(int x)
{
    return x <= 100 ? clamp((SIGHTMIN+(SIGHTMAX-SIGHTMIN))/100.f*float(x), float(SIGHTMIN), float(fog)) : float(fog);
}

float viewfieldx(int x)
{
    return x <= 100 ? clamp((VIEWMIN+(VIEWMAX-VIEWMIN))/100.f*float(x), float(VIEWMIN), float(VIEWMAX)) : float(VIEWMAX);
}

float viewfieldy(int x)
{
    return viewfieldx(x)*3.f/4.f;
}

bool canmove(fpsent *d)
{
    return d->state != CS_DEAD && !intermission;
}

float weapmindist(int weap)
{
    return guns[weap].projspeed ? guns[weap].splash : 2;
}

float weapmaxdist(int weap)
{
    return guns[weap].range + 4;
}

bool weaprange(fpsent *d, int weap, float dist)
{
    float mindist = weapmindist(weap), maxdist = weapmaxdist(weap);
    return dist >= mindist*mindist && dist <= maxdist*maxdist;
}

bool targetable(fpsent *d, fpsent *e)
{
    if(d == e || !canmove(d)) return false;
    return e->state == CS_ALIVE && !isteam(d->team, e->team);
}

bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy)
{
    float dist = o.dist(q);

    if(dist <= mdist)
    {
        float x = fmod(fabs(asin((q.z-o.z)/dist)/RAD-pitch), 360);
        float y = fmod(fabs(-atan2(q.x-o.x, q.y-o.y)/RAD-yaw), 360);
        if(min(x, 360-x) <= fovx && min(y, 360-y) <= fovy) return raycubelos(o, q, v);
    }
    return false;
}

bool cansee(fpsent *d, vec &x, vec &y, vec &targ)
{
    aistate &b = d->ai->getstate();
    if(canmove(d) && b.type != AI_S_WAIT)
        return getsight(x, d->yaw, d->pitch, y, targ, d->ai->views[2], d->ai->views[0], d->ai->views[1]);
    return false;
}

bool canshoot(fpsent *d, fpsent *e)
{
    if(weaprange(d, d->gunselect, e->o.squaredist(d->o)) && targetable(d, e))
        return d->ammo[d->gunselect] > 0 && lastmillis - d->lastaction >= d->gunwait;
    return false;
}

bool canshoot(fpsent *d)
{
    return !d->ai->becareful && d->ammo[d->gunselect] > 0 && lastmillis - d->lastaction >= d->gunwait;
}

bool hastarget(fpsent *d, aistate &b, fpsent *e, float yaw, float pitch, float dist)
{ // add margins of error
    if(weaprange(d, d->gunselect, dist) || (d->skill <= 100 && !rnd(d->skill)))
    {
        if(d->gunselect == GUN_FIST) return true;
        float skew = clamp(float(lastmillis-d->ai->enemymillis)/float((d->skill*guns[d->gunselect].attackdelay/200.f)), 0.f, guns[d->gunselect].projspeed ? 0.25f : 1e16f),
                offy = yaw-d->yaw, offp = pitch-d->pitch;
        if(offy > 180) offy -= 360;
        else if(offy < -180) offy += 360;
        if(fabs(offy) <= d->ai->views[0]*skew && fabs(offp) <= d->ai->views[1]*skew) return true;
    }
    return false;
}

vec getaimpos(fpsent *d, fpsent *e)
{
    vec o = e->headpos();
    
    // Weapon-specific aim points
    if(d->gunselect == GUN_RL) o.z += (e->aboveeye*0.2f)-(0.8f*d->eyeheight);
    else if(d->gunselect != GUN_HANDGRENADE) o.z += (e->aboveeye-e->eyeheight)*0.5f;
    
    if(d->aicancatch && lastmillis-d->ailastcatch>500) { o=e->headpos();}
    
    // Predictive aiming for skilled bots with projectile weapons
    if(d->skill > 40 && guns[d->gunselect].projspeed > 0)
    {
        vec predicted = predictposition(e, guns[d->gunselect].projspeed);
        
        // Blend between current and predicted based on skill
        float blend = clamp((d->skill - 40) / 60.f, 0.f, 1.f);
        o = vec(o).mul(1.f - blend).add(vec(predicted).mul(blend));
    }
    
    // Add skill-based inaccuracy
    if(d->skill <= 100)
    {
        if(lastmillis >= d->ai->lastaimrnd)
        {
            const int aiskew[NUMGUNS] = { 1, 10, 50, 5, 20, 1, 100, 10, 10, 10, 1, 1 };
#define rndaioffset(r) ((rnd(int(r*aiskew[d->gunselect]*2)+1)-(r*aiskew[d->gunselect]))*(1.f/float(max(d->skill, 1))))
            
            // Reduce error for high-skill bots
            float errormod = d->skill > 70 ? 0.5f : 1.0f;
            loopk(3) d->ai->aimrnd[k] = rndaioffset(e->radius) * errormod;
            
            int dur = (d->skill+10)*10;
            d->ai->lastaimrnd = lastmillis+dur+rnd(dur);
        }
        loopk(3) o[k] += d->ai->aimrnd[k];
    }
    
    return o;
}

void create(fpsent *d)
{
    if(!d->ai) d->ai = new aiinfo;
}

void destroy(fpsent *d)
{
    if(d->ai) DELETEP(d->ai);
}

void init(fpsent *d, int at, int ocn, int sk, int bn, int pm, const char *name, const char *team)
{
    loadwaypoints();

    fpsent *o = newclient(ocn);

    d->aitype = at;

    bool resetthisguy = false;
    if(!d->name[0])
    {
        if(aidebug) conoutf("%s assigned to %s at skill %d", colorname(d, name), o ? colorname(o) : "?", sk);
        else conoutf("connected: %s", colorname(d, name));
        resetthisguy = true;
    }
    else
    {
        if(d->ownernum != ocn)
        {
            if(aidebug) conoutf("%s reassigned to %s", colorname(d, name), o ? colorname(o) : "?");
            resetthisguy = true;
        }
        if(d->skill != sk && aidebug) conoutf("%s changed skill to %d", colorname(d, name), sk);
    }

    copystring(d->name, name, MAXNAMELEN+1);
    copystring(d->team, team, MAXTEAMLEN+1);
    d->ownernum = ocn;
    d->skill = sk;
    d->playermodel = chooserandomplayermodel(pm);

    if(resetthisguy) removeweapons(d);
    if(d->ownernum >= 0 && player1->clientnum == d->ownernum)
    {
        create(d);
        if(d->ai)
        {
            d->ai->views[0] = viewfieldx(d->skill);
            d->ai->views[1] = viewfieldy(d->skill);
            d->ai->views[2] = viewdist(d->skill);
        }
    }
    else if(d->ai) destroy(d);
}

void doreloadai(fpsent *d)
{
    //if(lastmillis-d->lastreload<2000 || !d->magprogress[d->gunselect] || lastmillis-d->lastaction<guns[d->lastattackgun].attackdelay) {setvar("tryreload", 0); return; }
    d->gunwait=2000;
    d->magprogress[d->gunselect]=0;
    if(d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2) { d->magprogress[GUN_SHOTGUN2]=0; d->magprogress[GUN_SG]=0; }
    msgsound(S_RELOAD, d);
    d->burstprogress=0;
    d->lastreload=lastmillis;
    //if(d==player1)setvar("tryreload", 0);
}

void update()
{
    if(intermission) { loopv(players) if(players[i]->ai) players[i]->stopmoving(); }
    else // Optimized update system with variable rates
    {
        if(!updateiteration && lastmillis-updatemillis > 1000)
        {
            avoid();
            forcegun = multiplayer(false) ? -1 : aiforcegun;
            updateiteration = 1;
            updatemillis = lastmillis;
        }
        
        int count = 0;
        loopv(players) if(players[i]->ai)
        {
            fpsent *d = players[i];
            
            // Simplified: always do full updates for now to ensure enemy detection works
            // The original game did 1 update per second which was sufficient
            bool needsFullUpdate = ++count == updateiteration;
            
            think(d, needsFullUpdate);
            
            // Auto-reload
            if(d->magprogress[d->gunselect] >= getmagsize(d->gunselect, d->mods) && getmagsize(d->gunselect, d->mods) > 1)
                doreloadai(d);
        }
        if(++updateiteration > count) updateiteration = 0;
    }
}

bool checkothers(vector<int> &targets, fpsent *d, int state, int targtype, int target, bool teams)
{ // checks the states of other ai for a match
    targets.setsize(0);
    loopv(players)
    {
        fpsent *e = players[i];
        if(e == d || !e->ai || e->state != CS_ALIVE) continue;
        if(targets.find(e->clientnum) >= 0) continue;
        if(teams && d && !isteam(d->team, e->team)) continue;
        aistate &b = e->ai->getstate();
        if(state >= 0 && b.type != state) continue;
        if(target >= 0 && b.target != target) continue;
        if(targtype >=0 && b.targtype != targtype) continue;
        targets.add(e->clientnum);
    }
    return !targets.empty();
}

bool makeroute(fpsent *d, aistate &b, int node, bool changed, bool retry)
{
    if(!waypoints.inrange(d->lastnode)) return false;
    if(changed && d->ai->route.length() > 1 && d->ai->route[0] == node) return true;
    if(route(d, d->lastnode, node, d->ai->route, obstacles, retry))
    {
        b.override = false;
        return true;
    }
    d->ai->clear(true);
    if(!retry) return makeroute(d, b, node, false, true);
    return false;
}

bool makeroute(fpsent *d, aistate &b, const vec &pos, bool changed, bool retry)
{
    int node = closestwaypoint(pos, SIGHTMIN, true);
    return makeroute(d, b, node, changed, retry);
}

bool randomnode(fpsent *d, aistate &b, const vec &pos, float guard, float wander)
{
    static vector<int> candidates;
    candidates.setsize(0);
    findwaypointswithin(pos, guard, wander, candidates);

    while(!candidates.empty())
    {
        int w = rnd(candidates.length()), n = candidates.removeunordered(w);
        if(n != d->lastnode && !d->ai->hasprevnode(n) && !obstacles.find(n, d) && makeroute(d, b, n)) return true;
    }
    return false;
}

bool randomnode(fpsent *d, aistate &b, float guard, float wander)
{
    return randomnode(d, b, d->feetpos(), guard, wander);
}

bool badhealth(fpsent *d)
{
    if(d->skill <= 100) return d->health <= (111-d->skill)/4;
    return false;
}

bool enemy(fpsent *d, aistate &b, const vec &pos, float guard = SIGHTMIN, bool pursue = false)
{
    fpsent *t = NULL;
    vec dp = d->headpos();
    float mindist = guard*guard, bestdist = 1e16f;
    loopv(players)
    {
        fpsent *e = players[i];
        if(e == d || !targetable(d, e)) continue;
        vec ep = getaimpos(d, e);
        float dist = ep.squaredist(dp);
        if(dist < bestdist && (cansee(d, dp, ep) || dist <= mindist))
        {
            t = e;
            bestdist = dist;
        }
    }
    if(t && violence(d, b, t, pursue)) return true;
    return false;
}

bool patrol(fpsent *d, aistate &b, const vec &pos, float guard, float wander, int walk, bool retry)
{
    vec feet = d->feetpos();
    if(walk == 2 || b.override || (walk && feet.squaredist(pos) <= guard*guard) || !makeroute(d, b, pos))
    { // run away and back to keep ourselves busy
        if(!b.override && randomnode(d, b, pos, guard, wander))
        {
            b.override = true;
            return true;
        }
        else if(d->ai->route.empty())
        {
            if(!retry)
            {
                b.override = false;
                return patrol(d, b, pos, guard, wander, walk, true);
            }
            b.override = false;
            return false;
        }
    }
    b.override = false;
    return true;
}

bool defend(fpsent *d, aistate &b, const vec &pos, float guard, float wander, int walk)
{
    bool hasenemy = enemy(d, b, pos, wander, d->gunselect == GUN_FIST);
    if(!walk)
    {
        if(d->feetpos().squaredist(pos) <= guard*guard)
        {
            b.idle = hasenemy ? 2 : 1;
            return true;
        }
        walk++;
    }
    return patrol(d, b, pos, guard, wander, walk);
}

bool violence(fpsent *d, aistate &b, fpsent *e, bool pursue)
{
    if(e && targetable(d, e))
    {
        if(pursue && waypoints.inrange(d->lastnode)) d->ai->switchstate(b, AI_S_PURSUE, AI_T_PLAYER, e->clientnum);
        if(d->ai->enemy != e->clientnum)
        {
            d->ai->enemyseen = d->ai->enemymillis = lastmillis;
            d->ai->enemy = e->clientnum;
        }
        return true;
    }
    return false;
}

bool target(fpsent *d, aistate &b, bool pursue = false, bool force = false, float mindist = 0.f)
{
    // Use original targeting - it works!
    fpsent *t = NULL;
    vec dp = d->headpos(), tp(0, 0, 0);
    loopv(players)
    {
        fpsent *e = players[i];
        if(e == d || !targetable(d, e)) continue;
        vec ep = getaimpos(d, e);
        float dist = ep.squaredist(dp);
        if((!t || dist < tp.squaredist(dp)) && ((mindist > 0 && dist <= mindist) || force || cansee(d, dp, ep)))
        {
            t = e;
            tp = ep;
        }
    }
    if(t) return violence(d, b, t, pursue);
    return false;
}

int isgoodammo(int gun) { return gun >= GUN_SG && gun <= GUN_HANDGRENADE; }

bool hasgoodammo(fpsent *d)
{
    static const int goodguns[] = { GUN_CG2, GUN_RL, GUN_SG, GUN_MAGNUM, GUN_SMG2 };
    loopi(sizeof(goodguns)/sizeof(goodguns[0])) if(d->hasammo(goodguns[0])) return true;
    if(d->ammo[GUN_HANDGRENADE] > 5) return true;
    return false;
}

void assist(fpsent *d, aistate &b, vector<interest> &interests, bool all, bool force)
{
    loopv(players)
    {
        fpsent *e = players[i];
        if(e == d || (!all && e->aitype != AI_NONE) || !isteam(d->team, e->team)) continue;
        interest &n = interests.add();
        n.state = AI_S_DEFEND;
        n.node = e->lastnode;
        n.target = e->clientnum;
        n.targtype = AI_T_PLAYER;
        n.score = e->o.squaredist(d->o)/(hasgoodammo(d) ? 1e8f : (force ? 1e4f : 1e2f));
    }
}

static void tryitem(fpsent *d, extentity &e, int id, aistate &b, vector<interest> &interests, bool force = false)
{
    // Don't try items we recently failed to reach
    if(d->ai->faileditemid == id && lastmillis - d->ai->faileditemtime < 10000)
        return;
    
    // Check if item is reachable before considering it
    if(!force && !isreachable(d, e.o, 5000))
        return;
    
    float score = 0;
    float basescore = 0;
    
    // Check if bot just spawned (within last 5 seconds) and needs weapons
    bool justSpawned = (lastmillis - d->lastpain) < 5000;
    int weaponCount = 0;
    loopi(NUMGUNS) if(d->hasgun[i]) weaponCount++;
    bool needsWeapons = weaponCount <= 3; // Only has starting weapons (pistol, SMG, grenades)
    
    switch(e.type)
    {
    case I_HEALTH: case I_SMALLHEALTH:
            if(d->canpickup(e.type))
            {
                basescore = 1e10f;
                // Higher priority if low health
                if(badhealth(d)) basescore *= 2.0f;
                score = basescore;
            }
        break;
    case I_QUAD: 
        basescore = 1e10f;
        // Aggressive personalities prioritize powerups
        if(d->ai->personality == AI_PERSONALITY_AGGRESSIVE) basescore *= 1.5f;
        score = basescore;
        break;
    case I_BOOST:
    case I_GREENARMOUR: case I_YELLOWARMOUR: case I_SHIELDBATTERY:
        if(d->canpickup(e.type))
        {
            basescore = 1e11f; // Equal to weapons after initial spawn
            // Defensive personalities prioritize armor
            if(d->ai->personality == AI_PERSONALITY_DEFENSIVE) basescore *= 1.3f;
            score = basescore;
        }
        break;
    case I_ELECTRO:
    case I_SHOTGUN:
    case I_MINIGUN:
    case I_MAGNUM:
    case I_TESLAORB:
    case I_SMGNADE:
    case I_RPG:
    case I_CROSSBOW:
        if(d->canpickup(e.type))
        {
            itemstat &is = itemstats[e.type-I_ELECTRO];
            
            // FIRST PRIORITY: Get ANY weapon if just spawned
            if(justSpawned && needsWeapons)
            {
                basescore = 1e13f; // HIGHEST priority - get first weapon
                // Still slightly prefer non-laser weapons
                if(is.info != GUN_ELECTRO && is.info != GUN_ELECTRO2)
                {
                    basescore *= 2.0f;
                }
            }
            // After initial weapons, equal priority to shield batteries
            else
            {
                basescore = 1e11f; // Same as shield batteries
                
                // Still de-prioritize laser weapons (mobility tool)
                if(is.info == GUN_ELECTRO || is.info == GUN_ELECTRO2)
                {
                    basescore *= 0.3f; // Lower but not completely avoid
                }
                // Slightly prefer good combat weapons
                else if(is.info == GUN_SMG || is.info == GUN_SMG2 || is.info == GUN_CG || is.info == GUN_CG2 ||
                        is.info == GUN_RL || is.info == GUN_MAGNUM || is.info == GUN_SG)
                {
                    basescore *= 1.2f; // Slight preference
                }
            }
            
            score = basescore;
        }
        break;

    default:
        break;
    }
    
    if(score != 0)
    {
        // Reduce priority if in combat, but not for initial weapon collection
        if(d->ai->incombat && d->ai->enemy >= 0 && !justSpawned)
        {
            // Weapons and shield batteries: minor reduction
            if(e.type >= I_ELECTRO && e.type <= I_CROSSBOW)
                score *= 0.8f; // Only 20% reduction for weapons
            else if(e.type == I_SHIELDBATTERY || e.type == I_GREENARMOUR || e.type == I_YELLOWARMOUR)
                score *= 0.8f; // Equal to weapons
            else
                score *= 0.5f; // More reduction for health/powerups
        }
        
        interest &n = interests.add();
        n.state = AI_S_INTEREST;
        n.node = closestwaypoint(e.o, SIGHTMIN, true);
        n.target = id;
        n.targtype = AI_T_ENTITY;
        n.score = d->feetpos().squaredist(e.o)/(force ? -1 : score);
    }
}

void items(fpsent *d, aistate &b, vector<interest> &interests, bool force = false)
{
    loopv(entities::ents)
    {
        extentity &e = *(extentity *)entities::ents[i];
        if(!e.spawned || !d->canpickup(e.type)) continue;
        tryitem(d, e, i, b, interests, force);
    }
}

static vector<int> targets;

bool parseinterests(fpsent *d, aistate &b, vector<interest> &interests, bool override, bool ignore)
{
    while(!interests.empty())
    {
        int q = interests.length()-1;
        loopi(interests.length()-1) if(interests[i].score < interests[q].score) q = i;
        interest n = interests.removeunordered(q);
        bool proceed = true;
        if(!ignore) switch(n.state)
        {
        case AI_S_DEFEND: // don't get into herds
            proceed = !checkothers(targets, d, n.state, n.targtype, n.target, true);
            break;
        default: break;
        }
        if(proceed && makeroute(d, b, n.node, false))
        {
            d->ai->switchstate(b, n.state, n.targtype, n.target);
            return true;
        }
    }
    return false;
}

bool find(fpsent *d, aistate &b, bool override = false)
{
    static vector<interest> interests;
    interests.setsize(0);
    if(!m_noitems)
    {
        if((!m_noammo && !hasgoodammo(d)) || d->health < min(d->skill - 15, 75))
            items(d, b, interests);
        else
        {
            static vector<int> nearby;
            nearby.setsize(0);
            findents(I_ELECTRO, I_QUAD, false, d->feetpos(), vec(32, 32, 24), nearby);
            loopv(nearby)
            {
                int id = nearby[i];
                extentity &e = *(extentity *)entities::ents[id];
                if(d->canpickup(e.type)) tryitem(d, e, id, b, interests);
            }
        }
    }
    if(cmode) cmode->aifind(d, b, interests);
    if(m_teammode) assist(d, b, interests);
    return parseinterests(d, b, interests, override);
}

bool findassist(fpsent *d, aistate &b, bool override = false)
{
    static vector<interest> interests;
    interests.setsize(0);
    assist(d, b, interests);
    while(!interests.empty())
    {
        int q = interests.length()-1;
        loopi(interests.length()-1) if(interests[i].score < interests[q].score) q = i;
        interest n = interests.removeunordered(q);
        bool proceed = true;
        switch(n.state)
        {
        case AI_S_DEFEND: // don't get into herds
            proceed = !checkothers(targets, d, n.state, n.targtype, n.target, true);
            break;
        default: break;
        }
        if(proceed && makeroute(d, b, n.node, false))
        {
            d->ai->switchstate(b, n.state, n.targtype, n.target);
            return true;
        }
    }
    return false;
}

void damaged(fpsent *d, fpsent *e)
{
    if(d->ai && canmove(d) && e)
    {
        // Track damage location and time
        d->ai->lastdamagepos = e->o;
        d->ai->lastdamagetime = lastmillis;
        
        // Update enemy tracking - mark this enemy as high threat
        loopv(d->ai->enemylist)
        {
            if(d->ai->enemylist[i].clientnum == e->clientnum)
            {
                d->ai->enemylist[i].lastdamaged = lastmillis;
                d->ai->enemylist[i].threat = min(d->ai->enemylist[i].threat + 0.3f, 1.f);
                break;
            }
        }
        
        if(targetable(d, e))
        {
            aistate &b = d->ai->getstate();
            if(violence(d, b, e, d->gunselect == GUN_FIST)) return;
        }
    }
    if(checkothers(targets, d, AI_S_DEFEND, AI_T_PLAYER, d->clientnum, true))
    {
        loopv(targets)
        {
            fpsent *t = getclient(targets[i]);
            if(!t->ai || !canmove(t) || !targetable(t, e)) continue;
            aistate &c = t->ai->getstate();
            if(violence(t, c, e, d->gunselect == GUN_FIST)) return;
        }
    }
}

void findorientation(vec &o, float yaw, float pitch, vec &pos)
{
    vec dir;
    vecfromyawpitch(yaw, pitch, 1, 0, dir);
    if(raycubepos(o, dir, pos, 0, RAY_CLIPMAT|RAY_SKIPFIRST) == -1)
        pos = dir.mul(2*getworldsize()).add(o); //otherwise 3dgui won't work when outside of map
}

void setup(fpsent *d, bool tryreset = false)
{
    d->ai->clearsetup();
    d->ai->reset(tryreset);
    d->ai->lastrun = lastmillis;
    
    // Initialize personality
    initpersonality(d);
    
    if(m_insta) d->ai->weappref = GUN_MAGNUM;
    else
    {
        if(forcegun >= 0 && forcegun < NUMGUNS) d->ai->weappref = forcegun;
        else if(m_noammo) d->ai->weappref = -1;
        else
        {
            // Personality affects weapon preference - prefer SMG/assault rifles
            switch(d->ai->personality)
            {
                case AI_PERSONALITY_AGGRESSIVE:
                    // Aggressive bots prefer close-range power: SMG, Shotgun, Rockets
                    { int choices[] = {GUN_SMG2, GUN_SG, GUN_RL, GUN_SMG};
                      d->ai->weappref = choices[rnd(4)]; }
                    break;
                case AI_PERSONALITY_DEFENSIVE:
                    // Defensive bots prefer ranged accuracy: Chaingun, Magnum, SMG
                    { int choices[] = {GUN_CG2, GUN_MAGNUM, GUN_SMG, GUN_CG};
                      d->ai->weappref = choices[rnd(4)]; }
                    break;
                case AI_PERSONALITY_TACTICAL:
                    // Tactical bots are versatile: SMG, Magnum, Rockets
                    { int choices[] = {GUN_SMG2, GUN_MAGNUM, GUN_RL, GUN_CG2};
                      d->ai->weappref = choices[rnd(4)]; }
                    break;
                case AI_PERSONALITY_SUPPORT:
                    // Support bots prefer reliable mid-range: SMG, Chaingun
                    { int choices[] = {GUN_SMG, GUN_SMG2, GUN_CG, GUN_CG2};
                      d->ai->weappref = choices[rnd(4)]; }
                    break;
                default:
                    // Balanced - prefer SMG as default
                    d->ai->weappref = rnd(2) ? GUN_SMG : GUN_CG;
                    break;
            }
        }
    }
    vec dp = d->headpos();
    findorientation(dp, d->yaw, d->pitch, d->ai->target);
}

void spawned(fpsent *d)
{
    if(d->ai) setup(d, false);
}

void killed(fpsent *d, fpsent *e)
{
    if(d->ai) d->ai->reset();
}

void itemspawned(int ent)
{
    if(entities::ents.inrange(ent) && entities::ents[ent]->type >= I_ELECTRO && entities::ents[ent]->type <= I_QUAD)
    {
        loopv(players) if(players[i] && players[i]->ai && players[i]->aitype == AI_BOT && players[i]->canpickup(entities::ents[ent]->type))
        {
            fpsent *d = players[i];
            bool wantsitem = false;
            switch(entities::ents[ent]->type)
            {
            case I_BOOST: case I_HEALTH: wantsitem = badhealth(d); break;
            case I_GREENARMOUR: case I_YELLOWARMOUR: case I_QUAD: break;
            default:
            {
                itemstat &is = itemstats[entities::ents[ent]->type-I_ELECTRO];
                wantsitem = isgoodammo(is.info) && d->ammo[is.info] <= (d->ai->weappref == is.info ? is.add : is.add/2);
                break;
            }
            }
            if(wantsitem)
            {
                aistate &b = d->ai->getstate();
                if(b.type == AI_S_PURSUE && b.targtype == AI_T_AFFINITY) continue;
                if(b.type == AI_S_INTEREST && b.targtype == AI_T_ENTITY)
                {
                    if(entities::ents.inrange(b.target))
                    {
                        if(d->o.squaredist(entities::ents[ent]->o) < d->o.squaredist(entities::ents[b.target]->o))
                            d->ai->switchstate(b, AI_S_INTEREST, AI_T_ENTITY, ent);
                    }
                    continue;
                }
                d->ai->switchstate(b, AI_S_INTEREST, AI_T_ENTITY, ent);
            }
        }
    }
}

bool check(fpsent *d, aistate &b)
{
    if(cmode && cmode->aicheck(d, b)) return true;
    return false;
}

int dowait(fpsent *d, aistate &b)
{
    if(check(d, b) || find(d, b)) return 1;
    if(target(d, b, true, true)) return 1;
    if(randomnode(d, b, SIGHTMIN, 1e16f))
    {
        d->ai->switchstate(b, AI_S_INTEREST, AI_T_NODE, d->ai->route[0]);
        return 1;
    }
    return 0; // but don't pop the state
}

int dodefend(fpsent *d, aistate &b)
{
    if(d->state == CS_ALIVE)
    {
        switch(b.targtype)
        {
        case AI_T_NODE:
            if(check(d, b)) return 1;
            if(waypoints.inrange(b.target)) return defend(d, b, waypoints[b.target].o) ? 1 : 0;
            break;
        case AI_T_ENTITY:
            if(check(d, b)) return 1;
            if(entities::ents.inrange(b.target)) return defend(d, b, entities::ents[b.target]->o) ? 1 : 0;
            break;
        case AI_T_AFFINITY:
            if(cmode) return cmode->aidefend(d, b) ? 1 : 0;
            break;
        case AI_T_PLAYER:
        {
            if(check(d, b)) return 1;
            fpsent *e = getclient(b.target);
            if(e && e->state == CS_ALIVE) return defend(d, b, e->feetpos()) ? 1 : 0;
            break;
        }
        default: break;
        }
    }
    return 0;
}

int dointerest(fpsent *d, aistate &b)
{
    if(d->state != CS_ALIVE) return 0;
    switch(b.targtype)
    {
    case AI_T_NODE:
        if(check(d, b)) return 1;
        if(d->lastnode != b.target && waypoints.inrange(b.target))
            return makeroute(d, b, b.target) ? 1 : 0;
        break;
    case AI_T_ENTITY:
        if(entities::ents.inrange(b.target))
        {
            extentity &e = *(extentity *)entities::ents[b.target];
            if(!e.spawned || e.type < I_ELECTRO || e.type > I_GRENADE || d->hasmaxammo(e.type)) return 0;
            
            float dist = d->feetpos().squaredist(e.o);
            if(dist <= CLOSEDIST*CLOSEDIST)
            {
                b.idle = 1;
                // Successfully reached item - clear failure tracking
                if(d->ai->faileditemid == b.target)
                {
                    d->ai->faileditemid = -1;
                    d->ai->faileditemtime = 0;
                }
                return true;
            }
            
            // Check if we're stuck trying to reach this item
            if(lastmillis - b.millis > 8000) // been trying for 8 seconds
            {
                // Mark this item as failed
                d->ai->faileditemid = b.target;
                d->ai->faileditemtime = lastmillis;
                return 0;
            }
            
            return makeroute(d, b, e.o) ? 1 : 0;
        }
        break;
    }
    return 0;
}

int dopursue(fpsent *d, aistate &b)
{
    if(d->state == CS_ALIVE)
    {
        switch(b.targtype)
        {
        case AI_T_AFFINITY:
        {
            if(cmode) return cmode->aipursue(d, b) ? 1 : 0;
            break;
        }

        case AI_T_PLAYER:
        {
            //if(check(d, b)) return 1;
            fpsent *e = getclient(b.target);
            if(e && e->state == CS_ALIVE)
            {
                float guard = SIGHTMIN, wander = guns[d->gunselect].range;
                if(d->gunselect == GUN_FIST) guard = 0.f;
                return patrol(d, b, e->feetpos(), guard, wander) ? 1 : 0;
            }
            break;
        }
        default: break;
        }
    }
    return 0;
}

int closenode(fpsent *d, bool retry = false)
{
    vec pos = d->feetpos();
    int node = -1;
    float mindist = SIGHTMIN*SIGHTMIN;
    loopv(d->ai->route) if(waypoints.inrange(d->ai->route[i]))
    {
        waypoint &w = waypoints[d->ai->route[i]];
        vec wpos = w.o;
        int id = obstacles.remap(d, d->ai->route[i], wpos, retry);
        if(waypoints.inrange(id) && (retry || id == d->ai->route[i] || !d->ai->hasprevnode(id)))
        {
            float dist = wpos.squaredist(pos);
            if(dist < mindist)
            {
                node = id;
                mindist = dist;
            }
        }
    }
    return node;
}

bool wpspot(fpsent *d, int n, bool retry = false)
{
    if(waypoints.inrange(n))
    {
        waypoint &w = waypoints[n];
        vec wpos = w.o;
        int id = obstacles.remap(d, n, wpos, retry);
        if(waypoints.inrange(id) && (retry || id == n || !d->ai->hasprevnode(id)))
        {
            d->ai->spot = wpos;
            d->ai->targnode = n;
            return true;
        }
    }
    return false;
}

bool anynode(fpsent *d, aistate &b, bool retry = false)
{
    if(!waypoints.inrange(d->lastnode)) return false;
    waypoint &w = waypoints[d->lastnode];
    static vector<int> anyremap; anyremap.setsize(0);
    if(w.links[0])
    {
        loopi(MAXWAYPOINTLINKS)
        {
            int link = w.links[i];
            if(!link) break;
            if(waypoints.inrange(link) && (retry || !d->ai->hasprevnode(link)))
                anyremap.add(link);
        }
    }
    while(!anyremap.empty())
    {
        int r = rnd(anyremap.length()), t = anyremap[r];
        if(wpspot(d, t, retry))
        {
            d->ai->route.add(t);
            d->ai->route.add(d->lastnode);
            return true;
        }
        anyremap.remove(r);
    }
    if(!retry) return anynode(d, b, true);
    return false;
}

bool hunt(fpsent *d, aistate &b, int retries = 0)
{
    if(!d->ai->route.empty() && waypoints.inrange(d->lastnode))
    {
        int n = retries%2 ? d->ai->route.find(d->lastnode) : closenode(d, retries >= 2);
        if(retries%2 && d->ai->route.inrange(n))
        {
            while(d->ai->route.length() > n+1) d->ai->route.pop(); // waka-waka-waka-waka
            if(!n)
            {
                if(wpspot(d, d->ai->route[n], retries >= 2))
                {
                    d->ai->clear(false);
                    return true;
                }
                else if(retries <= 2) return hunt(d, b, retries+1); // try again
            }
            else n--; // otherwise, we want the next in line
        }
        if(d->ai->route.inrange(n) && wpspot(d, d->ai->route[n], retries >= 2)) return true;
        if(retries <= 2) return hunt(d, b, retries+1); // try again
    }
    b.override = false;
    d->ai->clear(false);
    return anynode(d, b);
}

void jumpto(fpsent *d, aistate &b, const vec &pos)
{
    vec off = vec(pos).sub(d->feetpos()), dir(off.x, off.y, 0);
    bool offground = d->timeinair && !d->inwater, jumper = off.z >= JUMPMIN;
    
    // More aggressive jumping - jump more frequently for speed
    bool jump = false;
    if(!offground)
    {
        // Jump if there's a height difference
        if(jumper)
        {
            jump = true;
        }
        // Or jump periodically for speed (bunny hopping)
        else if(lastmillis >= d->ai->jumprand && lastmillis >= d->ai->jumpseed)
        {
            // Higher skill bots jump more frequently
            if(d->skill > 50 || rnd(100) < 60)
            {
                jump = true;
            }
        }
    }
    
    if(jump)
    {
        vec old = d->o;
        d->o = vec(pos).add(vec(0, 0, d->eyeheight));
        if(!collide(d, vec(0, 0, 1))) jump = false;
        d->o = old;
        if(jump)
        {
            float radius = 18*18;
            loopv(entities::ents) if(entities::ents[i]->type == JUMPPAD)
            {
                fpsentity &e = *(fpsentity *)entities::ents[i];
                if(e.o.squaredist(pos) <= radius) { jump = false; break; }
            }
        }
    }
    if(jump)
    {
        d->jumping = true;
        // Reduced cooldown for more frequent jumping
        int seed = max((111-d->skill)*(d->inwater ? 2 : 5), 200);
        d->ai->jumpseed = lastmillis+seed+rnd(seed/2);
        seed *= b.idle ? 50 : 15; // Shorter random jump interval
        d->ai->jumprand = lastmillis+seed+rnd(seed);
    }
    
    // Always sprint when moving
    if(!b.idle && !d->ai->dontmove)
    {
        d->sprinting = true;
    }
}

void fixfullrange(float &yaw, float &pitch, float &roll, bool full)
{
    if(full)
    {
        while(pitch < -180.0f) pitch += 360.0f;
        while(pitch >= 180.0f) pitch -= 360.0f;
        while(roll < -180.0f) roll += 360.0f;
        while(roll >= 180.0f) roll -= 360.0f;
    }
    else
    {
        if(pitch > 89.9f) pitch = 89.9f;
        if(pitch < -89.9f) pitch = -89.9f;
        if(roll > 89.9f) roll = 89.9f;
        if(roll < -89.9f) roll = -89.9f;
    }
    while(yaw < 0.0f) yaw += 360.0f;
    while(yaw >= 360.0f) yaw -= 360.0f;
}

void fixrange(float &yaw, float &pitch)
{
    float r = 0.f;
    fixfullrange(yaw, pitch, r, false);
}

void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch)
{
    float dist = from.dist(pos);
    yaw = -atan2(pos.x-from.x, pos.y-from.y)/RAD;
    pitch = asin((pos.z-from.z)/dist)/RAD;
}

void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float frame, float scale)
{
    if(yaw < targyaw-180.0f) yaw += 360.0f;
    if(yaw > targyaw+180.0f) yaw -= 360.0f;
    float offyaw = fabs(targyaw-yaw)*frame, offpitch = fabs(targpitch-pitch)*frame*scale;
    if(targyaw > yaw)
    {
        yaw += offyaw;
        if(targyaw < yaw) yaw = targyaw;
    }
    else if(targyaw < yaw)
    {
        yaw -= offyaw;
        if(targyaw > yaw) yaw = targyaw;
    }
    if(targpitch > pitch)
    {
        pitch += offpitch;
        if(targpitch < pitch) pitch = targpitch;
    }
    else if(targpitch < pitch)
    {
        pitch -= offpitch;
        if(targpitch > pitch) pitch = targpitch;
    }
    fixrange(yaw, pitch);
}

bool lockon(fpsent *d, fpsent *e, float maxdist)
{
    if(d->gunselect == GUN_FIST && !d->blocked && !d->timeinair)
    {
        vec dir = vec(e->o).sub(d->o);
        float xydist = dir.x*dir.x+dir.y*dir.y, zdist = dir.z*dir.z, mdist = maxdist*maxdist, ddist = d->radius*d->radius+e->radius*e->radius;
        if(zdist <= ddist && xydist >= ddist+4 && xydist <= mdist+ddist) return true;
    }
    return false;
}

int process(fpsent *d, aistate &b)
{
    int result = 0, stupify = d->skill <= 10+rnd(15) ? rnd(d->skill*1000) : 0, skmod = 101-d->skill;
    float frame = d->skill <= 100 ? float(lastmillis-d->ai->lastrun)/float(max(skmod,1)*10) : 1;
    vec dp = d->headpos();

    bool idle = b.idle == 1 || (stupify && stupify <= skmod);
    d->ai->dontmove = false;
    
    // Simplified retreat logic - only when very low health
    if(!idle && badhealth(d) && shouldretreat(d) && b.type != AI_S_RETREAT)
    {
        // Find health pack
        if(find(d, b))
        {
            // Found item to pursue
        }
    }
    
    if(idle)
    {
        d->ai->lastaction = d->ai->lasthunt = lastmillis;
        d->ai->dontmove = true;
    }
    else if(hunt(d, b))
    {
        getyawpitch(dp, vec(d->ai->spot).add(vec(0, 0, d->eyeheight)), d->ai->targyaw, d->ai->targpitch);
        d->ai->lasthunt = lastmillis;
    }
    else idle = d->ai->dontmove = true;

    if(!d->ai->dontmove) jumpto(d, b, d->ai->spot);

    fpsent *e = getclient(d->ai->enemy);
    bool enemyok = e && targetable(d, e);
    
    // Update combat status
    d->ai->incombat = enemyok;
    if(enemyok && d->ai->combatstart == 0)
        d->ai->combatstart = lastmillis;
    else if(!enemyok && d->ai->combatstart > 0)
        d->ai->combatstart = 0; // Reset when no enemy
    if(!enemyok || d->skill >= 50)
    {
        fpsent *f = (fpsent *)intersectclosest(dp, d->ai->target, d);
        if(f)
        {
            if(targetable(d, f))
            {
                if(!enemyok) violence(d, b, f, d->gunselect == GUN_FIST);
                enemyok = true;
                e = f;
            }
            else enemyok = false;
        }
        else if(!enemyok && target(d, b, d->gunselect == GUN_FIST, false, SIGHTMIN))
            enemyok = (e = getclient(d->ai->enemy)) != NULL;
    }
    if(enemyok)
    {
        vec ep = getaimpos(d, e);
        float yaw, pitch;
        getyawpitch(dp, ep, yaw, pitch);
        fixrange(yaw, pitch);
        bool insight = cansee(d, dp, ep), hasseen = d->ai->enemyseen && lastmillis-d->ai->enemyseen <= (d->skill*10)+3000,
                quick = d->ai->enemyseen && lastmillis-d->ai->enemyseen <= (d->gunselect == GUN_CG ? 300 : skmod)+30;
        if(insight) d->ai->enemyseen = lastmillis;
        if(idle || insight || hasseen || quick)
        {
            float sskew = insight || d->skill > 100 ? 1.5f : (hasseen ? 1.f : 0.5f);
            if(insight && lockon(d, e, 16))
            {
                d->ai->targyaw = yaw;
                d->ai->targpitch = pitch;
                if(!idle) frame *= 2;
                d->ai->becareful = false;
            }
            scaleyawpitch(d->yaw, d->pitch, yaw, pitch, frame, sskew);
            
            // Apply combat movement only when we have a valid target
            if(enemyok && e)
            {
                updatecombatmovement(d, b);
                
                // Apply weapon-specific tactics
                applyweapontactics(d, e, b);
                
                // Manage engagement range
                manageengagementrange(d, e, b);
            }
            
            if(insight || quick)
            {
                if(canshoot(d, e) && hastarget(d, b, e, yaw, pitch, dp.squaredist(ep)))
                {
                    // Burst fire logic for automatic weapons
                    bool shouldfire = true;
                    if(shouldburstfire(d, d->gunselect))
                    {
                        const int burstsize = 3 + rnd(5); // 3-7 shots per burst
                        const int burstdelay = 200 + rnd(300); // 200-500ms between bursts
                        
                        if(d->ai->burstcount >= burstsize)
                        {
                            if(lastmillis - d->ai->lastburstfire > burstdelay)
                            {
                                d->ai->burstcount = 0;
                            }
                            else
                            {
                                shouldfire = false;
                            }
                        }
                        
                        if(shouldfire)
                        {
                            d->ai->burstcount++;
                            d->ai->lastburstfire = lastmillis;
                        }
                    }
                    
                    if(shouldfire)
                    {
                    d->attacking = true;
                    d->ai->lastaction = lastmillis;
                    result = 3;
                    }
                    else
                    {
                        result = 2;
                    }
                }
                else result = 2;
            }
            else result = 1;
        }
        else
        {
            if(!d->ai->enemyseen || lastmillis-d->ai->enemyseen > (d->skill*50)+3000)
            {
                d->ai->enemy = -1;
                d->ai->enemyseen = d->ai->enemymillis = 0;
            }
            enemyok = false;
            result = 0;
        }
    }
    else
    {
        if(!enemyok)
        {
            d->ai->enemy = -1;
            d->ai->enemyseen = d->ai->enemymillis = 0;
        }
        enemyok = false;
        result = 0;
    }

    fixrange(d->ai->targyaw, d->ai->targpitch);
    if(!result) scaleyawpitch(d->yaw, d->pitch, d->ai->targyaw, d->ai->targpitch, frame*0.25f, 1.f);

    if(d->ai->becareful && d->physstate == PHYS_FALL)
    {
        float offyaw, offpitch;
        vec v = vec(d->vel).normalize();
        vectoyawpitch(v, offyaw, offpitch);
        offyaw -= d->yaw; offpitch -= d->pitch;
        if(fabs(offyaw)+fabs(offpitch) >= 135) d->ai->becareful = false;
        else if(d->ai->becareful) d->ai->dontmove = true;
    }
    else d->ai->becareful = false;

    // Always sprint when moving for maximum speed
    d->sprinting = !d->ai->dontmove && !idle;
    
    if(d->ai->dontmove) d->move = d->strafe = 0;
    else
    { // our guys move one way.. but turn another?! :)
        const struct aimdir { int move, strafe, offset; } aimdirs[8] =
        {
        {  1,  0,   0 },
        {  1,  -1,  45 },
        {  0,  -1,  90 },
        { -1,  -1, 135 },
        { -1,  0, 180 },
        { -1, 1, 225 },
        {  0, 1, 270 },
        {  1, 1, 315 }
    };
        float yaw = d->ai->targyaw-d->yaw;
        while(yaw < 0.0f) yaw += 360.0f;
        while(yaw >= 360.0f) yaw -= 360.0f;
        int r = clamp(((int)floor((yaw+22.5f)/45.0f))&7, 0, 7);
        const aimdir &ad = aimdirs[r];
        d->move = ad.move;
        d->strafe = ad.strafe;
        
        // Apply combat strafing
        if(d->ai->incombat && d->ai->strafemode > 0)
        {
            if(d->ai->strafemode == 1) // strafe left
            {
                d->strafe = -1;
            }
            else if(d->ai->strafemode == 2) // strafe right
            {
                d->strafe = 1;
            }
            else if(d->ai->strafemode == 3) // random/aggressive
            {
                d->strafe = (lastmillis / 500) % 2 ? -1 : 1;
            }
        }
    }
    
    // Apply crouching
    if(d->ai->shouldcrouch && d->ai->crouchtime == 0)
    {
        d->crouching = true;
        d->ai->crouchtime = lastmillis;
    }
    else if(d->ai->crouchtime > 0)
    {
        // Auto-uncrouch after 2 seconds or when shouldcrouch is false
        if(!d->ai->shouldcrouch || lastmillis - d->ai->crouchtime > 2000)
        {
            d->crouching = false;
            d->ai->crouchtime = 0;
        }
    }
    
    findorientation(dp, d->yaw, d->pitch, d->ai->target);
    return result;
}

bool hasrange(fpsent *d, fpsent *e, int weap)
{
    if(!e) return true;
    if(targetable(d, e))
    {
        vec ep = getaimpos(d, e);
        float dist = ep.squaredist(d->headpos());
        if(weaprange(d, weap, dist)) return true;
    }
    return false;
}

bool request(fpsent *d, aistate &b)
{
    fpsent *e = getclient(d->ai->enemy);
    float dist = e ? d->o.dist(e->o) : 0;
    
    // TELEKINESIS LOGIC - catch incoming projectiles and throw objects
    bool shouldUseTelekinesis = false;
    
    // Check if we can catch incoming grenades/orbs
    if(d->aicancatch && lastmillis - d->ailastcatch > 500 && d->hasammo(GUN_TELEKENESIS))
    {
        float catchdist = d->aicatch.dist(d->o);
        if(catchdist < guns[GUN_TELEKENESIS2].range)
        {
            shouldUseTelekinesis = true;
            gunselect(GUN_TELEKENESIS, d);
            d->attacking = true;
            return process(d, b) >= 2;
        }
    }
    
    // Check if we're holding an object - throw it at enemies!
    if((d->isholdingbarrel || d->isholdingnade || d->isholdingorb || d->isholdingprop) && e)
    {
        // Throw at enemy if they're in range and visible
        if(dist < guns[GUN_TELEKENESIS].range * 0.8f)
        {
            gunselect(GUN_TELEKENESIS, d);
            d->attacking = true;
            return process(d, b) >= 2;
        }
    }
    
    // Check if there are props/barrels nearby to pick up and throw
    if(!shouldUseTelekinesis && e && d->hasammo(GUN_TELEKENESIS) && dist < 500.f)
    {
        // Use telekinesis to grab objects occasionally (tactical bots more often)
        int chance = d->ai->personality == AI_PERSONALITY_TACTICAL ? 30 : 15;
        if(rnd(100) < chance && !d->isholdingbarrel && !d->isholdingprop)
        {
            // TODO: Check for nearby movable objects
            // For now, just allow occasional telekinesis usage
        }
    }
    
    if(!d->hasammo(d->gunselect) || !hasrange(d, e, d->gunselect) || (d->gunselect != d->ai->weappref && (!isgoodammo(d->gunselect) || d->hasammo(d->ai->weappref))))
    {
        int gun = -1;
        
        // Try preferred weapon first
        if(d->hasammo(d->ai->weappref) && hasrange(d, e, d->ai->weappref)) 
            gun = d->ai->weappref;
        else if(e)
        {
            // Intelligent range-based weapon selection
            struct weaponpref {
                int gun;
                float score;
            };
            
            static vector<weaponpref> available;
            available.setsize(0);
            
            // Score available weapons based on range and situation
            // EXCLUDED: GUN_ELECTRO and GUN_ELECTRO2 (laser weapons are mobility tools, not combat weapons)
            static const int guns[] = { GUN_SMG2, GUN_SMG, GUN_CG2, GUN_CG, GUN_RL, GUN_SG, GUN_MAGNUM, GUN_CROSSBOW, GUN_SHOTGUN2, GUN_HANDGRENADE, GUN_PISTOL, GUN_FIST };
            loopi(sizeof(guns)/sizeof(guns[0]))
            {
                int testgun = guns[i];
                if(!d->hasammo(testgun)) continue;
                if(!hasrange(d, e, testgun)) continue;
                
                float score = 0;
                float optrange = getweaponrange(testgun, true);
                float rangediff = fabs(dist - optrange);
                
                // Score based on how close we are to optimal range
                score = 100.f - clamp(rangediff / 10.f, 0.f, 90.f);
                
                // HIGH PRIORITY: SMG and assault rifles (starting weapons)
                if(testgun == GUN_SMG || testgun == GUN_SMG2) score += 40.f;
                if(testgun == GUN_CG || testgun == GUN_CG2) score += 35.f;
                
                // Bonus for high-damage weapons
                if(testgun == GUN_RL || testgun == GUN_MAGNUM || testgun == GUN_SG) score += 20.f;
                
                // LOWER PRIORITY: Grenades (save them, use SMG instead)
                if(testgun == GUN_HANDGRENADE) score -= 20.f;
                
                // Personality modifiers
                if(d->ai->personality == AI_PERSONALITY_AGGRESSIVE)
                {
                    if(testgun == GUN_SG || testgun == GUN_RL) score += 25.f;
                    if(testgun == GUN_SMG2) score += 15.f; // Aggressive bots like SMG
                }
                else if(d->ai->personality == AI_PERSONALITY_DEFENSIVE)
                {
                    if(testgun == GUN_MAGNUM || testgun == GUN_CG2) score += 25.f;
                }
                else if(d->ai->personality == AI_PERSONALITY_TACTICAL)
                {
                    // Prefer varied weapons based on situation
                    if(dist < 200.f && (testgun == GUN_SG || testgun == GUN_SHOTGUN2 || testgun == GUN_SMG2)) score += 30.f;
                    else if(dist > 400.f && (testgun == GUN_MAGNUM || testgun == GUN_CROSSBOW)) score += 30.f;
                    else if(testgun == GUN_SMG || testgun == GUN_CG) score += 20.f; // Good all-rounders
                }
                
                // Avoid fist unless desperate
                if(testgun == GUN_FIST) score = 1.f;
                
                weaponpref wp;
                wp.gun = testgun;
                wp.score = score;
                available.add(wp);
            }
            
            // Select best weapon
            float bestscore = -1;
            loopv(available)
            {
                if(available[i].score > bestscore)
                {
                    bestscore = available[i].score;
                    gun = available[i].gun;
                }
            }
        }
        
        if(gun >= 0 && gun != d->gunselect) gunselect(gun, d);
    }
    return process(d, b) >= 2;
}

void timeouts(fpsent *d, aistate &b)
{
    if(d->blocked)
    {
        d->ai->blocktime += lastmillis-d->ai->lastrun;
        if(d->ai->blocktime > (d->ai->blockseq+1)*500)
        {
            switch(d->ai->blockseq)
            {
            case 1: case 2: case 3:
                if(entities::ents.inrange(d->ai->targnode)) d->ai->addprevnode(d->ai->targnode);
                d->ai->clear(false);
                break;
            case 4: d->ai->reset(true); break;
            case 5: d->ai->reset(false); break;
            case 6: suicide(d); return; break; // this is our last resort..
            case 0: default: break;
            }
            d->ai->blockseq++;
        }
    }
    else d->ai->blocktime = d->ai->blockseq = 0;

    if(d->ai->targnode == d->ai->targlast)
    {
        d->ai->targtime += lastmillis-d->ai->lastrun;
        if(d->ai->targtime > (d->ai->targseq+1)*1000)
        {
            switch(d->ai->targseq)
            {
            case 1: case 2: case 3:
                if(entities::ents.inrange(d->ai->targnode)) d->ai->addprevnode(d->ai->targnode);
                d->ai->clear(false);
                break;
            case 4: d->ai->reset(true); break;
            case 5: d->ai->reset(false); break;
            case 6: suicide(d); return; break; // this is our last resort..
            case 0: default: break;
            }
            d->ai->targseq++;
        }
    }
    else
    {
        d->ai->targtime = d->ai->targseq = 0;
        d->ai->targlast = d->ai->targnode;
    }

    if(d->ai->lasthunt)
    {
        int millis = lastmillis-d->ai->lasthunt;
        if(millis <= 3000) { d->ai->tryreset = false; d->ai->huntseq = 0; }
        else if(millis > (d->ai->huntseq+1)*3000)
        {
            switch(d->ai->huntseq)
            {
            case 0: d->ai->reset(true); break;
            case 1: d->ai->reset(false); break;
            case 2: suicide(d); return; break; // this is our last resort..
            default: break;
            }
            d->ai->huntseq++;
        }
    }
}

void logic(fpsent *d, aistate &b, bool run)
{
    vec dp = d->headpos();
    bool allowmove = canmove(d) && b.type != AI_S_WAIT;
    if(d->state != CS_ALIVE || !allowmove) d->stopmoving();
    if(d->state == CS_ALIVE)
    {
        if(allowmove)
        {
            if(!request(d, b)) target(d, b, d->gunselect == GUN_FIST, b.idle ? true : false);
            shoot(d, d->ai->target);
        }
        if(!intermission)
        {
            if(d->ragdoll) cleanragdoll(d);
            moveplayer(d, 10, true);
            if(allowmove && !b.idle) timeouts(d, b);
            entities::checkitems(d);
            if(cmode) cmode->checkitems(d);
        }
    }
    else if(d->state == CS_DEAD)
    {
        if(d->ragdoll) moveragdoll(d);
        else if(lastmillis-d->lastpain<2000)
        {
            d->move = d->strafe = 0;
            moveplayer(d, 10, false);
        }
    }
    d->jumping =d->attacking= false;
}

void avoid()
{
    // guess as to the radius of ai and other critters relying on the avoid set for now
    float guessradius = player1->radius;
    obstacles.clear();
    loopv(players)
    {
        dynent *d = players[i];
        if(d->state != CS_ALIVE) continue;
        obstacles.avoidnear(d, d->o.z + d->aboveeye + 1, d->feetpos(), guessradius + d->radius);
    }
    extern avoidset wpavoid;
    obstacles.add(wpavoid);
    avoidweapons(obstacles, guessradius);
}

void think(fpsent *d, bool run)
{
    // CRITICAL: Always update enemy tracking when alive, regardless of 'run'
    // This ensures bots can detect and respond to enemies immediately
    if(d->state == CS_ALIVE && d->ai)
    {
        updateenemytracking(d);
    }
    
    // Other updates only on full update cycle
    if(run)
    {
        if(d->state == CS_ALIVE && d->ai)
        {
            // Update squad coordination
            if(m_teammode)
            {
                // Update squad membership periodically
                if(lastmillis - d->ai->lastcoordination > 3000)
                {
                    updatesquad(d);
                    d->ai->lastcoordination = lastmillis;
                }
                
                // Coordinate with squad every frame
                coordinatesquad(d);
            }
        }
        
        // Environmental awareness works even when dead (learning)
        if(d->ai) updateenvironmentalawareness(d);
    }
    
    // the state stack works like a chain of commands, certain commands simply replace each other
    // others spawn new commands to the stack the ai reads the top command from the stack and executes
    // it or pops the stack and goes back along the history until it finds a suitable command to execute
    if(d->ai->state.empty()) setup(d, false);
    bool cleannext = false, parse = run && waypoints.inrange(d->lastnode);
    loopvrev(d->ai->state)
    {
        aistate &c = d->ai->state[i];
        if(cleannext)
        {
            c.millis = lastmillis;
            c.override = false;
            cleannext = false;
        }
        if(d->state == CS_DEAD && d->respawned!=d->lifesequence && (!cmode || cmode->respawnwait(d) <= 0) && lastmillis - d->lastpain > 1500)
        {
            addmsg(N_TRYSPAWN, "rc", d);
            d->respawned = d->lifesequence;
        }
        else if(d->state == CS_ALIVE && parse)
        {
            int result = 0;
            c.idle = 0;
            switch(c.type)
            {
            case AI_S_WAIT: result = dowait(d, c); break;
            case AI_S_DEFEND: result = dodefend(d, c); break;
            case AI_S_PURSUE: result = dopursue(d, c); break;
            case AI_S_INTEREST: result = dointerest(d, c); break;
            case AI_S_RETREAT: result = dodefend(d, c); break; // retreat uses defend logic
            default: result = 0; break;
            }
            if(result <= 0)
            {
                d->ai->clear(true);
                if(c.type != AI_S_WAIT)
                {
                    switch(result)
                    {
                    case 0: default: d->ai->removestate(i); cleannext = true; break;
                    case -1: i = d->ai->state.length()-1; break;
                    }
                    continue; // shouldn't interfere
                }
            }
        }
        logic(d, c, parse);
        break;
    }
    if(d->ai->trywipe) d->ai->wipe();
    d->ai->lastrun = lastmillis;
}

void drawstate(fpsent *d, aistate &b, bool top, int above)
{
    const char *bnames[AI_S_MAX] = {
        "wait", "defend", "pursue", "interest"
    }, *btypes[AI_T_MAX+1] = {
        "none", "node", "player", "affinity", "entity"
    };
    string s;
    if(top)
    {
        formatstring(s)("\f0%s (%d) %s:%d (%d[%d])",
                        bnames[b.type],
                lastmillis-b.millis,
                btypes[clamp(b.targtype+1, 0, AI_T_MAX+1)], b.target,
                !d->ai->route.empty() ? d->ai->route[0] : -1,
            d->ai->route.length()
          );
    }
    else
    {
        formatstring(s)("\f2%s (%d) %s:%d",
                        bnames[b.type],
                lastmillis-b.millis,
                btypes[clamp(b.targtype+1, 0, AI_T_MAX+1)], b.target
                );
    }
    particle_textcopy(vec(d->abovehead()).add(vec(0, 0, above)), s, PART_TEXT, 1);
    if(b.targtype == AI_T_ENTITY && entities::ents.inrange(b.target))
    {
        formatstring(s)("GOAL: %s", colorname(d));
        particle_textcopy(entities::ents[b.target]->o, s, PART_TEXT, 1);
    }
}

void drawroute(fpsent *d, aistate &b, float amt = 1.f)
{
    int colour = 0xFFFFFF, last = -1;

    loopvrev(d->ai->route)
    {
        if(d->ai->route.inrange(last))
        {
            int index = d->ai->route[i], prev = d->ai->route[last];
            if(waypoints.inrange(index) && waypoints.inrange(prev))
            {
                waypoint &e = waypoints[index],
                        &f = waypoints[prev];
                vec fr(vec(f.o).add(vec(0, 0, 4.f*amt))),
                        dr(vec(e.o).add(vec(0, 0, 4.f*amt)));
                particle_flare(fr, dr, 1, PART_STREAK, colour);
            }
        }
        last = i;
    }
    if(aidebug > 4)
    {
        vec pos = d->feetpos();
        if(d->ai->spot != vec(0, 0, 0)) particle_flare(pos, d->ai->spot, 1, PART_LIGHTNING, 0xFFFFFF);
        if(waypoints.inrange(d->lastnode))
            particle_flare(pos, waypoints[d->lastnode].o, 1, PART_LIGHTNING, 0x00FFFF);
        if(waypoints.inrange(d->ai->prevnodes[1]))
            particle_flare(pos, waypoints[d->ai->prevnodes[1]].o, 1, PART_LIGHTNING, 0xFF00FF);
    }
}

VAR(showwaypoints, 0, 0, 1);
VAR(showwaypointsradius, 0, 200, 10000);

void render()
{
    if(aidebug > 1)
    {
        int total = 0, alive = 0;
        loopv(players) if(players[i]->ai) total++;
        loopv(players) if(players[i]->state == CS_ALIVE && players[i]->ai)
        {
            fpsent *d = players[i];
            bool top = true;
            int above = 0;
            alive++;
            loopvrev(d->ai->state)
            {
                aistate &b = d->ai->state[i];
                drawstate(d, b, top, above += 2);
                if(aidebug > 3 && top && b.type != AI_S_WAIT)
                    drawroute(d, b, float(alive)/float(total));
                if(top)
                {
                    if(aidebug > 2) top = false;
                    else break;
                }
            }
            if(aidebug > 2)
            {
                if(d->ai->weappref >= 0 && d->ai->weappref < NUMGUNS)
                    particle_textcopy(vec(d->abovehead()).add(vec(0, 0, above += 2)), guns[d->ai->weappref].name, PART_TEXT, 1);
                fpsent *e = getclient(d->ai->enemy);
                if(e) particle_textcopy(vec(d->abovehead()).add(vec(0, 0, above += 2)), colorname(e), PART_TEXT, 1);
            }
        }
    }
    if(showwaypoints || aidebug > 5)
    {
        vector<int> close;
        int len = waypoints.length();
        if(showwaypointsradius)
        {
            findwaypointswithin(camera1->o, 0, showwaypointsradius, close);
            len = close.length();
        }
        loopi(len)
        {
            waypoint &w = waypoints[showwaypointsradius ? close[i] : i];
            loopj(MAXWAYPOINTLINKS)
            {
                int link = w.links[j];
                if(!link) break;
                particle_flare(w.o, waypoints[link].o, 1, PART_STREAK, 0x0000FF);
            }
        }

    }
}

// ===== ENHANCED AI SYSTEMS =====

// Multiple enemy tracking system
void updateenemytracking(fpsent *d)
{
    if(!d || !d->ai) return;
    
    int now = lastmillis;
    vec dp = d->headpos();
    
    // Update existing tracked enemies and add new ones
    loopv(players)
    {
        fpsent *e = players[i];
        if(!e || e == d || !targetable(d, e)) continue;
        
        vec ep = e->headpos();
        
        // Quick distance check before expensive visibility check
        float dist = dp.squaredist(ep);
        if(dist > SIGHTMAX * SIGHTMAX) continue; // Too far
        
        bool visible = cansee(d, dp, ep);
        
        // Find or create enemy tracking entry
        enemyinfo *ei = NULL;
        loopvj(d->ai->enemylist)
        {
            if(d->ai->enemylist[j].clientnum == e->clientnum)
            {
                ei = &d->ai->enemylist[j];
                break;
            }
        }
        
        if(!ei && d->ai->enemylist.length() < MAXENEMYTRACK)
        {
            d->ai->enemylist.add(enemyinfo(e->clientnum));
            ei = &d->ai->enemylist.last();
        }
        
        if(ei)
        {
            if(visible)
            {
                // Update position and velocity
                vec oldpos = ei->lastpos;
                ei->lastpos = e->o;
                
                if(ei->lastseen > 0 && now - ei->lastseen < 500)
                {
                    // Calculate velocity
                    float dt = (now - ei->lastseen) / 1000.0f;
                    if(dt > 0) ei->velocity = vec(e->o).sub(oldpos).div(dt);
                }
                
                ei->lastseen = now;
                
                // Calculate threat based on distance, health, weapon
                float dist = dp.dist(ep);
                float distfactor = clamp(1.0f - (dist / SIGHTMAX), 0.f, 1.f);
                float healthfactor = e->health / 100.0f;
                ei->threat = distfactor * 0.6f + healthfactor * 0.4f;
            }
            else
            {
                // Decay threat over time when not visible
                if(now - ei->lastseen > 3000)
                {
                    ei->threat *= 0.95f;
                }
            }
        }
    }
    
    // Remove old enemies that haven't been seen in a while
    loopv(d->ai->enemylist)
    {
        if(now - d->ai->enemylist[i].lastseen > 10000 || d->ai->enemylist[i].threat < 0.1f)
        {
            d->ai->enemylist.remove(i--);
        }
    }
}

// Select best target from tracked enemies
fpsent *selectbesttarget(fpsent *d)
{
    if(!d->ai || d->ai->enemylist.empty()) return NULL;
    
    fpsent *best = NULL;
    float bestscore = -1;
    vec dp = d->headpos();
    
    loopv(d->ai->enemylist)
    {
        enemyinfo &ei = d->ai->enemylist[i];
        fpsent *e = getclient(ei.clientnum);
        
        if(!e || !targetable(d, e)) continue;
        
        vec ep = e->headpos();
        float dist = dp.dist(ep);
        bool visible = cansee(d, dp, ep);
        
        // Score based on: threat, visibility, distance, recent damage
        float score = ei.threat * 2.0f;
        if(visible) score += 1.5f;
        score += clamp(1.0f - (dist / SIGHTMAX), 0.f, 1.f);
        
        // Prioritize enemies who recently damaged us
        if(ei.lastdamaged > 0 && lastmillis - ei.lastdamaged < 3000)
            score += 2.0f;
        
        // Consider current weapon effectiveness at this range
        if(weaprange(d, d->gunselect, ep.squaredist(dp)))
            score += 1.0f;
        
        if(score > bestscore)
        {
            bestscore = score;
            best = e;
        }
    }
    
    // Update the legacy single-enemy field for compatibility
    if(best)
    {
        d->ai->enemy = best->clientnum;
        if(d->ai->enemyseen == 0) d->ai->enemyseen = lastmillis;
    }
    else
    {
        d->ai->enemy = -1;
    }
    
    return best;
}

// Predict enemy position based on velocity
vec predictposition(fpsent *e, float projspeed)
{
    if(!e || !e->ai) return e->o;
    
    vec predicted = e->o;
    
    // Find enemy tracking info
    loopv(players)
    {
        fpsent *bot = players[i];
        if(!bot->ai) continue;
        
        loopvj(bot->ai->enemylist)
        {
            if(bot->ai->enemylist[j].clientnum == e->clientnum)
            {
                enemyinfo &ei = bot->ai->enemylist[j];
                
                // Predict based on velocity if we have recent data
                if(lastmillis - ei.lastseen < 500 && ei.velocity.magnitude() > 0.1f)
                {
                    float dist = bot->o.dist(e->o);
                    float timetoimpact = projspeed > 0 ? dist / projspeed : 0;
                    
                    predicted = vec(e->o).add(vec(ei.velocity).mul(timetoimpact));
                }
                return predicted;
            }
        }
    }
    
    return predicted;
}

// Check if position is actually reachable
bool isreachable(fpsent *d, const vec &pos, int timeout)
{
    if(!waypoints.inrange(d->lastnode)) return false;
    
    int targetnode = closestwaypoint(pos, SIGHTMIN, true);
    if(!waypoints.inrange(targetnode)) return false;
    
    // Quick distance check
    float dist = d->o.dist(pos);
    if(dist < CLOSEDIST) return true;
    
    // Check if we've failed to reach this recently
    if(d->ai->faileditemid >= 0)
    {
        extentity *failed = entities::ents.inrange(d->ai->faileditemid) ? (extentity *)entities::ents[d->ai->faileditemid] : NULL;
        if(failed && pos.dist(failed->o) < CLOSEDIST && lastmillis - d->ai->faileditemtime < 10000)
            return false;
    }
    
    // Try pathfinding
    static vector<int> testroute;
    testroute.setsize(0);
    if(!route(d, d->lastnode, targetnode, testroute, obstacles, false))
        return false;
    
    // Path exists - now verify it's reasonable
    if(testroute.length() > 50) return false; // too long
    
    return true;
}

// Find cover from a threat position
bool findcover(fpsent *d, aistate &b, const vec &threat)
{
    if(!waypoints.inrange(d->lastnode)) return false;
    
    static vector<int> covercandidates;
    covercandidates.setsize(0);
    
    vec mypos = d->feetpos();
    float searchdist = min(mypos.dist(threat) * 0.7f, 512.f);
    
    findwaypointswithin(mypos, CLOSEDIST, searchdist, covercandidates);
    
    // Evaluate each candidate
    int bestcover = -1;
    float bestscore = -1;
    
    loopv(covercandidates)
    {
        int wp = covercandidates[i];
        if(!waypoints.inrange(wp)) continue;
        if(d->ai->hasprevnode(wp)) continue;
        
        waypoint &w = waypoints[wp];
        
        // Check if this waypoint blocks line of sight to threat
        vec testpos = vec(w.o).add(vec(0, 0, d->eyeheight));
        vec testtarg;
        
        bool blocked = !raycubelos(testpos, threat, testtarg);
        
        if(!blocked) continue; // not cover if threat can see it
        
        // Score based on distance (closer is better) and direction (away from threat)
        float dist = mypos.dist(w.o);
        vec dir = vec(w.o).sub(threat).normalize();
        vec mydir = vec(mypos).sub(threat).normalize();
        float alignment = dir.dot(mydir); // positive = same direction (away from threat)
        
        float score = (alignment + 1.0f) * 0.5f; // 0-1 range
        score += (1.0f - clamp(dist / searchdist, 0.f, 1.f)) * 0.5f;
        
        if(score > bestscore)
        {
            bestscore = score;
            bestcover = wp;
        }
    }
    
    if(waypoints.inrange(bestcover))
    {
        d->ai->coverpoint = bestcover;
        return makeroute(d, b, bestcover);
    }
    
    return false;
}

// Determine if bot should retreat
bool shouldretreat(fpsent *d)
{
    if(!d->ai) return false;
    
    // Health-based retreat
    if(badhealth(d)) return true;
    
    // Check if outnumbered
    int nearbyenemies = 0, nearbyallies = 0;
    vec mypos = d->o;
    
    loopv(players)
    {
        fpsent *p = players[i];
        if(p == d || p->state != CS_ALIVE) continue;
        
        float dist = mypos.dist(p->o);
        if(dist > SIGHTMAX) continue;
        
        if(isteam(d->team, p->team)) nearbyallies++;
        else nearbyenemies++;
    }
    
    // Retreat if outnumbered 2:1 or more
    if(nearbyenemies >= 2 && (nearbyallies == 0 || nearbyenemies > nearbyallies * 2))
        return true;
    
    // Retreat if low on ammo for current weapon
    if(!d->hasammo(d->gunselect) && !hasgoodammo(d))
        return true;
    
    // Personality-based retreat
    if(d->ai->personality == AI_PERSONALITY_DEFENSIVE && d->health < 75)
        return true;
    
    return false;
}

// Update combat movement (strafing, crouching, jumping)
void updatecombatmovement(fpsent *d, aistate &b)
{
    if(!d->ai) return;
    
    // Only apply if we actually have an enemy
    fpsent *e = getclient(d->ai->enemy);
    if(!e || !d->ai->incombat) 
    {
        // Reset combat variables when not in combat
        d->ai->strafemode = 0;
        d->ai->shouldcrouch = false;
        return;
    }
    
    int now = lastmillis;
    vec mypos = d->o, enemypos = e->o;
    float dist = mypos.dist(enemypos);
    
    // Strafing logic
    if(now - d->ai->laststrafechange > 1000 + rnd(1000))
    {
        // Change strafe direction periodically
        int oldmode = d->ai->strafemode;
        
        if(dist < 200.f) // close range - always strafe
        {
            d->ai->strafemode = 1 + rnd(2); // 1 or 2 (left or right)
        }
        else if(dist < 512.f && d->skill > 30) // medium range - sometimes strafe
        {
            d->ai->strafemode = rnd(100) < d->skill ? (1 + rnd(2)) : 0;
        }
        else
        {
            d->ai->strafemode = 0; // long range - don't strafe
        }
        
        d->ai->laststrafechange = now;
    }
    
    // Crouching logic (when under fire or reloading)
    bool shouldcrouch = false;
    
    if(now - d->ai->lastdamagetime < 1000) // just took damage
    {
        shouldcrouch = d->skill > 40 && rnd(100) < d->skill;
    }
    else if(d->reloading) // reloading
    {
        shouldcrouch = d->skill > 50;
    }
    else if(dist > 400.f && d->gunselect == GUN_MAGNUM) // sniping
    {
        shouldcrouch = d->skill > 60;
    }
    
    d->ai->shouldcrouch = shouldcrouch;
    
    // Combat jumping (to avoid shots)
    if(d->skill > 50 && now - d->ai->lastjumpcombat > 2000)
    {
        if(now - d->ai->lastdamagetime < 500) // just took damage - jump!
        {
            if(rnd(100) < (d->skill - 50) * 2)
            {
                d->jumping = true;
                d->ai->lastjumpcombat = now;
            }
        }
    }
    
    // Apply movement based on personality
    if(d->ai->personality == AI_PERSONALITY_AGGRESSIVE)
    {
        d->ai->strafemode = 3; // aggressive random strafing
        d->ai->shouldcrouch = false; // aggressive bots don't crouch
    }
    else if(d->ai->personality == AI_PERSONALITY_TACTICAL)
    {
        // Tactical bots use cover more
        if(shouldretreat(d) && now - d->ai->lastcovercheck > 5000)
        {
            findcover(d, b, enemypos);
            d->ai->lastcovercheck = now;
        }
    }
}

// Initialize bot personality
void initpersonality(fpsent *d)
{
    if(!d->ai) return;
    
    // Assign personality based on skill and randomness
    if(d->skill >= 80) // high skill - tactical or aggressive
    {
        d->ai->personality = rnd(2) ? AI_PERSONALITY_TACTICAL : AI_PERSONALITY_AGGRESSIVE;
    }
    else if(d->skill >= 50) // medium skill - balanced or support
    {
        d->ai->personality = rnd(2) ? AI_PERSONALITY_BALANCED : AI_PERSONALITY_SUPPORT;
    }
    else // low skill - defensive or balanced
    {
        d->ai->personality = rnd(2) ? AI_PERSONALITY_DEFENSIVE : AI_PERSONALITY_BALANCED;
    }
    
    // Set aggression and caution based on personality
    switch(d->ai->personality)
    {
        case AI_PERSONALITY_AGGRESSIVE:
            d->ai->aggression = 0.8f + rnd(20) / 100.f;
            d->ai->caution = 0.1f + rnd(20) / 100.f;
            break;
        case AI_PERSONALITY_DEFENSIVE:
            d->ai->aggression = 0.2f + rnd(20) / 100.f;
            d->ai->caution = 0.7f + rnd(30) / 100.f;
            break;
        case AI_PERSONALITY_TACTICAL:
            d->ai->aggression = 0.5f + rnd(30) / 100.f;
            d->ai->caution = 0.6f + rnd(20) / 100.f;
            break;
        case AI_PERSONALITY_SUPPORT:
            d->ai->aggression = 0.4f + rnd(20) / 100.f;
            d->ai->caution = 0.5f + rnd(20) / 100.f;
            break;
        default: // BALANCED
            d->ai->aggression = 0.5f;
            d->ai->caution = 0.5f;
            break;
    }
}

// Get weapon effective/optimal range
float getweaponrange(int gun, bool optimal)
{
    if(gun < 0 || gun >= NUMGUNS) return SIGHTMIN;
    
    if(optimal)
    {
        // Optimal engagement ranges
        switch(gun)
        {
            case GUN_FIST: return 8.f;
            case GUN_SG: 
            case GUN_SHOTGUN2: return 50.f;
            case GUN_CG:
            case GUN_CG2: return 256.f;
            case GUN_RL: return 384.f;
            case GUN_MAGNUM: return 768.f;
            case GUN_CROSSBOW: return 640.f;
            case GUN_SMG:
            case GUN_SMG2: return 320.f;
            default: return 256.f;
        }
    }
    else
    {
        // Maximum effective range
        return guns[gun].range;
    }
}

// Check if weapon should use burst fire
bool shouldburstfire(fpsent *d, int gun)
{
    if(gun < 0 || gun >= NUMGUNS) return false;
    
    // Automatic weapons should burst
    if(gun == GUN_CG || gun == GUN_CG2 || gun == GUN_SMG || gun == GUN_SMG2)
    {
        // Higher skill = better burst control
        if(d->skill > 60) return true;
    }
    
    return false;
}

// Team coordination - find nearby teammates
void findteammates(fpsent *d, vector<fpsent *> &teammates, float maxdist = SIGHTMAX)
{
    if(!m_teammode) return;
    
    vec mypos = d->o;
    loopv(players)
    {
        fpsent *p = players[i];
        if(p == d || !p->ai || p->state != CS_ALIVE) continue;
        if(!isteam(d->team, p->team)) continue;
        
        float dist = mypos.dist(p->o);
        if(dist <= maxdist)
        {
            teammates.add(p);
        }
    }
}

// Form or update squad
void updatesquad(fpsent *d)
{
    if(!d->ai || !m_teammode) return;
    
    static vector<fpsent *> nearbymates;
    nearbymates.setsize(0);
    findteammates(d, nearbymates, 512.f);
    
    // If we have a squad leader and they're nearby, stay with them
    if(d->ai->squadleader >= 0)
    {
        fpsent *leader = getclient(d->ai->squadleader);
        bool leadernearby = false;
        
        loopv(nearbymates)
        {
            if(nearbymates[i]->clientnum == d->ai->squadleader)
            {
                leadernearby = true;
                break;
            }
        }
        
        // Leader is gone or too far - find new squad
        if(!leader || !leadernearby || leader->state != CS_ALIVE)
        {
            d->ai->squadleader = -1;
            d->ai->squadmates.setsize(0);
        }
    }
    
    // Form new squad if we don't have one
    if(d->ai->squadleader < 0 && nearbymates.length() > 0)
    {
        // Higher skill bots become leaders
        bool shouldlead = true;
        loopv(nearbymates)
        {
            fpsent *mate = nearbymates[i];
            if(!mate) continue;
            if(mate->skill > d->skill + 10)
            {
                shouldlead = false;
                // Follow this higher-skilled bot
                if(mate->ai && mate->ai->squadleader == mate->clientnum)
                {
                    d->ai->squadleader = mate->clientnum;
                    break;
                }
            }
        }
        
        if(shouldlead)
        {
            // Become squad leader
            d->ai->squadleader = d->clientnum;
            d->ai->squadmates.setsize(0);
            
            // Recruit nearby teammates
            loopv(nearbymates)
            {
                fpsent *mate = nearbymates[i];
                if(!mate || !mate->ai) continue;
                if(d->ai->squadmates.length() < 3)
                {
                    // Set them to follow us
                    mate->ai->squadleader = d->clientnum;
                    d->ai->squadmates.add(mate->clientnum);
                }
            }
        }
    }
}

// Squad coordination - share target info
void coordinatesquad(fpsent *d)
{
    if(!d->ai || d->ai->squadleader < 0) return;
    
    fpsent *leader = d->ai->squadleader == d->clientnum ? d : getclient(d->ai->squadleader);
    if(!leader || !leader->ai) return;
    
    // Share enemy information with squad
    if(d->ai->enemy >= 0)
    {
        fpsent *enemy = getclient(d->ai->enemy);
        if(enemy && targetable(d, enemy))
        {
            // Inform squad leader about enemy
            if(leader != d && leader->ai)
            {
                bool alreadytracked = false;
                loopv(leader->ai->enemylist)
                {
                    if(leader->ai->enemylist[i].clientnum == enemy->clientnum)
                    {
                        alreadytracked = true;
                        // Increase threat since teammate sees them too
                        leader->ai->enemylist[i].threat = min(leader->ai->enemylist[i].threat + 0.2f, 1.f);
                        leader->ai->enemylist[i].lastpos = enemy->o;
                        leader->ai->enemylist[i].lastseen = lastmillis;
                        break;
                    }
                }
                
                if(!alreadytracked && leader->ai->enemylist.length() < MAXENEMYTRACK)
                {
                    enemyinfo ei(enemy->clientnum);
                    ei.lastpos = enemy->o;
                    ei.lastseen = lastmillis;
                    ei.threat = 0.7f;
                    leader->ai->enemylist.add(ei);
                }
            }
            
            // Inform squad members
            loopv(d->ai->squadmates)
            {
                fpsent *mate = getclient(d->ai->squadmates[i]);
                if(!mate || !mate->ai || mate == d) continue;
                
                // Share enemy info
                bool tracked = false;
                loopvj(mate->ai->enemylist)
                {
                    if(mate->ai->enemylist[j].clientnum == enemy->clientnum)
                    {
                        tracked = true;
                        mate->ai->enemylist[j].threat = min(mate->ai->enemylist[j].threat + 0.15f, 1.f);
                        break;
                    }
                }
                
                if(!tracked && mate->ai->enemylist.length() < MAXENEMYTRACK)
                {
                    enemyinfo ei(enemy->clientnum);
                    ei.lastpos = enemy->o;
                    ei.lastseen = lastmillis - 500; // slightly older info
                    ei.threat = 0.5f;
                    mate->ai->enemylist.add(ei);
                }
            }
        }
    }
    
    // Leader assigns roles based on situation
    if(leader == d && d->ai->squadmates.length() > 0)
    {
        // Simple role assignment: some attack, some defend
        loopv(d->ai->squadmates)
        {
            fpsent *mate = getclient(d->ai->squadmates[i]);
            if(!mate || !mate->ai) continue;
            
            // Defensive bots protect, others attack
            if(mate->ai->personality == AI_PERSONALITY_DEFENSIVE || mate->ai->personality == AI_PERSONALITY_SUPPORT)
            {
                // These bots stay closer to objectives/team
            }
            else
            {
                // Aggressive bots push forward
            }
        }
    }
}

// Tactical spacing - don't clump up
void maintainspacing(fpsent *d, aistate &b)
{
    if(!d->ai) return;
    
    static vector<fpsent *> nearbymates;
    nearbymates.setsize(0);
    findteammates(d, nearbymates, 128.f);
    
    if(nearbymates.length() > 2) // getting crowded
    {
        // Find a nearby waypoint that's away from teammates
        vec avgpos(0, 0, 0);
        loopv(nearbymates) avgpos.add(nearbymates[i]->o);
        avgpos.div(nearbymates.length());
        
        // Move away from average position
        vec awayfrom = vec(d->o).sub(avgpos).normalize();
        vec targetpos = vec(d->o).add(vec(awayfrom).mul(100.f));
        
        int spreadnode = closestwaypoint(targetpos, SIGHTMIN, true);
        if(waypoints.inrange(spreadnode) && !d->ai->hasprevnode(spreadnode))
        {
            makeroute(d, b, spreadnode);
        }
    }
}

// Weapon-specific tactics
void applyweapontactics(fpsent *d, fpsent *e, aistate &b)
{
    if(!d->ai || !e) return;
    
    float dist = d->o.dist(e->o);
    
    // Simplified tactics - only apply for high-skill bots
    if(d->skill < 60) return;
    
    switch(d->gunselect)
    {
        case GUN_MAGNUM:
        case GUN_CROSSBOW: // Sniper weapons
        {
            // Crouch for accuracy at long range
            if(dist > 400.f && d->skill > 70 && !d->timeinair)
            {
                d->ai->shouldcrouch = true;
            }
            break;
        }
        
        case GUN_SG:
        case GUN_SHOTGUN2: // Shotgun tactics
        {
            // Jump-shoot technique for skilled bots
            if(dist < 100.f && d->skill > 80 && !d->timeinair && rnd(100) < 20)
            {
                d->jumping = true;
            }
            break;
        }
    }
}

// Engagement range management
bool manageengagementrange(fpsent *d, fpsent *e, aistate &b)
{
    // Simplified - just maintain some spacing for high-skill bots
    if(!d->ai || !e || d->skill < 70) return false;
    
    float dist = d->o.dist(e->o);
    
    // Back away if too close with rocket launcher (to avoid self-damage)
    if(d->gunselect == GUN_RL && dist < 150.f)
    {
        d->ai->becareful = true;
        return false;
    }
    
    return false;
}

// Environmental awareness - learn good positions
struct mapknowledge
{
    vector<int> sniperpoints;    // Good long-range positions
    vector<int> ambushpoints;    // Good ambush locations
    vector<int> dangerpoints;    // Dangerous areas (died here often)
    int lastupdated;
    
    mapknowledge() : lastupdated(0) {}
};

static mapknowledge mapdata;

void updateenvironmentalawareness(fpsent *d)
{
    if(!d->ai) return;
    
    // Learn from death - mark current area as dangerous
    if(d->state == CS_DEAD && waypoints.inrange(d->lastnode))
    {
        bool alreadymarked = false;
        loopv(mapdata.dangerpoints)
        {
            if(mapdata.dangerpoints[i] == d->lastnode)
            {
                alreadymarked = true;
                break;
            }
        }
        
        if(!alreadymarked)
        {
            mapdata.dangerpoints.add(d->lastnode);
        }
    }
    
    // Identify good sniper positions (high ground, good visibility)
    if(d->ai->personality == AI_PERSONALITY_TACTICAL && d->state == CS_ALIVE)
    {
        if(lastmillis - mapdata.lastupdated > 10000) // Update every 10 seconds
        {
            // Check if current position is a good vantage point
            if(waypoints.inrange(d->lastnode))
            {
                vec mypos = d->o;
                int visibleenemies = 0;
                
                loopv(players)
                {
                    fpsent *e = players[i];
                    if(!targetable(d, e)) continue;
                    
                    vec ep = e->headpos();
                    vec targ;
                    if(raycubelos(mypos, ep, targ))
                    {
                        visibleenemies++;
                    }
                }
                
                // Good position if we can see multiple enemies
                if(visibleenemies >= 2)
                {
                    bool alreadyknown = false;
                    loopv(mapdata.sniperpoints)
                    {
                        if(mapdata.sniperpoints[i] == d->lastnode)
                        {
                            alreadyknown = true;
                            break;
                        }
                    }
                    
                    if(!alreadyknown && mapdata.sniperpoints.length() < 20)
                    {
                        mapdata.sniperpoints.add(d->lastnode);
                    }
                }
            }
            
            mapdata.lastupdated = lastmillis;
        }
    }
}

// Use environmental knowledge
bool useenvironmentknowledge(fpsent *d, aistate &b)
{
    if(!d->ai) return false;
    
    // Avoid danger points when low health
    if(badhealth(d) && waypoints.inrange(d->lastnode))
    {
        loopv(mapdata.dangerpoints)
        {
            if(mapdata.dangerpoints[i] == d->lastnode)
            {
                // We're in a danger zone - leave immediately
                if(randomnode(d, b, SIGHTMIN, SIGHTMAX))
                {
                    return true;
                }
            }
        }
    }
    
    // Tactical/defensive bots use sniper points
    if((d->ai->personality == AI_PERSONALITY_TACTICAL || d->ai->personality == AI_PERSONALITY_DEFENSIVE) && d->gunselect == GUN_MAGNUM)
    {
        if(mapdata.sniperpoints.length() > 0 && rnd(100) < 30)
        {
            int point = mapdata.sniperpoints[rnd(mapdata.sniperpoints.length())];
            if(waypoints.inrange(point) && point != d->lastnode)
            {
                return makeroute(d, b, point);
            }
        }
    }
    
    return false;
}

}

