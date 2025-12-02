// weapon.cpp: all shooting and effects code, projectile management
#include "game.h"

namespace game
{
static const int MONSTERDAMAGEFACTOR = 4;
static const int OFFSETMILLIS = 500;
vec rays[MAXRAYS];

struct hitmsg
{
    int target, lifesequence, info1, info2;
    ivec dir;
};
vector<hitmsg> hits;

VARP(maxdebris, 10, 25, 1000);
VARP(maxbarreldebris, 5, 10, 1000);

// Physics lag compensation - delays explosions to give all players fair catch windows
VARP(phys_lag_comp, 0, 1, 1);                      // Toggle feature (default: 1 = ON)
VAR(phys_lag_comp_maxdelay, 100, 300, 1000);      // Maximum delay in milliseconds (default: 300ms)
VAR(phys_lag_comp_range, 50, 100, 500);           // Range to check for nearby players (default: 100 units)

// Test mode - simulates artificial latency for testing with bots
VARP(phys_lag_comp_testmode, 0, 0, 1);            // Enable test mode (default: 0 = OFF)
VAR(phys_lag_comp_testping, 0, 100, 500);         // Simulated ping in test mode (default: 100ms)

ICOMMAND(getweapon, "", (), intret(player1->gunselect));


int startyaw=6;  // Reduced from 12 to 6 (half intensity)
int sdir=1;
int iters=0;
int shakestarttime=0;
float shakeprogress=0.0f;

void updatescreenshake() {
    // Clock-based system: twice as fast as the slow version
    int shakeinterval = 30; // 30ms interval (twice as fast as 60ms)
    int totalduration = startyaw * shakeinterval * 2;
    
    if(lastmillis-player1->lastshake < shakeinterval) return;
    
    // Initialize shake start time ONLY if we're starting a new shake
    if(shakestarttime == 0) {
        shakestarttime = lastmillis;
        shakeprogress = 0.0f;
        sdir = 1; // Reset direction
        iters = 0;
    }
    
    int elapsed = lastmillis - shakestarttime;
    if(elapsed >= totalduration) {
        // Completely reset everything when shake ends
        player1->doshake = 0;
        shakestarttime = 0;
        shakeprogress = 0.0f;
        iters = 0;
        sdir = 1;
        return;
    }
    
    // Calculate current shake position based on time
    shakeprogress = (float)elapsed / (float)totalduration;
    float intensity = (1.0f - shakeprogress) * startyaw; // decreases over time
    
    player1->yaw += intensity * sdir;
    sdir *= -1;
    iters++;
    player1->lastshake = lastmillis;
}


void gunselect(int gun, fpsent *d)
{
    if(gun!=d->gunselect)
    {
        if(lastmillis-d->lastreload<1500)return;
        //if(player1->gunselect==GUN_ELECTRO2 && player1->burstprogress<3 && player1->burstprogress>0 && player1->ammo[player1->gunselect]>0)return;
        addmsg(N_GUNSELECT, "rci", d, gun);
        //if(d->magprogress[d->gunselect]>0) playsound(S_RELOAD, d==hudplayer()?NULL:&d->o);
        if(gun!=d->lastattackgun)d->gunwait=(d->gunselect!=GUN_CG && d->gunselect!=GUN_SMG && gun!=GUN_HANDGRENADE && gun!=GUN_CG2) ? 500 : 25;
        //if(guns[d->gunselect].attackdelay>900)d->gunwait=500;
        if(gun==GUN_TELEKENESIS)d->gunwait=80;
        if(gun==GUN_HANDGRENADE)d->attacking=d->altattacking=0;
        d->burstprogress=0;
        // Reset recoil state when switching weapons
        d->recoilPatternIndex = 0;
        if(!d->isholdingnade && !d->isholdingorb && !d->isholdingprop && !d->isholdingbarrel && !d->isholdingshock)d->lastswitch=lastmillis;
    }
    if(!d->isholdingnade && !d->isholdingorb && !d->isholdingprop && !d->isholdingbarrel && !d->isholdingshock)d->gunselect = gun;
}

void nextweapon(int dir, bool force = false)
{
    if(player1->state!=CS_ALIVE) return;
    dir = (dir < 0 ? NUMGUNS-1 : 1);
    int gun = player1->gunselect;
    loopi(NUMGUNS)
    {
        gun = (gun + dir)%NUMGUNS;
        if(force || player1->ammo[gun]) break;
    }
    if(gun != player1->gunselect) gunselect(gun, player1);
    else playsound(S_NOAMMO);
}
ICOMMAND(nextweapon, "ii", (int *dir, int *force), nextweapon(*dir, *force!=0));

//enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_MAGNUM, GUN_HANDGRENADE, GUN_ELECTRO, GUN_ELECTRO2, GUN_CG2, GUN_SHOTGUN2, GUN_SMG, GUN_SMG2, GUN_TELEKENESIS, GUN_TELEKENESIS2, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };

int getweapon(const char *name)
{
    const char *abbrevs[] = { "melee", "shotgun", "minigun", "rpg", "magnum", "hand_grenade", "beam_rifle", "0", "0", "0", "smg", "0", "telekeneis", "0", "pistol" };
    if(isdigit(name[0])) return parseint(name);
    else loopi(sizeof(abbrevs)/sizeof(abbrevs[0])) if(!strcasecmp(abbrevs[i], name)) return i;
    return -1;
}

void setweapon(const char *name, bool force = false)
{
    int gun = getweapon(name);
    if(player1->state!=CS_ALIVE || gun<GUN_FIST || gun>GUN_PISTOL) return;
    //if(force || (player1->ammo[gun] || player1->hasgun[gun])) gunselect(gun, player1);
    if(player1->ammo[GUN_CG2] && player1->hasgun[GUN_CG] && gun==GUN_CG) gunselect(gun, player1);
    if(player1->ammo[GUN_SMG2] && player1->hasgun[GUN_SMG] && gun==GUN_SMG) gunselect(gun, player1);
    if(player1->ammo[GUN_ELECTRO2] && player1->hasgun[GUN_ELECTRO] && gun==GUN_ELECTRO) gunselect(gun, player1);
    else if(player1->ammo[gun] || force) gunselect(gun, player1);
    else playsound(S_NOAMMO);
}
ICOMMAND(setweapon, "si", (char *name, int *force), setweapon(name, *force!=0));

void cycleweapon(int numguns, int *guns, bool force = false)
{
    if(numguns<=0 || player1->state!=CS_ALIVE) return;
    int offset = 0;
    loopi(numguns) if(guns[i] == player1->gunselect) { offset = i+1; break; }
    loopi(numguns)
    {
        int gun = guns[(i+offset)%numguns];
        if(gun>=0 && gun<NUMGUNS && (force || player1->ammo[gun]))
        {
            gunselect(gun, player1);
            return;
        }
    }
    playsound(S_NOAMMO);
}
ICOMMAND(cycleweapon, "sssssss", (char *w1, char *w2, char *w3, char *w4, char *w5, char *w6, char *w7),
{
             int numguns = 0;
             int guns[7];
             if(w1[0]) guns[numguns++] = getweapon(w1);
             if(w2[0]) guns[numguns++] = getweapon(w2);
             if(w3[0]) guns[numguns++] = getweapon(w3);
             if(w4[0]) guns[numguns++] = getweapon(w4);
             if(w5[0]) guns[numguns++] = getweapon(w5);
             if(w6[0]) guns[numguns++] = getweapon(w6);
             if(w7[0]) guns[numguns++] = getweapon(w7);
             cycleweapon(numguns, guns);
         });

void weaponswitch(fpsent *d)
{
    if(d->state!=CS_ALIVE) return;
    int s = d->gunselect;
    if     (s!=GUN_CG     && d->ammo[GUN_CG])     s = GUN_CG;
    else if(s!=GUN_RL     && d->ammo[GUN_RL])     s = GUN_RL;
    else if(s!=GUN_SG     && d->ammo[GUN_SG])     s = GUN_SG;
    else if(s!=GUN_MAGNUM  && d->ammo[GUN_MAGNUM])  s = GUN_MAGNUM;
    else if(s!=GUN_HANDGRENADE     && d->ammo[GUN_HANDGRENADE])     s = GUN_HANDGRENADE;
    else if(s!=GUN_PISTOL && d->ammo[GUN_PISTOL]) s = GUN_PISTOL;
    else                                          s = GUN_FIST;

    gunselect(s, d);
}

#define TRYWEAPON(w) do { \
    if(w[0]) \
{ \
    int gun = getweapon(w); \
    if(gun >= GUN_FIST && gun <= GUN_PISTOL && gun != player1->gunselect && player1->ammo[gun]) { gunselect(gun, player1); return; } \
} \
    else { weaponswitch(player1); return; } \
} while(0)
ICOMMAND(weapon, "sssssss", (char *w1, char *w2, char *w3, char *w4, char *w5, char *w6, char *w7),
{
             if(player1->state!=CS_ALIVE) return;
             TRYWEAPON(w1);
             TRYWEAPON(w2);
             TRYWEAPON(w3);
             TRYWEAPON(w4);
             TRYWEAPON(w5);
             TRYWEAPON(w6);
             TRYWEAPON(w7);
             playsound(S_NOAMMO);
         });

//    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest)
//    {
//        float f = to.dist(from)*spread/1000;
//        for(;;)
//        {
//            #define RNDD rnd(101)-50
//            vec v(RNDD, RNDD, RNDD);
//            if(v.magnitude()>50) continue;
//            v.mul(f);
//            v.z /= 2;
//            dest = to;
//            dest.add(v);
//            vec dir = dest;
//            dir.sub(from);
//            dir.normalize();
//            raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
//            return;
//        }
//    }
void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest, bool recoil)
{
    vec offset;
    //do offset = vec(rndscale(1), rndscale(1), rndscale(1));
    do offset = vec(rndscale(1), rndscale(1), rndscale(1)).sub(0.5f);
    while(offset.squaredlen() > 0.5f*0.5f);
    offset.mul((to.dist(from)/1024)*spread);
    if(recoil)offset.z += min(spread/10, 10);
    dest = vec(offset).add(to);
    vec dir = vec(dest).sub(from).normalize();
    raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
}

VARP(rifle, 0, 1, 1);
void createrays(int gun, const vec &from, const vec &to, fpsent *d)             // create random spread of rays
{
    loopi(guns[gun].rays) offsetray(from, to, (rifle&&gun==GUN_SHOTGUN2)?0:guns[gun].spread * (d->crouching ? .9f : 1), guns[gun].range, rays[i], false);
}

enum { BNC_GRENADE, BNC_GIBS, BNC_DEBRIS, BNC_BARRELDEBRIS, BNC_MISSILE, BNC_XBOLT, BNC_SMGNADE, BNC_ELECTROBOLT, BNC_ORB, BNC_WEAPON, BNC_PROP, BNC_SHELL, BNC_BARREL };

struct bouncer : physent
{
    int lifetime, bounces, gun;
    int model;
    float lastyaw, lastpitch, roll;
    bool local;
    int caught;
    int beepleft;
    fpsent *owner;
    int bouncetype, variant;
    int disttoowner;
    vec offset;
    vec flarepos;
    int flareleft;
    int wmdl; //weapon model, set to int of gun
    int offsetmillis;
    vec stoppedpos;
    //vec stoppeddir;
    int id;
    entitylight light;

    // Lag compensation fields
    bool pendingExplosion;          // True when lifetime expired but waiting for delay
    int explosionScheduledTime;     // When to actually explode (lastmillis + delay)
    int capturedOwnerPing;          // Snapshot of owner's ping at creation time
    vec frozenPos;                  // Position where object should explode (frozen at lifetime=0)


    bouncer() : bounces(0), roll(0), variant(0), rocketchan(-1), rocketsound(-1),
                pendingExplosion(false), explosionScheduledTime(0), capturedOwnerPing(0), frozenPos(0, 0, 0)
    {
        type = ENT_BOUNCE;
    }
    int rocketchan, rocketsound;
    bool lastposs;
    vec lastpos;

    ~bouncer()
    {
        if(rocketchan>=0) stopsound(rocketsound, rocketchan);
    }
};

vector<bouncer *> bouncers;

vec hudgunorigin(int gun, const vec &from, const vec &to, fpsent *d);

void newbouncer(const vec &from, const vec &to, bool local, int id, fpsent *owner, int type, int lifetime, int speed, entitylight *light = NULL)
{
    bouncer &bnc = *bouncers.add(new bouncer);
    bnc.o = from;
    bnc.radius = bnc.xradius = bnc.yradius = (type==BNC_DEBRIS || type==BNC_SHELL) ? 1.f : 1.5f;
    if(bnc.bouncetype==BNC_PROP)bnc.radius=bnc.xradius=bnc.yradius=10.f;
    if(bnc.bouncetype==BNC_XBOLT)bnc.radius=bnc.xradius=bnc.yradius=2.f;
    if(bnc.bouncetype==BNC_WEAPON)bnc.radius=bnc.xradius=bnc.yradius=5.f;
    bnc.eyeheight = bnc.radius;
    bnc.aboveeye = bnc.radius;
    bnc.lifetime = lifetime;
    bnc.local = local;
    bnc.wmdl = owner->gunselect;
    bnc.flarepos = owner->o;
    bnc.owner = owner;
    bnc.caught=0;
    bnc.gun = owner->gunselect;
    bnc.bouncetype = type;
    bnc.beepleft=20;
    bnc.flareleft=5;
    
    // Snapshot owner's ping for lag compensation (or use test ping in test mode)
    if(phys_lag_comp_testmode) {
        bnc.capturedOwnerPing = phys_lag_comp_testping;
    } else {
        bnc.capturedOwnerPing = owner->ping;
    }
    //bnc.model=model;
    //strncpy(owner->propmodeldir, bnc.model, 30);
    //bnc.model=bnc.bouncetype==BNC_PROP?"xeno/box1":"dcp/barrel"; //
    if(bnc.bouncetype==BNC_BARREL || bnc.bouncetype==BNC_PROP)bnc.model=owner->propmodeldir;
    //conoutf(CON_GAMEINFO, "%s", bnc.model);
    bnc.id = local ? lastmillis : id;
    if(light) bnc.light = *light;



    switch(type)
    {
    case BNC_DEBRIS: case BNC_BARRELDEBRIS: bnc.variant = rnd(4); break;
    case BNC_GIBS: bnc.variant = rnd(3); break;
    }

    vec dir(to);
    dir.sub(from).normalize();
    bnc.vel = dir;
    bnc.vel.mul(speed);

    avoidcollision(&bnc, dir, owner, 0.1f);

    if(type==BNC_GRENADE || type==BNC_XBOLT || type==BNC_SMGNADE || type==BNC_ELECTROBOLT || type==BNC_ORB || type==BNC_PROP || type==BNC_BARREL)
    {
        bnc.offset = hudgunorigin(bnc.gun, from, to, owner);
        if(owner==hudplayer() && !isthirdperson()) bnc.offset.sub(owner->o).rescale(16).add(owner->o);
    }

    else bnc.offset = from;
    bnc.offset.sub(bnc.o);
    bnc.offsetmillis = OFFSETMILLIS;

    bnc.resetinterp();
}

void bounced(physent *d, const vec &surface)
{
    if(d->type != ENT_BOUNCE) return;
    bouncer *b = (bouncer *)d;
    if(b->bouncetype==BNC_ORB||b->bouncetype==BNC_XBOLT){ particle_splash(PART_SPARK, 40, 150, b->o, 0xFFFFFF, 0.1f); if(b->bouncetype==BNC_ORB){playsound(S_ORBBOUNCE, &b->o);}}
    if(b->bouncetype==BNC_XBOLT)playsound(S_NADEBOUNCE+rnd(3), &b->o);
    if((b->bouncetype != BNC_GIBS && b->bouncetype!=BNC_GRENADE && b->bouncetype!=BNC_MISSILE && b->bouncetype!=BNC_SMGNADE && b->bouncetype!=BNC_XBOLT && b->bouncetype!=BNC_PROP && b->bouncetype!=BNC_BARREL) || b->bounces >= 4) return;
    if(b->bouncetype==BNC_XBOLT)b->stoppedpos=b->o;
    b->bounces++;
    if(b->bouncetype==BNC_GIBS){ adddecal(DECAL_BLOOD, vec(b->o).sub(vec(surface).mul(b->radius)), surface, 6.96f/b->bounces*2, bvec(0x99, 0xFF, 0xFF), rnd(4));
        playsound(S_BNC_GIB1+rnd(3), &b->o);}
    if(b->bouncetype==BNC_BARREL || b->bouncetype==BNC_SMGNADE)adddecal(DECAL_SCORCH, vec(b->o).sub(vec(surface).mul(b->radius)), surface, guns[b->bouncetype==BNC_BARREL?GUN_BARREL:GUN_SMG2].splash/2);
    if(b->bouncetype==BNC_GRENADE || b->bouncetype==BNC_BARREL)playsound(S_NADEBOUNCE+rnd(3), &b->o);
}

