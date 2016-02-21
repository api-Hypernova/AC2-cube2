#include "game.h"

namespace game
{      
    vector<fpsent *> bestplayers;
    vector<const char *> bestteams;

    VARP(ragdoll, 1, 1, 1);
    VARP(ragdollmillis, 0, 10000, 300000);
    VARP(ragdollfade, 0, 1000, 300000);
    VARFP(playermodel, 0, 0, 4, changedplayermodel());
    VARP(forceplayermodels, 0, 0, 0);
    VARP(allplayermodels, 0, 0, 1);

    vector<fpsent *> ragdolls;

    void saveragdoll(fpsent *d)
    {
        if(!d->ragdoll || !ragdollmillis || (!ragdollfade && lastmillis > d->lastpain + ragdollmillis)) return;
        fpsent *r = new fpsent(*d);
        r->lastupdate = ragdollfade && lastmillis > d->lastpain + max(ragdollmillis - ragdollfade, 0) ? lastmillis - max(ragdollmillis - ragdollfade, 0) : d->lastpain;
        r->edit = NULL;
        r->ai = NULL;
        r->attackchan = r->idlechan = -1;
        if(d==player1) r->playermodel = playermodel;
        ragdolls.add(r);
        d->ragdoll = NULL;   
    }

    void clearragdolls()
    {
        ragdolls.deletecontents();
    }

    void moveragdolls()
    {
        loopv(ragdolls)
        {
            fpsent *d = ragdolls[i];
            if(lastmillis > d->lastupdate + ragdollmillis)
            {
                delete ragdolls.remove(i--);
                continue;
            }
            moveragdoll(d);
        }
    }

