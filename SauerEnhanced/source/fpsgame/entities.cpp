#include "game.h"
#include "../engine/engine.h"

namespace entities
{
    using namespace game;

    vector<extentity *> ents;

    vector<extentity *> &getents() { return ents; }

    bool mayattach(extentity &e) { return false; }
    bool attachent(extentity &e, extentity &a) { return false; }

    const char *itemname(int i)
    {
        int t = ents[i]->type;
        if(t<I_ELECTRO || t>I_QUAD) return NULL;
        return itemstats[t-I_ELECTRO].name;
    }

    int itemicon(int i)
    {
        int t = ents[i]->type;
        if(t<I_ELECTRO || t>I_QUAD) return -1;
        return itemstats[t-I_ELECTRO].icon;
    }

    const char *entmdlname(int type)
    {
        static const char * const entmdlnames[] =
        {
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            "items/rroundsrifle", "items/bullets", "items/rockets", "items/rrounds", "pickups/crossbow", "pickups/hand_grenade", "pickups/pistol_ammo", "pickups/smg_ammo", //pickups/railgun
            "pickups/smg_grenade", "pickups/shotgun_ammo", "pickups/electro_ammo", "pickups/electroball", "pickups/minigun_ammo", "pickups/orb",
            "pickups/crossbow_ammo", "pickups/rpg_ammo", "pickups/sniper_ammo",  "items/shotgun", //"items/cartridges",
            "pickups/bighealth", "boost", "pickups/health", "items/battery2", "switch2", "supercharger", "quad", "teleporter",
            NULL, NULL,
            "carrot",
            NULL, NULL,
            "checkpoint",
            NULL, NULL,
            NULL, NULL,
            NULL, NULL
        };
        return entmdlnames[type];
    }

    const char *entmodel(const entity &e)
    {
        if(e.type == TELEPORT)
        {
            if(e.attr2 > 0) return mapmodelname(e.attr2);
            if(e.attr2 < 0) return NULL;
        }
        return e.type < MAXENTTYPES ? entmdlname(e.type) : NULL;
    }

    void preloadentities()
    {
        loopi(MAXENTTYPES)
        {
            switch(i)
            {
            case I_ELECTRO: case I_MINIGUN: case I_RPG: case I_MAGNUM: case I_CROSSBOW: case I_SHOTGUN:
        case I_GRENADE: case I_PISTOLAMMO: case I_SMGAMMO: case I_SMGNADE: case I_SHELLS: case I_ELECTROAMMO:
        case I_ELECTROORBS: case I_MINIGUNAMMO: case I_TESLAORB: case I_XBOWAMMO: case I_RPGAMMO: case I_SNIPERAMMO:
                    if(m_noammo) continue;
                    break;
            case I_HEALTH: case I_BOOST: case I_GREENARMOUR: case I_YELLOWARMOUR: case I_QUAD: case I_SHIELDBATTERY: case I_SMALLHEALTH:
                    if(m_noitems) continue;
                    break;
                case CARROT: case RESPAWNPOINT:
                    if(!m_classicsp) continue;
                    break;
            }
            const char *mdl = entmdlname(i);
            if(!mdl) continue;
            preloadmodel(mdl);
        }
    }