float projdist(dynent *o, vec &dir, const vec &v)
{
    vec middle = o->o;
    middle.z += (o->aboveeye-o->eyeheight)/2;
    float dist = middle.dist(v, dir);
    dir.div(dist);
    if(dist<0) dist = 0;
    return dist;
}
HVARP(nadetrailcol, 0x000000, 0x0789FC, 0xFFFFFF);
void updatebouncers(int time)
{
    loopv(bouncers)
    {
        bool stopped = false;

        bouncer &bnc = *bouncers[i];

        int lastdist=bnc.o.dist(bnc.owner->o);
        if(bnc.bouncetype==BNC_SMGNADE && bnc.vel.magnitude() > 50.0f)
        {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            if(lookupmaterial(pos)==MAT_WATER)
                regular_particle_splash(PART_BUBBLE, 1, 500, pos, 0xFFFFFF, 1.0f, 25, 500);
            else
                regular_particle_splash(PART_SMOKE, 1, 150, pos, 0x404040, 2.4f, 50, -20);

        }
        //            if(bnc.bouncetype==BNC_XBOLT)
        //            {
        //                vec pos(bnc.o);
        //                pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
        //                bool del;
        //                loopi(numdynents())
        //                {
        //                    dynent *o = iterdynents(i);
        //                    vec dir;
        //                    float dist = projdist(o, dir, pos);
        //                    if(bnc.owner==o || o->state!=CS_ALIVE)continue;
        //                    if(dist<(25)) {
        //                        explode(bnc.local, bnc.owner, bnc.o, NULL, guns[GUN_CROSSBOW].damage, GUN_CROSSBOW);
        //                        del=true;
        //                        if(bnc.local)
        //                            addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, GUN_CROSSBOW, bnc.id-maptime,
        //                                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        //                    }
        //                }
        //                if(del)delete bouncers.remove(i--);
        //            }
        if(bnc.bouncetype==BNC_GRENADE)
        {
            vec pos(bnc.o);
            bnc.beepleft-=1;
            bnc.flareleft-=1;
            if(!bnc.beepleft){playsound(S_NADEBEEP, &pos);  bnc.beepleft=20; }
            if(pos.dist(bnc.flarepos)>1) {particle_flare(bnc.flarepos, pos, 500, PART_RAILTRAIL, nadetrailcol, .2f); bnc.flarepos=bnc.o; bnc.flareleft=5;}
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            if(lookupmaterial(pos)==MAT_WATER)
                regular_particle_splash(PART_BUBBLE, 1, 500, pos, 0xFFFFFF, 1.0f, 25, 500);
            else {
                //regular_particle_splash(PART_SMOKE, 6, 500, pos, 0xFF0000, 1.2f, 1, 0);
            }
        }
        if(bnc.bouncetype==BNC_DEBRIS && bnc.vel.magnitude() > 10.0f)
        {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            //regular_particle_splash(PART_SMOKE, 1, 25, pos, 0x404040, 1.2f, 50, -20);
            regular_particle_splash(PART_SPARK, 1, 25, pos, 0x553322, 1.2f, 50, -20);
        }

        if (bnc.bouncetype == BNC_BARRELDEBRIS) {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis / float(OFFSETMILLIS)));
            //regular_particle_flame(PART_FLAME, pos, 1, 1, 0x903020, 3, 4.0f);
            //regular_particle_flame(PART_SMOKE, pos, 1, 1, 0x303020, 1, 8.0f, 100.0f, 2000.0f, -20);
        }

        if(bnc.bouncetype==BNC_GIBS && bnc.vel.magnitude() > 50.f)
        {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            regular_particle_splash(PART_BLOOD, bnc.vel.magnitude()/50, 300, pos, 0x99FFFF, 2.0f, 50*2); // 1000, 100);
            //                regular_particle_splash(PART_BLOOD, 5, 300, pos, 0x99FFFF, .6f, 1, 0, 0); // 1000, 100);

        }
        if(bnc.bouncetype==BNC_SHELL && bnc.vel.magnitude() > 15.f)
        {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            if(lookupmaterial(pos)!=MAT_WATER)
                regular_particle_splash(PART_SMOKE, 1, 150, pos, 0x404040, .8f, 50, -20);

        }
        if(bnc.bouncetype==BNC_ELECTROBOLT && bnc.vel.magnitude() > 50.f)
        {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            //vec oldpos(bnc.o);
            regular_particle_splash(PART_SPARK, 5, 250, pos, 0x0789FC, 0.4f, 5, 0);
            //particle_flare(old, pos, 500, PART_STREAK, 0x0789FC, 1.f);
        }
        if(bnc.bouncetype==BNC_ORB||bnc.bouncetype==BNC_PROP) // this logic is responsible for making the proj blow up when it's close to you
        {
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            vec occlusioncheck;
            if(raycubelos(pos, camera1->o, occlusioncheck))
            {
                if(bnc.bouncetype==BNC_ORB)particle_flare(pos, pos, 75, PART_GLOW, 0x553322, 8.0f);
            }
            if(bnc.o.dist(player1->o)<16 && bnc.bouncetype==BNC_PROP && bnc.vel.magnitude()<80)
            {
                if(player1->lastattackgun!=GUN_TELEKENESIS || lastmillis-player1->lastaction>300){
                    bnc.vel.x=player1->vel.x*2.f;
                    bnc.vel.y=player1->vel.y*2.f;
                    //bnc.vel.z=30; //make slightly randomized so two objects don't float into the air stuck together
                }
            }
            loopv(bouncers)
            {
                bouncer &b = *bouncers[i];
                if(b.bouncetype!=BNC_PROP || bnc.bouncetype!=BNC_PROP)continue;
                if(bnc.o.dist(b.o)<18&&bnc.id!=b.id){
                    bnc.vel.x*=-.5f; bnc.vel.y*=-.5f;
                    //bnc.vel.z=5.f+rnd(20);
                    //b.vel.x=bnc.vel.x*1.5f;
                    //b.vel.y=bnc.vel.y*1.5f;
                }
            }
            loopi(numdynents())
            {
                dynent *o = iterdynents(i);
                vec dir;
                float dist = projdist(o, dir, pos);
                if(bnc.owner==o || o->state!=CS_ALIVE || o->type==ENT_INANIMATE)continue; //don't let this apply to yourself if it's your orb; check explode to make no damage to yourself on direct impact
                if(dist<(bnc.bouncetype==BNC_PROP?16:14) && bnc.vel.magnitude()>250.f) {
                    explode(bnc.local, bnc.owner, bnc.o, NULL, guns[bnc.bouncetype==BNC_ORB?GUN_CG2:GUN_BITE].damage, bnc.bouncetype==BNC_ORB?GUN_CG2:GUN_BITE); //ignore quad in this case for orbs ^_^
                    if(bnc.local)
                        addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, bnc.bouncetype==BNC_ORB?GUN_CG2:GUN_BITE, bnc.id-maptime,
                               hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                }
            }
        }



        if(bnc.bouncetype==BNC_MISSILE && bnc.vel.magnitude() > 50)
        {
            vec pos(bnc.o);
            //pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            vec occlusioncheck;
            if(raycubelos(pos, camera1->o, occlusioncheck))
            {
                particle_flare(pos, pos, 75, PART_GLOW, 0x553322, 8.0f);
            }
            if(lookupmaterial(pos)!=MAT_WATER)
                regular_particle_splash(PART_SMOKE, 2, 300, pos, 0x555555, 2.4f, 12, 502);
            if(lookupmaterial(camera1->o)!=MAT_WATER && lookupmaterial(pos)!=MAT_WATER)
            {
                stopsound(S_ROCKETUW, bnc.rocketchan);
                bnc.rocketsound = S_ROCKET;
            }
            if(lookupmaterial(camera1->o)==MAT_WATER || lookupmaterial(pos)==MAT_WATER)
            {
                stopsound(S_ROCKET, bnc.rocketchan);
                bnc.rocketsound = S_ROCKETUW;
            }
            if(lookupmaterial(pos)!=MAT_WATER)
            {
                vec check;
                //regular_particle_splash(PART_SMOKE, 16, 300, pos, 0x444444, 2.4f, 12, 502);
                regular_particle_splash(PART_SMOKE, 8, 500, pos, 0x444444, 1.2f, 12, 500, NULL, NULL, 2);
                if(raycubelos(pos, camera1->o, check))
                {
                    //particle_flare(pos, pos, 1, PART_MUZZLE_FLASH5, 0xFFFFFF, 10.0f+rndscale(5), NULL);
                    particle_flare(pos, pos, 1, PART_GLOW, 0xFFC864, 5.0f+rndscale(5), NULL);
                }
                particle_fireball(pos, 2.0f, PART_EXPLOSION, -1, 0xA0C080, 2.0f);
                regular_particle_splash(PART_FLAME1, 2, 50, pos, 0xFFFFFF, 1.2f, 25, 500);
                regular_particle_splash(PART_FLAME2, 2, 25, pos, 0xFFFFFF, 1.2f, 25, 500);
                regular_particle_splash(PART_SPARK, 8, 100, pos, 0xFFC864, 0.4f, 50, 500);
                regular_particle_splash(PART_FLAME3, 1, 100, pos, 0xFFFFFF, 0.8f, 50, 500);
                regular_particle_splash(PART_FLAME4, 1, 100, pos, 0xFFFFFF, 0.8f, 50, 500);
                //                     if(bnc.owner->quadmillis)
                //                     {
                //                         regular_particle_splash(PART_FLAME1, 2, 200, pos, 0xFF0000, 2.4f, 25, 777);
                //                         regular_particle_splash(PART_FLAME2, 2, 100, pos, 0xFF0000, 2.4f, 25, 777);
                //                         regular_particle_splash(PART_SPARK, 8, 400, pos, 0xFF0000, 0.4f, 50, 777);
                //                         regular_particle_splash(PART_FLAME3, 1, 400, pos, 0xFF0000, 0.8f, 50, 777);
                //                         regular_particle_splash(PART_FLAME4, 1, 400, pos, 0xFF0000, 0.8f, 50, 777);
                //                     }
            }
            else
            {
                regular_particle_splash(PART_BUBBLE, 4, 500, pos, 0xFFFFFF, 0.5f, 25, 500);
            }
            {

                //p.to.x = p.to.y = p.to.z = 0.f;
                vecfromyawpitch(bnc.owner->yaw, bnc.owner->pitch, 1, 0, worldpos);
                //float barrier = raycube(bnc.owner->o, worldpos, guns[GUN_RL].range, RAY_CLIPMAT|RAY_ALPHAPOLY);
                //bnc.vel(barrier).add(bnc.owner->o);
                pos.add(worldpos).mul(bnc.owner->o);
                //player1->guiding=1;

                //guiding code goes here
            }
            bnc.rocketchan = playsound(bnc.rocketsound, &pos, NULL, -1, 128, bnc.rocketchan);

        }
        if(bnc.bouncetype==BNC_ORB || bnc.bouncetype==BNC_GRENADE || (bnc.bouncetype==BNC_PROP && bnc.vel.magnitude()>300) || bnc.bouncetype==BNC_BARREL)
        {
            loopv(players)
            {
                fpsent *d = players[i];
                if(!d->ai || bnc.o.dist(d->o)>guns[GUN_TELEKENESIS2].range || d->isholdingbarrel || d->isholdingnade || d->isholdingorb || d->isholdingprop || d->isholdingshock)continue;
                vec check;
                if(raycubelos(bnc.o, d->o, check) && bnc.owner!=d && bnc.o.dist(d->o)<=guns[GUN_TELEKENESIS2].range) { d->aicatch=bnc.o; d->aicancatch=1; }
                else d->aicancatch=0;
            }
        }


        vec old(bnc.o);
        
        // Skip physics for frozen bouncers (lag compensation active)
        // But DON'T skip the explosion check at the end!
        if(bnc.pendingExplosion && (bnc.bouncetype == BNC_GRENADE || bnc.bouncetype == BNC_ORB || bnc.bouncetype == BNC_PROP)) {
            // Object is frozen, keep it at frozen position
            bnc.o = bnc.frozenPos;
            stopped = false; // Skip physics, but let explosion logic run
        }
        else if(bnc.bouncetype==BNC_ORB) stopped = bounce(&bnc, 1.001f, 1.0001f, 0.0f) || (bnc.lifetime -= time)<0;
        else if(bnc.bouncetype==BNC_GRENADE) stopped = bounce(&bnc, 0.45f, 0.5f, 0.8f) || (bnc.lifetime -= time)<0;
        else if(bnc.bouncetype==BNC_SHELL)stopped = bounce(&bnc, .6f, .8f, .8f) || (bnc.lifetime -= time)<0;
        else if(bnc.bouncetype==BNC_SMGNADE) stopped = bounce(&bnc, 0.6f, 0.5f, 0.6f) || (bnc.bounces) > 0;
        else if(bnc.bouncetype==BNC_PROP) stopped = bounce(&bnc, 0.4f, 0.5f, 1.f)||(bnc.lifetime -= time)<0;
        else if(bnc.bouncetype==BNC_BARREL) stopped = bounce(&bnc, 0.6f, 0.5f, 0.99f) || (bnc.bounces) > 0;
        else if(bnc.bouncetype==BNC_XBOLT) stopped = bounce(&bnc, 1.f, 0.5f, 0.8f) || bnc.o.dist(bnc.owner->o)<lastdist || (bnc.lifetime -= time)<0;
        else if(bnc.bouncetype==BNC_ELECTROBOLT) stopped = bounce(&bnc, 0.6f, 0.5f, 0.8f) || (bnc.lifetime -= time)<0 || bnc.owner->detonateelectro;
        else if(bnc.bouncetype==BNC_MISSILE) stopped = bounce(&bnc, 0.6f, 0.5f, 0.0f) || (bnc.lifetime -= time)<0;
        else if(bnc.bouncetype==BNC_WEAPON) stopped = bounce(&bnc, .6f, .8f, .8f) || (bnc.lifetime -= time)<0 || bnc.caught==1;
        else if(bnc.bouncetype==BNC_BARRELDEBRIS) stopped = bounce(&bnc, .6f, .8f, .6f) || (bnc.lifetime -= time)<0;
        else
        {
            // cheaper variable rate physics for debris, gibs, etc.
            for(int rtime = time; rtime > 0;)
            {
                int qtime = min(30, rtime);
                rtime -= qtime;
                if((bnc.lifetime -= qtime)<0 || bounce(&bnc, qtime/1000.0f, 0.6f, 0.5f, 1)) { stopped = true; break; }
            }
        }

        //            if(bnc.bouncetype==BNC_XBOLT)
        //            {
        //                vec pos(bnc.o);
        //                pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
        //                if(bnc.o.dist(bnc.owner->o)<lastdist) { bnc.o=bnc.stoppedpos; bnc.vel.x=bnc.vel.y=0; bnc.vel.z=1.f; }
        //            }


        if(stopped)
        {

            if(bnc.bouncetype == BNC_SMGNADE || bnc.bouncetype == BNC_XBOLT)
            {
                int qdam;
                if(bnc.bouncetype==BNC_XBOLT || bnc.bouncetype==BNC_GRENADE) qdam = guns[bnc.bouncetype==BNC_XBOLT ? GUN_CROSSBOW : GUN_HANDGRENADE].damage*(bnc.owner->quadmillis ? 2 : 1);
                else if(bnc.bouncetype==BNC_SMGNADE) qdam = guns[GUN_SMG2].damage*(bnc.owner->quadmillis ? 2 : 1);
                hits.setsize(0);
                if(bnc.bouncetype==BNC_SMGNADE) explode(bnc.local, bnc.owner, bnc.o, NULL, qdam, GUN_SMG2);
                else explode(bnc.local, bnc.owner, bnc.o, NULL, qdam, bnc.bouncetype==BNC_XBOLT ? GUN_CROSSBOW : GUN_HANDGRENADE);
                if(bnc.bouncetype==BNC_GRENADE)adddecal(DECAL_SCORCH, bnc.o, vec(0, 0, 1), guns[GUN_SMG2].splash/2);
                if(bnc.bouncetype==BNC_XBOLT)adddecal(DECAL_BULLET, bnc.stoppedpos, vec(0, 0, 1), 1.5f);
                if(bnc.local)
                {
                    if(bnc.bouncetype==BNC_XBOLT || bnc.bouncetype==BNC_GRENADE) addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, bnc.bouncetype==BNC_XBOLT ? GUN_CROSSBOW : GUN_HANDGRENADE, bnc.id-maptime,
                                                                                        hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                    else if(bnc.bouncetype==BNC_SMGNADE)addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, GUN_SMG2, bnc.id-maptime,
                                                               hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                }
            }

            else if(bnc.bouncetype==BNC_ELECTROBOLT || bnc.bouncetype==BNC_BARREL)
            {
                int qdam = guns[bnc.bouncetype==BNC_BARREL?GUN_BARREL:GUN_ELECTRO2].damage*(bnc.owner->quadmillis ? 2 : 1);
                hits.setsize(0);
                explode(bnc.local, bnc.owner, bnc.o, NULL, qdam, bnc.bouncetype==BNC_BARREL?GUN_BARREL:GUN_ELECTRO2);
                if(bnc.bouncetype==BNC_ELECTROBOLT)adddecal(DECAL_SCORCH, bnc.o, vec(0, 0, 1), guns[GUN_ELECTRO2].splash/2);
                if(bnc.local)
                    addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, bnc.bouncetype==BNC_BARREL?GUN_BARREL:GUN_ELECTRO2, bnc.id-maptime,
                           hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            }
            else if(bnc.bouncetype == BNC_GRENADE || bnc.bouncetype==BNC_ORB)
            {
                if(bnc.local)
                    addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, bnc.bouncetype==BNC_PROP?GUN_BITE:GUN_FIREBALL, bnc.id-maptime, //fireball = orb for server and bite=prop for server
                           hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                //playsound(S_NEXIMPACT, &bnc.o);
                //particle_fireball(bnc.o, guns[GUN_CG2].splash, PART_EXPLOSION, 150, 0xFFFFFF, guns[GUN_CG2].splash);
            }
            //                else if(bnc.bouncetype==BNC_BARREL)
            //                {
            //                    int qdam=guns[GUN_BARREL].damage*(bnc.owner->quadmillis?4:1);
            //                    if(bnc.local)
            //                        addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, GUN_BARREL, bnc.id-maptime,
            //                                hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            //                    explode(bnc.local, bnc.owner, bnc.o, NULL, qdam, GUN_BARREL);

            //                }
            else if(bnc.bouncetype==BNC_WEAPON) //and the player is hudplayer???
            {
                int maxammo = 0;

                switch(bnc.gun)
                {
                case GUN_FIST:
                case GUN_TELEKENESIS:
                case GUN_TELEKENESIS2:
                    maxammo = 1;
                    break;
                case GUN_SMG:
                    maxammo = 140;
                    break;


                case GUN_RL:
                    maxammo = 15; break;
                case GUN_CG2:
                    maxammo=3; break;
                case GUN_SMG2:
                    maxammo = 5;
                    break;
                case GUN_MAGNUM:
                    maxammo = 24;
                    break;

                case GUN_SG:
                case GUN_SHOTGUN2:
                    maxammo = 30;
                    break;
                case GUN_HANDGRENADE: maxammo = 5; break;

                case GUN_CG:
                    maxammo = 120;
                    break;
                case GUN_ELECTRO2:
                    maxammo = 80;
                    break;
                case GUN_ELECTRO:
                    maxammo = 30;
                    break;
                case GUN_CROSSBOW:
                    maxammo = 10;
                    break;

                case GUN_PISTOL:
                    maxammo = 150;
                    break;
                }
                if(player1->feetpos().dist(bnc.o)<50) {
                    player1->hasgun[bnc.gun]=1;
                    player1->ammo[bnc.gun]=maxammo;
                    if(bnc.gun==GUN_SG)player1->ammo[GUN_SHOTGUN2]=maxammo;
                    if(bnc.gun==GUN_SHOTGUN2)player1->ammo[GUN_SG]=maxammo;
                    msgsound(S_ROCKETPICKUP, player1);
                    conoutf(CON_TEAMCHAT, "You picked up a %s", guns[bnc.gun].name);
                }
            }
            if(bnc.bouncetype!=BNC_ORB && bnc.bouncetype!=BNC_PROP && bnc.bouncetype!=BNC_GRENADE) delete bouncers.remove(i--);
        }
        else
        {
            bnc.roll += old.sub(bnc.o).magnitude()/(4*RAD);
            bnc.offsetmillis = max(bnc.offsetmillis-time, 0);
        }
        
        // ============================================================================
        // PHYSICS LAG COMPENSATION SYSTEM (with Test Mode)
            // ============================================================================
            // Problem: In clientside prediction, if player2 catches a grenade but the 
            // message doesn't reach player1 in time, player1's grenade explodes and 
            // damages player2 even though on player2's screen they caught it.
            //
            // Solution: Delay explosion by (thrower_ping + max_nearby_player_ping) to 
            // give all nearby players a fair window to catch the object.
            //
            // Toggle:     phys_lag_comp (0=off, 1=on, default: 1)
            // Max delay:  phys_lag_comp_maxdelay (default: 300ms)
            // Check range: phys_lag_comp_range (default: 100 units)
            //
            // TEST MODE (for bot testing):
            // Enable:     phys_lag_comp_testmode 1
            // Test ping:  phys_lag_comp_testping 100 (simulates 100ms ping for all players)
            // Example:    Test 200ms total delay = testping 100 + bot simulated 100 = 200ms delay
            // ============================================================================
            
            // Handle grenade/orb/prop explosion with lag compensation
            if (bnc.bouncetype == BNC_ORB || bnc.bouncetype == BNC_PROP || bnc.bouncetype == BNC_GRENADE) {
                // Debug: Check if we're even processing pending explosions
                if(phys_lag_comp_testmode && bnc.pendingExplosion) {
                    conoutf(CON_GAMEINFO, "\f6[LAG COMP DEBUG] Processing pending explosion: lastmillis=%d, scheduledTime=%d, timeLeft=%d", 
                            lastmillis, bnc.explosionScheduledTime, bnc.explosionScheduledTime - lastmillis);
                }
                
                // Only decrement lifetime if not already pending explosion (frozen)
                if (!bnc.pendingExplosion) {
                    bnc.lifetime -= time;
                }
                
                // Check if lifetime has expired but explosion hasn't been scheduled yet
                if (bnc.lifetime <= 0 && !bnc.pendingExplosion) {
                    // FREEZE the bouncer at this position
                    bnc.frozenPos = bnc.o;
                    bnc.vel = vec(0, 0, 0); // Stop all movement
                    
                    if (phys_lag_comp && bnc.local) {
                        // Calculate delay based on owner's ping + max nearby player ping
                        int maxNearbyPing = 0;
                        
                        if(phys_lag_comp_testmode) {
                            // TEST MODE: Use simulated ping for all players (including bots)
                            maxNearbyPing = phys_lag_comp_testping;
                            conoutf(CON_GAMEINFO, "\f2[LAG COMP TEST] Object frozen at (%.1f, %.1f, %.1f), simulating %dms + %dms = %dms delay", 
                                    bnc.frozenPos.x, bnc.frozenPos.y, bnc.frozenPos.z,
                                    bnc.capturedOwnerPing, maxNearbyPing, bnc.capturedOwnerPing + maxNearbyPing);
                        } else {
                            // NORMAL MODE: Check real player pings
                            loopv(players) {
                                fpsent *p = players[i];
                                if (p && p != bnc.owner && p->state == CS_ALIVE) {
                                    float dist = p->o.dist(bnc.frozenPos);
                                    // Only consider players within catch range
                                    if (dist < phys_lag_comp_range) {
                                        maxNearbyPing = max(maxNearbyPing, p->ping);
                                    }
                                }
                            }
                        }
                        
                        // Calculate total delay: owner's ping (at throw time) + max nearby ping
                        int explosionDelay = bnc.capturedOwnerPing + maxNearbyPing;
                        
                        // Cap delay at maximum to prevent excessive lag
                        explosionDelay = min(explosionDelay, phys_lag_comp_maxdelay);
                        
                        // Schedule explosion
                        bnc.pendingExplosion = true;
                        bnc.explosionScheduledTime = lastmillis + explosionDelay;
                        if(phys_lag_comp_testmode) {
                            conoutf(CON_GAMEINFO, "\f3[LAG COMP DEBUG] Scheduled explosion: lastmillis=%d, explosionTime=%d, delay=%d", 
                                    lastmillis, bnc.explosionScheduledTime, explosionDelay);
                        }
                    } else {
                        // Lag comp disabled or not local - explode immediately
                        bnc.pendingExplosion = true;
                        bnc.explosionScheduledTime = lastmillis;
                    }
                }
                
                // Check if it's time to actually explode
                if (bnc.pendingExplosion && lastmillis >= bnc.explosionScheduledTime) {
                    if(phys_lag_comp_testmode) {
                        conoutf(CON_GAMEINFO, "\f3[LAG COMP DEBUG] Explosion check TRUE! lastmillis=%d, scheduledTime=%d, diff=%d", 
                                lastmillis, bnc.explosionScheduledTime, lastmillis - bnc.explosionScheduledTime);
                    }
                    // Trigger explosion AT THE FROZEN POSITION
                if (bnc.bouncetype == BNC_GRENADE) {
                    int qdam = guns[GUN_HANDGRENADE].damage * (bnc.owner->quadmillis ? 2 : 1);
                    hits.setsize(0);
                        explode(bnc.local, bnc.owner, bnc.frozenPos, NULL, qdam, GUN_HANDGRENADE);
                        adddecal(DECAL_SCORCH, bnc.frozenPos, vec(0, 0, 1), guns[GUN_HANDGRENADE].splash / 2);
                    if (bnc.local) {
                        addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis - maptime, GUN_HANDGRENADE, bnc.id - maptime,
                            hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
                    }
                        if(phys_lag_comp_testmode) {
                            conoutf(CON_GAMEINFO, "\f1[LAG COMP TEST] Grenade exploded at frozen position (%.1f, %.1f, %.1f)!", 
                                    bnc.frozenPos.x, bnc.frozenPos.y, bnc.frozenPos.z);
                        }
                    }
                    else if (bnc.bouncetype == BNC_ORB || bnc.bouncetype == BNC_PROP) {
                        // Orbs and props - explode/damage at frozen position
                        if (bnc.local) {
                            // For orbs, we want to damage players at the frozen position
                            int qdam = bnc.bouncetype == BNC_ORB ? guns[GUN_CG2].damage * (bnc.owner->quadmillis ? 4 : 1) : 
                                       guns[GUN_BITE].damage * (bnc.owner->quadmillis ? 2 : 1);
                            hits.setsize(0);
                            explode(bnc.local, bnc.owner, bnc.frozenPos, NULL, qdam, 
                                   bnc.bouncetype == BNC_PROP ? GUN_BITE : GUN_FIREBALL);
                            
                            addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis - maptime, 
                                   bnc.bouncetype == BNC_PROP ? GUN_BITE : GUN_FIREBALL, bnc.id - maptime,
                                   hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
                        }
                        if(phys_lag_comp_testmode) {
                            conoutf(CON_GAMEINFO, "\f1[LAG COMP TEST] %s exploded at frozen position!", 
                                    bnc.bouncetype == BNC_ORB ? "Orb" : "Prop");
                        }
                    }
                    // Remove the bouncer after explosion
                    delete bouncers.remove(i--);
                }
            }
    }
}