    static const playermodelinfo playermodels[5] =
    {
        { "mrfixit", "mrfixit/blue", "mrfixit/red", "snoutx10k/hudguns", NULL, "mrfixit/horns", { "mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow" }, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
        { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", "snoutx10k/hudguns", NULL, "snoutx10k/wings", { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "snoutx10k", "snoutx10k_blue", "snoutx10k_red", true, true },
        { "mrfixit", "mrfixit/blue", "mrfixit/red", "snoutx10k/hudguns", NULL, "mrfixit/horns", { "mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow" }, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
        { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", "snoutx10k/hudguns", NULL, "snoutx10k/wings", { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "snoutx10k", "snoutx10k_blue", "snoutx10k_red", true, true },
        //sorry ogro :){ "ogro/green", "ogro/blue", "ogro/red", "mrfixit/hudguns", "ogro/vwep", NULL, { NULL, NULL, NULL }, "ogro", "ogro_blue", "ogro_red", false, false },
        { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", "snoutx10k/hudguns", NULL, "snoutx10k/wings", { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "snoutx10k", "snoutx10k_blue", "snoutx10k_red", true, true },
        //{ "inky", "inky/blue", "inky/red", "inky/hudguns", NULL, "inky/quad", { "inky/armor/blue", "inky/armor/green", "inky/armor/yellow" }, "inky", "inky_blue", "inky_red", true, true },
        //{ "captaincannon", "captaincannon/blue", "captaincannon/red", "captaincannon/hudguns", NULL, "captaincannon/quad", { "captaincannon/armor/blue", "captaincannon/armor/green", "captaincannon/armor/yellow" }, "captaincannon", "captaincannon_blue", "captaincannon_red", true, true }
    };

    int chooserandomplayermodel(int seed)
    {
        static int choices[sizeof(playermodels)/sizeof(playermodels[0])];
        int numchoices = 0;
        loopi(sizeof(playermodels)/sizeof(playermodels[0])) if(i == playermodel || playermodels[i].selectable || allplayermodels) choices[numchoices++] = i;
        if(numchoices <= 0) return -1;
        return choices[(seed&0xFFFF)%numchoices];
    }

    const playermodelinfo *getplayermodelinfo(int n)
    {
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0])) return NULL;
        return &playermodels[n];
    }

    const playermodelinfo &getplayermodelinfo(fpsent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(d==player1 || forceplayermodels ? playermodel : d->playermodel);
        if(!mdl || (!mdl->selectable && !allplayermodels)) mdl = getplayermodelinfo(playermodel);
        return *mdl;
    }

    void changedplayermodel()
    {
        if(player1->clientnum < 0) player1->playermodel = playermodel;
        if(player1->ragdoll) cleanragdoll(player1);
        loopv(ragdolls) 
        {
            fpsent *d = ragdolls[i];
            if(!d->ragdoll) continue;
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl && (mdl->selectable || allplayermodels)) continue;
            }
            cleanragdoll(d);
        }
        loopv(players)
        {
            fpsent *d = players[i];
            if(d == player1 || !d->ragdoll) continue;
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl && (mdl->selectable || allplayermodels)) continue;
            }
            cleanragdoll(d);
        }
    }

    void preloadplayermodel()
    {
        loopi(3)
        {
            const playermodelinfo *mdl = getplayermodelinfo(i);
            if(!mdl) break;
            if(i != playermodel && (!multiplayer(false) || forceplayermodels || (!mdl->selectable && !allplayermodels))) continue;
            if(m_teammode)
            {
                preloadmodel(mdl->blueteam);
                preloadmodel(mdl->redteam);
            }
            else preloadmodel(mdl->ffa);
            if(mdl->vwep) preloadmodel(mdl->vwep);
            if(mdl->quad) preloadmodel(mdl->quad);
            loopj(3) if(mdl->armour[j]) preloadmodel(mdl->armour[j]);
        }
    }
    
    VAR(testquad, 0, 0, 1);
    VAR(testarmour, 0, 0, 1);
    VAR(testteam, 0, 0, 3);
    VARP(hidedead, 0, 0, 1);
    VARP(vwep, 0, 1, 1);

    void renderplayer(fpsent *d, const playermodelinfo &mdl, int team, float fade, bool mainpass)
    {
        int lastaction = d->lastaction, hold = mdl.vwep || d->gunselect==GUN_PISTOL ? 0 : (ANIM_HOLD1+d->gunselect)|ANIM_LOOP, attack = ANIM_ATTACK1+d->gunselect, delay = mdl.vwep ? 300 : guns[d->gunselect].attackdelay+50;
        if(intermission && d->state!=CS_DEAD)
        {
            lastaction = 0;
            hold = attack = ANIM_LOSE|ANIM_LOOP;
            delay = 0;
            if(m_teammode) loopv(bestteams) { if(!strcmp(bestteams[i], d->team)) { hold = attack = ANIM_WIN|ANIM_LOOP; break; } }
            else if(bestplayers.find(d)>=0) hold = attack = ANIM_WIN|ANIM_LOOP;
        }
        else if(d->state==CS_ALIVE && d->lasttaunt && lastmillis-d->lasttaunt<1000 && lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            hold = attack = ANIM_TAUNT;
            delay = 1000;
        }
        modelattach a[5];
        //enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_MAGNUM, GUN_HANDGRENADE, GUN_ELECTRO, GUN_ELECTRO2, GUN_CG2, GUN_SHOTGUN2, GUN_SMG, GUN_SMG2, GUN_CROSSBOW, GUN_TELEKENESIS, GUN_TELEKENESIS2, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };
        static const char *vweps[] = {"vwep/pistoldefault", "vwep/shotgun", "vwep/minigun", "vwep/rocket", "vwep/pistoldefault", "projectiles/grenade", "vwep/railgun", "vwep/railgun", "vwep/minigun", "vwep/shotgun", "vwep/rifle", "vwep/rifle", "vwep/crossbow", "vwep/minigun", "vwep/minigun", "vwep/pistoldefault"};
        int ai = 0;
        if((!mdl.vwep || d->gunselect!=GUN_FIST) && d->gunselect<=GUN_PISTOL && vwep)
        {
            int vanim = ANIM_VWEP_IDLE|ANIM_LOOP, vtime = 0;
            if(lastaction && d->lastattackgun==d->gunselect && lastmillis < lastaction + delay)
            {
                vanim = ANIM_VWEP_SHOOT;
                vtime = lastaction;
            }
            if(d->holdingweapon)a[ai++] = modelattach("tag_weapon", mdl.vwep ? mdl.vwep : vweps[d->gunselect], vanim, vtime);
        }
        if(d->state==CS_ALIVE)
        {
            if((testquad || d->quadmillis) && mdl.quad)
                a[ai++] = modelattach("tag_powerup", mdl.quad, ANIM_POWERUP|ANIM_LOOP, 0);
            if(testarmour || d->armour)
            {
                int type = clamp(d->armourtype, (int)A_BLUE, (int)A_YELLOW);
                if(mdl.armour[type])
                    a[ai++] = modelattach("tag_shield", mdl.armour[type], ANIM_SHIELD|ANIM_LOOP, 0);
            }
        }
        if(mainpass)
        {
            d->muzzle = vec(-1, -1, -1);
            a[ai++] = modelattach("tag_muzzle", &d->muzzle);
        }
        const char *mdlname = mdl.ffa;
        switch(testteam ? testteam-1 : team)
        {
            case 1: mdlname = mdl.blueteam; break;
            case 2: mdlname = mdl.redteam; break;
        }
        if(d->health>-40 ||d->health<-3000 || guns[d->diedgun].splash<=30)renderclient(d, mdlname, a[0].tag ? a : NULL, hold, attack, delay, lastaction, intermission && d->state!=CS_DEAD ? 0 : d->lastpain, fade, ragdoll && mdl.ragdoll);
        d->o.z-=5.f;
        if(d->diedgun==GUN_CROSSBOW && (lastmillis-d->lastpain)<2000)rendermodel(NULL, "projectiles/xbolt", ANIM_MAPMODEL|ANIM_LOOP, d->o, 0, 90, MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHT_FAST|MDL_DYNSHADOW);
        d->o.z+=5.f;
#if 0
        if(d->state!=CS_DEAD && d->quadmillis) 
        {
            entitylight light;
            rendermodel(&light, "quadrings", ANIM_MAPMODEL|ANIM_LOOP, vec(d->o).sub(vec(0, 0, d->eyeheight/2)), 360*lastmillis/1000.0f, 0, MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_DIST);
        }
#endif
    }

    VARP(teamskins, 1, 1, 1);

    void rendergame(bool mainpass)
    {
        if(mainpass) ai::render();

        if(intermission)
        {
            bestteams.shrink(0);
            bestplayers.shrink(0);
            if(m_teammode) getbestteams(bestteams);
            else getbestplayers(bestplayers);
        }

        startmodelbatches();

        fpsent *exclude = isthirdperson() ? NULL : followingplayer();
        loopv(players)
        {
            fpsent *d = players[i];
            if(d == player1 || d->state==CS_SPECTATOR || d->state==CS_SPAWNING || d->lifesequence < 0 || d == exclude) continue;
            int team = 0;
            if(teamskins || m_teammode) team = isteam(player1->team, d->team) ? 1 : 2;
            renderplayer(d, getplayermodelinfo(d), team, 1, mainpass);
            copystring(d->info, colorname(d));
            if(d->maxhealth>100) { defformatstring(sn)(" +%d", d->maxhealth-100); concatstring(d->info, sn); }
            /*if(d->state!=CS_DEAD) */particle_text(d->abovehead(), d->info, PART_TEXT, 1, team ? (team==1 ? 0x6496FF : 0xFF4B19) : 0x1EC850, 2.0f);
        }
        loopv(ragdolls)
        {
            fpsent *d = ragdolls[i];
            int team = 0;
            if(teamskins || m_teammode) team = isteam(player1->team, d->team) ? 1 : 2;
            float fade = 1.0f;
            if(ragdollmillis && ragdollfade) 
                fade -= clamp(float(lastmillis - (d->lastupdate + max(ragdollmillis - ragdollfade, 0)))/min(ragdollmillis, ragdollfade), 0.0f, 1.0f);
            renderplayer(d, getplayermodelinfo(d), team, fade, mainpass);
        } 
        if(isthirdperson() && !followingplayer()) renderplayer(player1, getplayermodelinfo(player1), teamskins || m_teammode ? 1 : 0, 1, mainpass);
        rendermonsters();
        rendermovables();
        entities::renderentities();
        renderbouncers();
        renderprojectiles();
        rendercaughtitems();
        if(cmode) cmode->rendergame();

        endmodelbatches();
    }

    void weaponloop(const vec &to)
    {
        loopv(ragdolls) {
            fpsent *s= ragdolls[i];
            if(s->diedgun!=GUN_TELEKENESIS && s->diedgun!=GUN_TELEKENESIS2  && s->diedgun!=GUN_FIST && s->o.dist(to) <= 10 && !player1->isholdingprop && !player1->isholdingnade && !player1->isholdingorb && !player1->isholdingbarrel){
                s->holdingweapon=0;
                msgsound(S_ROCKETPICKUP, player1);
                int type;
                if(s->diedgun==GUN_CG || s->gunselect==GUN_CG2)type==I_MINIGUN;
                else if(s->diedgun==GUN_CROSSBOW)type==I_CROSSBOW;
                else if(s->diedgun==GUN_ELECTRO || s->diedgun==GUN_ELECTRO2)type==I_ELECTRO;
                else if(s->diedgun==GUN_HANDGRENADE)type==I_GRENADE;
                else if(s->diedgun==GUN_MAGNUM)type==I_MAGNUM;
                else if(s->diedgun==GUN_PISTOL)type==I_PISTOLAMMO;
                else if(s->diedgun==GUN_RL)type==I_RPG;
                else if(s->diedgun==GUN_SG || s->diedgun==GUN_SHOTGUN2)type==I_SHOTGUN;
                else if(s->diedgun==GUN_SMG|| s->diedgun==GUN_SMG2)type==I_SMGAMMO;
                if(type && player1->canpickup(type))player1->pickup(type);
            }
        }
    }

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);
    VARP(teamhudguns, 0, 1, 1);
    VARP(chainsawhudgun, 0, 1, 1);
    VAR(testhudgun, 0, 0, 1);

    FVAR(swaystep, 1, 35.0f, 100);
    FVAR(swayside, 0, 0.04f, 1);
    FVAR(swayup, 0, 0.05f, 1);

    float swayfade = 0, swayspeed = 0, swaydist = 0;
    vec swaydir(0, 0, 0);

    void swayhudgun(int curtime)
    {
        fpsent *d = hudplayer();
        if(d->state != CS_SPECTATOR)
        {
            if(d->physstate >= PHYS_SLOPE)
            {
                swayspeed = min(sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y), d->maxspeed);
                swaydist += swayspeed*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade = 1;
            }
            else if(swayfade > 0)
            {
                swaydist += swayspeed*swayfade*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade -= 0.5f*(curtime*d->maxspeed)/(swaystep*1000.0f);
            }

            float k = pow(0.7f, curtime/10.0f);
            swaydir.mul(k);
            vec vel(d->vel);
            vel.add(d->falling);
            swaydir.add(vec(vel).mul((1-k)/(15*max(vel.magnitude(), d->maxspeed))));
        }
    }

    struct hudent : dynent
    {
        hudent() { type = ENT_CAMERA; }
    } guninterp;

    SVARP(hudgunsdir, "");
    int hudgunpitch;
    int jump;
    int dir;
    void drawhudmodel(fpsent *d, int anim, float speed = 0, int base = 0)
    {
        if(d->gunselect>GUN_PISTOL) return;

        vec sway;
        vecfromyawpitch(d->yaw, 0, 0, 1, sway);
        float steps = swaydist/swaystep*M_PI;
        sway.mul(swayside*cosf(steps));
        sway.z = swayup*(fabs(sinf(steps)) - 1);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway) sway = d->o;

