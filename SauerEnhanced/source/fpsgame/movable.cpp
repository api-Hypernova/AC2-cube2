// movable.cpp: implements physics for inanimate models
#include "game.h"

extern int physsteps;

namespace game
{
    enum
    {
        BOXWEIGHT = 25,
        BARRELHEALTH = 1000000,
        BARRELWEIGHT = 25,
        PLATFORMWEIGHT = 1000,
        PLATFORMSPEED = 8,
        EXPLODEDELAY = 200
    };

    struct movable : dynent
    {
        int etype, mapmodel, health, weight, exploding, tag, dir, iscaught, lasttime, id;
        physent *stacked;
        vec stackpos;

        movable(const entity &e) :
            etype(e.type),
            mapmodel(e.attr2),
            health(e.type==BARREL ? (e.attr4 ? e.attr4 : BARRELHEALTH) : 0),
            weight(e.type==PLATFORM || e.type==ELEVATOR ? PLATFORMWEIGHT : (e.attr3 ? e.attr3 : (e.type==BARREL ? BARRELWEIGHT : BOXWEIGHT))),
            exploding(0),
            tag(e.type==PLATFORM || e.type==ELEVATOR ? e.attr3 : 0),
            dir(e.type==PLATFORM || e.type==ELEVATOR ? (e.attr4 < 0 ? -1 : 1) : 0),
            stacked(NULL),
            stackpos(0, 0, 0)
        {
            state = CS_ALIVE;
            type = ENT_INANIMATE;
            yaw = float((e.attr1+7)-(e.attr1+7)%15);
            if(e.type==PLATFORM || e.type==ELEVATOR)
            {
                maxspeed = e.attr4 ? fabs(float(e.attr4)) : PLATFORMSPEED;
                if(tag) vel = vec(0, 0, 0);
                else if(e.type==PLATFORM) { vecfromyawpitch(yaw, 0, 1, 0, vel); vel.mul(dir*maxspeed); }
                else vel = vec(0, 0, dir*maxspeed);
            }

            const  char *mdlname = mapmodelname(e.attr2);
            if(mdlname) setbbfrommodel(this, mdlname);
        }
       
        void hitpush(int damage, const vec &dir, fpsent *actor, int gun)
        {
            if(etype!=BOX && etype!=BARREL) return;
            vec push(dir);
            int pushmul = gun==GUN_TELEKENESIS2?-800/weight:80*damage/weight;
            if(gun==GUN_TELEKENESIS)pushmul=800;
            push.mul(pushmul);
            vel.add(push);
        }

        void explode(dynent *at)
        {
            state = CS_DEAD;
            exploding = 0;
            game::explode(true, (fpsent *)at, o, this, guns[GUN_BARREL].damage, GUN_BARREL);
        }
 
        void damaged(int damage, fpsent *at, int gun = -1)
        {
            if(etype!=BARREL || state!=CS_ALIVE || exploding) return;
            health -= damage;
            if(health>0) return;
            if(gun==GUN_BARREL) exploding = lastmillis + EXPLODEDELAY;
            else explode(at);
        }

//        void suicide()
//        {
//            state = CS_DEAD;
//            //if(etype==BARREL) explode(player1);
//            addmsg(N_DELMOVABLE, "rci", &player1);
//            conoutf(CON_GAMEINFO, "%d", &player1);
//        }
    };

    vector<movable *> movables;
   int lastreset;
    void clearmovables()
    {
        if(movables.length())
        {
            cleardynentcache();
            movables.deletecontents();
        }
        //if(!m_dmsp && !m_classicsp) return;
        loopv(entities::ents) 
        {
            const entity &e = *entities::ents[i];
            if(e.type!=BOX && e.type!=BARREL && e.type!=PLATFORM && e.type!=ELEVATOR) continue;
            //if(e.type==BOX&&!m_edit&&lastmillis<1000)return;
            movable *m = new movable(e);
            //if(e.type==BOX && m->iscaught)continue;
            m->id=i;
            movables.add(m);
            m->o = e.o;
            m->id=lastmillis;
            entinmap(m);
            updatedynentcache(m);
            lastreset=lastmillis;
        }
    }
//    void onshoot(fpsent *d)
//    {
//        movable *m = new movable(e);
//        //if(e.type==BOX && m->iscaught)continue;
//        movables.add(m);
//        m->o = e.o;
//        entinmap(m);
//        updatedynentcache(m);
//    }

    void triggerplatform(int tag, int newdir)
    {
        newdir = max(-1, min(1, newdir));
        loopv(movables)
        {
            movable *m = movables[i];
            if(m->state!=CS_ALIVE || (m->etype!=PLATFORM && m->etype!=ELEVATOR) || m->tag!=tag) continue;
            if(!newdir)
            {
                if(m->tag) m->vel = vec(0, 0, 0);
                else m->vel.neg();
            }
            else
            {
                if(m->etype==PLATFORM) { vecfromyawpitch(m->yaw, 0, 1, 0, m->vel); m->vel.mul(newdir*m->dir*m->maxspeed); }
                else m->vel = vec(0, 0, newdir*m->dir*m->maxspeed);
            }
        }
    }
    ICOMMAND(platform, "ii", (int *tag, int *newdir), triggerplatform(*tag, *newdir));