void removebouncers(fpsent *owner)
{
    loopv(bouncers) if(bouncers[i]->owner==owner) { delete bouncers[i]; bouncers.remove(i--); }
}

void clearbouncers() { bouncers.deletecontents(); }

struct projectile
{
    vec dir, o, to, offset;
    float speed;
    fpsent *owner;
    int gun;
    bool local;
    int offsetmillis;
    int id;
    entitylight light;
    int rocketchan, rocketsound;
    bool lastposs;
    vec lastpos;

    projectile() : rocketchan(-1), rocketsound(-1)
    {
    }
    ~projectile()
    {
        if(rocketchan>=0) stopsound(rocketsound, rocketchan);
    }
};
vector<projectile> projs;

void clearprojectiles() { projs.shrink(0); }

void newprojectile(const vec &from, const vec &to, float speed, bool local, int id, fpsent *owner, int gun)
{
    projectile &p = projs.add();
    p.dir = vec(to).sub(from).normalize();
    p.o = from;
    p.to = to;
    p.offset = hudgunorigin(gun, from, to, owner);
    p.offset.sub(from);
    p.speed = speed;
    p.local = local;
    p.owner = owner;
    p.gun = gun;
    p.offsetmillis = OFFSETMILLIS;
    p.id = local ? lastmillis : id;
}

void removeprojectiles(fpsent *owner)
{
    // can't use loopv here due to strange GCC optimizer bug
    int len = projs.length();
    loopi(len) if(projs[i].owner==owner) { projs.remove(i--); len--; }
}

VARP(blood, 0, 1, 1);

void damageeffect(int damage, fpsent *d, bool thirdperson)
{
    vec p = d->o;
    p.z += 0.6f*(d->eyeheight + d->aboveeye) - d->eyeheight;
    //if(damage==10||damage==30)d->vel.x=d->vel.y=0;
    if(damage)playsound(S_SCH1+rnd(3), d==player1?NULL:&d->o);
    if(damage>500)
    {
        //loopi(7) particle_splash(PART_SNOW, 1, 1000, p, 0xFFFFFF, 10.f, 18, rnd(3)-6);
        loopi(3)playsound(S_LIGHTNING, &p);
    }
    if(d==player1&& damage)
    {
        float droll = damage/1.9f;
        player1->roll += player1->roll>0 ? droll : (player1->roll<0 ? -droll : (rnd(2) ? droll : -droll));
    }
    //int hp=d->health+d->armour;
    if(d->health<0)d->dropitem=1;
}

VARP(spawnbouncerttl, 0, 15000, 60000);
void spawnbouncer(const vec &p, const vec &vel, fpsent *d, int type, int speed, entitylight *light = NULL)
{
    if(type==BNC_DEBRIS)return;
    vec to(rnd(100)-50, rnd(100)-50, rnd(100)-50);
    if(to.iszero()) to.z += 1;
    to.normalize();
    to.add(p);
    /*if (d != player1 && worldpos.dist(d->o)<200 && guns[d->diedgun].splash <= 30 && type == BNC_GIBS)
    {
        to=worldpos;
        to.x+=rnd(15)-30;
        to.y+=rnd(15)-30;
        to.z+=rnd(15)-30;
    }*/
    int timer = type!=BNC_DEBRIS?rnd(1000)+spawnbouncerttl:3000;
    if(type==BNC_GRENADE || type==BNC_ORB || type==BNC_PROP || type==BNC_BARREL)timer=type==BNC_GRENADE?1500:(type==BNC_ORB?5000:20000);
    if(type==BNC_WEAPON)timer=20000;
    newbouncer(p, to, true, 0, d, type, timer, rnd(100)+speed, NULL); //default speed was rnd(100)+20
}