    void renderentities()
    {
        loopv(ents)
        {
            extentity &e = *ents[i];
            int revs = 10;
            switch(e.type)
            {
                case CARROT:
                case RESPAWNPOINT:
                    if(e.attr2) revs = 1;
                    break;
                case TELEPORT:
                    if(e.attr2 < 0) continue;
                    break;
                case I_GREENARMOUR:
                case I_YELLOWARMOUR:
                    break;
                default:
                    if(!e.spawned /*|| e.type < I_ELECTRO || e.type > I_QUAD*/) continue;
            }
            const char *mdlname = entmodel(e);
            if(mdlname)
            {
                vec p = e.o;
                if((e.type>=I_ELECTRO && e.type<=I_SHOTGUN) || e.type==I_SHIELDBATTERY || e.type==I_SMALLHEALTH || e.type==I_HEALTH || e.type==I_GREENARMOUR || e.type==I_YELLOWARMOUR){revs=0; rendermodel(&e.light, mdlname, ANIM_MAPMODEL|ANIM_LOOP, p, e.attr1, e.attr2, MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);}
                if(e.type==CARROT || e.type==RESPAWNPOINT || e.type==TELEPORT || e.type==I_QUAD || e.type==I_BOOST)revs=10;
                if(revs>0)p.z += 1+sinf(lastmillis/100.0+e.o.x+e.o.y)/20;
                if((e.type < I_ELECTRO || e.type > I_SHOTGUN) && e.type!=I_SHIELDBATTERY && e.type!=I_SMALLHEALTH && e.type!=I_HEALTH && e.type!=I_GREENARMOUR && e.type!=I_YELLOWARMOUR)rendermodel(&e.light, mdlname, ANIM_MAPMODEL|ANIM_LOOP, p, revs>0?lastmillis/(float)revs:0, 0, MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
            }
        }
    }

    void addammo(int type, int &v, bool local)
    {
        itemstat &is = itemstats[type-I_ELECTRO];
        v += is.add;
        if(v>is.max) v = is.max;
        if(local) msgsound(is.sound);
    }

    void repammo(fpsent *d, int type, bool local)
    {
        addammo(type, d->ammo[type-I_ELECTRO+GUN_SG], local);
    }

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void pickupeffects(int n, fpsent *d)
    {
        if(!ents.inrange(n)) return;
        int type = ents[n]->type;
        if(type<I_ELECTRO || type>I_QUAD) return;
        ents[n]->spawned = false;
        if(!d) return;
        //itemstat &is = itemstats[type-I_ELECTRO];
        if(d!=player1 || isthirdperson())
        {
            //particle_text(d->abovehead(), is.name, PART_TEXT, 2000, 0xFFC864, 4.0f, -8);
            //particle_icon(d->abovehead(), is.icon%4, is.icon/4, PART_HUD_ICON_GREY, 2000, 0xFFFFFF, 2.0f, -8);
        }
        playsound(itemstats[type-I_ELECTRO].sound, d!=player1 ? &d->o : NULL, NULL, 0, 0, -1, 0, 1500);
        //if(type>=I_ELECTRO && type<=I_GRENADE && d==player1) game::stats[15]++;
        //else if(type==I_HEALTH && d==player1) game::stats[16]++;
        d->pickup(type);
        if(d==player1)
        {
            crosshairbump();
            switch(type)
            {

            case I_BOOST:
               // conoutf(CON_TEAMCHAT, "Charging Health...");
                conoutf(CON_TEAMCHAT, "Max Health Boosted to %d", d->maxhealth);
                playsound(S_V_BOOST);
                break;

            case I_QUAD:
                conoutf(CON_TEAMCHAT, "Damage Amplifier");
                playsound(S_V_QUAD);
                break;
            case I_ELECTRO:
                conoutf(CON_TEAMCHAT, "Shock Rifle");
                break;
            case I_GRENADE:
                conoutf(CON_TEAMCHAT, "+1 Hand Grenade");
                break;
            case I_PISTOLAMMO:
                conoutf(CON_TEAMCHAT, "+24 Blaster Ammo");
                break;
            case I_SMGAMMO:
                conoutf(CON_TEAMCHAT, "+45 Assault Rifle Ammo");
                break;
            case I_SHELLS:
                conoutf(CON_TEAMCHAT, "+12 Shotgun Shells");
                break;
            case I_ELECTROAMMO:
                conoutf(CON_TEAMCHAT, "+10 Rifle Rounds");
                break;
            case I_ELECTROORBS:
                conoutf(CON_TEAMCHAT, "+40 Plasma Cells");
                break;
            case I_MINIGUNAMMO:
                conoutf(CON_TEAMCHAT, "+30 Pulse Ammo");
                break;
            case I_TESLAORB:
                conoutf(CON_TEAMCHAT, "+1 Photon Orb");
                break;
            case I_XBOWAMMO:
                conoutf(CON_TEAMCHAT, "+5 Crossbow Bolts");
                break;
            case I_RPGAMMO:
                conoutf(CON_TEAMCHAT, "+5 RPG Missiles");
                break;
            case I_SMALLHEALTH:
                conoutf(CON_TEAMCHAT, "+15 Health");
                break;
            case I_SHIELDBATTERY:
                conoutf(CON_TEAMCHAT, "+15 Shield Battery");
                break;
            case I_SNIPERAMMO:
                conoutf(CON_TEAMCHAT, "+10 Sniper Ammo");
                break;
            case I_MINIGUN:
                conoutf(CON_TEAMCHAT, "Pulse Rifle");
                break;
            case I_RPG:
                conoutf(CON_TEAMCHAT, "RPG");
                break;
            case I_MAGNUM:
                conoutf(CON_TEAMCHAT, "Sniper Rifle");
                break;
            case I_CROSSBOW:
                conoutf(CON_TEAMCHAT, "Crossbow");
                break;
            case I_SHOTGUN:
                conoutf(CON_TEAMCHAT, "Shotgun");
                break;
            case I_GREENARMOUR:
                conoutf(CON_TEAMCHAT, "Charging Shield...");
                break;
            case I_YELLOWARMOUR:
                conoutf(CON_TEAMCHAT, "Supercharging...");
                break;
            case I_HEALTH:
                conoutf(CON_TEAMCHAT, "+25 Health");
                break;
            case I_SMGNADE:
                conoutf(CON_TEAMCHAT, "+1 SMG Grenade");
                break;
            }
        }
    }

    // these functions are called when the client touches the item

    void teleporteffects(fpsent *d, int tp, int td, bool local)
    {
        if(d == player1)
        {
            playsound(S_TELEPORT);
            adddynlight(d->o, 1.15f*20, vec(0.5f, 0.5f, 2), 900, 100);
            vec p = d->o;
            particle_explodesplash(vec(p.x, p.y, p.z-2), 200, PART_TELE, 0xFFFFFF, 0.75f, 500, 128);
            particle_explodesplash(vec(p.x, p.y, p.z-10), 200, PART_TELE, 0xFFFFFF, 0.75f, 500, 128);
        }
        else
        {
            if(ents.inrange(tp)) playsound(S_TELEPORT, &ents[tp]->o);
            if(ents.inrange(td)) playsound(S_TELEPORT, &ents[td]->o);
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(32, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_TELEPORT);
            putint(p, d->clientnum);
            putint(p, tp);
            putint(p, td);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void jumppadeffects(fpsent *d, int jp, bool local)
    {
        if(d == player1) playsound(S_JUMPPAD);
        else if(ents.inrange(jp)) playsound(S_JUMPPAD, &ents[jp]->o);
        particle_splash(PART_STEAM, 10, 800, ents[jp]->o, 0xFFFFFF, 3.24f, 20, -5); //here
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(16, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_JUMPPAD);
            putint(p, d->clientnum);
            putint(p, jp);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void teleport(int n, fpsent *d)     // also used by monsters
    {
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(TELEDEST, e+1);
            if(e==beenhere || e<0) { conoutf(CON_WARN, "no teleport destination for tag %d", tag); return; }
            if(beenhere<0) beenhere = e;
            if(ents[e]->attr2==tag)
            {
                teleporteffects(d, n, e, true);
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                if(ents[e]->attr3 > 0)
                {
                    vec dir;
                    vecfromyawpitch(d->yaw, 0, 1, 0, dir);
                    float speed = d->vel.magnitude2();
                    d->vel.x = dir.x*speed;
                    d->vel.y = dir.y*speed;
                }
                else d->vel = vec(0, 0, 0);
                entinmap(d);
                updatedynentcache(d);
                ai::inferwaypoints(d, ents[n]->o, ents[e]->o, 16.f);
                break;
            }
        }
    }

    void trypickup(int n, fpsent *d)
    {
        switch(ents[n]->type)
        {
            default:
                if(d->canpickup(ents[n]->type))
                {
                    addmsg(N_ITEMPICKUP, "rci", d, n);
                    ents[n]->spawned = false; // even if someone else gets it first
                }
                break;

            case TELEPORT:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<500) break;
                if(ents[n]->attr3 > 0)
                {
                    defformatstring(hookname)("can_teleport_%d", ents[n]->attr3);
                    if(identexists(hookname) && !execute(hookname)) break;
                }
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                teleport(n, d);
                break;
            }

            case RESPAWNPOINT:
                if(d!=player1) break;
                if(n==respawnent) break;
                respawnent = n;
                conoutf(CON_GAMEINFO, "\f2respawn point set!");
                playsound(S_V_RESPAWNPOINT);
                break;

            case JUMPPAD:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<300) break;
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                jumppadeffects(d, n, true);
                vec v((int)(char)ents[n]->attr3*10.0f, (int)(char)ents[n]->attr2*10.0f, ents[n]->attr1*10.7f);
                if(d->ai) d->ai->becareful = true;
				d->falling = vec(0, 0, 0);
				d->physstate = PHYS_FALL;
                d->timeinair = 1;
                float lastx=d->vel.x;
                float lasty=d->vel.y;
                d->vel = v;
                if(v.x==0&&v.y==0) { d->vel.x=lastx; d->vel.y=lasty; }
                break;
            }
        }
    }

    void checkitems(fpsent *d)
    {
        if(d->state!=CS_ALIVE) return;
        vec o = d->feetpos();
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==NOTUSED) continue;
            if(!e.spawned && e.type!=TELEPORT && e.type!=JUMPPAD && e.type!=RESPAWNPOINT) continue;
            float dist = e.o.dist(o);
            if(dist<((e.type==TELEPORT || e.type==I_GREENARMOUR || e.type==I_YELLOWARMOUR || e.type==I_BOOST) ? 16 : 12)) trypickup(i, d);
        }
    }

    void checkquad(int time, fpsent *d)
    {
        if(d->quadmillis && (d->quadmillis -= time)<=0)
        {
            d->quadmillis = 0;
            playsound(S_PUPOUT, d==player1 ? NULL : &d->o);
            if(d==player1) conoutf(CON_TEAMCHAT, "\f2Damage Amplifier expired");
        }
    }

    void putitems(packetbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, N_ITEMLIST);
        loopv(ents) if(ents[i]->type>=I_ELECTRO && ents[i]->type<=I_QUAD && (!m_noammo || ents[i]->type<I_ELECTRO || ents[i]->type>I_GRENADE))
        {
            putint(p, i);
            putint(p, ents[i]->type);
        }
        putint(p, -1);
    }

    void resetspawns() { loopv(ents) ents[i]->spawned = false; }

    void spawnitems(bool force)
    {
        if(m_noitems) return;
        loopv(ents) if(ents[i]->type>=I_ELECTRO && ents[i]->type<=I_QUAD && (!m_noammo || ents[i]->type<I_ELECTRO || ents[i]->type>I_GRENADE))
        {
            ents[i]->spawned = force || m_sp || !server::delayspawn(ents[i]->type);
        }
    }

    void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->spawned = on; }

    extentity *newentity() { return new fpsentity(); }
    void deleteentity(extentity *e) { delete (fpsentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }

    enum
    {
        TRIG_COLLIDE    = 1<<0,
        TRIG_TOGGLE     = 1<<1,
        TRIG_ONCE       = 0<<2,
        TRIG_MANY       = 1<<2,
        TRIG_DISAPPEAR  = 1<<3,
        TRIG_AUTO_RESET = 1<<4,
        TRIG_RUMBLE     = 1<<5,
        TRIG_LOCKED     = 1<<6,
        TRIG_ENDSP      = 1<<7
    };

    static const int NUMTRIGGERTYPES = 32;

    static const int triggertypes[NUMTRIGGERTYPES] =
    {
        0,
        TRIG_ONCE,                    // 1
        TRIG_RUMBLE,                  // 2
        TRIG_TOGGLE,                  // 3
        TRIG_TOGGLE | TRIG_RUMBLE,    // 4
        TRIG_MANY,                    // 5
        TRIG_MANY | TRIG_RUMBLE,      // 6
        TRIG_MANY | TRIG_TOGGLE,      // 7
        TRIG_MANY | TRIG_TOGGLE | TRIG_RUMBLE,    // 8
        TRIG_COLLIDE | TRIG_TOGGLE | TRIG_RUMBLE, // 9
        TRIG_COLLIDE | TRIG_TOGGLE | TRIG_AUTO_RESET | TRIG_RUMBLE, // 10
        TRIG_COLLIDE | TRIG_TOGGLE | TRIG_LOCKED | TRIG_RUMBLE,     // 11
        TRIG_DISAPPEAR,               // 12
        TRIG_DISAPPEAR | TRIG_RUMBLE, // 13
        TRIG_DISAPPEAR | TRIG_COLLIDE | TRIG_LOCKED, // 14
        0 /* reserved 15 */,
        0 /* reserved 16 */,
        0 /* reserved 17 */,
        0 /* reserved 18 */,
        0 /* reserved 19 */,
        0 /* reserved 20 */,
        0 /* reserved 21 */,
        0 /* reserved 22 */,
        0 /* reserved 23 */,
        0 /* reserved 24 */,
        0 /* reserved 25 */,
        0 /* reserved 26 */,
        0 /* reserved 27 */,
        0 /* reserved 28 */,
        TRIG_DISAPPEAR | TRIG_RUMBLE | TRIG_ENDSP, // 29
        0 /* reserved 30 */,
        0 /* reserved 31 */,
    };

    #define validtrigger(type) (triggertypes[(type) & (NUMTRIGGERTYPES-1)]!=0)
    #define checktriggertype(type, flag) (triggertypes[(type) & (NUMTRIGGERTYPES-1)] & (flag))

    static inline void setuptriggerflags(fpsentity &e)
    {
        e.flags = extentity::F_ANIM;
        if(checktriggertype(e.attr3, TRIG_COLLIDE|TRIG_DISAPPEAR)) e.flags |= extentity::F_NOSHADOW;
        if(!checktriggertype(e.attr3, TRIG_COLLIDE)) e.flags |= extentity::F_NOCOLLIDE;
        switch(e.triggerstate)
        {
            case TRIGGERING:
                if(checktriggertype(e.attr3, TRIG_COLLIDE) && lastmillis-e.lasttrigger >= 500) e.flags |= extentity::F_NOCOLLIDE;
                break;
            case TRIGGERED:
                if(checktriggertype(e.attr3, TRIG_COLLIDE)) e.flags |= extentity::F_NOCOLLIDE;
                break;
            case TRIGGER_DISAPPEARED:
                e.flags |= extentity::F_NOVIS | extentity::F_NOCOLLIDE;
                break;
        }
    }

    void resettriggers()
    {
        loopv(ents)
        {
            fpsentity &e = *(fpsentity *)ents[i];
            if(e.type != ET_MAPMODEL || !validtrigger(e.attr3)) continue;
            e.triggerstate = TRIGGER_RESET;
            e.lasttrigger = 0;
            setuptriggerflags(e);
        }
    }

    void unlocktriggers(int tag, int oldstate = TRIGGER_RESET, int newstate = TRIGGERING)
    {
        loopv(ents)
        {
            fpsentity &e = *(fpsentity *)ents[i];
            if(e.type != ET_MAPMODEL || !validtrigger(e.attr3)) continue;
            if(e.attr4 == tag && e.triggerstate == oldstate && checktriggertype(e.attr3, TRIG_LOCKED))
            {
                if(newstate == TRIGGER_RESETTING && checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) continue;
                e.triggerstate = newstate;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) playsound(S_RUMBLE, &e.o);
            }
        }
    }

    ICOMMAND(trigger, "ii", (int *tag, int *state),
    {
        if(*state) unlocktriggers(*tag);
        else unlocktriggers(*tag, TRIGGERED, TRIGGER_RESETTING);
    });

    VAR(triggerstate, -1, 0, 1);

    void doleveltrigger(int trigger, int state)
    {
        defformatstring(aliasname)("level_trigger_%d", trigger);
        if(identexists(aliasname))
        {
            triggerstate = state;
            execute(aliasname);
        }
    }

    void checktriggers()
    {
        if(player1->state != CS_ALIVE) return;
        vec o = player1->feetpos();
        loopv(ents)
        {
            fpsentity &e = *(fpsentity *)ents[i];
            if(e.type != ET_MAPMODEL || !validtrigger(e.attr3)) continue;
            switch(e.triggerstate)
            {
                case TRIGGERING:
                case TRIGGER_RESETTING:
                    if(lastmillis-e.lasttrigger>=1000)
                    {
                        if(e.attr4)
                        {
                            if(e.triggerstate == TRIGGERING) unlocktriggers(e.attr4);
                            else unlocktriggers(e.attr4, TRIGGERED, TRIGGER_RESETTING);
                        }
                        if(checktriggertype(e.attr3, TRIG_DISAPPEAR)) e.triggerstate = TRIGGER_DISAPPEARED;
                        else if(e.triggerstate==TRIGGERING && checktriggertype(e.attr3, TRIG_TOGGLE)) e.triggerstate = TRIGGERED;
                        else e.triggerstate = TRIGGER_RESET;
                    }
                    setuptriggerflags(e);
                    break;
                case TRIGGER_RESET:
                    if(e.lasttrigger)
                    {
                        if(checktriggertype(e.attr3, TRIG_AUTO_RESET|TRIG_MANY|TRIG_LOCKED) && e.o.dist(o)-player1->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                            e.lasttrigger = 0;
                        break;
                    }
                    else if(e.o.dist(o)-player1->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12)) break;
                    else if(checktriggertype(e.attr3, TRIG_LOCKED))
                    {
                        if(!e.attr4) break;
                        doleveltrigger(e.attr4, -1);
                        e.lasttrigger = lastmillis;
                        break;
                    }
                    e.triggerstate = TRIGGERING;
                    e.lasttrigger = lastmillis;
                    setuptriggerflags(e);
                    if(checktriggertype(e.attr3, TRIG_RUMBLE)) playsound(S_RUMBLE, &e.o);
                    if(checktriggertype(e.attr3, TRIG_ENDSP)) endsp(false);
                    if(e.attr4) doleveltrigger(e.attr4, 1);
                    break;
                case TRIGGERED:
                    if(e.o.dist(o)-player1->radius<(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                    {
                        if(e.lasttrigger) break;
                    }
                    else if(checktriggertype(e.attr3, TRIG_AUTO_RESET))
                    {
                        if(lastmillis-e.lasttrigger<6000) break;
                    }
                    else if(checktriggertype(e.attr3, TRIG_MANY))
                    {
                        e.lasttrigger = 0;
                        break;
                    }
                    else break;
                    if(checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) break;
                    e.triggerstate = TRIGGER_RESETTING;
                    e.lasttrigger = lastmillis;
                    setuptriggerflags(e);
                    if(checktriggertype(e.attr3, TRIG_RUMBLE)) playsound(S_RUMBLE, &e.o);
                    if(checktriggertype(e.attr3, TRIG_ENDSP)) endsp(false);
                    if(e.attr4) doleveltrigger(e.attr4, 0);
                    break;
            }
        }
    }

    void animatemapmodel(const extentity &e, int &anim, int &basetime)
    {
        const fpsentity &f = (const fpsentity &)e;
        if(validtrigger(f.attr3)) switch(f.triggerstate)
        {
            case TRIGGER_RESET: anim = ANIM_TRIGGER|ANIM_START; break;
            case TRIGGERING: anim = ANIM_TRIGGER; basetime = f.lasttrigger; break;
            case TRIGGERED: anim = ANIM_TRIGGER|ANIM_END; break;
            case TRIGGER_RESETTING: anim = ANIM_TRIGGER|ANIM_REVERSE; basetime = f.lasttrigger; break;
        }
    }

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case FLAG:
            case BOX:
            case BARREL:
            case PLATFORM:
            case ELEVATOR:
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
            case TELEDEST:
                e.attr3 = e.attr2;
            case MONSTER:
                e.attr2 = e.attr1;
            case RESPAWNPOINT:
                e.attr1 = (int)player1->yaw;
                break;
        }
    }

    void entradius(extentity &e, bool color)
    {
        switch(e.type)
        {
            case TELEPORT:
                loopv(ents) if(ents[i]->type == TELEDEST && e.attr1==ents[i]->attr2)
                {
                    renderentarrow(e, vec(ents[i]->o).sub(e.o).normalize(), e.o.dist(ents[i]->o));
                    break;
                }
                break;

            case JUMPPAD:
                renderentarrow(e, vec((int)(char)e.attr3*10.0f, (int)(char)e.attr2*10.0f, e.attr1*12.5f).normalize(), 4);
                break;

            case FLAG:
            case MONSTER:
            case TELEDEST:
            case RESPAWNPOINT:
            case BOX:
            case BARREL:
            case PLATFORM:
            case ELEVATOR:
            {
                vec dir;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                break;
            }
            case MAPMODEL:
                if(validtrigger(e.attr3)) renderentring(e, checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12);
                break;
        }
    }

    bool printent(extentity &e, char *buf)
    {
        return false;
    }

    const char *entnameinfo(entity &e) { return ""; }
    const char *entname(int i)
    {
        static const char * const entnames[] =
        {
            "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight",
            "beamrifle", "minigun", "rpg", "magnum", "crossbow", "handgrenade", "pistolammo", "smgammo", "smgnade", "shells",
            "beamammo", "shaftammo", "minigunammo", "photonorb", "crossbowammo", "rpgammo", "magnumammo", "shotgun", //gaussammo = teleport, minigunammo = teledest,
            "health", "healthboost", "smallhealth", "shieldbattery", "shieldcharger", "supercharger", "quaddamage",
            "teleport", "teledest",
            "monster", "carrot", "jumppad",
            "base", "respawnpoint",
            "box", "barrel",
            "platform", "elevator",
            "flag",
            "", "", "", "",
        };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }

    int extraentinfosize() { return 0; }       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
    {
    }

    void readent(entity &e, char *buf)     // read from disk, and init
    {
        int ver = getmapversion();
        if(ver <= 30) switch(e.type)
        {
            case FLAG:
            case MONSTER:
            case TELEDEST:
            case RESPAWNPOINT:
            case BOX:
            case BARREL:
            case PLATFORM:
            case ELEVATOR:
                e.attr1 = (int(e.attr1)+180)%360;
                break;
        }
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        if(e.type == ET_MAPMODEL && validtrigger(e.attr3))
        {
            fpsentity &f = (fpsentity &)e;
            f.triggerstate = TRIGGER_RESET;
            f.lasttrigger = 0;
            setuptriggerflags(f);
        }
        else e.flags = 0;
        if(local) addmsg(N_EDITENT, "rii3ii5", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
    }

    float dropheight(entity &e)
    {
        if(e.type==BASE || e.type==FLAG) return 0.0f;
        return 4.0f;
    }
}