    void stackmovable(movable *d, physent *o)
    {
        d->stacked = o;
        d->stackpos = o->o;
    }

    void updatemovables(int curtime)
    {
        if(!curtime) return;
        loopv(movables)
        {
            movable *m = movables[i];
            if(m->state!=CS_ALIVE) continue;
            if(m->etype==BARREL && m->health<BARRELHEALTH)
            {
                regular_particle_flame(PART_FLAME, m->o, 1, 1, 0x903020, 3, 2.0f);
                 regular_particle_flame(PART_SMOKE, m->o, 1, 1, 0x303020, 1, 4.0f, 100.0f, 2000.0f, -20);
            }
            if(lastmillis-lastreset>30000) {clearmovables();lastreset=lastmillis;}
            if(m->etype==PLATFORM || m->etype==ELEVATOR)
            {
                if(m->vel.iszero()) continue;
                for(int remaining = curtime; remaining>0;)
                {
                    int step = min(remaining, 20);
                    remaining -= step;
                    if(!moveplatform(m, vec(m->vel).mul(step/1000.0f)))
                    {
                        if(m->tag) { m->vel = vec(0, 0, 0); break; }
                        else m->vel.neg();
                    }
                }
            }
            else if(m->exploding && lastmillis >= m->exploding)
            {
                m->explode(m);
                adddecal(DECAL_SCORCH, m->o, vec(0, 0, 1), RL_DAMRAD/2);
            }
            else if(m->maymove() || (m->stacked && (m->stacked->state!=CS_ALIVE || m->stackpos != m->stacked->o)))
            {
                if(physsteps > 0) m->stacked = NULL;
                moveplayer(m, 1, true);
            }
//            if((m->etype==BOX||m->etype==BARREL) && m->vel.magnitude()>10 && player1->gunselect==GUN_TELEKENESIS2  && player1->altattacking && !player1->isholdingbarrel&&!player1->isholdingnade&&!player1->isholdingorb&&!player1->isholdingprop)
//            {
//                if(m->etype==BOX && m->state==CS_ALIVE)player1->isholdingprop=1;
//                else if(m->state==CS_ALIVE)player1->isholdingbarrel=m->health<25?2:1;
//                //player1->holdingobj=m;
//                addmsg(N_CATCH, "rciiii", player1, player1->isholdingnade, player1->isholdingorb, player1->isholdingprop, player1->isholdingbarrel);
//                long *mv = ((long*)m);
//                suicidemovable((movable*)mv);
//                player1->altattacking=0;
//                m->lasttime=lastmillis;
//                //conoutf(CON_GAMEINFO, "movable caught");
//                //conoutf(CON_GAMEINFO, "%d", m);
//            }
//            vec v;
//            float d = worldpos.dist(player1->muzzle, v);
//            int steps = clamp(int(d*2), 1, 2048);
//            v.div(steps);
//            vec p = player1->muzzle;
//            loopi(20)p.add(v);
//            if(m->iscaught)
//            {

//            }
        }
    }

    void rendermovables()
    {
        loopv(movables)
        {
            movable &m = *movables[i];
            if(m.state!=CS_ALIVE) continue;
            vec o = m.feetpos();
            const char *mdlname = mapmodelname(m.mapmodel);
            if(!mdlname) continue;
			rendermodel(NULL, mdlname, ANIM_MAPMODEL|ANIM_LOOP, o, m.yaw, 0, MDL_LIGHT | MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, &m);
        }
    }
    
    void suicidemovable(movable *m)
    {
        m->state=CS_DEAD;
        //conoutf(CON_GAMEINFO, "%d", m);
        //addmsg(N_DELMOVABLE, "rci", m);
    }

    void hitmovable(int damage, movable *m, fpsent *at, const vec &vel, int gun)
    {
        int pushmul = gun==GUN_TELEKENESIS2?100000:damage;
        m->hitpush(pushmul, vel, at, gun);
        m->damaged(damage, at, gun);
        if(gun==GUN_TELEKENESIS2 && m->state==CS_ALIVE) {
            if(at->isholdingbarrel || at->isholdingnade || at->isholdingorb || at->isholdingprop)return;
        if(at==player1 || at->ai)at->propmodeldir = m->mapmodel;
        if(m->etype==BOX)at->isholdingprop=1;
        //m->iscaught=1;
        else if(m->etype==BARREL) at->isholdingbarrel=m->health<BARRELHEALTH?2:1;
        //const char *mdlname = mapmodelname(m->mapmodel);
        //at->propmodeldir=string(mdlname);
        addmsg(N_CATCH, "rciiiii", at, at->isholdingnade, at->isholdingorb, at->isholdingprop, at->isholdingbarrel, m->mapmodel);
        if(at->ai)at->ailastcatch=lastmillis;
        //conoutf(CON_GAMEINFO, "%d", m->id);
        //at->propmodeldir=mdl;
        //long mv = ((long)m);
        //at->movable=mv;
        //conoutf(CON_GAMEINFO, "at->movable=%d", at->movable);
        }
    }
}