VARP(gibnum, 1, 10, 1000);
void gibeffect(int damage, const vec &vel, fpsent *d)
{
    vec from = d->o;
    //if(blood || damage <= 40) //return;

    if (d->diedbyheadshot && !d->quadmillis) {
        // lookup the playermodel, spawn gib for head
        spawnbouncer(d->headpos(), vel, d, BNC_GIBS, damage / 2);
    }


    /*if (damage < 40)
    {
        //loopi(min(gibnum, 50)+1) diebouncer(from, vel, d, BNC_DEBRIS); //20 gibnum
    }
    else if(damage<500 && blood && guns[d->diedgun].splash>30)
    {
        loopi(min(gibnum, 50)+1) spawnbouncer(from, vel, d, BNC_GIBS, damage/2);
        particle_splash(PART_BLOOD, damage*3, 10000, d->o, 0x99FFFF, 2.f, 4000); //damage/5
        //particle_splash(PART_SMOKE, 5, 200, d->feetpos(), 0xFF0000, 12.0f, 50, 250, NULL, 1, false, 4);
        //particle_splash(PART_SMOKE, 5, 1250, d->feetpos(), 0xFF0000, 12.0f, 50, 250, NULL, 1, false, 3);
        playsound (S_GIB, &d->o);
    }*/
    //if(d->gunselect!=GUN_TELEKENESIS)
    //spawnbouncer(d->o, vel, d, BNC_WEAPON, damage/4);
    int proptype;
    if(d->isholdingnade)proptype = BNC_GRENADE;
    if(d->isholdingorb)proptype = BNC_ORB;
    if(d->isholdingprop)proptype = BNC_PROP;
    if(d->isholdingbarrel)proptype=BNC_BARREL;
    if(d->isholdingbarrel||d->isholdingnade||d->isholdingorb||d->isholdingprop||d->gunselect==GUN_HANDGRENADE)
    {
        spawnbouncer(from, vel, d, proptype, damage/3);
        //shoteffects(d->gunselect, from, vel, d, true, 0, d->lastaction);
        if(d==player1 || d->ai)
        {
            addmsg(N_SHOOT, "rici2i6iv", 0, d, lastmillis-maptime, d->gunselect,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
                   (int)(vel.x*DMF), (int)(vel.y*DMF), (int)(vel.z*DMF),
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        }
    }
    if(d->gunselect!=GUN_TELEKENESIS&&d->gunselect!=GUN_TELEKENESIS2&&d->gunselect!=GUN_FIST&&d->gunselect!=GUN_CG&&d->gunselect!=GUN_CG2)spawnbouncer(from, vel, d, BNC_WEAPON, damage/3);


    if (d->quadmillis) {
        loopi(5)spawnbouncer(from, vel, d, BNC_BARRELDEBRIS, rnd(50)+10);
    }
}

void hit(int damage, dynent *d, fpsent *at, const vec &vel, int gun, float info1, int info2 = 1)
{
    if(at==player1 && d!=at && d->type!=ENT_INANIMATE)
    {
        extern int hitsound;
        if(hitsound && lasthit != lastmillis) playsound(S_HIT);
        lasthit = lastmillis;

        //game::stats[1] += damage;
    }

    if(d->type==ENT_INANIMATE)
    {
        hitmovable(damage, (movable *)d, at, vel, gun);
        movable *m=(movable *)d;
        if(gun==GUN_TELEKENESIS2 && at==player1) { suicidemovable((movable *)d); } //addmsg(N_DELMOVABLE, "rci", d, (long *)d); }
        //conoutf(CON_GAMEINFO, "%d", (long *)d);
        return;
    }
    if(!guns[at->lastattackgun].projspeed && damage==(guns[at->lastattackgun].damage*3) && at!=player1)at->headshots=1;

    fpsent *f = (fpsent *)d;

    f->lastpain = lastmillis;
    if(at->type==ENT_PLAYER) at->totaldamage += damage;

    //at->lasthitgun==at->lastattackgun;

    if(f->type==ENT_AI || !m_mp(gamemode) || f==at) f->hitpush(damage, vel, at, gun);

    if(f->type==ENT_AI) hitmonster(damage, (monster *)f, at, vel, gun);
    else if(!m_mp(gamemode)) damaged(damage, f, at);
    else
    {
        hitmsg &h = hits.add();
        h.target = f->clientnum;
        h.lifesequence = f->lifesequence;
        h.info1 = int(info1*DMF);
        h.info2 = info2;
        h.dir = f==at ? ivec(0, 0, 0) : ivec(int(vel.x*DNF), int(vel.y*DNF), int(vel.z*DNF));
        //vec to(rnd(100)-50, rnd(100)-50, rnd(100)-50);
        if(at==player1)
        {
            //if((player1->gunselect==GUN_MAGNUM || player1->gunselect==GUN_ELECTRO) && player1->vel.magnitude()>300)playsound(S_AMAZING);
            damageeffect(damage, f);
            if(f==player1 && lookupmaterial(camera1->o)!=MAT_WATER && damage)
            {
                damageblend(damage*6, false);
                damagecompass(damage, at ? at->o : f->o);
                //playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_PAIN1+rnd(5):S_UWPN4+rnd(3));
                //if((player1->health+player1->armour)-damage >= 75) playsound(S_PAIN1);
                //else if((player1->health+player1->armour)-damage >= 50) playsound(S_PAIN2);
                //else if((player1->health+player1->armour)-damage >= 25) playsound(S_PAIN3);
                //else if((player1->health+player1->armour)-damage >= 0) playsound(S_PAIN4);
                if(damage)playsound(S_IMPACT+rnd(2));

                //if(damage)playsound(S_SCH1+rnd(3));
            }
            else
            {
                //                    {
                //                        defformatstring(ds)("%d", at->headshot?damage*3:damage);
                //                        particle_textcopy(f->abovehead(), ds, PART_TEXT, 2000, 0xFF4B19, 4.0f, -8);
                //                    }
                if(lookupmaterial(camera1->o)!=MAT_WATER)
                {
                    //playsound(lookupmaterial(f->o)!=MAT_WATER?S_PAIN1+rnd(5):S_UWPN4+rnd(3), &f->o);
                    //if((f->health+f->armour)-damage >= 75) playsound(S_PAIN1, &f->o);
                    //else if((f->health+f->armour)-damage >= 50) playsound(S_PAIN2, &f->o);
                    //else if((f->health+f->armour)-damage >= 25) playsound(S_PAIN3, &f->o);
                    //else if((f->health+f->armour)-damage >= 0) playsound(S_PAIN4, &f->o);
                    if(damage)playsound(S_IMPACT+rnd(2), &f->o);
                }
                else
                {
                    //playsound(S_UWPN4+rnd(3), &f->o);
                }
            }
        }
        else
        {
            //if(damage) playsound(S_SCH1+rnd(3), &f->o);
        }
    }
}

void hitpush(int damage, dynent *d, fpsent *at, vec &from, vec &to, int gun, int rays)
{
    hit(damage, d, at, vec(to).sub(from).normalize(), gun, from.dist(to), rays);
}



void radialeffect(dynent *o, const vec &v, int qdam, fpsent *at, int gun) //v=point of explosion
{
    if(o->state!=CS_ALIVE) return;
    vec dir;
    float dist = projdist(o, dir, v);
    if(dist<guns[gun].splash)
    {
        int damage = (int)(qdam*(1-dist/RL_DISTSCALE/guns[gun].splash));
        if((gun==GUN_BITE || gun==GUN_FIREBALL || gun==GUN_CG2 || gun==GUN_CROSSBOW) && o==at) damage=0;
        if(gun==GUN_CROSSBOW && at!=o)damage=100;
        if(gun==GUN_CROSSBOW && at==o)damage=0;
        if(at==player1 || at->ai)hit(damage, o, at, dir, gun, dist);
        if(blood && qdam<800 && o->type==ENT_PLAYER && damage) { particle_splash(PART_BLOOD, 1, 200, o->o, 0x99FFFF, 8.f, 50, 0, NULL, 2, false, 3);
            adddecal(DECAL_BLOOD, o->o, vec(v).sub(o->o).normalize(), rnd(15)+10.f, bvec(0x99, 0xFF, 0xFF));
            particle_splash(PART_SPARK, 100, 300, o->o, 0x0000FF, 0.1f, 250);
            //particle_splash(PART_SMOKE, 5, 200, o->o, 0x0000FF, 2.0f, 50, 250, NULL, 1, false, 2);
        }
        if(at!=o && o->timeinair>250 && at==player1)playsound(S_AIRSHOT);
    }
}

VARP(seimprovedexplosions, 1, 1, 1);

void explode(bool local, fpsent *owner, const vec &v, dynent *safe, int damage, int gun)
{
    if(gun==GUN_CG2)
    {
        playsound(gun==GUN_CG2 ? S_NEXIMPACT : S_LASERIMPACT, &v);
        particle_fireball(v, guns[gun].splash, PART_EXPLOSION, 150, 0xFFFFFF, guns[gun].splash);
    }
    if((gun==GUN_HANDGRENADE ||gun==GUN_RL||gun==GUN_SMG2||gun==GUN_BARREL))
    {
        if(v.dist(player1->o)<100)player1->doshake=1;
        particle_splash(PART_SMOKE, 5, 1250, v, 0x222222, 12.0f, 50, 250, NULL, 1, false, 3);
        particle_splash(PART_SMOKE, 5, 200, v, 0x222222, 12.0f, 50, 250, NULL, 1, false, 4);

        //if(gun==GUN_RL)
        {
            particle_splash(PART_FLAME1, 15, 70, v, 0xFFFFFF, 3.2f, 150, NULL, NULL, NULL, true);
            particle_splash(PART_FLAME2, 15, 70, v, 0xFFFFFF, 3.2f, 150, NULL, NULL, NULL, true);
            particle_splash(PART_FLAME2, 15, 70, v, 0xFFFFFF, 3.2f, 150, NULL, NULL, NULL, true);
            //particle_splash(PART_EXPLODE, 1, 100, v, 0xFFFFFF, 10.0f, 300, 500, true, NULL, NULL, 4);
            particle_flare(v, v, 150, PART_MUZZLE_FLASH2, 0xFFFFFF, guns[gun].splash);
            particle_splash(PART_SPARK, 160, 30, v, 0xFFC864, 1.4f, 300, NULL, NULL, NULL, true);
            particle_fireball(v, guns[gun].splash, PART_EXPLOSION, 300, 0x333333, guns[gun].splash/2);
            //loopi(10)particle_explodesplash(v, 200, PART_BULLET, 0xFFFFFF, 0.6f, 10, 10);

            //particle_fireball(v, guns[gun].splash, PART_EXPLOSION, 100, 0xFFFFFF, 4.0f);
        }

        //            else if(gun==GUN_HANDGRENADE)
        //            {
        //                particle_fireball(v, RL_DAMRAD, PART_EXPLOSION_BLUE, 200, 0xFFFFFF, 4.0f);
        //                particle_fireball(v, 32, PART_EXPLOSION_PLASMA, 500, 0xFFFFFF, 4.0f);
        //            }
        if(lookupmaterial(camera1->o)==MAT_WATER || lookupmaterial(v)==MAT_WATER)
        {
            playsound(S_UWEN, &v);
        }
        else
        {
            //playsound(gun==GUN_RL?S_RLHIT1+rnd(3):S_RLEX1+rnd(3), &v);
            playsound(S_RLHIT, &v);
        }

    }

    if(gun==GUN_ELECTRO)
    {
        playsound(S_NEXIMPACT, &v);
        particle_flare(v, v, 150, PART_MUZZLE_FLASH2, 0x0789FC, guns[gun].splash/2);
        //particle_splash(PART_SPARK, 1000, 550, v, 0xFFC864, 0.10f, guns[gun].splash/2);
        particle_splash(PART_SPARK, 2, 200, v, 0x0789FC, 8.f, 2, 50);
        //particle_splash(PART_SPARK, 500, 500, v, 0x0789FC, 0.5f, guns[gun].splash*4);
        particle_fireball(v, 8.f, PART_EXPLOSION, 200, 0x0789FC, guns[gun].splash/2);

    }

    if (gun == GUN_ELECTRO2) {
        playsound(S_COMBO, &v);
        //void particle_fireball(const vec &dest, float maxsize, int type, int fade, int color, float size)
        particle_fireball(v, guns[gun].splash/2, PART_EXPLOSION, 1000, 0x0789FC, guns[gun].splash/2);

    }

    if(gun==GUN_ELECTRO||gun==GUN_ELECTRO2)adddynlight(v, 1.15f*guns[gun].splash, vec(0, 0, 1), 100, 100, 0, guns[gun].splash/2, vec(0, 0, 1));
    else if(gun!=GUN_CROSSBOW&&gun!=GUN_BITE)adddynlight(v, 2.15f*guns[gun].splash, vec(2, 1.5f, 1), 100, 100, 0, guns[gun].splash, vec(1, 0.75f, 0.5f));
    //else if(gun==GUN_HANDGRENADE) adddynlight(v, 1.15f*RL_DAMRAD, vec(2, 1.5f, 1), 100, 100, 0, 8, vec(0.25f, 1, 1));
    //else adddynlight(v, 1.15f*guns[gun].splash, vec(2, 1.5f, 1), 100, 100);
    int numdebris = gun==GUN_BARREL ? rnd(max(maxbarreldebris-5, 1))+5 : rnd(maxdebris-5)+5;
    vec debrisvel = owner->o==v ? vec(0, 0, 0) : vec(owner->o).sub(v).normalize(), debrisorigin(v);
    //if(gun==GUN_RL)
    debrisorigin.add(vec(debrisvel).mul(8));
    if(numdebris/2)
    {
        entitylight light;
        lightreaching(debrisorigin, light.color, light.dir);
        if(gun==GUN_RL)debrisorigin.z+=10;
        loopi(numdebris)
                if(gun==GUN_RL||gun==GUN_HANDGRENADE || gun==GUN_SMG2 || gun==GUN_BARREL)spawnbouncer(debrisorigin, debrisvel, owner, gun==GUN_BARREL ? BNC_BARRELDEBRIS : BNC_DEBRIS, 20, &light);
    }
    //if(!local) return;
    loopi(numdynents())
    {
        dynent *o = iterdynents(i);
        if(o==safe) continue;
        radialeffect(o, v, damage, owner, gun);
    }
}

void projsplash(projectile &p, vec &v, dynent *safe, int damage, bool impact = true)
{
    if(guns[p.gun].part && (p.gun != GUN_ELECTRO2 || impact))
    {
        particle_splash(PART_SPARK, 100, 200, v, 0xFFFFFF, 0.1f);
        playsound(S_FEXPLODE, &v);
        // no push?
    }
    else
    {
        explode(p.local, p.owner, v, safe, damage, p.gun);
        adddecal(DECAL_SCORCH, v, vec(p.dir).neg(), guns[p.gun].splash/2);
    }
}

void explodeeffects(int gun, fpsent *d, bool local, int id)
{
    if(local) return;
        switch(gun)
        {
        case GUN_RL: case GUN_ELECTRO2:
            //case GUN_RL2:
            //case GUN_CROSSBOW:
            loopv(projs)
            {
                projectile &p = projs[i];
                if(p.gun == gun && p.owner == d && p.id == id && !p.local)
                {
                    vec pos(p.o);
                    pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
                    explode(p.local, p.owner, pos, NULL, 0, p.gun);
                    adddecal(DECAL_SCORCH, pos, vec(p.dir).neg(), guns[gun].splash/2);
                    projs.remove(i);
                    break;
                }
            }
            break;
        case GUN_HANDGRENADE: case GUN_CROSSBOW: case GUN_SMG2:
            loopv(bouncers)
            {
                bouncer &b = *bouncers[i];
                if((b.bouncetype == BNC_GRENADE || b.bouncetype==BNC_XBOLT || b.bouncetype==BNC_SMGNADE) && b.owner == d && b.id == id && !b.local)
                {
                    vec pos(b.o);
                    pos.add(vec(b.offset).mul(b.offsetmillis/float(OFFSETMILLIS)));
                    if(b.bouncetype==BNC_SMGNADE)explode(b.local, b.owner, pos, NULL, 0, GUN_SMG2);
                    else explode(b.local, b.owner, pos, NULL, 0, b.bouncetype==BNC_XBOLT ? GUN_CROSSBOW : GUN_HANDGRENADE);
                    adddecal(DECAL_SCORCH, pos, vec(0, 0, 1), guns[gun].splash/2);
                    delete bouncers.remove(i);
                    break;
                }
            }
        /*case GUN_ELECTRO2:
            loopv(bouncers)
            {
                bouncer &b = *bouncers[i];
                if(b.bouncetype == BNC_ELECTROBOLT && b.owner == d && b.id == id && !b.local)
                {
                    vec pos(b.o);
                    pos.add(vec(b.offset).mul(b.offsetmillis/float(OFFSETMILLIS)));
                    explode(b.local, b.owner, pos, NULL, 0, GUN_ELECTRO2);
                    adddecal(DECAL_SCORCH, pos, vec(0, 0, 1), guns[gun].splash/2);
                    delete bouncers.remove(i);
                    break;
                }
            }*/
        case GUN_CG2:
            loopv(bouncers)
            {
                projectile &p = projs[i];
                if(p.gun == gun && p.owner == d && p.id == id && !p.local)
                {
                    vec pos(p.o);
                    pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
                    explode(p.local, p.owner, pos, NULL, 0, p.gun);
                    adddecal(DECAL_SCORCH, pos, vec(p.dir).neg(), guns[gun].splash/2);
                    projs.remove(i);
                    break;
                }
            }

        case GUN_ELECTRO: //case GUN_ELECTRO2:
            loopv(projs)
            {
                projectile &p = projs[i];
                if(p.gun == gun && p.owner == d && p.id == id && !p.local)
                {
                    vec pos(p.o);
                    pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
                    explode(p.local, p.owner, pos, NULL, 0, p.gun);
                    adddecal(DECAL_SCORCH, pos, vec(p.dir).neg(), guns[gun].splash);
                    projs.remove(i);
                    break;
                }
            }

            break;
        default:
            break;
        }
}

bool isheadshot(dynent *d, vec to)
{
    if ((to.z - (d->o.z - d->eyeheight)) / (d->eyeheight + d->aboveeye) > 0.8f) return true;
    return false;
}

bool projdamage(dynent *o, projectile &p, vec &v, int qdam)
{
    if(o->state!=CS_ALIVE) return false;
    if(!intersect(o, p.o, v)) return false;
    projsplash(p, v, o, qdam);
    vec dir;
    projdist(o, dir, v);
    hit(qdam, o, p.owner, dir, p.gun, 0);
    return true;
}

VARP(seimprovedprojectiles, 1, 1, 1);

void updateprojectiles(int time)
{
    loopv(projs)
    {
        projectile &p = projs[i];
        p.offsetmillis = max(p.offsetmillis-time, 0);
        int qdam = guns[p.gun].damage*(p.owner->quadmillis ? 4 : 1);
        if(p.owner->type==ENT_AI) qdam /= MONSTERDAMAGEFACTOR;
        vec v;
        float dist = p.to.dist(p.o, v);
        float dtime = dist*1000/p.speed;
        if(time > dtime) dtime = time;
        v.mul(time/dtime);
        v.add(p.o);
        bool exploded = false;
        hits.setsize(0);
        if(p.local)
        {
            loopj(numdynents())
            {
                dynent *o = iterdynents(j);
                if(p.owner==o || o->o.reject(v, 10.0f)) continue;
                if(projdamage(o, p, v, qdam)) { exploded = true; break; }
            }
        }
        if(!exploded)
        {
            if(dist<14 && !p.lastposs)
            {
                p.lastpos = vec(p.o);
                p.lastposs = true;
            }
            if(dist<4)
            {
                if(p.o!=p.to) // if original target was moving, reevaluate endpoint
                {
                    if(raycubepos(p.o, p.dir, p.to, 0, RAY_CLIPMAT|RAY_ALPHAPOLY)>=4) continue;
                }
                vec occlusioncheck;
                if(raycubelos(p.lastpos, camera1->o, occlusioncheck))
                {
                    particle_flare(p.lastpos, p.lastpos, 75, PART_GLOW, 0x553322, 80.0f);
                }
                projsplash(p, v, NULL, qdam);
                exploded = true;
                p.lastposs = false;
            }
            else
            {
                p.lastposs = false;
                vec pos(v);
                pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
                if(guns[p.gun].part)
                {
                    regular_particle_splash(PART_SMOKE, 2, 300, pos, 0x404040, 0.6f, 150, -20);
                    int color = 0xFFFFFF;
                    switch(guns[p.gun].part)
                    {
                    case PART_FIREBALL1: color = 0xFFC8C8; break;
                    }
                    //if(p.gun==GUN_ELECTRO) particle_flare(pos, pos, 75, PART_MUZZLE_FLASH1, 0x0789FC, 2.f);
                    particle_splash(guns[p.gun].part, 1, 1, pos, color, 4.8f, 150, 20);
                }
                else if(lookupmaterial(pos)!=MAT_WATER && p.gun==GUN_RL)
                    regular_particle_splash(PART_SMOKE, 2, 300, pos, 0x555555, 2.4f, 12, 502);
                /*if (p.gun == GUN_ELECTRO2 && pos.dist(p.owner->o)>50) {
                    vec occcheck;
                    if(raycubelos(pos, camera1->o, occcheck) && p.gun==GUN_ELECTRO2)
                    {
                        //particle_flare(pos, pos, 1, PART_MUZZLE_FLASH5, 0xFFFFFF, 10.0f+rndscale(5), NULL);
                        particle_flare(pos, pos, 10, PART_GLOW, 0x0789FC, 2.5f);
                        //particle_flare(pos, pos, 300, PART_GLOW, 0x0789FC, 1.8f);
                    }
                }*/

                if(seimprovedprojectiles && p.gun==GUN_RL)
                {
                    if(lookupmaterial(camera1->o)!=MAT_WATER && lookupmaterial(pos)!=MAT_WATER)
                    {
                        stopsound(S_ROCKETUW, p.rocketchan);
                        p.rocketsound = S_ROCKET;
                    }
                    if(lookupmaterial(camera1->o)==MAT_WATER || lookupmaterial(pos)==MAT_WATER)
                    {
                        stopsound(S_ROCKET, p.rocketchan);
                        p.rocketsound = S_ROCKETUW;
                    }
                    if(lookupmaterial(pos)!=MAT_WATER)
                    {
                        vec check;
                        //regular_particle_splash(PART_SMOKE, 16, 300, pos, 0x444444, 2.4f, 12, 502);
                        regular_particle_splash(PART_SMOKE, 8, 500, pos, 0x444444, 1.2f, 12, 500, NULL, NULL, 2);
                        if(raycubelos(pos, camera1->o, check))
                        {
                            //particle_flare(pos, pos, 1, PART_MUZZLE_FLASH5, 0xFFFFFF, 10.0f+rndscale(5), NULL);
                            particle_flare(pos, pos, 1, PART_GLOW, 0xFFC864, 5.0f+rndscale(5), NULL);
                        }
                        particle_fireball(pos, 2.0f, PART_EXPLOSION, -1, 0xA0C080, 2.0f);
                        regular_particle_splash(PART_FLAME1, 2, 50, pos, 0xFFFFFF, 1.2f, 25, 500);
                        regular_particle_splash(PART_FLAME2, 2, 25, pos, 0xFFFFFF, 1.2f, 25, 500);
                        regular_particle_splash(PART_SPARK, 8, 100, pos, 0xFFC864, 0.4f, 50, 500);
                        regular_particle_splash(PART_FLAME3, 1, 100, pos, 0xFFFFFF, 0.8f, 50, 500);
                        regular_particle_splash(PART_FLAME4, 1, 100, pos, 0xFFFFFF, 0.8f, 50, 500);
                        //                            if(p.owner->quadmillis)
                        //                            {
                        //                                regular_particle_splash(PART_FLAME1, 2, 200, pos, 0xFF0000, 2.4f, 25, 777);
                        //                                regular_particle_splash(PART_FLAME2, 2, 100, pos, 0xFF0000, 2.4f, 25, 777);
                        //                                regular_particle_splash(PART_SPARK, 8, 400, pos, 0xFF0000, 0.4f, 50, 777);
                        //                                regular_particle_splash(PART_FLAME3, 1, 400, pos, 0xFF0000, 0.8f, 50, 777);
                        //                                regular_particle_splash(PART_FLAME4, 1, 400, pos, 0xFF0000, 0.8f, 50, 777);
                        //                            }
                    }
                    else
                    {
                        regular_particle_splash(PART_BUBBLE, 4, 500, pos, 0xFFFFFF, 0.5f, 25, 500);
                    }

                    p.rocketchan = playsound(p.rocketsound, &pos, NULL, -1, 128, p.rocketchan);
                }
            }
        }
        if(exploded)
        {
            if(p.local)
                addmsg(N_EXPLODE, "rci3iv", p.owner, lastmillis-maptime, p.gun, p.id-maptime,
                       hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            projs.remove(i--);
        }
        else p.o = v;
        //vec rpgcheck;
        if(p.gun==GUN_RL && p.owner->gunselect==GUN_RL /*&& raycubelos(p.o, worldpos, rpgcheck)*/){
            p.to.x = p.to.y = p.to.z = 0.f;
            vecfromyawpitch(p.owner->yaw, p.owner->pitch, 1, 0, p.to);
            float barrier = guns[p.gun].range; //raycube(p.owner->o, p.to, guns[p.gun].range, RAY_CLIPMAT|RAY_ALPHAPOLY);
            //float barrier = dspeed ? raycube(o, dir, dspeed, RAY_CLIPMATE, RAY_ALPHAPOLY) : 0;
            p.to.mul(barrier).add(p.owner->o);
            //player1->guiding=1;
        }
        if(!raycube(p.o, p.o, guns[p.gun].range, RAY_CLIPMAT|RAY_ALPHAPOLY) || (p.local && p.owner->altattacking && p.owner->gunselect == GUN_RL))
        {
            int qdam = guns[p.gun].damage*(p.owner->quadmillis ? 2 : 1);
            projsplash(p, p.o, NULL, qdam);
            if(p.local)addmsg(N_EXPLODE, "rci3iv", p.owner, lastmillis-maptime, p.gun, p.id-maptime,
                              hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            projs.remove(i--);
        }
    }
}

extern int chainsawhudgun;

VARP(muzzleflash, 1, 1, 1);
VARP(muzzlelight, 1, 1, 1);
VARP(seimprovedguneffects, 1, 1, 1);
VARP(sebulletimpactsound, 1, 1, 1);
VARP(sebulletimpactsmoke, 1, 1, 1);

int sgnearmiss[3];

void sound_nearmiss(int sound, const vec &s, const vec &e, bool limit = false)
{
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*2), 1, 2048);
    v.div(steps);
    vec p = s;
    bool soundused = false;
    loopi(steps)
    {
        p.add(v);
        //vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        if(camera1->o.dist(p) <= 32)
        {
            if(limit)
            {
                loopi(3)
                {
                    if(!sgnearmiss[i])
                    {
                        sgnearmiss[i] = playsound(sound, &p, NULL, NULL, NULL, sgnearmiss[i]);
                        return;
                    }
                }
            }
            else if(!soundused)
            {
                playsound(sound, &p);
                soundused = true;
                return;
            }
        }
    }
}

float intersectdist = 1e16f;

bool intersect(dynent *d, const vec &from, const vec &to, float &dist)   // if lineseg hits entity bounding box
{
    vec bottom(d->o), top(d->o);
    bottom.z -= d->eyeheight;
    top.z += d->aboveeye;
    return linecylinderintersect(from, to, bottom, top, d->radius, dist);
}

dynent *intersectclosest(const vec &from, const vec &to, fpsent *at, float &bestdist)
{
    dynent *best = NULL;
    bestdist = 1e16f;
    loopi(numdynents())
    {
        dynent *o = iterdynents(i);
        if(o==at || o->state!=CS_ALIVE) continue;
        float dist;
        if(!intersect(o, from, to, dist)) continue;
        if(dist<bestdist)
        {
            best = o;
            bestdist = dist;
        }
    }
    return best;
}

void shorten(vec &from, vec &target, float dist)
{
    target.sub(from).mul(min(1.0f, dist)).add(from);
}



void raydamage(vec &from, vec &to, fpsent *d)
{
    int qdam = guns[d->gunselect].damage;
    d->headshots=0;
    if(d->quadmillis) qdam *= 2;
    if(d->type==ENT_AI) qdam /= MONSTERDAMAGEFACTOR;
    dynent *o, *cl;
    float dist;

    if(guns[d->gunselect].rays>1)
    {

        bool done[12];
        loopj(guns[d->gunselect].rays) done[j] = false;
        for(;;)
        {
            bool raysleft = false;
            int hitrays = 0;
            int headshots = 0;
            o = NULL;

            loop(r, guns[d->gunselect].rays) if(!done[r] && (cl = intersectclosest(from, rays[r], d, dist)))
            {
                if(!o || o==cl)
                {
                    hitrays++;
                    o = cl;
                    done[r] = true;
                    vec blooddest = rays[r];
                    blooddest.z += rnd(2)+2;
                    if(blood && (cl->type==ENT_PLAYER || cl->type==ENT_AI) && r%2==0)adddecal(DECAL_BLOOD, blooddest, vec(from).sub(blooddest).normalize(), rnd(8)+13.f, bvec(0x99, 0xFF, 0xFF)); //don't do blood for all bbs that hit
                    shorten(from, rays[r], dist);
                    if(blood && qdam<500 && qdam>0 && (cl->type==ENT_PLAYER || cl->type==ENT_AI)){
                        particle_splash(PART_BLOOD, 1, 200, rays[r], 0x99FFFF, 8.f, 50, 0, NULL, 2, false, 3); particle_splash(PART_WATER, 50, 300, rays[r], 0xFF0000, 0.1f, 230);
                        //particle_splash(PART_SMOKE, 5, 200, rays[r], 0x0000FF, 2.0f, 50, 250, NULL, 1, false, 2);

                    }
                    if(isheadshot(cl, rays[r]) && (d==player1 || d->ai))  { headshots++;}
                }
                else raysleft = true;
            }
            if(hitrays) hitpush(hitrays*qdam, o, d, from, to, d->gunselect, hitrays);
            //if(headshots>=(guns[d->gunselect].rays/6))d->headshot=1;
            d->headshots=headshots;
            if(d==player1 && hitrays && o->type!=ENT_INANIMATE)
            {
                int damage=guns[d->gunselect].damage;
                damage*=hitrays;
                for(int x=0; x<headshots; x++)
                {
                    damage+=guns[d->gunselect].damage*2;
                }
            }
            if(!raysleft) break;
        }
        //loopj(guns[d->gunselect].rays) if(!done[j]) adddecal(DECAL_BULLET, rays[j], vec(from).sub(rays[j]).normalize(), 2.0f);
    }
    if(d->gunselect==GUN_ELECTRO)
    {
        ///*loopi(2)*/particle_flare(d->muzzle.x >= 0 ? d->muzzle : hudgunorigin(d->gunselect, from, to, d), to, 400, PART_STREAK, 0x0789FC, 0.35f);
        //particle_trail(PART_RAILSPIRAL, 500, d->muzzle.x>=0?d->muzzle:hudgunorigin(d->gunselect, from, to, d), to, 0, 0.5f, 0, true);
    }
    if((o = intersectclosest(from, to, d, dist)) && guns[d->gunselect].rays==1)
    {
        vec blooddest=to;
        blooddest.z+=rnd(2)+2;
        if(blood && d->gunselect!=GUN_TELEKENESIS2  && (o->type==ENT_PLAYER || o->type==ENT_AI))adddecal(DECAL_BLOOD, blooddest, vec(from).sub(blooddest).normalize(), rnd(8)+13.f, bvec(0x99, 0xFF, 0xFF));
        shorten(from, to, dist);
        if(blood && qdam<500 && d->gunselect!=GUN_TELEKENESIS2 && (o->type==ENT_PLAYER || o->type==ENT_AI)){
            particle_splash(PART_BLOOD, 1, 200, to, 0x99FFFF, 8.f, 50, 0, NULL, 2, false, 3); particle_splash(PART_WATER, 50, 300, to, 0xFF0000, 0.1f, 230);
            //particle_splash(PART_SMOKE, 5, 200, to, 0x0000FF, 2.0f, 50, 250, NULL, 1, false, 2);
        }
        hitpush(qdam, o, d, from, to, d->gunselect, 1);
        if(isheadshot(o, to)&& (d==player1 || d->ai)) { qdam *= 3; d->headshots=1; }
    }
}


VARP(serifletrail, 0, 0, 1);
VARP(nadetimer, 500, 1000, 3000);

void set_mod(int modset, int mod, int cost, char* name) {
    if (modset) {
        if (player1->money - cost < 0) {
            conoutf("You don't have enough money to buy the %s mod", name);
            return;
        }
        player1->mods |= mod;
        addmsg(N_MODS, "rci", player1, player1->mods);
        player1->money -= cost;
        conoutf("You bought the %s mod!", name);
    }
    else {
        player1->mods &= ~mod;
        addmsg(N_MODS, "rci", player1, player1->mods);
    }
}

/*VARFP(mod_lightninggun, 0, 0, 1,
    if (mod_lightninggun) { 
        int cost = 2000;
        if (player1->money - cost < 0) {
            conoutf("You don't have enough money to buy the lightning gun mod");
            return;
        }
        player1->mods |= MOD_LIGHTNINGGUN; 
        addmsg(N_MODS, "rci", player1, player1->mods);
        player1->money -= cost;
        conoutf("You bought the lightning gun mod!");
    } else {
        player1->mods &= ~MOD_LIGHTNINGGUN;
        addmsg(N_MODS, "rci", player1, player1->mods);
    }
);*/

VARFP(mod_lightninggun, 0, 0, 1, set_mod(mod_lightninggun, MOD_LIGHTNINGGUN, 2000, "lightning gun"));
VARP(lgsniper, 0, 0, 1);


//void screenjump();
void shoteffects(int gun, const vec &from, const vec &to, fpsent *d, bool local, int id, int prevaction)     // create visual effect from a shot
{
    int sound = guns[gun].sound, pspeed = guns[gun].projspeed; //if any problem with screenjump, just recompile rendergl.cpp
    vec check;
    float droll = 0.f; //8.f; turn this off for now
    vec dest = to;
    if(!guns[d->gunselect].projspeed && d!=player1) raydamage(d->o, dest, d);

    switch(gun)
    {
    //            case GUN_FIST:
    //                if(d->type==ENT_PLAYER && chainsawhudgun) sound = S_CHAINSAW_ATTACK;
    //                break;

    case GUN_SG:
    case GUN_SHOTGUN2:
    {
        if(!local) createrays(d->gunselect, from, to, d);
        if(muzzleflash && d->muzzle.x >= 0)
        {
            particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH3, 0xFFFFFF, 2.75f, d);
        }
        if(seimprovedguneffects)
        {
            particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x222222, 1.4f, 50, 501, NULL, 2, NULL, 2);
        }
        //                if(d->quadmillis)
        //                {
        //                    particle_splash(PART_FLAME1, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME2, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME3, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                }
        loopi(guns[gun].rays)
        {
            particle_splash(PART_SPARK, d->quadmillis ? 50 : 10, 100, rays[i], 0xFFFFFF, 0.1f);
            if(d!=hudplayer()) sound_nearmiss(S_NEARMISS1+rnd(3), from, rays[i], true);
            particle_explodesplash(rays[i], 50, PART_BULLET, 0xFFFFFF, 0.2f, 500, 8);
            //                    if(d->quadmillis)
            //                    {
            //                        particle_splash(PART_FLAME1, 5, 50, to, 0xFF0000, 0.48f, 500);
            //                        particle_splash(PART_FLAME2, 5, 50, to, 0xFF0000, 0.48f, 500);
            //                        particle_splash(PART_FLAME3, 5, 50, to, 0xFF0000, 0.48f, 500);
            //                    }
            if(seimprovedguneffects) particle_trail(0, 0, from, rays[i], 0, 0, 0, true);
            if(sebulletimpactsmoke)
            {
                particle_splash(PART_SMOKE, 3, 500, rays[i], 0x444444, 1.4f, 50, 504, NULL, 2, NULL, 1);
            }
            //if(d->quadmillis) particle_trail(PART_FLAME1, 50, hudgunorigin(gun, from, rays[i], d), rays[i], 0xFF0000, 0.6f, 200);
            //particle_flare(hudgunorigin(gun, from, sg[i], d), sg[i], 175, PART_STREAK, 0xFFC864, 0.28f);
            vec lolwut(rays[i]);
            lolwut.sub(d->muzzle.x>0?d->muzzle:hudgunorigin(gun, from, rays[i], d));
            lolwut.normalize().mul(6000.0f);
            // ALWAYS show bullet tracers for other players (no occlusion check)
            particle_flying_flare(d->muzzle.x>=0?d->muzzle:hudgunorigin(gun, from, rays[i], d), lolwut, 500, PART_BULLET, 0xFFFFFF, 0.5f, 100);
            adddecal(DECAL_BULLET, rays[i], vec(from).sub(rays[i]).normalize(), 2.0f);
            //                    dynent *o = intersectclosest(d->muzzle, worldpos, d); //don't use &to, this is shortened!!
            //                    if(o && o->type==ENT_PLAYER && blood) {
            //                        particle_splash(PART_BLOOD, 7, 150, rays[i], 0x99FFFF, 6.f, 8, 0);
            //                        //adddecal(DECAL_BLOOD, worldpos, vec(from).sub(worldpos).normalize(), 10.f, bvec(0x99, 0xFF, 0xFF));
            //                    }
            //                   float dist;
            //                   dynent o = intersectclosest(from, to, d, dist);
            //if(isheadshot(d, rays[i]))d->headshot=1;

        }
        loopi(guns[gun].rays/2)
        {
            sgnearmiss[i] = 0;
            if(sebulletimpactsound && lookupmaterial(camera1->o)!=MAT_WATER) playsound(S_BHIT1+rnd(3), &rays[i]);
        }
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 30, vec(0.5f, 0.375f, 0.25f), 50, 50, DL_FLASH, 0, vec(0, 0, 0), d);
        //                if(d->roll >= 0)
        //                {
        //                    d->roll -= droll;
        //                }
        //                else d->roll += droll;
        //dynent *o = intersectclosest(d->muzzle, worldpos, d); //don't use &to, this is shortened!!
        //if(o && o->type==ENT_PLAYER && blood) {
        //    loopi(3)adddecal(DECAL_BLOOD, worldpos, vec(from).sub(worldpos).normalize(), 25.f, bvec(0x99, 0xFF, 0xFF));
        // }
        // Different screenjump for each shotgun - GUN_SG (double-shot) stronger than SHOTGUN2 (single-shot)
        if(d==hudplayer()) { 
            d->screenjumpheight = (gun==GUN_SG) ? 40 : 30; // GUN_SG=40 (doubled from 20), SHOTGUN2=30 (weaker but still strong)
            screenjump(); 
            screenjump();
        }
        if(d->gunselect==GUN_SG)playsound(S_SHOTGUN, d==hudplayer()?NULL:&d->o);
        // Eject shell casings for shotguns
        loopi(gun==GUN_SG?2:1)
        {
            vec shellstart = hudgunorigin(gun, d->o, to, d);
            shellstart.z -= 0.5f;

            vec forward, left;
            vecfromyawpitch(d->yaw, 0, 1, 0, forward);
            vecfromyawpitch(d->yaw - 90, 0, 1, 0, left);

            conoutf("SHELL EJECT DEBUG:");
            conoutf("  Entity: %s, local: %d", d->name, local);
            conoutf("  Player yaw: %f, pitch: %f", d->yaw, d->pitch);
            conoutf("  Left vector: (%f, %f, %f)", left.x, left.y, left.z);

            vec shelldir = vec(left).mul(1.5f + (rnd(11) / 10.0f));
            shelldir.z = (1.2f + (rnd(7) / 10.0f));
            shelldir.add(vec(forward).mul(0.2f + (rnd(11) / 10.0f)));

            conoutf("  Shell direction BEFORE newbouncer: (%f, %f, %f)", shelldir.x, shelldir.y, shelldir.z);

            vec shellto = vec(shellstart).add(shelldir);
            newbouncer(shellstart, shellto, local, id, d, BNC_SHELL, 8000, 50+rnd(20));
            
            conoutf("  newbouncer called successfully");
        }
        break;
    }
    case GUN_PISTOL:
    {
        particle_splash(PART_SPARK, 200, 250, to, 0x50FF50, 0.10f);
        particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x50FF50, 1.f, 50, 501, NULL, 2, NULL, 2);
        particle_trail(1, 1, from, to, 1, 1, 1, true);
        particle_flare(hudgunorigin(gun, from, to, d), to, 50, PART_LIGHTNING, 0x50FF50, 1.f);
        d->attacking=d->altattacking=0;
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, gun==GUN_CG ? 100 : 200, PART_MUZZLE_FLASH3, 0x50FF50, 1.5f, d);
        adddecal(DECAL_BULLET, to, vec(from).sub(to).normalize(), 2.0f); //BULLET //4.0
        //adddecal(DECAL_GLOW, to, vec(from).sub(to).normalize(), 3.0f, bvec(0x07, 0x89, 0xFC)); //  0x0789FC);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), gun==GUN_CG ? 30 : 15, vec(0.3f, 1.5f, .3f), gun==GUN_CG ? 50 : 100, gun==GUN_CG ? 50 : 100, DL_FLASH, 0, vec(0, 1, 0), d);
        break;
    }

    /*case GUN_ELECTRO2:
    {
        particle_splash(PART_SPARK, 200, 250, to, 0x0789FC, 0.10f);
        particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x0789FC, 1.f, 50, 501, NULL, 2, NULL, 2);
        particle_flare(hudgunorigin(gun, from, to, d), to, 15, PART_LIGHTNING, 0x0789FC, .5f);
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, gun==GUN_CG ? 100 : 200, PART_MUZZLE_FLASH3, 0x0789FC, 1.5f, d);
        adddecal(DECAL_BULLET, to, vec(from).sub(to).normalize(), 2.0f); //BULLET //4.0
        //adddecal(DECAL_GLOW, to, vec(from).sub(to).normalize(), 3.0f, bvec(0x07, 0x89, 0xFC)); //  0x0789FC);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), gun==GUN_CG ? 30 : 15, vec(0.3f, .3f, 1.5f), gun==GUN_CG ? 50 : 100, gun==GUN_CG ? 50 : 100, DL_FLASH, 0, vec(0, 0, 1), d);
        break;
    }*/

    case GUN_TELEKENESIS:
    { //server should add grenade and orb bouncers
        //float dist;
        // dynent *o = intersectclosest(from, to, d, dist);
        if((!d->isholdingnade && !d->isholdingorb && !d->isholdingprop && !d->isholdingbarrel && !d->isholdingshock)) break;
        particle_splash(PART_SPARK, 200, 250, to, 0xFF64FF, 0.10f);
        particle_flare(hudgunorigin(gun, from, to, d), to, 200, PART_LIGHTNING, 0xFF64FF, 2.f);
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, gun==GUN_CG ? 100 : 200, PART_MUZZLE_FLASH1, 0xFF64FF, 2.25f, d);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), gun==GUN_CG ? 30 : 15, vec(0.5f, 0.5f, 1.f), gun==GUN_CG ? 50 : 100, gun==GUN_CG ? 50 : 100, DL_FLASH, 0, vec(0, 0, 1), d);
        int newtimer = d->caughttime+(d->isholdingnade?1500:5000);
        int finaltimer = newtimer-lastmillis;
        if(d->isholdingprop || d->isholdingbarrel)finaltimer = 20000;
        if(d->isholdingorb)finaltimer = 5000;
        d->attacking=0;
        d->altattacking=0;
        int proptype = -1;
        if(d->isholdingnade)proptype = BNC_GRENADE;
        if(d->isholdingorb)proptype = BNC_ORB;
        if(d->isholdingprop)proptype = BNC_PROP;
        if(d->isholdingbarrel)proptype=BNC_BARREL;
        //vec v;
        //float w = to.dist(d->muzzle, v);
        //int steps = clamp(int(w*2), 1, 2048);
        //v.div(steps);
        //vec q = d->muzzle;
        //loopi(50)q.add(v);
        // float dist = from.dist(to);
        // vec up = to;
        // up.z -= dist/8;
        //char *mdlname= { d->propmodeldir };
        // TODO: Shoot projectile here instead of bouncer if holding shockball
        int speed = d->dropitem ? 10 : (d->isholdingorb ? 700 : 500);
        if (d->isholdingshock) speed = 400;
        if(proptype>-1) newbouncer(from, to, local, id, d, proptype, finaltimer, speed, NULL); //fixme, need from = p and noo bncer offset compensation
        else if (d->isholdingshock) {
            newprojectile(from, to, (float)speed, local, id, d, GUN_ELECTRO2);
        }
        //particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0xFF64FF, 1.f, 50, 501, NULL, 2, NULL, 2);
        //strcpy(d->propmodeldir, "");
        //conoutf(CON_GAMEINFO, "%s", d->propmodeldir);
        //if(d->state!=CS_ALIVE)newbouncer(from, to, local, id, d, proptype, finaltimer, d->isholdingorb?350:50);
        //conoutf(CON_GAMEINFO, "Launching with %d ttl", finaltimer);
        d->isholdingorb=0;
        d->isholdingnade=0;
        d->isholdingprop=0;
        d->isholdingshock=0;
        d->isholdingbarrel=0;
        if(d==player1 || d->ai)addmsg(N_CATCH, "rciiiiii", d, d->isholdingnade, d->isholdingorb, d->isholdingprop, d->isholdingbarrel, d->isholdingshock, d->propmodeldir);
        //d->gunwait+=500;
        if(d==hudplayer()) { d->screenjumpheight=30; screenjump(); screenjump();}
        //d->blewup=0;
        break;
    }


    case GUN_TELEKENESIS2:
    {
        vec v;
        float dist = to.dist(from, v);
        int steps = clamp(int(dist*2), 1, 200);
        v.div(steps);
        vec p = from;
        if(d->isholdingbarrel || d->isholdingnade || d->isholdingorb || d->isholdingprop || d->isholdingshock)return;
        // ALWAYS show muzzle flash for other players (no occlusion check)
        if(muzzleflash && d->muzzle.x >= 0 && d!=player1)
            particle_flare(d->muzzle, d->muzzle, 25, PART_GLOW, 0xFF64FF, 2.5f);

        loopi(steps)
        {
            p.add(v);

            loopv(projs) // Try catching projectiles
            {
                projectile& proj = projs[i];
                if (proj.o.dist(p) <= 15 && !d->isholdingprop && !d->isholdingnade && !d->isholdingorb && !d->isholdingshock && !d->isholdingbarrel && proj.gun==GUN_ELECTRO2)
                {
                    vec pos(proj.o);
                    if (proj.local)
                        if (d == player1 || d->ai) addmsg(N_EXPLODE, "rci3iv", proj.owner, lastmillis - maptime, GUN_TELEKENESIS, proj.id - maptime, //delete all tk objects
                            hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
                    if (d == player1 || d->ai)addmsg(N_CATCH, "rciiiiii", d, d->isholdingnade, d->isholdingorb, d->isholdingprop, d->isholdingbarrel, d->isholdingshock, "");
                    projs.remove(i--);

                    if(d == player1 || d->ai) d->isholdingshock = 1;
                    if (d->ai)d->ailastcatch = lastmillis;
                    d->caughttime = lastmillis;
                    d->altattacking = 0;
                    d->flarepos = d->o;
                    if (d == player1 || d->ai)d->propmodeldir = NULL;

                }
            }

            loopv(bouncers){ // Try catching bouncers
                bouncer &bnc = *bouncers[i];
                vec pos = bnc.o;
                if(pos.dist(p) <= 15 && !d->isholdingprop && !d->isholdingnade && !d->isholdingorb && !d->isholdingshock && !d->isholdingbarrel &&(bnc.bouncetype==BNC_GRENADE || bnc.bouncetype==BNC_ORB || bnc.bouncetype==BNC_PROP || bnc.bouncetype==BNC_BARREL || bnc.bouncetype==BNC_WEAPON))
                {
                    if(bnc.bouncetype==BNC_WEAPON && d==player1)
                    {
                        bnc.caught=1;
                        int maxammo = 0;

                        switch(bnc.gun)
                        {
                        case GUN_FIST:
                        case GUN_TELEKENESIS:
                        case GUN_TELEKENESIS2:
                            maxammo = 1;
                            break;
                        case GUN_SMG:
                            maxammo = 140;
                            break;


                        case GUN_RL:
                            maxammo = 15; break;
                        case GUN_CG2:
                            maxammo=3; break;
                        case GUN_SMG2:
                            maxammo = 5;
                            break;
                        case GUN_MAGNUM:
                            maxammo = 24;
                            break;

                        case GUN_SG:
                        case GUN_SHOTGUN2:
                            maxammo = 30;
                            break;
                        case GUN_HANDGRENADE: maxammo = 5; break;

                        case GUN_CG:
                            maxammo = 120;
                            break;
                        case GUN_ELECTRO2:
                            maxammo = 80;
                            break;
                        case GUN_ELECTRO:
                            maxammo = 30;
                            break;
                        case GUN_CROSSBOW:
                            maxammo = 10;
                            break;

                        case GUN_PISTOL:
                            maxammo = 150;
                            break;
                        }
                        player1->hasgun[bnc.gun]=1;
                        player1->ammo[bnc.gun]=maxammo;
                        if(bnc.gun==GUN_SG)player1->ammo[GUN_SHOTGUN2]=maxammo;
                        if(bnc.gun==GUN_SHOTGUN2)player1->ammo[GUN_SG]=maxammo;
                        msgsound(S_ROCKETPICKUP, player1);
                        conoutf(CON_TEAMCHAT, "You picked up a %s", guns[bnc.gun].name);
                        return;
                    }
                    if(bnc.bouncetype==BNC_GRENADE && (d==player1 || d->ai))  { d->isholdingnade=1; }
                    if(d->isholdingnade)particle_flare(bnc.o, d->o, 500, PART_RAILTRAIL, nadetrailcol, .2f);
                    if(bnc.bouncetype==BNC_ORB && (d==player1 || d->ai)) { d->isholdingorb=1; }
                    if(d->isholdingorb)particle_flare(bnc.o, d->o, 75, PART_GLOW, 0x553322, 4.0f);

                    //const char *mdlname = bnc.model;
                    //strcpy(d->propmodeldir, mdlname);
                    //conoutf(CON_GAMEINFO, "%s", d->propmodeldir);
                    if(d==player1 || d->ai)d->propmodeldir=bnc.model;

                    if(bnc.bouncetype==BNC_PROP && (d==player1 || d->ai))   { d->isholdingprop=1;  }
                    if(bnc.bouncetype==BNC_BARREL && (d==player1 || d->ai)) { d->isholdingbarrel=1;  }
                    //else if(bnc.bouncetype==BNC_PROP)d->isholdingprop=1;
                    d->caughttime=lastmillis;
                    d->altattacking=0;
                    d->flarepos=d->o;
                    //int guntype=0;
                    //if(bnc.bouncetype==BNC_GRENADE)guntype==GUN_HANDGRENADE
                    if(d==player1 || d->ai) addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, GUN_TELEKENESIS, bnc.id-maptime, //delete all tk objects
                                                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                    delete bouncers.remove(i--);
                    //int skillshot =0;
                    //if(d->isholdingnade) skillshot=1;
                    //else if(d->isholdingorb) skillshot=2;
                    if(d==player1 || d->ai)addmsg(N_CATCH, "rciiiiii", d, d->isholdingnade, d->isholdingorb, d->isholdingprop, d->isholdingbarrel, d->isholdingshock, bnc.model);
                    if(d->ai)d->ailastcatch=lastmillis;
                    //particle_flare(d->muzzle, pos, 200, PART_LIGHTNING, 0x5050FF, 1.f);

                }

            }
        }
        if(!d->isholdingbarrel&&!d->isholdingnade&&!d->isholdingorb&&!d->isholdingprop&&!d->isholdingshock)
            loopv(entities::ents) //picking up items
        {
            extentity &e = *entities::ents[i];
            if(e.type==NOTUSED) continue;
            if(!e.spawned || e.type==TELEPORT || e.type==JUMPPAD || e.type==RESPAWNPOINT) continue;
            if(!d->isholdingorb && !d->isholdingnade && !d->isholdingprop && !d->isholdingbarrel && !d->isholdingshock)
            {
                float dist2 = e.o.dist(to);
                if(dist2<(8) && d==player1) { entities::trypickup(i, d); d->altattacking=0; if(d->canpickup(i)){particle_flare(d->muzzle, e.o, 50, PART_LIGHTNING, 0xFF64FF, 1.f); break;}
                }
            }
        }
        break;
    }

    case GUN_ELECTRO:
    {
        playsound(S_NEXIMPACT, &to);
        particle_flare(to, to, 200, PART_MUZZLE_FLASH2, 0x0789FC, 6.f);
        particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x0789FC, 1.4f, 50, 501, NULL, 2, NULL, 2);
        //particle_trail(1, 1, from, to, 1, 1, 1, true);
        vec lolwut(to);
        lolwut.sub(d->muzzle.x >= 0 ? d->muzzle : hudgunorigin(gun, from, to, d));
        loopi(40) {
            vec temp(lolwut);
            temp.normalize().mul(8000.f-(i*150));
            // ALWAYS show electric/spark effects (no occlusion check)
            particle_flying_flare(d->muzzle.x >= 0 ? d->muzzle : hudgunorigin(gun, from, to, d), temp, 1000, PART_SPARK, 0x0789FC, 5.f - (i * .05), 100);
        }

        particle_splash(PART_SPARK, 2, 300, to, 0x0789FC, 5.f, 2, 50);
        //particle_fireball(to, 5.f, PART_EXPLOSION, 200, 0x0789FC, 5.f);
        //particle_flare(hudgunorigin(gun, from, to, d), to, 1000, PART_STREAK, 0x0789FC, 0.65f);
        //particle_trail(0, 0, from, to, 0, 0, 0, true);
        //particle_flare(hudgunorigin(gun, from, to, d), to, 250, PART_LIGHTNING, 0x0789FC, 3.f);
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, gun==GUN_CG ? 100 : 200, PART_MUZZLE_FLASH1, 0x0789FC, gun==GUN_CG ? 2.25f : 1.25f, d);
        adddecal(DECAL_BULLET, to, vec(from).sub(to).normalize(), 3.0f); //BULLET //4.0
        //adddecal(DECAL_GLOW, to, vec(from).sub(to).normalize(), 3.0f, bvec(0x07, 0x89, 0xFC)); //  0x0789FC);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 15, vec(0.5f, 0.5f, 1.f), 100, 100, DL_FLASH, 0, vec(0, 0, 1), d);
        if(muzzlelight) adddynlight(to, 30, vec(0.5f, 0.5f, 1.f), 100, 100, DL_FLASH, 0, vec(0, 0, 1), d);
        //if (d == hudplayer()) { d->screenjumpheight = 10; screenjump(); screenjump(); }



        // Check if we hit a shock combo. If so, explode the projectile in the air
        vec v;
        float dist = to.dist(from, v);
        int steps = clamp(int(dist * 2), 1, 200);
        v.div(steps);
        vec p = from;
        vec raycheck;
        bool hitCombo = false;
        loopi(steps)
        {
            if (hitCombo) break;
            p.add(v);
            loopv(projs)
            {
                projectile& proj = projs[i];
                if (proj.o.dist(p) <= 5 && !hitCombo)
                {
                    if (proj.gun == GUN_ELECTRO2 && d == player1)
                    {
                        hitCombo = true;
                        int qdam = guns[proj.gun].damage * (proj.owner->quadmillis ? 2 : 1);
                        projsplash(proj, proj.o, NULL, qdam, false);
                        if (d == player1 || d->ai) {
                            addmsg(N_EXPLODE, "rci3iv", proj.owner, lastmillis - maptime, proj.gun, proj.id - maptime,
                                hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
                            projs.remove(i--);
                        }
                        break;
                    }
                }
            }
        }
        break;
    }



    case GUN_CG:
    case GUN_MAGNUM:
    //case GUN_PISTOL:
    case GUN_SMG:
        //case GUN_ELECTRO:
    {
        if (d->gunselect==GUN_MAGNUM && d->mods & MOD_LIGHTNINGGUN && !lgsniper) {
            playsound(S_LG_IMPACT, &to);
            particle_splash(PART_SPARK, 200, 250, to, 0x5050FF, 0.10f);
            particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x5050FF, 1.5f, 50, 501, NULL, 2, NULL, 2);
            particle_splash(PART_SMOKE, 3, 500, to, 0x5050FF, 2.f, 50, 501, NULL, 2, NULL, 2);
            particle_trail(1, 1, from, to, 1, 1, 1, true);
            particle_flare(hudgunorigin(gun, from, to, d), to, 300, PART_LIGHTNING, 0x5050FF, 0.7f);
            particle_flare(d->muzzle.x >= 0 ? d->muzzle : hudgunorigin(gun, from, to, d), d->muzzle.x >= 0 ? d->muzzle : hudgunorigin(gun, from, to, d), 200, PART_MUZZLE_FLASH3, 0x5050FF, .5f, d);
            adddecal(DECAL_BULLET, to, vec(from).sub(to).normalize(), 2.0f); //BULLET //4.0
            //adddecal(DECAL_GLOW, to, vec(from).sub(to).normalize(), 3.0f, bvec(0x07, 0x89, 0xFC)); //  0x0789FC);
            if (muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 15, vec(0.3f, 0.3f, 1.5f), 100, 100, DL_FLASH, 0, vec(0, 1, 0), d);
            if (d == hudplayer()) { d->screenjumpheight = 25; screenjump(); screenjump(); }
            break;
        }

        //if(isheadshot(d, from, to))d->headshot=1;
        if(sebulletimpactsound && lookupmaterial(camera1->o)!=MAT_WATER) playsound(S_BHIT1+rnd(3), &to);
        //particle_splash(PART_SPARK, d->quadmillis ? 100 : 20, 100, to, 0xFFFFFF, 0.1f);
        particle_splash(PART_SPARK, 20, 100, to, 0xFFFFFF, 0.1f);
        particle_explodesplash(to, 50, PART_BULLET, 0xFFFFFF, 0.2f, 500, 8);
        particle_flare(d->muzzle.x>=0?d->muzzle:hudgunorigin(gun, from, to, d), to, 500, PART_SMOKETRAIL, 0x333333, 0.01f, d, 1);
        if(d!=hudplayer()) sound_nearmiss(S_NEARMISS1+rnd(3), from, to);
        //                if(d->quadmillis)
        //                {
        //                    particle_splash(PART_FLAME1, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME2, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME3, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME1, 15, 50, to, 0xFF0000, 0.48f, 500);
        //                    particle_splash(PART_FLAME2, 15, 50, to, 0xFF0000, 0.48f, 500);
        //                    particle_splash(PART_FLAME3, 15, 50, to, 0xFF0000, 0.48f, 500);
        //                    if(sebulletimpactsound) playsound(S_QHIT1+rnd(3), &to, NULL, NULL, NULL, NULL, 100);
        //                }
        if(sebulletimpactsmoke)
        {
            particle_splash(PART_SMOKE, 3, 500, to, 0x444444, 1.4f, 50, 504, NULL, 2, NULL, 1);
        }
        //if(d->quadmillis) particle_trail(PART_FLAME1, 50, hudgunorigin(gun, from, to, d), to, 0xFF0000, 0.6f, 200);
        vec lolwut(to);
        lolwut.sub(d->muzzle.x>=0?d->muzzle:hudgunorigin(gun, from, to, d));
        lolwut.normalize().mul(6000.0f);
        // ALWAYS show bullet tracers (no occlusion check)
        particle_flying_flare(d->muzzle.x>=0?d->muzzle:hudgunorigin(gun, from, to, d), lolwut, 500, PART_BULLET, 0xFFFFFF, 0.5f, 100);
        if(seimprovedguneffects)
        {
            particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x222222, 1.4f, 50, 501, NULL, 2, NULL, 2);
            particle_trail(1, 1, from, to, 1, 1, 1, true);
        }

        adddecal(DECAL_BULLET, to, vec(from).sub(to).normalize(), 2.0f);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), gun==GUN_CG ? 30 : 15, vec(0.5f, 0.375f, 0.25f), gun==GUN_CG ? 50 : 100, gun==GUN_CG ? 50 : 100, DL_FLASH, 0, vec(0, 0, 0), d);
        if ((gun == GUN_MAGNUM || gun == GUN_ELECTRO) && d == hudplayer()) { d->screenjumpheight = 20; screenjump(); screenjump(); } //d->attacking=0; d->altattacking=0;}
        //else if(d==player1) {d->screendown=1; d->screenup=1; screenjump(); screenjump(); }
        if(gun==GUN_PISTOL)d->attacking=d->altattacking=0;
        // Eject shell casings for SMG and Pulse Rifle
        if(gun == GUN_CG || gun == GUN_SMG || gun == GUN_SMG2 || gun == GUN_MAGNUM)
        {
            vec shellstart = hudgunorigin(gun, d->o, to, d);
            shellstart.z -= 0.5f;

            vec forward, left;
            vecfromyawpitch(d->yaw, 0, 1, 0, forward);
            if(gun == GUN_CG) {
                shellstart.add(vec(forward).mul(-4.0f));
            } else if(gun == GUN_SMG || gun == GUN_SMG2) {
                shellstart.add(vec(forward).mul(3.0f));
            }

            vecfromyawpitch(d->yaw - 90, 0, 1, 0, left);

            vec shelldir = vec(left).mul(1.5f + (rnd(11) / 10.0f));
            shelldir.z = (1.2f + (rnd(7) / 10.0f));
            shelldir.add(vec(forward).mul(0.2f + (rnd(11) / 10.0f)));

            vec shellto = vec(shellstart).add(shelldir);
            newbouncer(shellstart, shellto, local, id, d, BNC_SHELL, 8000, 40 + rnd(20));
        }
        if(gun!=GUN_CG && gun!=GUN_SMG)
        {
            if(d->roll >= 0)
            {
                d->roll -= droll;
            }
            else d->roll += droll; }

        if(d->gunselect!=GUN_MAGNUM && d->gunselect!=GUN_ELECTRO && d==hudplayer())
        {
            float rnd=(float)-1+rnd(3);
            rnd/=2;
                d->pitch+=rnd;
                d->yaw+=rnd;
        }


        if(muzzleflash && d->muzzle.x >= 0)
        {
            if(gun==GUN_MAGNUM)particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH1, 0xFFFFFF, 3.0f, d);
            else particle_flare(d->muzzle, d->muzzle, 25, PART_MUZZLE_FLASH1, 0xFFFFFF, 1.875f, d);
        }
        else //if(muzzleflash && d->muzzle.x <= 0)
        {
            d->lastflash=lastmillis;
        }
        break;
    }
        //        case GUN_ELECTRO2:
        //            newprojectile(from, to, (float)pspeed, local, id, d, gun);
        //            if(d->roll >= 0)
        //            {
        //                d->roll -= droll;
        //            }
        //            else d->roll += droll;
        //            //if(d==player1){d->screendown=1; d->screenup=1; screenjump(); screenjump();}
        //            break;
    case GUN_RL:
    case GUN_ELECTRO2:
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, 250, PART_MUZZLE_FLASH2, 0xFFFFFF, 3.0f, d);
        //        if(gun==GUN_ELECTRO){
        //            vec lolwut(to);
        //            lolwut.sub(d->o);
        //            lolwut.normalize().mul(2000.0f);
        //            particle_flying_flare(d->muzzle, lolwut, 500, PART_BULLET, 0xFFFFFF, 2.5f, 100);
        //        }

    case GUN_FIREBALL:
    case GUN_ICEBALL:
    case GUN_SLIMEBALL:
        if(d->type==ENT_AI) pspeed /= 2;
        newprojectile(from, to, (float)pspeed, local, id, d, gun);
        if(d->roll >= 0)
        {
            d->roll -= droll;
        }
        else d->roll += droll;
        break;

    case GUN_SMG2: //case GUN_CG2:
    {
        //float dist = from.dist(to);
        vec up = to;
        //up.z += gun==GUN_SMG2 ? dist/8 : 0;
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH2, 0xFFFFFF, 2.5f, d);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 20, vec(0.5f, 0.375f, 0.25f), 100, 100, DL_FLASH, 0, vec(0, 0, 0), d);
        newbouncer(from, up, local, id, d, gun==GUN_SMG2 ? BNC_SMGNADE : BNC_ORB, guns[gun].ttl, guns[gun].projspeed);
        if(d->roll >= 0)
        {
            d->roll -= droll;
        }
        else d->roll += droll;
        if(d==hudplayer()) { d->screenjumpheight=10; screenjump(); screenjump();}
        // Eject shell casing for double barrel shotgun
        {
            vec shellstart = hudgunorigin(gun, d->o, to, d);
            shellstart.z -= 0.5f;

            vec forward, left;
            vecfromyawpitch(d->yaw, 0, 1, 0, forward);
            vecfromyawpitch(d->yaw - 90, 0, 1, 0, left);

            vec shelldir = vec(left).mul(1.5f + (rnd(11) / 10.0f));
            shelldir.z = (1.2f + (rnd(7) / 10.0f));
            shelldir.add(vec(forward).mul(0.2f + (rnd(11) / 10.0f)));

            vec shellto = vec(shellstart).add(shelldir);
            newbouncer(shellstart, shellto, local, id, d, BNC_SHELL, 8000, 40+rnd(20));
        }
        break;
    }


    case GUN_CG2:
    {
        //float dist = from.dist(to);
        vec up = to;
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH2, 0xFFFFFF, 2.5f, d);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 20, vec(0.5f, 0.375f, 0.25f), 100, 100, DL_FLASH, 0, vec(0, 0, 0), d);
        newbouncer(from, up, local, id, d, BNC_ORB, guns[gun].ttl, d->ai?700:guns[gun].projspeed); //so messages sent to the server when the orb vaporizes can be sent with cg2 and expires with GUN_FIREBALL
        if(d->roll >= 0)
        {
            d->roll -= droll;
        }
        else d->roll += droll;
        if(d==hudplayer()) { d->screenjumpheight=5; screenjump(); screenjump();}
        break;
    }

    case GUN_HANDGRENADE:
    case GUN_CROSSBOW:


    {
        //float dist = from.dist(to);
        vec up = to;
        //up.z += dist/8;
        if(muzzleflash && d->muzzle.x >= 0)
            particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH2, 0xFFFFFF, 1.5f, d);
        if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 20, vec(0.5f, 0.375f, 0.25f), 100, 100, DL_FLASH, 0, vec(0, 0, 0), d);
        int speed=guns[gun].projspeed;
        int timer=guns[gun].ttl;
        if(d->o.dist(to)>150 && d->gunselect==GUN_HANDGRENADE && d->ai)timer=d->o.dist(to);
        if(d->ai && d->gunselect==GUN_HANDGRENADE) { timer; speed=500; }
        newbouncer(from, up, local, id, d, gun==GUN_CROSSBOW ? BNC_XBOLT : BNC_GRENADE, timer, speed); //nadetimer
        //if(d->gunselect==GUN_CROSSBOW)playsound(S_CROSSBOWRELOAD, d==hudplayer()?NULL:&d->o);
        if(d->gunselect==GUN_CROSSBOW&& d==hudplayer()){d->screenjumpheight=10; screenjump(); screenjump();}
        if(d->roll >= 0)
        {
            d->roll -= droll;
        }
        else d->roll += droll;
        break;
    }
        //        case GUN_ELECTRO2:
        //       {
        //            d->detonateelectro=0;
        //           if(muzzleflash && d->muzzle.x >= 0)
        //               particle_flare(d->muzzle, d->muzzle, 250, PART_MUZZLE_FLASH3, 0x0789FC, 2.0f, d);//0xFF4B19
        //           //float dist = from.dist(to);
        //           vec up = to;
        //           //up.z += dist/8;
        //           if(muzzleflash && d->muzzle.x >= 0)
        //               particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH2, 0xFFFFFF, 1.5f, d);
        //           if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 20, vec(0.5f, 0.375f, 0.25f), 100, 100, DL_FLASH, 0, vec(0, 0, 0), d);
        //           newbouncer(from, up, local, id, d, BNC_ELECTROBOLT, guns[gun].ttl, guns[gun].projspeed);
        //           if(d->roll >= 0)
        //           {
        //               d->roll -= droll;
        //           }
        //           else d->roll += droll;
        //           break;
        //       }

        //            case GUN_MAGNUM:
        //                if(seimprovedguneffects)
        //                {
        //                    particle_flare(hudgunorigin(gun, from, to, d), to, 175, PART_STREAK, 0xFFC864, 0.28f);
        //                    particle_splash(PART_SMOKE, 3, 500, d->muzzle, 0x222222, 1.4f, 50, 501, NULL, 2, NULL, 2);
        //                    particle_trail(0, 0, from, to, 0, 0, 0, true);
        //                }
        //                if(d!=hudplayer()) sound_nearmiss(S_NEARMISS1+rnd(3), from, to);
        //                if(sebulletimpactsound && lookupmaterial(camera1->o)!=MAT_WATER) playsound(S_BHIT1+rnd(3), &to, NULL, NULL, NULL, NULL, 200);
        //                particle_splash(PART_SPARK, d->quadmillis ? 100 : 20, 100, to, 0xFFFFFF, 0.1f);
        //                particle_explodesplash(to, 50, PART_BULLET, 0xFFFFFF, 0.2f, 500, 8);
        //                if(d->quadmillis)
        //                {
        //                    particle_splash(PART_FLAME1, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME2, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME3, 15, 25, d->muzzle, 0xFF0000, 1.0f, 500);
        //                    particle_splash(PART_FLAME1, 15, 50, to, 0xFF0000, 0.48f, 500);
        //                    particle_splash(PART_FLAME2, 15, 50, to, 0xFF0000, 0.48f, 500);
        //                    particle_splash(PART_FLAME3, 15, 50, to, 0xFF0000, 0.48f, 500);
        //                    if(sebulletimpactsound) playsound(S_QHIT1+rnd(3), &to, NULL, NULL, NULL, NULL, 100);
        //                }
        //                if(sebulletimpactsmoke)
        //                {
        //                    particle_splash(PART_SMOKE, 3, 500, to, 0x444444, 1.4f, 50, 504, NULL, 2, NULL, 1);
        //                    //particle_splash_d(PART_SMOKE, 1, 500, to, 0x444444, 2.4f, 50, 500);
        //                }
        //                if(d->quadmillis) particle_trail(PART_FLAME1, 50, hudgunorigin(gun, from, to, d), to, 0xFF0000, 0.6f, 200);
        //                if(serifletrail) particle_trail(PART_SMOKE, 500, hudgunorigin(gun, from, to, d), to, 0x555555, 0.6f, 200);
        //                particle_flare(d->muzzle, to, 500, PART_SMOKETRAIL, 0x333333, 0.1f, d, 2);
        //                if(muzzleflash && d->muzzle.x >= 0)
        //                {
        //                    particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH4, 0xFFFFFF, 5.0f, d);
        //                }
        //                if(!local) adddecal(DECAL_BULLET, to, vec(from).sub(to).normalize(), 3.0f);
        //                if(muzzlelight) adddynlight(hudgunorigin(gun, d->o, to, d), 25, vec(0.5f, 0.375f, 0.25f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
        //                break;
    }

    bool looped = false;
    if(d->attacksound >= 0 && d->attacksound != sound) d->stopattacksound();
    if(d->idlesound >= 0) d->stopidlesound();
    if (d->gunselect==GUN_MAGNUM && d->mods & MOD_LIGHTNINGGUN && !lgsniper) sound = S_LG_FIRE;
    if (d->gunselect == GUN_MAGNUM && d->mods & MOD_LIGHTNINGGUN && lgsniper) sound = S_MAGNUM;
    switch(sound)
    {
    case S_CHAINSAW_ATTACK:
        if(d->attacksound >= 0) looped = true;
        d->attacksound = sound;
        d->attackchan = playsound(sound, d==hudplayer() ? NULL : &d->o, NULL, -1, 100, d->attackchan);
        break;
    case S_SG:
        if(d==hudplayer()) {
            playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_SG1+rnd(3):S_UWST3);
            if(lookupmaterial(camera1->o) != MAT_WATER) playsound(S_SGLOAD);
        } else {
            if(lookupmaterial(camera1->o)==MAT_WATER)
                playsound(S_UWST3+rnd(3), &d->o);
            else
                playsound(lookupmaterial(d->o)!=MAT_WATER?S_SG1+rnd(3):S_UWST3, &d->o);
        }
        break;
    case S_RIFLE:
        if(d==hudplayer()) {
            playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_RIFLE1+rnd(3):S_UWST3);
            //if(lookupmaterial(camera1->o) != MAT_WATER) playsound(S_RIFLELOAD);
        } else {
            if(lookupmaterial(camera1->o)==MAT_WATER)
                playsound(S_UWST3, &d->o);
            else
                playsound(lookupmaterial(d->o)!=MAT_WATER?S_RIFLE1+rnd(3):S_UWST3, &d->o);
        }
        break;
    case S_FLAUNCH:
        if(d==hudplayer()) {
            playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_FLAUNCH1+rnd(3):S_UWST2);
            if(lookupmaterial(camera1->o)!=MAT_WATER)
                playsound(S_GLLOAD);
        } else {
            if(lookupmaterial(camera1->o)==MAT_WATER)
                playsound(S_UWST2, &d->o);
            else
                playsound(lookupmaterial(d->o)!=MAT_WATER?S_FLAUNCH1+rnd(3):S_UWST2, &d->o);
        }
        break;
    case S_CG:
        if(d==hudplayer()) {
            playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_CG1+rnd(3):S_UWST2);
        } else {
            if(lookupmaterial(camera1->o)==MAT_WATER)
                playsound(S_UWST2, &d->o);
            else
                playsound(lookupmaterial(d->o)!=MAT_WATER?S_CG1+rnd(3):S_UWST2, &d->o);
        }
        break;
    case S_PL:
        if(d==hudplayer()) {
            playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_PL1+rnd(3):S_UWST2);
        } else {
            if(lookupmaterial(camera1->o)==MAT_WATER)
                playsound(S_UWST2, &d->o);
            else
                playsound(lookupmaterial(d->o)!=MAT_WATER?S_PL1+rnd(3):S_UWST2, &d->o);
        }
        break;
    case S_RLFIRE:
        if(d==hudplayer()) {
            playsound(lookupmaterial(camera1->o)!=MAT_WATER?sound:S_UWST3);
        } else {
            if(lookupmaterial(camera1->o)==MAT_WATER)
                playsound(S_UWST3, &d->o);
            else
                playsound(lookupmaterial(d->o)!=MAT_WATER?sound:S_UWST3, &d->o);
        }
        break;
    default:
        playsound(sound, d==hudplayer() ? NULL : &d->o);
        break;
    }
    //if(d->quadmillis && lastmillis-prevaction>200 && !looped) playsound(S_ITEMPUP, d==hudplayer() ? NULL : &d->o);
}