#if 0
        if(player1->state!=CS_DEAD && player1->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
        const playermodelinfo &mdl = getplayermodelinfo(d);
        defformatstring(gunname)("%s/%s", hudgunsdir[0] ? hudgunsdir : mdl.hudguns, guns[d->gunselect].file);
        if((m_teammode || teamskins) && teamhudguns)
            concatstring(gunname, d==player1 || isteam(d->team, player1->team) ? "/blue" : "/red");
        else if(testteam > 1)
            concatstring(gunname, testteam==2 ? "/blue" : "/red");
        modelattach a[2];
        d->muzzle = vec(-1, -1, -1);
        a[0] = modelattach("tag_muzzle", &d->muzzle);
        dynent *interp = NULL;
//        if(d->gunselect==GUN_FIST && chainsawhudgun)
//        {
//            anim |= ANIM_LOOP;
//            base = 0;
//            interp = &guninterp;
//        }
        if(lastmillis-d->lastswitch<3)hudgunpitch=d->pitch-90;
        if(lastmillis-d->lastaction<3 && (d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_ELECTRO) && (lastmillis-d->lastaction)<10)jump=1;
        if(jump && (d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_ELECTRO))hudgunpitch+=d->gunselect==GUN_SMG?1:2;
        if((d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_ELECTRO) && jump && hudgunpitch>d->pitch+((d->gunselect==GUN_SMG||d->gunselect==GUN_ELECTRO)?6:25))jump=0;
        if(hudgunpitch<d->pitch-2 && !jump)hudgunpitch+=2; //raise hudgun to normal pos in case player looked up during firing
        else if(hudgunpitch>d->pitch+2 && !jump)hudgunpitch-=d->gunselect==GUN_SMG?1:2;
        if(lastmillis-d->lastaction>350&&lastmillis-d->lastswitch>200&&(d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2|| d->gunselect==GUN_ELECTRO))hudgunpitch=d->pitch;
        if(lastmillis-d->lastswitch>200&&(d->gunselect!=GUN_MAGNUM && d->gunselect!=GUN_SMG && d->gunselect!=GUN_SMG2&& d->gunselect!=GUN_ELECTRO))hudgunpitch=d->pitch;
        if(lastmillis-d->lastreload<1300)dir=30;
        else dir=90;
        //if(dir>=45)dir+=2;
        //if(lastmillis-d->lastreload>=2000)dir=90;
        rendermodel(NULL, gunname, anim, sway, testhudgun ? 0 : d->yaw+dir, testhudgun ? 0 :  ((hudgunpitch<d->pitch-2||hudgunpitch>d->pitch+2)?hudgunpitch:d->pitch), MDL_LIGHT, interp, a, base, (int)ceil(speed));
        if(d->muzzle.x >= 0) d->muzzle = calcavatarpos(d->muzzle, 12);
    }

    void drawhudgun()
    {
        fpsent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_EDITING || (!hudgun && d->gunselect!=GUN_TELEKENESIS && d->gunselect!=GUN_TELEKENESIS2) || editmode)
        { 
            d->muzzle = player1->muzzle = vec(-1, -1, -1);
            return;
        }

        int rtime = d->gunselect==GUN_SMG2?350:guns[d->gunselect].attackdelay;
        if(d->gunselect==GUN_FIST)rtime=500;
        if(d->gunselect==GUN_SHOTGUN2)rtime=260;
        if(d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2)rtime=0;
        if(d->gunselect==GUN_ELECTRO2)rtime=0;// || d->gunselect==GUN_ELECTRO)rtime=0;
        if(d->gunselect==GUN_TELEKENESIS2)rtime=2;
        if(d->gunselect==GUN_RL && !strcmp(guns[d->gunselect].file, "rocket"))rtime=700;
        if(d->lastaction && d->lastattackgun==d->gunselect && lastmillis-d->lastaction<rtime)
        {
            drawhudmodel(d, ANIM_GUN_SHOOT|ANIM_SETSPEED, rtime/17.0f, d->lastaction);
        }
        else
        {
            drawhudmodel(d, ANIM_GUN_IDLE|ANIM_LOOP);
        }
    }

    void renderavatar()
    {
        drawhudgun();
    }

    vec hudgunorigin(int gun, const vec &from, const vec &to, fpsent *d)
    {
        if(d->muzzle.x >= 0) return d->muzzle;
        vec offset(from);
        if(d!=hudplayer() || isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            if(d->type!=ENT_AI)
            {
                offset.z += (d->aboveeye + d->eyeheight)*0.75f - d->eyeheight;
                vecfromyawpitch(d->yaw, 0, 0, -1, right);
                offset.add(right.mul(0.5f*d->radius));
                offset.add(front);
            }
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if(hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        else offset.sub(vec(camup).mul(0.8f));
        return offset;
    }

    void preloadweapons()
    {
        const playermodelinfo &mdl = getplayermodelinfo(player1);
        loopi(NUMGUNS)
        {
            const char *file = guns[i].file;
            if(!file) continue;
            string fname;
            if((m_teammode || teamskins) && teamhudguns)
            {
                formatstring(fname)("%s/%s/blue", hudgunsdir[0] ? hudgunsdir : mdl.hudguns, file);
                preloadmodel(fname);
            }
            else
            {
                formatstring(fname)("%s/%s", hudgunsdir[0] ? hudgunsdir : mdl.hudguns, file);
                preloadmodel(fname);
            }
            formatstring(fname)("vwep/%s", file);
            preloadmodel(fname);
        }
    }

    void preload()
    {
        if(hudgun) preloadweapons();
        preloadbouncers();
        preloadplayermodel();
        entities::preloadentities();
        if(m_sp) preloadmonsters();
    }

}