void particletrack(physent *owner, vec &o, vec &d)
{
    if(owner->type!=ENT_PLAYER && owner->type!=ENT_AI) return;
    fpsent *pl = (fpsent *)owner;
    if(pl->muzzle.x < 0 || pl->lastattackgun != pl->gunselect) return;
    float dist = o.dist(d);
    o = pl->muzzle;
    if(dist <= 0) d = o;
    else
    {
        vecfromyawpitch(owner->yaw, owner->pitch, 1, 0, d);
        float newdist = raycube(owner->o, d, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
        d.mul(min(newdist, dist)).add(owner->o);
    }
}

void dynlighttrack(physent *owner, vec &o)
{
    if(owner->type!=ENT_PLAYER && owner->type!=ENT_AI) return;
    fpsent *pl = (fpsent *)owner;
    if(pl->muzzle.x < 0 || pl->lastattackgun != pl->gunselect) return;
    o = pl->muzzle;
}

VARP(tryreload, 0, 0, 1);


void doreload(fpsent *d)
{
    if(lastmillis-d->lastreload<1500 || d->ammo[d->gunselect]==0 || !d->magprogress[d->gunselect] || lastmillis-d->lastaction<guns[d->lastattackgun].attackdelay) {setvar("tryreload", 0); return; }
    d->gunwait=1500;
    d->magprogress[d->gunselect]=0;
    if(d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2) { d->magprogress[GUN_SHOTGUN2]=0; d->magprogress[GUN_SG]=0; }
    msgsound(S_RELOAD, d);
    d->burstprogress=0;
    d->lastreload=lastmillis;
    if(d==player1)setvar("tryreload", 0);
}

// CS 1.6-style spray patterns (yaw, pitch offsets for each shot)
// Pattern repeats after last entry
struct SprayPattern {
    float yaw;   // horizontal offset (degrees)
    float pitch; // vertical offset (degrees)
};

// Default spray pattern (similar to AK-47 in CS 1.6)
// Each entry is TOTAL offset from center (not incremental)
// Reaches max spread at shot 15 - REDUCED BY HALF for better accuracy
static const SprayPattern defaultSprayPattern[] = {
    {0.0f, 0.0f},       // Shot 1: accurate
    {0.0f, 0.45f},      // Shot 2: slight upward
    {0.05f, 1.0f},      // Shot 3: slight right
    {-0.15f, 1.6f},     // Shot 4: left
    {0.25f, 2.25f},     // Shot 5: right
    {-0.4f, 2.95f},     // Shot 6: left
    {0.55f, 3.7f},      // Shot 7: right
    {-0.75f, 4.5f},     // Shot 8: left
    {0.95f, 5.3f},      // Shot 9: right
    {-1.2f, 6.1f},      // Shot 10: left
    {1.45f, 6.9f},      // Shot 11: right
    {-1.75f, 7.65f},    // Shot 12: left
    {2.05f, 8.35f},     // Shot 13: right
    {-2.4f, 9.0f},      // Shot 14: left
    {2.75f, 9.6f},      // Shot 15: right - APPROACHING MAX
    {-3.0f, 10.0f},     // Shot 16: left - MAX SPREAD
    {3.0f, 10.0f},      // Shot 17: right (capped)
    {-3.0f, 10.0f},     // Shot 18: left (capped)
    {3.0f, 10.0f},      // Shot 19: right (capped)
    {-3.0f, 10.0f},     // Shot 20: left (capped)
    {3.0f, 10.0f},      // Shot 21+: stays at max spread
    {-3.0f, 10.0f},
    {3.0f, 10.0f},
    {-3.0f, 10.0f},
    {3.0f, 10.0f},
    {-3.0f, 10.0f},
    {3.0f, 10.0f},
    {-3.0f, 10.0f},
    {3.0f, 10.0f},
    {-3.0f, 10.0f}
};
#define DEFAULT_PATTERN_LEN (sizeof(defaultSprayPattern)/sizeof(defaultSprayPattern[0]))

// Recoil configuration per weapon
struct WeaponRecoil {
    const SprayPattern* pattern;
    int patternLength;
    float multiplier;        // scales the pattern intensity
    float recoveryRate;      // degrees per millisecond of recovery
    int resetTime;          // ms before pattern resets
    float viewPunchScale;   // how much camera moves with recoil
};

// Weapon-specific recoil settings
WeaponRecoil weaponRecoils[NUMGUNS];

void initRecoilPatterns() {
    static bool initialized = false;
    if(initialized) return;
    initialized = true;
    
    // Initialize all weapons with no recoil by default
    loopi(NUMGUNS) {
        weaponRecoils[i].pattern = NULL;
        weaponRecoils[i].patternLength = 0;
        weaponRecoils[i].multiplier = 0.0f;
        weaponRecoils[i].recoveryRate = 0.01f;
        weaponRecoils[i].resetTime = 400;
        weaponRecoils[i].viewPunchScale = 0.0f;
    }
    
    // SMG - moderate recoil
    weaponRecoils[GUN_SMG].pattern = defaultSprayPattern;
    weaponRecoils[GUN_SMG].patternLength = DEFAULT_PATTERN_LEN;
    weaponRecoils[GUN_SMG].multiplier = 0.8f;  // less aggressive
    weaponRecoils[GUN_SMG].recoveryRate = 0.027f;
    weaponRecoils[GUN_SMG].resetTime = 300;
    weaponRecoils[GUN_SMG].viewPunchScale = 1.2f;
    
    // Pulse Rifle (CG) - more aggressive recoil
    weaponRecoils[GUN_CG].pattern = defaultSprayPattern;
    weaponRecoils[GUN_CG].patternLength = DEFAULT_PATTERN_LEN;
    weaponRecoils[GUN_CG].multiplier = 1.2f;  // more aggressive
    weaponRecoils[GUN_CG].recoveryRate = 0.0216f;
    weaponRecoils[GUN_CG].resetTime = 350;
    weaponRecoils[GUN_CG].viewPunchScale = 1.5f;
}

#define RECOIL_COOLDOWN 200 //200ms until our recoil is reset and next shot will have 0 spread
#define MAXSPREAD 25

void shoot(fpsent *d, const vec &targ)
{
    // Initialize recoil patterns on first use
    initRecoilPatterns();
    
    if (d->gunselect == GUN_MAGNUM && d->altattacking && !d->attacking) return;
    int prevaction = d->lastaction, attacktime = lastmillis-prevaction;
    
    // CS 1.6-style timing-based recoil system for automatic weapons
    if(d->gunselect == GUN_SMG || d->gunselect == GUN_CG) {
        WeaponRecoil &recoil = weaponRecoils[d->gunselect];
        
        // Check if enough time has passed to reset the spray pattern
        int timeSinceLastShot = lastmillis - d->lastShotTime;
        if(timeSinceLastShot > recoil.resetTime) {
            // Reset pattern to beginning
            d->recoilPatternIndex = 0;
        }
    }
    // Old recoil system for pistol (frame-based)
    else if(d->gunselect == GUN_PISTOL) {
        if(d->lastattackmillis<MAXSPREAD && d->attacking) d->lastattackmillis+=6;
        if(!d->attacking && d->lastattackmillis>0) d->lastattackmillis-=1;
    }

    if (attacktime < (d->quadmillis ? d->gunwait / 2 : d->gunwait) || lastmillis - d->lastreload < 1500 || (d->state == CS_DEAD && !d->dropitem)) return;
    d->gunwait = 0;
    if(!d->attacking && !d->altattacking) return;
    if(d->gunselect==GUN_HANDGRENADE && d->altattacking) return;
    if(d->gunselect==GUN_RL && d->altattacking) return;
    //if(d->gunselect==GUN_TELEKENESIS && d->altattacking) d->tkisbusy=1; //make sure to set this because multiple tk functions should not apply at once (pickup item, catch bnc, throw bnc)
    if(d->attacking && !d->ai)
    {
        if(d->gunselect==GUN_SG){d->gunselect=GUN_SHOTGUN2; addmsg(N_GUNSELECT, "rci", d, GUN_SHOTGUN2); }
        if(d->gunselect==GUN_CG2){d->gunselect=GUN_CG; addmsg(N_GUNSELECT, "rci", d, GUN_CG);  }
        if(d->gunselect==GUN_TELEKENESIS2){d->gunselect=GUN_TELEKENESIS; addmsg(N_GUNSELECT, "rci", d, GUN_TELEKENESIS);  }
        if(d->gunselect==GUN_ELECTRO2){d->gunselect=GUN_ELECTRO; addmsg(N_GUNSELECT, "rci", d, GUN_ELECTRO); }
        //if(d->gunselect==GUN_ELECTRO){d->gunselect=GUN_ELECTRO2; addmsg(N_GUNSELECT, "rci", d, GUN_ELECTRO2); }
        //if(d->gunselect==GUN_HANDGRENADE){d->gunselect=GUN_CROSSBOW; addmsg(N_GUNSELECT, "rci", d, GUN_CROSSBOW); }
        //if(d->gunselect==GUN_PISTOL2){d->gunselect=GUN_PISTOL; addmsg(N_GUNSELECT, "rci", d, GUN_PISTOL); }
        //if(d->gunselect==GUN_RL2){d->gunselect=GUN_RL; addmsg(N_GUNSELECT, "rci", d, GUN_RL); }
        if(d->gunselect==GUN_SMG2){d->gunselect=GUN_SMG; addmsg(N_GUNSELECT, "rci", d, GUN_SMG); }
    }
    if(d->altattacking)
    {
        if(d->gunselect==GUN_SHOTGUN2){d->gunselect=GUN_SG; addmsg(N_GUNSELECT, "rci", d, GUN_SG); }
        if(d->gunselect==GUN_CG){d->gunselect=GUN_CG2; addmsg(N_GUNSELECT, "rci", d, GUN_CG2); }
        if(d->gunselect==GUN_TELEKENESIS){d->gunselect=GUN_TELEKENESIS2; addmsg(N_GUNSELECT, "rci", d, GUN_TELEKENESIS2); }
        if(d->gunselect==GUN_ELECTRO){d->gunselect=GUN_ELECTRO2; addmsg(N_GUNSELECT, "rci", d, GUN_ELECTRO2); }
        //if(d->gunselect==GUN_CROSSBOW){d->gunselect=GUN_HANDGRENADE; addmsg(N_GUNSELECT, "rci", d, GUN_HANDGRENADE); }
        //if(d->gunselect==GUN_PISTOL){d->gunselect=GUN_PISTOL2; addmsg(N_GUNSELECT, "rci", d, GUN_PISTOL2); }
        //if(d->gunselect==GUN_RL){d->gunselect=GUN_RL2; addmsg(N_GUNSELECT, "rci", d, GUN_RL2); }
        if(d->gunselect==GUN_SMG){d->gunselect=GUN_SMG2; addmsg(N_GUNSELECT, "rci", d, GUN_SMG2); }
    }


    if((d->lastattackgun==GUN_SHOTGUN2 || d->lastattackgun==GUN_SG) && d->gunselect==GUN_SHOTGUN2 || d->gunselect==GUN_SG)d->lastattackgun=d->gunselect; //no hax switching from sg1 to sg2 :)
    if(d->gunselect==GUN_TELEKENESIS && (!d->isholdingnade && !d->isholdingorb && !d->isholdingbarrel && !d->isholdingprop && !d->isholdingshock)) return;
    if(d->lastattackgun!=d->gunselect){
        d->magprogress[d->lastattackgun]=0;
        if(d->lastattackgun==GUN_SG || d->lastattackgun==GUN_SHOTGUN2)d->magprogress[GUN_SG]=d->magprogress[GUN_SHOTGUN2]=0;
    }
    d->lastattackgun = d->gunselect;
    //if(d->gunselect==GUN_TELEKENESIS && (!d->isholdingnade && !d->isholdingorb && !d->isholdingprop && !d->isholdingbarrel)) return;
    if((d->ammo[d->gunselect]-guns[d->gunselect].sub)<0) //magleft ->ammo
    {
        if(d==player1)
        {
            { msgsound(S_NOAMMO, d);
                d->gunwait = 600;
                d->attacking=0;
                d->lastattackgun = -1;
                d->burstprogress=0;
                d->altattacking=0;
            }
        }
        return;
    }
    if((guns[d->gunselect].magsize-d->magprogress[d->gunselect]-guns[d->gunselect].sub)<0 && guns[d->gunselect].magsize>1)
    {
        if(d==player1)
        {
            { msgsound(S_NOAMMO, d);
                d->gunwait = 600;
                d->lastattackgun = -1;
                d->attacking=0;
                d->burstprogress=0;
                d->altattacking=0;
            }
        }
        return;
    }
    if(d->gunselect && !d->quadmillis) d->ammo[d->gunselect]-=guns[d->gunselect].sub;
    if(d->gunselect==GUN_SG) d->ammo[GUN_SHOTGUN2]-=guns[GUN_SG].sub;
    if(d->gunselect==GUN_SHOTGUN2) d->ammo[GUN_SG]-=guns[GUN_SHOTGUN2].sub;

    if(guns[d->gunselect].magsize>1 && !d->quadmillis)d->magprogress[d->gunselect]+=guns[d->gunselect].sub;
    if(d->gunselect==GUN_SG) d->magprogress[GUN_SHOTGUN2]+=guns[GUN_SG].sub;
    if(d->gunselect==GUN_SHOTGUN2) d->magprogress[GUN_SG]+=guns[GUN_SHOTGUN2].sub;

    d->lastaction = lastmillis;

    vec from = d->o;
    vec to = targ;
    
    // Apply CS 1.6-style spray pattern for automatic weapons
    if(d->gunselect == GUN_SMG || d->gunselect == GUN_CG) {
        WeaponRecoil &recoil = weaponRecoils[d->gunselect];
        
        // Update shot time for timing-based system
        d->lastShotTime = lastmillis;
        
        // Get current pattern offset (cap at last entry, don't wrap)
        int patternIdx = min(d->recoilPatternIndex, recoil.patternLength - 1);
        const SprayPattern &pattern = recoil.pattern[patternIdx];
        
        // Pattern values are TOTAL offset from center (not incremental)
        float yawOffset = pattern.yaw * recoil.multiplier;
        float pitchOffset = pattern.pitch * recoil.multiplier;
        
        // Apply view punch (camera kick) - only for local player
        // This is a FIXED kick per shot, independent of pattern position
        if(d == player1) {
            // VERY strong kick for visual impact
            float kickStrength = d->gunselect == GUN_CG ? 4.0f : 3.0f; // CG kicks harder
            // Add small random horizontal component for realism
            float horizontalKick = (rnd(21) - 10) * 0.05f; // -0.5 to +0.5 degrees
            
            // Accumulate recoil for recovery system
            d->recoilPitchAccum += kickStrength * recoil.viewPunchScale;
            d->recoilYawAccum += horizontalKick * recoil.viewPunchScale;
            
            // Apply immediately to camera
            d->pitch += kickStrength * recoil.viewPunchScale;
            d->yaw += horizontalKick * recoil.viewPunchScale;
        }
        
        // Apply recoil to shot direction (total offset from aim point)
        vec dir;
        vecfromyawpitch(d->yaw + yawOffset, d->pitch + pitchOffset, 1, 0, dir);
        to = dir;
        to.mul(guns[d->gunselect].range);
        to.add(from);
        
        // Advance pattern index (capped at pattern length)
        if(d->recoilPatternIndex < recoil.patternLength) {
            d->recoilPatternIndex++;
        }
    }

    vec unitv;
    float dist = to.dist(from, unitv);
    unitv.div(dist);
    if(!d->quadmillis) {
        vec kickback(unitv);
        kickback.mul(guns[d->gunselect].kickamount*-2.5f);
        d->vel.add(kickback);
    }
    float shorten = 0;
    if(guns[d->gunselect].range && dist > guns[d->gunselect].range)
        shorten = guns[d->gunselect].range;
    float barrier = raycube(d->o, unitv, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
    
    // Store wall info BEFORE modifying anything
    bool hitWall = (barrier > 0 && barrier < dist);
    vec wallHitPoint = from;
    if(hitWall) {
        wallHitPoint = vec(unitv).mul(barrier).add(from);
    }
    
    if(barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
        shorten = barrier;
    
    if(shorten)
    {
        to = unitv;
        to.mul(shorten);
        to.add(from);
    }

    // Use pattern-based recoil for SMG/CG, old system for others
    if(d->gunselect == GUN_SMG || d->gunselect == GUN_CG) {
        // Pattern already applied above, no additional spread
    }
    else if(guns[d->gunselect].rays>1) createrays(d->gunselect, from, to, d);
    else if(d->lastattackmillis) offsetray(from, to, d->lastattackmillis*4, guns[d->gunselect].range, to, d->quadmillis ? false : true);

    //d->lastattackmillis = lastmillis;

    hits.setsize(0);

    vec gaussdest;
    vecfromyawpitch(d->yaw, d->pitch, 1, 0, gaussdest);
    
    // Normal damage on first segment
    if(!guns[d->gunselect].projspeed) raydamage(from, to, d);
    
    // CS 1.6-style wallbanging - SIMPLIFIED CONDITIONS
    // Check: is this a hitscan weapon that hit a wall?
    bool canWallbang = (d->gunselect == GUN_SMG || d->gunselect == GUN_CG || 
                        d->gunselect == GUN_PISTOL || 
                        (d->gunselect == GUN_MAGNUM && !(d->mods & MOD_LIGHTNINGGUN)));
    
    // CS 1.6-style wallbanging - shoot through walls with FULL damage
    if(canWallbang && hitWall && !guns[d->gunselect].projspeed) {
        float penetrationDepth = 500.0f; // Back to 500 for testing
        
        // Preserve headshot status from first ray
        int savedHeadshots = d->headshots;
        
        // Damage ray through wall (FULL DAMAGE, no reduction)
        vec damageStart = vec(wallHitPoint).add(vec(unitv).mul(5.0f));
        vec damageEnd = vec(damageStart).add(vec(unitv).mul(penetrationDepth));
        raydamage(damageStart, damageEnd, d);
        
        // Restore headshot status from first ray (prioritize direct hits)
        if(savedHeadshots > 0) d->headshots = savedHeadshots;
    }
    if(guns[d->gunselect].projspeed)d->headshots=0;
    if(d->lastattackgun==GUN_TELEKENESIS || d->lastattackgun==GUN_TELEKENESIS2) d->headshots=0;

    shoteffects(d->gunselect, from, to, d, true, 0, prevaction);
    if(d==player1 || d->ai)
    {
        int loopval = (d->gunselect == GUN_MAGNUM && d->mods & MOD_LIGHTNINGGUN) ? 3 : 1;
        if (d->gunselect == GUN_MAGNUM && !(d->mods & MOD_LIGHTNINGGUN)) loopval = 2;
        loopi(loopval)
            addmsg(N_SHOOT, "rici2i6iv", d->headshots, d, lastmillis-maptime, d->gunselect,
               (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
               (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
               hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        if (d->gunselect == GUN_SG && d->attacking) { // invisibly fire both primary and secondary at same time
            addmsg(N_SHOOT, "rici2i6iv", d->headshots, d, lastmillis - maptime, GUN_SHOTGUN2,
                (int)(from.x * DMF), (int)(from.y * DMF), (int)(from.z * DMF),
                (int)(to.x * DMF), (int)(to.y * DMF), (int)(to.z * DMF),
                hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
        }
    }

    //        if(d->gunselect==GUN_ELECTRO2)d->burstprogress++;


    //        if((d->gunselect==GUN_ELECTRO2) && d->burstprogress<3)
    //        {
    //            d->gunwait = 200;
    //        }
    //        else if(d->gunselect==GUN_ELECTRO2 && d->burstprogress>=3)
    //        {
    //            d->gunwait = guns[d->gunselect].attackdelay;
    //            d->burstprogress=0;
    //            d->altattacking=0;
    //        }
    //        d->magprogress[d->gunselect]+=guns[d->gunselect].sub;
    //        conoutf(CON_GAMEINFO, "mag progress = %d", d->magprogress[d->gunselect]);
    //        if(d->magprogress[d->gunselect]<=guns[d->gunselect].magsize)
    //        {
    //            d->gunwait = guns[d->gunselect].attackdelay;
    //            conoutf(CON_GAMEINFO, "mag left = %d", guns[d->gunselect].magsize-d->magprogress[d->gunselect]);
    //        }
    //        else if(d->magprogress[d->gunselect]>=guns[d->gunselect].magsize)
    //        {
    //            d->gunwait = d->gunselect==GUN_SHOTGUN2||d->gunselect==GUN_SG?200:1000;
    //            msgsound(S_CROSSBOWRELOAD, d);
    //            if(d->gunselect==GUN_SG||d->gunselect==GUN_SHOTGUN2)
    //            {
    //                d->magprogress[GUN_SG]=0;
    //                d->magprogress[GUN_SHOTGUN2]=0;
    //            }
    //            else
    //                d->magprogress[d->gunselect]=0;
    //        }
    if(d==player1 && d->gunselect==GUN_HANDGRENADE && d->ammo[GUN_HANDGRENADE]==0){ d->attacking=0; d->gunwait=300; weaponswitch(d);}
    d->gunwait=(d->gunselect==GUN_MAGNUM && d->mods & MOD_LIGHTNINGGUN) ? 1200 : guns[d->gunselect].attackdelay;
    if(d->gunselect == GUN_PISTOL && d->ai) d->gunwait += int(d->gunwait*(((101-d->skill)+rnd(111-d->skill))/100.f));
    d->totalshots += guns[d->gunselect].damage*(d->quadmillis ? 2 : 1)*(guns[d->gunselect].rays);
    //if(d->magprogress[d->gunselect]>=guns[d->gunselect].magsize && guns[d->gunselect].magsize && d->ammo[d->gunselect]) doreload(d);

}

void adddynlights()
{
    loopv(projs)
    {
        projectile &p = projs[i];
        if(p.gun!=GUN_RL) /*|| p.gun!=GUN_RL2)*/ continue;
        vec pos(p.o);
        pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
        adddynlight(pos, 20, vec(1, 0.75f, 0.5f));
    }
    loopv(bouncers)
    {
        bouncer &bnc = *bouncers[i];
        if(bnc.bouncetype!=BNC_SMGNADE || bnc.bouncetype!=BNC_ORB) continue;
        vec pos(bnc.o);
        pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
        //adddynlight(pos, 8, vec(0.25f, 1, 1));
        adddynlight(pos, 20, vec(1, 0.75f, 0.5f));
    }
    loopv(bouncers)
    {
        bouncer &bnc = *bouncers[i];
        if(bnc.bouncetype!=BNC_ELECTROBOLT) continue;
        vec pos(bnc.o);
        pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
        //adddynlight(pos, 8, vec(0.25f, 1, 1));
        adddynlight(pos, 20, vec(0, 0, 1.0f));
    }
}


static const char * const projnames[7] = { "projectiles/grenade", "projectiles/rocket", "projectils/orb", "pickups/hand_grenade", "projectiles/plasmabolt", "mrfixit", "snoutx10k" };
//static const char * const gibnames[6] = { "gibs/gib01", "gibs/gib01d", "gibs/gib02", "gibs/gib02d", "gibs/gib03", "gibs/gib03d" };
static const char * const gibnames[1] = { "mrfixit_head" };
static const char * const debrisnames[4] = { "debris/debris01", "debris/debris02", "debris/debris03", "debris/debris04" };
static const char * const barreldebrisnames[4] = { "barreldebris/debris01", "barreldebris/debris02", "barreldebris/debris03", "barreldebris/debris04" };
static const char * const weaponmodelnames[NUMGUNS] =
{ "", "items/cartridges", "items/bullets", "items/rockets", "items/rrounds" "pickups/hand_grenade", "pickups/electro", "pickups/electro", "items/bullets", "items/cartridges", "Hypercubehudguns/pyccna_m4a1", "Hypercubehudguns/pyccna_m4a1", "", "", "Hypercubehudguns/pyccna_fn57" };
// Weapon pickup models for dropped weapons (matches weapon pickup entities)
//enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_MAGNUM, GUN_HANDGRENADE, GUN_ELECTRO, GUN_ELECTRO2, GUN_CG2, GUN_SHOTGUN2, GUN_SMG, GUN_SMG2, GUN_CROSSBOW, GUN_TELEKENESIS, GUN_TELEKENESIS2, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };
static const char * const weaponpickupmodels[NUMGUNS] =
{ 
    "",                         // GUN_FIST
    "items/shotgun",           // GUN_SG
    "items/bullets",           // GUN_CG (minigun)
    "items/rockets",           // GUN_RL
    "items/rrounds",           // GUN_MAGNUM
    "pickups/hand_grenade",    // GUN_HANDGRENADE
    "pickups/electro",         // GUN_ELECTRO
    "pickups/electro",         // GUN_ELECTRO2
    "items/bullets",           // GUN_CG2
    "items/shotgun",           // GUN_SHOTGUN2
    "pickups/smg_ammo",        // GUN_SMG
    "pickups/smg_grenade",     // GUN_SMG2
    "pickups/crossbow",        // GUN_CROSSBOW
    "",                         // GUN_TELEKENESIS
    "",                         // GUN_TELEKENESIS2
    "pickups/pistol_ammo"      // GUN_PISTOL
};

void preloadbouncers()
{
    loopi(sizeof(projnames)/sizeof(projnames[0])) preloadmodel(projnames[i]);
    loopi(sizeof(gibnames)/sizeof(gibnames[0])) preloadmodel(gibnames[i]);
    loopi(sizeof(debrisnames)/sizeof(debrisnames[0])) preloadmodel(debrisnames[i]);
    loopi(sizeof(barreldebrisnames)/sizeof(barreldebrisnames[0])) preloadmodel(barreldebrisnames[i]);
    // Preload weapon pickup models for dropped weapons
    loopi(NUMGUNS) if(weaponpickupmodels[i] && weaponpickupmodels[i][0]) preloadmodel(weaponpickupmodels[i]);
}

void renderbouncers()
{
    float yaw, pitch;
    loopv(bouncers)
    {
        bouncer &bnc = *bouncers[i];
        fpsent *d = players[i];
        vec pos(bnc.o);
        pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
        vec vel(bnc.vel);
        if(vel.magnitude() <= 25.0f) {
            if (bnc.bouncetype == BNC_XBOLT) {
                pitch = bnc.lastpitch;
            }
        }
        else
        {
            vectoyawpitch(vel, yaw, pitch);
            yaw += 90;
            bnc.lastyaw = yaw;
            if(bnc.bouncetype==BNC_XBOLT)bnc.lastpitch = pitch;
        }
        // Shell casings: tumble when moving, land flat when stopped
        if(bnc.bouncetype==BNC_SHELL) {
            if(vel.magnitude() > 10.0f) {
                pitch = -bnc.roll / 15; // Tumbling when moving
            } else {
                pitch = 0; // Land flat when velocity is low
            }
        }
        else if(bnc.bouncetype!=BNC_XBOLT)pitch =-bnc.roll/15;
        
        if(bnc.bouncetype==BNC_PROP)pos.z-=1;
        //if(bnc.vel.magnitude()<(bnc.bouncetype==BNC_BARRELDEBRIS)?50.0f:200.0f)pitch=bnc.bouncetype==BNC_BARRELDEBRIS?90:0;
        if(bnc.vel.magnitude()<50&&bnc.bouncetype==BNC_BARRELDEBRIS)pitch=90;
        if(bnc.bouncetype==BNC_PROP) pitch=0;
        
        // Handle pending explosion objects (frozen and waiting to explode)
        if(phys_lag_comp && bnc.pendingExplosion && (bnc.bouncetype == BNC_GRENADE || bnc.bouncetype == BNC_ORB || bnc.bouncetype == BNC_PROP)) {
            // Use frozen position for rendering
            pos = bnc.frozenPos;
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            
            // Calculate pulsing intensity based on time until explosion
            int timeUntilExplosion = max(0, bnc.explosionScheduledTime - lastmillis);
            float pulseFreq = 5.0f; // Pulses per second
            float pulse = 0.5f + 0.5f * sinf(lastmillis * pulseFreq * 2.0f * M_PI / 1000.0f);
            
            // Color based on type: red for grenades, blue for orbs, orange for props
            int glowColor = bnc.bouncetype == BNC_GRENADE ? 0xFF3030 : 
                           bnc.bouncetype == BNC_ORB ? 0x3030FF : 0xFF8030;
            
            // Add pulsing glow effect at frozen position
            float glowSize = 8.0f + pulse * 4.0f;
            particle_flare(pos, pos, 100, PART_GLOW, glowColor, glowSize);
            
            // Add spark particles for extra warning
            if(lastmillis % 100 < 50) { // Intermittent sparks
                particle_splash(PART_SPARK, 3, 50, pos, glowColor, 0.5f, 100);
            }
            
            // Skip rendering the actual model (object is "invisible" during delay)
            // The glow effect shows where it will explode
            continue;
        }
        
        if(bnc.bouncetype==BNC_XBOLT)
            rendermodel(&bnc.light, "projectiles/xbolt", ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        if(bnc.bouncetype==BNC_SHELL) {//not when too close otherwise it'll be in the gun
            pos.z-=.5f;
            // Add 90 degree yaw offset to compensate for model misalignment
            float shellYaw = yaw + 90.0f;
            if(pos.dist(bnc.owner->o)>6)rendermodel(&bnc.light, "shells/shell1", ANIM_MAPMODEL|ANIM_LOOP, pos, shellYaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        }
        if(bnc.bouncetype==BNC_SMGNADE)
            rendermodel(&bnc.light, "projectiles/rocket", ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        if(bnc.bouncetype==BNC_GRENADE)
            rendermodel(&bnc.light, "pickups/hand_grenade", ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        if(bnc.bouncetype==BNC_ELECTROBOLT)
            rendermodel(&bnc.light, "projectiles/orb", ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        if(bnc.bouncetype==BNC_ORB)
            rendermodel(&bnc.light, "projectiles/teslaball", ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        if(bnc.bouncetype==BNC_WEAPON) {
            pos.z+=2.f;  // Add 2 units to z position so weapon isn't underground
            // Get the correct pickup model for this weapon type
            const char *pickupmodel = (bnc.gun >= 0 && bnc.gun < NUMGUNS) ? weaponpickupmodels[bnc.gun] : "";
            if(pickupmodel && pickupmodel[0])  // Only render if valid model exists
                rendermodel(&bnc.light, pickupmodel, ANIM_MAPMODEL|ANIM_LOOP, pos, 0, 0, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        }
        if(bnc.bouncetype==BNC_PROP || bnc.bouncetype==BNC_BARREL) {

            //const char *mdlname=bnc.bouncetype==BNC_PROP?"xeno/box1":"dcp/barrel";//
            const char *mdlname=mapmodelname(bnc.model);
            //conoutf(CON_GAMEINFO, "%s", bnc.model);
            if(!mdlname) continue;
            rendermodel(NULL, mdlname, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        }

        else
        {
            const char *mdl = NULL;
            int cull = MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED;
            float fade = 1;
            if(bnc.lifetime < 250) fade = bnc.lifetime/250.0f;
            switch(bnc.bouncetype)
            {
            case BNC_GIBS: mdl = gibnames[0]; cull |= MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW; break;
            case BNC_DEBRIS: mdl = debrisnames[bnc.variant]; break;
            case BNC_BARRELDEBRIS: mdl = barreldebrisnames[bnc.variant]; break;
            default: continue;
            }
            if (bnc.bouncetype == BNC_GIBS) pos.z -= 12.5f;
            rendermodel(&bnc.light, mdl, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, cull, NULL, NULL, 0, 0, fade);
        }
    }
}

// Helper function to record position in history buffer
void recordholdposition(fpsent *d, const vec &pos)
{
    d->holdposhistory[d->holdposhistoryindex].pos = pos;
    d->holdposhistory[d->holdposhistoryindex].timestamp = lastmillis;
    d->holdposhistoryindex = (d->holdposhistoryindex + 1) % fpsstate::HOLDPOS_HISTORY_SIZE;
}

// Helper function to get lagged position from history
vec getlaggedholdposition(fpsent *d, int lagms, const vec &currentpos)
{
    int targettime = lastmillis - lagms;
    
    // Find the oldest and newest entries to check how much history we have
    int oldestidx = -1;
    int newestidx = -1;
    int oldesttime = 2000000000;
    int newesttime = 0;
    
    loopi(fpsstate::HOLDPOS_HISTORY_SIZE)
    {
        if(d->holdposhistory[i].timestamp == 0) continue;
        
        if(d->holdposhistory[i].timestamp < oldesttime)
        {
            oldesttime = d->holdposhistory[i].timestamp;
            oldestidx = i;
        }
        if(d->holdposhistory[i].timestamp > newesttime)
        {
            newesttime = d->holdposhistory[i].timestamp;
            newestidx = i;
        }
    }
    
    // If we have no history, return current position
    if(oldestidx == -1)
        return currentpos;
    
    // Calculate how much history we actually have
    int historyspan = newesttime - oldesttime;
    
    // If we don't have enough history yet, reduce the lag proportionally
    int effectivelag = lagms;
    if(historyspan < lagms)
    {
        effectivelag = historyspan;
        if(effectivelag < 16) // Less than one frame of history
            return currentpos;
    }
    
    targettime = lastmillis - effectivelag;
    
    // Find the two history entries that bracket the target time
    int bestidx = -1;
    int nextidx = -1;
    int besttimediff = 1000000;
    
    loopi(fpsstate::HOLDPOS_HISTORY_SIZE)
    {
        if(d->holdposhistory[i].timestamp == 0) continue;
        
        int timediff = d->holdposhistory[i].timestamp - targettime;
        
        // Find the closest entry before or at target time
        if(timediff <= 0 && -timediff < besttimediff)
        {
            besttimediff = -timediff;
            bestidx = i;
        }
    }
    
    // If we couldn't find a good match, use the oldest entry we have
    if(bestidx == -1)
        bestidx = oldestidx;
    
    // Find next entry after bestidx for interpolation
    loopi(fpsstate::HOLDPOS_HISTORY_SIZE)
    {
        if(d->holdposhistory[i].timestamp > d->holdposhistory[bestidx].timestamp)
        {
            if(nextidx == -1 || d->holdposhistory[i].timestamp < d->holdposhistory[nextidx].timestamp)
                nextidx = i;
        }
    }
    
    // If we have both points, interpolate
    if(nextidx != -1 && d->holdposhistory[nextidx].timestamp > d->holdposhistory[bestidx].timestamp)
    {
        float t = float(targettime - d->holdposhistory[bestidx].timestamp) / 
                  float(d->holdposhistory[nextidx].timestamp - d->holdposhistory[bestidx].timestamp);
        t = clamp(t, 0.0f, 1.0f);
        
        vec result = d->holdposhistory[bestidx].pos;
        vec diff = d->holdposhistory[nextidx].pos;
        diff.sub(d->holdposhistory[bestidx].pos);
        diff.mul(t);
        result.add(diff);
        return result;
    }
    
    // Otherwise just return the best match we have
    return d->holdposhistory[bestidx].pos;
}

void rendercaughtitems()
{
    loopv(players)
    {
        fpsent *d = players[i];
        if(d->state!=CS_ALIVE)return;
        vec v;
        /*if(d==player1)*///d->aimpos=worldpos;
        d->aimpos.x=d->aimpos.y=d->aimpos.z=0.f;
        vecfromyawpitch(d->yaw, d->pitch, 1, 0, d->aimpos);
        float barrier=raycube(d->o, d->aimpos, 2048, RAY_CLIPMAT|RAY_ALPHAPOLY);
        d->aimpos.mul(barrier).add(d->o);
        float w = d->aimpos.dist(d==hudplayer()&&!thirdperson?d->muzzle:d->o, v);
        int steps = clamp(int(w*2), 1, 2048);
        v.div(steps);
        vec p = d==hudplayer()&&!thirdperson?d->muzzle:d->o;
        loopi(30)p.add(v);
        
        // Only record and use lag if actually holding something
        bool isholding = d->isholdingnade || d->isholdingorb || d->isholdingprop || d->isholdingbarrel || d->isholdingshock;
        
        vec currentholdpos = p;
        if(thirdperson||d!=hudplayer())currentholdpos.z-=5;
        
        vec renderpos = currentholdpos;  // Default to current position
        
        if(isholding && d->gunselect==GUN_TELEKENESIS2)
        {
            // Record current hold position in history
            recordholdposition(d, currentholdpos);
            
            // Get lagged position (300ms ago) for rendering to simulate heavy object feel
            renderpos = getlaggedholdposition(d, 300, currentholdpos);
        }
        
        //entity &e = d->holdingobj;
        //const char *mdlname=mapmodelname(e.attr2);
        //char *mdlname= { d->propmodeldir };
        //const char *mdlname = d->isholdingprop?"xeno/box1":"dcp/barrel";
        const char *mdlname = mapmodelname(d->propmodeldir);
        //particle_flare(d->muzzle, p, 2, PART_LIGHTNING, 0xFF64FF, 1.f);
        if(d->isholdingnade && d->gunselect==GUN_TELEKENESIS2) {
            rendermodel(NULL, "pickups/hand_grenade", ANIM_MAPMODEL|ANIM_LOOP, renderpos, d->yaw, 0, MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
            d->flareleft-=1;
            d->beepleft-=1;
            if(!d->beepleft) { playsound(S_NADEBEEP, &renderpos); d->beepleft=50;} //beep delay longer because this is looped faster
            if(renderpos.dist(d->flarepos)>1 && d->state==CS_ALIVE) {particle_flare(d->flarepos, renderpos, 500, PART_RAILTRAIL, nadetrailcol, .2f); d->flarepos=renderpos; d->flareleft=2;}
            if(d->state==CS_ALIVE)particle_flare(d == hudplayer() ? d->muzzle : d->o, renderpos, 1, PART_LIGHTNING, 0xFF64FF, .5f);
        }
        if(d->isholdingorb && d->gunselect==GUN_TELEKENESIS2) {
            rendermodel(NULL, "projectiles/teslaball", ANIM_MAPMODEL|ANIM_LOOP, renderpos, d->yaw, 0, MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
            vec occlusioncheck;
            if(raycubelos(renderpos, camera1->o, occlusioncheck))
            {
                particle_flare(renderpos, renderpos, 75, PART_GLOW, 0x553322, 4.0f);
            }
            if(d->state==CS_ALIVE)particle_flare(d == hudplayer() ? d->muzzle : d->o, renderpos, 1, PART_LIGHTNING, 0xFF64FF, .5f);
        }

        if (d->isholdingshock && d->gunselect==GUN_TELEKENESIS2) {
            if (d->state == CS_ALIVE)particle_flare(d == hudplayer() ? d->muzzle : d->o, renderpos, 1, PART_LIGHTNING, 0xFF64FF, .5f);
            vec occlusioncheck;
            if (raycubelos(renderpos, camera1->o, occlusioncheck))
            {
                regular_particle_splash(PART_SMOKE, 2, 300, renderpos, 0x404040, 0.6f, 150, -20);
                int color = 0xFFFFFF;
                particle_splash(PART_FIREBALL2, 1, 1, renderpos, color, 4.8f, 150, 20);
            }
        }

        if(d->isholdingprop || d->isholdingbarrel) {
            if(!mdlname) continue;
            if(d->state==CS_ALIVE)particle_flare(d == hudplayer() ? d->muzzle : d->o, renderpos, 1, PART_LIGHTNING, 0xFF64FF, .5f);
            renderpos.z-=3;
            if(d->state==CS_ALIVE)rendermodel(NULL, mdlname, ANIM_MAPMODEL|ANIM_LOOP, renderpos, d->yaw, 0, MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
            if(d->isholdingbarrel==2)
            {
                renderpos.z+=10;
                regular_particle_flame(PART_FLAME, renderpos, 1, 1, 0x903020, 3, 2.0f);
                regular_particle_flame(PART_SMOKE, renderpos, 1, 1, 0x303020, 1, 4.0f, 100.0f, 2000.0f, -20);
            }
        }

        //TODO: Add rendering for shock here
    }
}

//    void rendermovables() //for mapmodel props
//    {
//        loopv(movables)
//        {
//            movable &m = *movables[i];
//            if(m.state!=CS_ALIVE) continue;
//            vec o = m.feetpos();
//            const char *mdlname = mapmodelname(m.mapmodel);
//            if(!mdlname) continue;
//			rendermodel(NULL, mdlname, ANIM_MAPMODEL|ANIM_LOOP, o, m.yaw, 0, MDL_LIGHT | MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, &m);
//        }
//    }

void renderprojectiles()
{
    float yaw, pitch;
    loopv(projs)
    {
        projectile &p = projs[i];
        if(p.gun!=GUN_RL && p.gun!=GUN_ELECTRO2) continue;
        vec pos(p.o);
        pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
        if(p.to==pos) continue;
        vec v(p.to);
        v.sub(pos);
        v.normalize();
        // the amount of distance in front of the smoke trail needs to change if the model does
        vectoyawpitch(v, yaw, pitch);
        yaw += 90;
        v.mul(3);
        v.add(pos);
        if(p.owner->o.dist(v)<=100 && p.gun==GUN_RL)rendermodel(&p.light, "projectiles/rocket", ANIM_MAPMODEL|ANIM_LOOP, v, yaw, pitch, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT);
    }
}

void checkattacksound(fpsent *d, bool local)
{
    int gun = -1;
    switch(d->attacksound)
    {
    case S_CHAINSAW_ATTACK:
        if(chainsawhudgun) gun = GUN_FIST;
        break;
    default:
        return;
    }
    if(gun >= 0 && gun < NUMGUNS &&
            d->clientnum >= 0 && d->state == CS_ALIVE &&
            d->lastattackgun == gun && lastmillis - d->lastaction < guns[gun].attackdelay + 50)
    {
        d->attackchan = playsound(d->attacksound, local ? NULL : &d->o, NULL, -1, -1, d->attackchan);
        if(d->attackchan < 0) d->attacksound = -1;
    }
    else d->stopattacksound();
}

void checkidlesound(fpsent *d, bool local)
{
    int sound = -1, radius = 0;
    if(d->clientnum >= 0 && d->state == CS_ALIVE) switch(d->gunselect)
    {
    //            case GUN_FIST:
    //                if(chainsawhudgun && d->attacksound < 0)
    //                {
    //                    sound = S_CHAINSAW_IDLE;
    //                    radius = 50;
    //                }
    //                break;
    }
    if(d->idlesound != sound)
    {
        if(d->idlesound >= 0) d->stopidlesound();
        if(sound >= 0)
        {
            d->idlechan = playsound(sound, local ? NULL : &d->o, NULL, -1, 100, d->idlechan, radius);
            if(d->idlechan >= 0) d->idlesound = sound;
        }
    }
    else if(sound >= 0)
    {
        d->idlechan = playsound(sound, local ? NULL : &d->o, NULL, -1, -1, d->idlechan, radius);
        if(d->idlechan < 0) d->idlesound = -1;
    }
}

void removeweapons(fpsent *d)
{
    removebouncers(d);
    removeprojectiles(d);
}

void updateweapons(int curtime)
{
    updateprojectiles(curtime);
    if(player1->doshake)updatescreenshake();
    //if(player1->health<=0&&lastmillis-player1->lastpain<2000) { setvar("hudgun", 0); }
    //else setvar("hudgun", 1);
    if(player1->clientnum>=0 && player1->state==CS_ALIVE) shoot(player1, worldpos); // only shoot when connected to server
    //if(player1->gunselect==GUN_ELECTRO2 && player1->burstprogress<3 && player1->burstprogress>0 && player1->ammo[player1->gunselect]>0)player1->altattacking=1;
    //if(player1->gunselect==GUN_ELECTRO2 && player1->burstprogress && player1->ammo[player1->gunselect]==0)player1->altattacking=0;
    updatebouncers(curtime); // need to do this after the player shoots so grenades don't end up inside player's BB next frame
    fpsent *following = followingplayer();
    if(!following) following = player1;
    loopv(players)
    {
        fpsent *d = players[i];
        checkattacksound(d, d==following);
        checkidlesound(d, d==following);
        if(d==player1 && tryreload && lastmillis-d->lastreload>500)doreload(player1);
        if(d->isholdingnade || d->isholdingorb || d->isholdingprop || d->isholdingbarrel)
        {
            d->altattacking=0;
            int newtimer = d->caughttime+(d->isholdingnade?1500:5000);
            int finaltimer = newtimer-lastmillis;
            if(d->isholdingprop)finaltimer=20000;
            if((finaltimer<0 || d->state==CS_DEAD) && !d->isholdingbarrel) d->attacking=1; //force player firing bouncer (which will blow up in their face) if timer has run out
            vec raycheck;
            if(raycubelos(camera1->o, d->muzzle, raycheck) && d==player1)particle_flare(d->muzzle, d->muzzle, 2, PART_GLOW, 0xFF64FF, 2.5f);
        }
        vec raycheck;
        if(d->lastattackgun==GUN_TELEKENESIS2 && lastmillis-d->lastaction<200  && raycubelos(camera1->o, d->muzzle, raycheck) && d==player1)particle_flare(d->muzzle, d->muzzle, 2, PART_GLOW, 0xFF64FF, 2.5f);
        if(lastmillis-d->lastflash>5)return;
        //track muzzle flash particles for weapon that don't have a muzzle tag
        vec v;
        d->aimpos.x=d->aimpos.y=d->aimpos.z=0.f;
        vecfromyawpitch(d->yaw, d->pitch, 1, 0, d->aimpos);
        float barrier=raycube(hudgunorigin(d->gunselect, d->o, worldpos, d), d->aimpos, 2048, RAY_CLIPMAT|RAY_ALPHAPOLY);
        d->aimpos.mul(barrier).add(hudgunorigin(d->gunselect, d->o, worldpos, d));
        float w = d->aimpos.dist(hudgunorigin(d->gunselect, d->o, worldpos, d), v);
        int steps = clamp(int(w*2), 1, 2048);
        v.div(steps);
        vec p = hudgunorigin(d->gunselect, d->o, worldpos, d);
        loopi(d->gunselect==GUN_CG?7:10)p.add(v);
        if(d->gunselect==GUN_MAGNUM || d->gunselect==GUN_CG)particle_flare(p, p, 1, PART_MUZZLE_FLASH4, 0xFFFFFF, 2.f, d);
        else particle_flare(p, p, 1, PART_MUZZLE_FLASH1, 0xFFFFFF, 1.875f, d);
    }
}

void avoidweapons(ai::avoidset &obstacles, float radius)
{
    loopv(projs)
    {
        projectile &p = projs[i];
        obstacles.avoidnear(NULL, p.o.z + guns[p.gun].splash + 1, p.o, radius + guns[p.gun].splash);
    }
    loopv(bouncers)
    {
        bouncer &bnc = *bouncers[i];
        if(bnc.bouncetype != BNC_GRENADE && bnc.bouncetype!=BNC_MISSILE && bnc.bouncetype!=BNC_PROP) continue;
        obstacles.avoidnear(NULL, bnc.o.z + guns[GUN_HANDGRENADE].splash + 1, bnc.o, radius + guns[GUN_HANDGRENADE].splash);
    }
    //        loopv(movables)
    //        {
    //            movable &m = *movables[i];
    //            obstacles.avoidnear(NULL, m.o.z + guns[GUN_HANDGRENADE].splash + 1, m.o, radius + guns[GUN_HANDGRENADE].splash);
    //        }

}
};

