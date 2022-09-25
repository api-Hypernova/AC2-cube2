#include "game.h"

namespace game
{
    bool intermission = false;
    int maptime = 0, maprealtime = 0, maplimit = -1;
    int respawnent = -1;
    int lasthit = 0, lastspawnattempt = 0;

    int following = -1, followdir = 0;

    fpsent *player1 = NULL;         // our client
    vector<fpsent *> players;       // other clients
    int savedammo[NUMGUNS];

    bool clientoption(const char *arg) { return false; }

    void taunt()
    {
        if(player1->state!=CS_ALIVE || player1->physstate<PHYS_SLOPE) return;
        if(lastmillis-player1->lasttaunt<1000) return;
        player1->lasttaunt = lastmillis;
        addmsg(N_TAUNT, "rc", player1);
    }
    COMMAND(taunt, "");

    ICOMMAND(getfollow, "", (),
    {
        fpsent *f = followingplayer();
        intret(f ? f->clientnum : -1);
    });

	void follow(char *arg)
    {
        if(arg[0] ? player1->state==CS_SPECTATOR : following>=0)
        {
            following = arg[0] ? parseplayer(arg) : -1;
            if(following==player1->clientnum) following = -1;
            followdir = 0;
            conoutf("follow %s", following>=0 ? "on" : "off");
        }
	}
    COMMAND(follow, "s");

    void nextfollow(int dir)
    {
        if(player1->state!=CS_SPECTATOR || clients.empty())
        {
            stopfollowing();
            return;
        }
        int cur = following >= 0 ? following : (dir < 0 ? clients.length() - 1 : 0);
        loopv(clients)
        {
            cur = (cur + dir + clients.length()) % clients.length();
            if(clients[cur] && clients[cur]->state!=CS_SPECTATOR)
            {
                if(following<0) conoutf("follow on");
                following = cur;
                followdir = dir;
                return;
            }
        }
        stopfollowing();
    }
    ICOMMAND(nextfollow, "i", (int *dir), nextfollow(*dir < 0 ? -1 : 1));


    const char *getclientmap() { return clientmap; }

    void resetgamestate()
    {
        if(m_classicsp)
        {
            clearmovables(); ////////////////////////////////////////////////////////////////////////
            clearmonsters();                 // all monsters back at their spawns for editing
            entities::resettriggers();
        }
        clearmovables();
        clearprojectiles();
        clearbouncers();
    }

    fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode);
        return d;
    }

    void respawnself()
    {
        if(paused || ispaused()) return;
        if(m_mp(gamemode))
        {
            if(player1->respawned!=player1->lifesequence)
            {
                addmsg(N_TRYSPAWN, "rc", player1);
                player1->respawned = player1->lifesequence;
            }
        }
        else
        {
            spawnplayer(player1);
            showscores(false);
            lasthit = 0;
            if(cmode) cmode->respawned(player1);
        }
    }

    fpsent *pointatplayer()
    {
        loopv(players) if(players[i] != player1 && intersect(players[i], player1->o, worldpos)) return players[i];
        return NULL;
    }

    void stopfollowing()
    {
        if(following<0) return;
        following = -1;
        followdir = 0;
        conoutf("follow off");
    }

    fpsent *followingplayer()
    {
        if(player1->state!=CS_SPECTATOR || following<0) return NULL;
        fpsent *target = getclient(following);
        if(target && target->state!=CS_SPECTATOR) return target;
        return NULL;
    }

    fpsent *hudplayer()
    {
        if(thirdperson) return player1;
        fpsent *target = followingplayer();
        return target ? target : player1;
    }

    void setupcamera()
    {
        fpsent *target = followingplayer();
        if(target)
        {
            player1->yaw = target->yaw;
            player1->pitch = target->state==CS_DEAD ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    //VARP(camera_follow_dead_player, 0, 1, 1);
    bool detachcamera()
    {
        fpsent *d = hudplayer();
        return d->state==CS_DEAD;
    }

    bool collidecamera()
    {
        switch(player1->state)
        {
            case CS_EDITING: return false;
            case CS_SPECTATOR: return followingplayer()!=NULL;
        }
        return true;
    }

    VARP(smoothmove, 0, 75, 100);
    VARP(smoothdist, 0, 32, 64);

    void predictplayer(fpsent *d, bool move)
    {
        d->o = d->newpos;
        d->yaw = d->newyaw;
        d->pitch = d->newpitch;
        if(move)
        {
            moveplayer(d, 1, false);
            d->newpos = d->o;
        }
        float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove;
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw<0) d->yaw += 360;
            else if(d->yaw>=360) d->yaw -= 360;
            d->pitch += d->deltapitch*k;
        }
    }

    void otherplayers(int curtime)
    {
        loopv(players)
        {
            fpsent *d = players[i];
            if(d==hudplayer())
            {
                if(d->health<=25 && d->state==CS_ALIVE && !m_insta)
                {
                    d->hurtchan = playsound(S_HEARTBEAT, NULL, NULL, -1, 1000, d->hurtchan);
                    damageblend(1, true);
                }
                else
                {
                    d->stopheartbeat();
                    removedamagescreen();
                }
            }
            if(d == player1 || d->ai) continue;

            if(d->state==CS_ALIVE)
            {
                if(lastmillis - d->lastaction >= d->gunwait) d->gunwait = 0;
                if(d->quadmillis) entities::checkquad(curtime, d);
            }
            else if(d->state==CS_DEAD && d->ragdoll) moveragdoll(d);

            const int lagtime = lastmillis-d->lastupdate;
            if(!lagtime || intermission) continue;
            else if(lagtime>1000 && d->state==CS_ALIVE)
            {
                d->state = CS_LAGGED;
                continue;
            }
            if(d->state==CS_ALIVE || d->state==CS_EDITING)
            {
                if(smoothmove && d->smoothmillis>0) predictplayer(d, true);
                else moveplayer(d, 1, false);
            }
            else if(d->state==CS_DEAD && !d->ragdoll && lastmillis-d->lastpain<2000) moveplayer(d, 1, true);
        }
    }

    VARFP(slowmosp, 0, 0, 1,
    {
        if(m_sp && !slowmosp) setvar("gamespeed", 100);
    });

    void checkslowmo()
    {
        static int lastslowmohealth = 0;
        setvar("gamespeed", intermission ? 100 : clamp(player1->health, 25, 200), true, false);
        if(player1->health<player1->maxhealth && lastmillis-max(maptime, lastslowmohealth)>player1->health*player1->health/2)
        {
            lastslowmohealth = lastmillis;
            player1->health++;
        }
    }


    void updateworld()        // main game update loop
    {
        if(!maptime) { maptime = lastmillis; maprealtime = totalmillis; return; }
        if(!curtime) { gets2c(); if(player1->clientnum>=0) c2sinfo(); return; }

        physicsframe();
        ai::navigate();
        entities::checkquad(curtime, player1);
        updateweapons(curtime);
        otherplayers(curtime);
        ai::update();
        moveragdolls();
        gets2c();
        updatemovables(curtime);
        updatemonsters(curtime);
        if(!player1->move && player1->strafe && player1->vel.magnitude2()>=50 && abs(long(player1->lastyaw-player1->yaw))<3 && abs(long(player1->lastyaw-player1->yaw))>0)
        {
            player1->vel.x += -sinf(RAD*player1->yaw)*(player1->vel.magnitude2()/15);
            player1->vel.y += cosf(RAD*player1->yaw)*(player1->vel.magnitude2()/15);
            player1->vel.x += player1->strafe*cosf(RAD*player1->yaw)*(player1->vel.magnitude2()/15);
            player1->vel.y += player1->strafe*sinf(RAD*player1->yaw)*(player1->vel.magnitude2()/15);
            player1->vel.x/=1.06;
            player1->vel.y/=1.06; //woohoo!
        }
        if(lastmillis-player1->uncrouchtime<10 && player1->timeinair<500) {player1->vel.z+=30; entinmap(player1); }
        if(player1->state==CS_DEAD)
        {
            int playedspawnsnd=0;
            if(lastmillis-player1->lastpain>2000)
            {
                respawnself();
                player1->burstprogress=0;
                player1->lastswitch=lastmillis;
                if(!playedspawnsnd){msgsound(S_ROCKETPICKUP, player1); playedspawnsnd=1;}
                entinmap(player1, true);
            }
            if(player1->ragdoll) moveragdoll(player1);
            else if(lastmillis-player1->lastpain<2000)
            {
                player1->move = player1->strafe = 0;
                moveplayer(player1, 10, true);
            }
        }
        else if(!intermission)
        {
            if(player1->ragdoll) cleanragdoll(player1);
            moveplayer(player1, 10, true);
            swayhudgun(curtime);
            entities::checkitems(player1);
            if(m_sp)
            {
                if(slowmosp) checkslowmo();
                if(m_classicsp) entities::checktriggers();
            }
            else if(cmode) cmode->checkitems(player1);
        }
        if(player1->clientnum>=0) c2sinfo();   // do this last, to reduce the effective frame lag
    }

    void spawnplayer(fpsent *d)   // place at random spawn
    {
        if(cmode) cmode->pickspawn(d);
        else findplayerspawn(d, d==player1 && respawnent>=0 ? respawnent : -1);
        spawnstate(d);
        if(d==player1)
        {
            if(editmode) d->state = CS_EDITING;
            else if(d->state != CS_SPECTATOR) d->state = CS_ALIVE;
        }
        else d->state = CS_ALIVE;
    }

    VARP(spawnwait, 0, 0, 1000);

    void respawn()
    {
        if(player1->state==CS_DEAD)
        {
            player1->reloading = false;
            player1->altattacking = false;
            player1->attacking = false;
            //addmsg(N_CATCH, "rcii", player1, player1->isholdingnade, player1->isholdingorb);
            player1->crouching = false;
            addmsg(N_CROUCH, "rci", player1, player1->crouching);
            int wait = cmode ? cmode->respawnwait(player1) : 2;
            if(wait>0)
            {
                lastspawnattempt = lastmillis;
                //conoutf(CON_GAMEINFO, "\f2you must wait %d second%s before respawn!", wait, wait!=1 ? "s" : "");
                return;
            }
            if(lastmillis < player1->lastpain + spawnwait) return;
            if(m_dmsp) { changemap(clientmap, gamemode); return; }    // if we die in SP we try the same map again
            respawnself();
            if(m_classicsp)
            {
                conoutf(CON_GAMEINFO, "\f2You wasted another life! The monsters stole your armour and some ammo...");
                loopi(NUMGUNS) if(i!=GUN_PISTOL && (player1->ammo[i] = savedammo[i]) > 5) player1->ammo[i] = max(player1->ammo[i]/3, 5);
            }
        }
    }

    // inputs

    void doattack(bool on)
    {
        if(intermission) return;
        if((player1->attacking = on)) respawn();
    }

    void doaltattack(bool on)
    {
        if(intermission) return;
        if((player1->altattacking = on)) respawn();
    }
    void dosprint(bool on)
    {
        if(intermission) return;
        if((player1->sprinting = on)) respawn();
    }
    void docrouch(bool on)
    {
        if(intermission) return;
        if((player1->crouching = on)) respawn();
        if(player1->eyeheight==9)player1->uncrouchtime=lastmillis; //avoid falling through floor when standing up
        addmsg(N_CROUCH, "rci", player1, player1->crouching);
        //if(!player1->ai)player1->vel.z = max(player1->vel.z, 50);
    }

    bool canjump()
    {
        if(!intermission) respawn();
        return player1->state!=CS_DEAD && !intermission;
    }

    bool allowmove(physent *d)
    {
        if(d->type!=ENT_PLAYER) return true;
        return !((fpsent *)d)->lasttaunt || lastmillis-((fpsent *)d)->lasttaunt>=1000;
    }

    VARP(hitsound, 0, 0, 1);

    void damaged(int damage, fpsent *d, fpsent *actor, bool local)
    {
        if(d->state!=CS_ALIVE || intermission) return;

        if(local) damage = d->dodamage(damage);
        else if(actor==player1) return;

        fpsent *h = hudplayer();
        if(h!=player1 && actor==h && d!=actor && d->type!=ENT_INANIMATE)
        {
            if(hitsound && lasthit != lastmillis) playsound(S_HIT);
            lasthit = lastmillis;
        }
        if(d==h && damage)
        {
            damageblend(damage*6, false);
            damagecompass(damage, actor->o);
        }
        damageeffect(damage, d, d!=h);
        if(!guns[actor->lastattackgun].projspeed && damage==(guns[actor->lastattackgun].damage*3) && actor!=player1)actor->headshots=1;

		ai::damaged(d, actor);

        if(m_sp && slowmosp && d==player1 && d->health < 1) d->health = 1;

        if(d->health<=0) { if(local) killed(d, actor); }
        else if(d==h && lookupmaterial(camera1->o)!=MAT_WATER && damage)
        {
            //playsound(lookupmaterial(camera1->o)!=MAT_WATER?S_PAIN1+rnd(5):S_UWPN4+rnd(3));
            //if(player1->health >= 75) playsound(S_PAIN1);
            //else if(player1->health >= 50) playsound(S_PAIN2);
            //else if(player1->health >= 25) playsound(S_PAIN3);
            //else if(player1->health >= 0) playsound(S_PAIN4);
            playsound(S_IMPACT+rnd(2));
        }
        else
        {
            if(lookupmaterial(camera1->o)!=MAT_WATER && damage)
            {
                //playsound(lookupmaterial(d->o)!=MAT_WATER?S_PAIN1+rnd(5):S_UWPN4+rnd(3), &d->o);
                //if(d->health >= 75) playsound(S_PAIN1, &d->o);
                //else if(d->health >= 50) playsound(S_PAIN2, &d->o);
                //else if(d->health >= 25) playsound(S_PAIN3, &d->o);
                //else if(d->health >= 0) playsound(S_PAIN4, &d->o);
                playsound(S_IMPACT+rnd(2), &d->o);
            }
            else
            {
                //playsound(S_UWPN4+rnd(3), &d->o);
            }
        }
    }

    VARP(deathscore, 0, 1, 1);

    void deathstate(fpsent *d, bool restore)
    {
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        if(!restore) gibeffect(max(-d->health, 0), d->vel, d);
        //dropweapon(d->health==0?20:-d->health, d->vel, d);
        //d->diedgun=d->gunselect;
        //vec to(rnd(100)-50, rnd(100)-50, rnd(100)-50);
        if(d==player1)
        {
            if(deathscore) showscores(true);
            //disablezoom();
            if(!restore) loopi(NUMGUNS) savedammo[i] = player1->ammo[i];
            d->reloading = false;
            if(d->isholdingbarrel || d->isholdingnade || d->isholdingorb || d->isholdingprop){d->dropitem=1; d->attacking=1; }
            d->altattacking = false;
            //addmsg(N_CATCH, "rcii", player1, player1->isholdingnade, player1->isholdingorb);
            d->crouching=false;
            addmsg(N_CROUCH, "rci", player1, player1->crouching);
            if(!restore) d->deaths++;
            //d->pitch = 0;
            d->roll = 0;
            if(lookupmaterial(camera1->o)!=MAT_WATER)
            {
                //playsound(S_DIE1+rnd(2));
                //if(d->playermodel==0 || d->playermodel==2) playsound(S_DEATHR); //fixit
               playsound(S_D1+rnd(3)); //snout
            }
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            d->smoothmillis = 0;
            if(lookupmaterial(camera1->o)!=MAT_WATER && lookupmaterial(d->o)!=MAT_WATER)
            {
                //playsound(S_DIE1+rnd(2), &d->o);
               // if(d->playermodel==0 || d->playermodel==2) playsound(S_DEATHR, &d->o); //fixit
                playsound(S_D1+rnd(3), &d->o); //snout
            }
        }
    }

    int lastacc = 0;
    bool firstcheck = true;
    int oldacc = 0;

    /*void statsacc() //accuracy stats updating
    {
        int accuracy = player1->totaldamage*100/max(player1->totalshots, 1);    //current accuracy
        if((totalmillis >= lastacc+30000 && firstcheck) || (totalmillis >= lastacc+5000 && !firstcheck))  //First check - 30s to prevent getting 100% accuracy into avg
        {
            if(!game::stats[3] && accuracy)
            {
                game::stats[3] = accuracy;
                game::stats[11]++;
                oldacc = accuracy;
                firstcheck = false;
            }
            else if(game::stats[3] && accuracy && accuracy != oldacc)
            {
                game::stats[3] = ((game::stats[3]*game::stats[11])+accuracy) / (game::stats[11]+1);
                game::stats[11]++;
                oldacc = accuracy;
            }
            lastacc = totalmillis;
        }
    }*/

    VARP(seautosay, 0, 0, 0);


    void retoserver(char *text) { if(!seautosay) return; else conoutf(CON_CHAT, "%s%s:\f0 %s", player1->state==CS_SPECTATOR?"(spec)":"", colorname(player1), text); addmsg(N_TEXT, "rcs", player1, text); }
    void resayteam(char *text) { if(!seautosay) return; else conoutf(CON_TEAMCHAT, "%s:\f1 %s", colorname(player1), text); addmsg(N_SAYTEAM, "rcs", player1, text); }

    int ffrag = 0;
    int when = 0;
    string who;
    int ffrag2 = 0;
    int when2 = 0;
    string who3;
    string who5;
    string who6;
    int ismate = 0;
    int ismate2 = 0;

    void killed(fpsent *d, fpsent *actor)
    {
        d->diedgun=actor->lastattackgun;
        d->dropgun=d->gunselect;
        if(d->state==CS_EDITING)
        {
            d->editstate = CS_DEAD;
            if(d==player1) d->deaths++;
            else d->resetinterp();
            return;
        }
        else if(d->state!=CS_ALIVE || intermission) return;

        if(d->isholdingbarrel || d->isholdingnade||d->isholdingorb||d->isholdingprop) { d->dropitem=1; d->attacking=true;}
        fpsent *h = followingplayer();
        if(!h) h = player1;
        //int contype = d==h || actor==h ? CON_FRAG_SELF : CON_FRAG_OTHER;
        int contype = CON_FRAG_SELF;
        string dname, aname;
        copystring(dname, colorname(d));
        copystring(aname, colorname(actor));

        //if(d==player1) game::stats[4]++;
        if(actor==player1 && player1->headshots && d!=player1)playsound(S_HEADSHOT2);
        if(d->diedgun==GUN_CROSSBOW)playsound(S_SKEWER, &d->o);
        if(actor==player1 && d->vel.magnitude2()>150)playsound(S_AWESOME);
        if(actor==player1&&d!=player1&& !isteam(player1->team, d->team))player1->spreelength++;
        if(player1->spreelength==3 && actor==player1)playsound(S_SPREE1);
        if(player1->spreelength==5 && actor==player1)playsound(S_SPREE2);
        if(player1->spreelength==8 && actor==player1)playsound(S_SPREE3);
        if(player1->spreelength==10 && actor==player1)playsound(S_SPREE4);

        if(actor->type==ENT_AI)
            {
                conoutf(contype, "\f2%s got killed by %s!", dname, aname);
                ffrag2 = 1;
                when2 = totalmillis;
                copystring(who3, aname);
                ismate = 0;
            }
        else if(d==actor || actor->type==ENT_INANIMATE)
        {
            if(d==player1)
            {
                //if(hudplayer()->health<0)conoutf(CON_TEAMCHAT, "\f6You killed yourself!");
                //else if(hudplayer()->health>0)conoutf(CON_TEAMCHAT, "\f6You didn't watch your step!");
                //game::stats[18]++;
            }
            //conoutf(contype, "%s%s \f3[SELF]", isteam(d->team, player1->team)?"\f1":"\f3", dname);
            conoutf(contype, "\fx%s%s %s", d->health<=0?guns[actor->lastattackgun].name:"[SELF]", isteam(actor->team, player1->team)?"\f1":"\f3", colorname(actor));
            if(d==player1)playsound(S_BOTLIKE);
            //pushfont();
            //setfont("digit_blue");
            //conoutf(CON_GAMEINFO, "%d", 12345);
            //popfont();
        }
        else if(isteam(d->team, actor->team))
        {
            if(actor==player1) {/*conoutf(CON_TEAMCHAT, "\f2you \f3[TEAM] \f2%s", dname);*/ defformatstring(sry)("Sorry %s!", dname); resayteam(sry);
                ffrag = 1;
                when = totalmillis;
                copystring(who, dname);
                ismate2 = 1;
                playsound(S_TEAMKILL);
            }
            else if(d==player1) {//conoutf(CON_TEAMCHAT, "\f2%s \f3[TEAM] \f2you", aname);
            ffrag2 = 1;
            when2 = totalmillis;
            copystring(who3, aname);
            ismate = 1;
            }

            //conoutf(contype, "%s%s \f3[TEAM] %s%s", isteam(actor->team, player1->team)?"\f1":"\f3",aname, isteam(d->team, player1->team)?"\f1":"\f3", dname);
            //conoutf(contype, "%s%s \fx%s %s%s", isteam(actor->team, player1->team)?"\f1":"\f3", aname, guns[actor->lastattackgun].name, isteam(d->team, player1->team)?"\f1":"\f3", dname);
            conoutf(contype, "%s%s \fx%s %s%s%s%s", isteam(actor->team, player1->team)?"\f1":"\f3", colorname(actor), d->health>-800?guns[actor->lastattackgun].name:guns[GUN_CG2].name, actor->quadmillis?"\\ ":"", actor->headshots?"~ ":"", isteam(d->team, player1->team)?"\f1":"\f3", colorname(d));
        }

        else
        {
            conoutf(contype, "%s%s \fx%s %s%s%s%s", isteam(actor->team, player1->team)?"\f1":"\f3", colorname(actor), d->health>-800?guns[actor->lastattackgun].name:guns[GUN_CG2].name,  actor->quadmillis?"\\ ":"", actor->headshots?"~ ":"", isteam(d->team, player1->team)?"\f1":"\f3", colorname(d));
            if(actor->headshots)playsound(S_HEADSHOT, &d->o);

            if(d==player1)
            {
                //conoutf(CON_TEAMCHAT, "\f2%s \f3>[%s]> \f2you", aname, guns[actor->lastattackgun].name);
                ffrag2 = 1;
                when2 = totalmillis;
                copystring(who3, aname);
                ismate = 0;
            }
            else
            {
                //conoutf(contype, "%s \f3>[%s]> \f0%s", aname, guns[actor->lastattackgun].name, dname);
                if(actor==player1)
                {
                    ffrag = 1;
                    when = totalmillis;
                    copystring(who, dname);
                    ismate2 = 0;

                    //if(d->ai) game::stats[6]++;
                    //if(!d->ai) game::stats[8]++;
                    //if(player1->gunselect == 0) game::stats[17]++;
                    //if(!strcmp(d->name, "unnamed")) game::stats[7]++;

                    if(d->vel.magnitude() >= 190)
                    {
                        playsound(S_GREATSHOT);
                    }
                    else
                    {
                        playsound(S_KILL);
                    }
                }
            }
        }
        defformatstring(who4)("\f2You were killed by %s %s%s", who3, ismate?"\f3your TEAMMATE ":"", actor->quadmillis?"with a \\ damage boost":"");
        copystring(who5, who4);
        defformatstring(who2)("You killed %s %s%s", who, ismate2?"\f3your TEAMMATE ":"", actor->quadmillis?"with a \\ damage boost":"");
        copystring(who6, who2);
        deathstate(d);
        ai::killed(d, actor);
    }

    SVARP(seendmsg, "Good game!");

    void timeupdate(int secs)
    {
        if(secs > 0)
        {
            maplimit = lastmillis + secs*1000;
        }
        else
        {
            intermission = true;
            player1->attacking = false;
            player1->reloading = false;
            player1->altattacking = false;
            //addmsg(N_CATCH, "rcii", player1, player1->isholdingnade, player1->isholdingorb);
            player1->crouching=false;
            addmsg(N_CROUCH, "rci", player1, player1->crouching);
            if(cmode) cmode->gameover();
            conoutf(CON_GAMEINFO, "\f2intermission:");
            conoutf(CON_GAMEINFO, "\f2game has ended!");
            //if(m_ctf) conoutf(CON_GAMEINFO, "\f2player frags %d | flags %d | deaths %d", player1->frags, player1->flags, player1->deaths);
            //else conoutf(CON_GAMEINFO, "\f2player frags %d | deaths %d", player1->frags, player1->deaths);
            //int accuracy = (player1->totaldamage*100)/max(player1->totalshots, 1);
            //conoutf(CON_GAMEINFO, "\f2player total damage dealt %d | damage wasted %d | accuracy %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);
            //if(m_sp) spsummary(accuracy);

            showscores(true);
            //disablezoom();
            if(!m_sp && seautosay) toserver(seendmsg);

            if(identexists("intermission")) execute("intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(player1->frags));
    ICOMMAND(getflags, "", (), intret(player1->flags));
    ICOMMAND(getdeaths, "", (), intret(player1->deaths));
    ICOMMAND(getaccuracy, "", (), intret((player1->totaldamage*100)/max(player1->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(player1->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(player1->totalshots));

    vector<fpsent *> clients;

    fpsent *newclient(int cn)   // ensure valid entity
    {
        if(cn < 0 || cn > max(0xFF, MAXCLIENTS + MAXBOTS))
        {
            neterr("clientnum", false);
            return NULL;
        }

        if(cn == player1->clientnum) return player1;

        while(cn >= clients.length()) clients.add(NULL);
        if(!clients[cn])
        {
            fpsent *d = new fpsent;
            d->clientnum = cn;
            clients[cn] = d;
            players.add(d);
        }
        return clients[cn];
    }

    fpsent *getclient(int cn)   // ensure valid entity
    {
        if(cn == player1->clientnum) return player1;
        return clients.inrange(cn) ? clients[cn] : NULL;
    }

    void clientdisconnected(int cn, bool notify)
    {
        if(!clients.inrange(cn)) return;
        if(following==cn)
        {
            if(followdir) nextfollow(followdir);
            else stopfollowing();
        }
        fpsent *d = clients[cn];
        if(!d) return;
        if(notify && d->name[0]) conoutf("player %s disconnected", colorname(d));
        removeweapons(d);
        removetrackedparticles(d);
        removetrackeddynlights(d);
        if(cmode) cmode->removeplayer(d);
        players.removeobj(d);
        DELETEP(clients[cn]);
        cleardynentcache();
    }

    void clearclients(bool notify)
    {
        loopv(clients) if(clients[i]) clientdisconnected(i, notify);
    }

    void initclient()
    {
        player1 = spawnstate(new fpsent);
        players.add(player1);
    }

    VARP(showmodeinfo, 0, 1, 1);

    void startgame()
    {
        clearmovables();
        clearmonsters();

        clearprojectiles();
        clearbouncers();
        clearragdolls();

        // reset perma-state
        loopv(players)
        {
            fpsent *d = players[i];
            d->frags = d->flags = 0;
            d->deaths = 0;
            d->totaldamage = 0;
            d->totalshots = 0;
            d->maxhealth = 100;
            d->lifesequence = -1;
            d->respawned = d->suicided = -2;
        }

        setclientmode();

        intermission = false;
        maptime = maprealtime = 0;
        maplimit = -1;

        if(cmode)
        {
            cmode->preload();
            cmode->setup();
        }

        conoutf(CON_GAMEINFO, "\f2game mode is %s", server::modename(gamemode));

        if(m_sp)
        {
            defformatstring(scorename)("bestscore_%s", getclientmap());
            const char *best = getalias(scorename);
            if(*best) conoutf(CON_GAMEINFO, "\f2try to beat your best score so far: %s", best);
        }
        else
        {
            const char *info = m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
            if(showmodeinfo && info) conoutf(CON_GAMEINFO, "\f0%s", info);
        }

        if(player1->playermodel != playermodel) switchplayermodel(playermodel);

        showscores(false);
        //disablezoom();
        lasthit = 0;

        if(identexists("mapstart")) execute("mapstart");
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);

        respawnent = -1; // so we don't respawn at an old spot
        if(!m_mp(gamemode)) spawnplayer(player1);
        else findplayerspawn(player1, -1);
        entities::resetspawns();
        copystring(clientmap, name ? name : "");

        sendmapinfo();
    }

    const char *getmapinfo()
    {
        return showmodeinfo && m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
    {
        if(d->type==ENT_INANIMATE) return;
        if     (waterlevel>0) { if(material!=MAT_LAVA) playsound(S_SPLASH1, d==player1 ? NULL : &d->o); }
        else if(waterlevel<0) playsound(material==MAT_LAVA ? S_BURN : S_SPLASH2, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(d==player1 || d->type!=ENT_PLAYER || ((fpsent *)d)->ai) msgsound(S_JUMP2+rnd(3), d); loopi(3)msgsound(S_WALK1+rnd(8), d); }
        else if(floorlevel<0) { if(d==player1 || d->type!=ENT_PLAYER || ((fpsent *)d)->ai) msgsound(S_LAND1+rnd(3), d); if(d==player1)player1->roll+=12;}
    }

    void walksound(physent *d)
    {
        //playsound(S_WALK1+rnd(8), &d->o);
        msgsound(S_WALK1+rnd(8), d);
    }

    void dynentcollide(physent *d, physent *o, const vec &dir)
    {
        switch(d->type)
        {
            case ENT_AI: if(dir.z > 0) stackmonster((monster *)d, o); break;
            case ENT_INANIMATE: if(dir.z > 0) stackmovable((movable *)d, o); break;
        }
    }

    void msgsound(int n, physent *d)
    {
        if(!d || d==player1)
        {
            addmsg(N_SOUND, "ci", d, n);
            playsound(n);
        }
        else
        {
            if(d->type==ENT_PLAYER && ((fpsent *)d)->ai)
                addmsg(N_SOUND, "ci", d, n);
            playsound(n, &d->o);
        }
    }

    int numdynents() { return players.length()+monsters.length()+movables.length(); }

    dynent *iterdynents(int i)
    {
        if(i<players.length()) return players[i];
        i -= players.length();
        if(i<monsters.length()) return (dynent *)monsters[i];
        i -= monsters.length();
        if(i<movables.length()) return (dynent *)movables[i];
        return NULL;
    }

    bool duplicatename(fpsent *d, const char *name = NULL)
    {
        if(!name) name = d->name;
        loopv(players) if(d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    const char *colorname(fpsent *d, const char *name, const char *prefix)
    {
        if(!name) name = d->name;
        if(name[0] && !duplicatename(d, name) && d->aitype == AI_NONE) return name;
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx])(d->aitype == AI_NONE ? "%s%s \fs\f5(%d)\fr" : "%s%s \fs\f5[%d]\fr", prefix, name, d->clientnum);
        return cname[cidx];
    }

    void suicide(physent *d)
    {
        if(d==player1 || (d->type==ENT_PLAYER && ((fpsent *)d)->ai))
        {
            if(d->state!=CS_ALIVE) return;
            fpsent *pl = (fpsent *)d;
            if(!m_mp(gamemode)) killed(pl, pl);
            else if(pl->suicided!=pl->lifesequence)
            {
                addmsg(N_SUICIDE, "rc", pl);
                pl->suicided = pl->lifesequence;
            }
        }
        else if(d->type==ENT_AI) suicidemonster((monster *)d);
        else if(d->type==ENT_INANIMATE) suicidemovable((movable *)d);
    }
    ICOMMAND(kill, "", (), suicide(player1));

    bool needminimap() { return m_ctf || m_protect || m_hold || m_capture; }

    VARP(senewhud, 1, 1, 1);

    void drawicon(int icon, float x, float y, float sz, int w, int h)
    {
        if(senewhud)
            settexture("packages/hud/flags.png");
        else
            settexture("packages/hud/items.png");

        glBegin(GL_TRIANGLE_STRIP);
        float tsz = 0.25f, tx = tsz*(icon%4), ty = tsz*(icon/4);
        glTexCoord2f(tx,     ty);     glVertex2f(x,    y);
        glTexCoord2f(tx+tsz, ty);     glVertex2f(x+sz, y);
        glTexCoord2f(tx,     ty+tsz); glVertex2f(x,    y+sz);
        glTexCoord2f(tx+tsz, ty+tsz); glVertex2f(x+sz, y+sz);
        glEnd();
    }

    float abovegameplayhud(int w, int h)
    {
        switch(hudplayer()->state)
        {
            case CS_EDITING:
            case CS_SPECTATOR:
                return 1;
            default:
                return 1650.0f/1800.0f;
        }
    }

    int ammohudup[3] = { GUN_CG, GUN_RL, GUN_HANDGRENADE },
        ammohuddown[3] = { GUN_MAGNUM, GUN_SG, GUN_PISTOL },
        ammohudcycle[7] = { -1, -1, -1, -1, -1, -1, -1 };

    ICOMMAND(ammohudup, "sss", (char *w1, char *w2, char *w3),
    {
        int i = 0;
        if(w1[0]) ammohudup[i++] = parseint(w1);
        if(w2[0]) ammohudup[i++] = parseint(w2);
        if(w3[0]) ammohudup[i++] = parseint(w3);
        while(i < 3) ammohudup[i++] = -1;
    });

    ICOMMAND(ammohuddown, "sss", (char *w1, char *w2, char *w3),
    {
        int i = 0;
        if(w1[0]) ammohuddown[i++] = parseint(w1);
        if(w2[0]) ammohuddown[i++] = parseint(w2);
        if(w3[0]) ammohuddown[i++] = parseint(w3);
        while(i < 3) ammohuddown[i++] = -1;
    });

    ICOMMAND(ammohudcycle, "sssssss", (char *w1, char *w2, char *w3, char *w4, char *w5, char *w6, char *w7),
    {
        int i = 0;
        if(w1[0]) ammohudcycle[i++] = parseint(w1);
        if(w2[0]) ammohudcycle[i++] = parseint(w2);
        if(w3[0]) ammohudcycle[i++] = parseint(w3);
        if(w4[0]) ammohudcycle[i++] = parseint(w4);
        if(w5[0]) ammohudcycle[i++] = parseint(w5);
        if(w6[0]) ammohudcycle[i++] = parseint(w6);
        if(w7[0]) ammohudcycle[i++] = parseint(w7);
        while(i < 7) ammohudcycle[i++] = -1;
    });

    VARP(ammohud, 0, 1, 1);

    void drawammohud(fpsent *d)
    {
        float x = HICON_X + 2*HICON_STEP, y = HICON_Y, sz = HICON_SIZE;
        glPushMatrix();
        glScalef(1/3.2f, 1/3.2f, 1);
        float xup = (x+sz)*3.2f, yup = y*3.2f + 0.1f*sz;
        loopi(3)
        {
            int gun = ammohudup[i];
            if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
            drawicon(HICON_FIST+gun, xup, yup, sz);
            yup += sz;
        }
        float xdown = x*3.2f - sz, ydown = (y+sz)*3.2f - 0.1f*sz;
        loopi(3)
        {
            int gun = ammohuddown[3-i-1];
            if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
            ydown -= sz;
            drawicon(HICON_FIST+gun, xdown, ydown, sz);
        }
        int offset = 0, num = 0;
        loopi(7)
        {
            int gun = ammohudcycle[i];
            if(gun < GUN_FIST || gun > GUN_PISTOL) continue;
            if(gun == d->gunselect) offset = i + 1;
            else if(d->ammo[gun]) num++;
        }
        float xcycle = (x+sz/2)*3.2f + 0.5f*num*sz, ycycle = y*3.2f-sz;
        loopi(7)
        {
            int gun = ammohudcycle[(i + offset)%7];
            if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
            xcycle -= sz;
            drawicon(HICON_FIST+gun, xcycle, ycycle, sz);
        }
        glPopMatrix();
    }

    void drawhudicons(fpsent *d)
    {
        if(senewhud) return;
        glPushMatrix();
        glScalef(2, 2, 1);

        draw_textf("%d", (HICON_X + HICON_SIZE + HICON_SPACE)/2, HICON_TEXTY/2, d->state==CS_DEAD ? 0 : d->health);
        if(d->state!=CS_DEAD)
        {
            if(d->armour) draw_textf("%d", (HICON_X + HICON_STEP + HICON_SIZE + HICON_SPACE)/2, HICON_TEXTY/2, d->armour);
            draw_textf("%d", (HICON_X + 2*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, HICON_TEXTY/2, d->ammo[d->gunselect]);
        }

        glPopMatrix();

        drawicon(HICON_HEALTH, HICON_X, HICON_Y);
        if(d->state!=CS_DEAD)
        {
            if(d->armour) drawicon(HICON_BLUE_ARMOUR+d->armourtype, HICON_X + HICON_STEP, HICON_Y);
            drawicon(HICON_FIST+d->gunselect, HICON_X + 2*HICON_STEP, HICON_Y);
            if(d->quadmillis) drawicon(HICON_QUAD, HICON_X + 3*HICON_STEP, HICON_Y);
            if(ammohud) drawammohud(d);
        }
    }

    int chh, cah, camh;
    float chs, cas, cams;

    void newhud(int w, int h) //new SauerEnhanced HUD
    {
        if(hudplayer()->state==CS_DEAD || hudplayer()->state==CS_SPECTATOR) return;
        glPushMatrix();
        glScalef(1/1.2f, 1/1.2f, 1);
        if(!m_insta) draw_textf("%s%d", 80, h*1.2f-128, hudplayer()->health>51?"":(hudplayer()->health>26?"\f2":"\f3"), hudplayer()->health);
        if(!m_insta && hudplayer()->armour) draw_textf("%s%d", 300, h*1.2f-80, hudplayer()->armour>51?"":(hudplayer()->armour>26?"\f2":"\f3"), hudplayer()->armour);
        //defformatstring(ammo)("%d", hudplayer()->ammo[hudplayer()->gunselect]);
        int magleft[NUMGUNS];
        loopi(NUMGUNS) magleft[i]=guns[i].magsize-hudplayer()->magprogress[i];
        loopi(NUMGUNS)if(hudplayer()->ammo[i]<magleft[i])magleft[i]=hudplayer()->ammo[i];
        int wb = 0, hb = 0;
        //text_bounds(ammo, wb, hb);
        if(hudplayer()->gunselect==GUN_CG || hudplayer()->gunselect==GUN_CG2){
            draw_textf("%d", w*1.2f-wb-260, h*1.2f-128, magleft[GUN_CG]);
            draw_textf("|%d", w*1.2f-wb-130, h*1.2f-128, hudplayer()->ammo[GUN_CG2]);
        }
        else if(hudplayer()->gunselect==GUN_ELECTRO || hudplayer()->gunselect==GUN_ELECTRO2){
            draw_textf("%d", w*1.2f-wb-250, h*1.2f-128, hudplayer()->ammo[GUN_ELECTRO]);
            draw_textf("|%d", w*1.2f-wb-150, h*1.2f-128, hudplayer()->ammo[GUN_ELECTRO2]);
        }
        else if(hudplayer()->gunselect==GUN_SMG || hudplayer()->gunselect==GUN_SMG2){
            draw_textf("%d", w*1.2f-wb-260, h*1.2f-128, magleft[GUN_SMG]);
            draw_textf("|%d", w*1.2f-wb-145, h*1.2f-128, hudplayer()->ammo[GUN_SMG2]);
        }
        //else draw_textf("%d", w*1.2f-wb-180, h*1.2f-128, hudplayer()->ammo[hudplayer()->gunselect]);
        else draw_textf("%d", w*1.2f-wb-180, h*1.2f-128, guns[hudplayer()->gunselect].magsize?magleft[hudplayer()->gunselect]:hudplayer()->ammo[hudplayer()->gunselect]);

        if(hudplayer()->quadmillis)
        {
            settexture("packages/hud/hud_quaddamage_left.png");  //QuadDamage left glow
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);   glVertex2f(0,   h*1.2f-207);
            glTexCoord2f(1.0f, 0.0f);   glVertex2f(539, h*1.2f-207);
            glTexCoord2f(1.0f, 1.0f);   glVertex2f(539, h*1.2f);
            glTexCoord2f(0.0f, 1.0f);   glVertex2f(0,   h*1.2f);
            glEnd();

            settexture("packages/hud/hud_quaddamage_right.png"); //QuadDamage right glow
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);   glVertex2f(w*1.2f-135, h*1.2f-207);
            glTexCoord2f(1.0f, 0.0f);   glVertex2f(w*1.2f,     h*1.2f-207);
            glTexCoord2f(1.0f, 1.0f);   glVertex2f(w*1.2f,     h*1.2f);
            glTexCoord2f(0.0f, 1.0f);   glVertex2f(w*1.2f-135, h*1.2f);
            glEnd();
        }

        if(hudplayer()->maxhealth > 100)
        {
            settexture("packages/hud/hud_megahealth.png");  //HealthBoost indicator
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);   glVertex2f(0,   h*1.2f-207);
            glTexCoord2f(1.0f, 0.0f);   glVertex2f(539, h*1.2f-207);
            glTexCoord2f(1.0f, 1.0f);   glVertex2f(539, h*1.2f);
            glTexCoord2f(0.0f, 1.0f);   glVertex2f(0,   h*1.2f);
            glEnd();
        }

        int health = (hudplayer()->health*100)/hudplayer()->maxhealth,
            armour = (hudplayer()->armour*100)/200,
            hh = (health*101)/100,
            ah = (armour*167)/100;

        float hs = (health*1.0f)/100,
              as = (armour*1.0f)/100;

        if     (chh>hh) chh -= max(1, ((chh-hh)/4));
        else if(chh<hh) chh += max(1, ((hh-chh)/4));
        if     (chs>hs) chs -= max(0.01f, ((chs-hs)/4));
        else if(chs<hs) chs += max(0.01f, ((hs-chs)/4));

        if     (cah>ah) cah -= max(1, ((cah-ah)/4));
        else if(cah<ah) cah += max(1, ((ah-cah)/4));
        if     (cas>as) cas -= max(0.01f, ((cas-as)/4));
        else if(cas<as) cas += max(0.01f, ((as-cas)/4));

        if(hudplayer()->health > 0 && !m_insta)
        {
            settexture("packages/hud/hud_health.png");  //Health bar
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f-chs);   glVertex2f(47, h*1.2f-chh-56);
            glTexCoord2f(1.0f, 1.0f-chs);   glVertex2f(97, h*1.2f-chh-56);
            glTexCoord2f(1.0f, 1.0f);      glVertex2f(97, h*1.2f-57);
            glTexCoord2f(0.0f, 1.0f);      glVertex2f(47, h*1.2f-57);
            glEnd();
        }

        if(hudplayer()->armour > 0)
        {
            settexture("packages/hud/hud_armour.png");  //Armour bar
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f,    0.0f);   glVertex2f(130,    h*1.2f-62);
            glTexCoord2f(cas,      0.0f);   glVertex2f(130+cah, h*1.2f-62);
            glTexCoord2f(cas,      1.0f);   glVertex2f(130+cah, h*1.2f-44);
            glTexCoord2f(0.0f,    1.0f);   glVertex2f(130,    h*1.2f-44);
            glEnd();
        }

        if(!m_insta)
        {
            settexture("packages/hud/hud_left.png"); //left HUD
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);   glVertex2f(0,   h*1.2f-207);
            glTexCoord2f(1.0f, 0.0f);   glVertex2f(539, h*1.2f-207);
            glTexCoord2f(1.0f, 1.0f);   glVertex2f(539, h*1.2f);
            glTexCoord2f(0.0f, 1.0f);   glVertex2f(0,   h*1.2f);
            glEnd();
        }

        settexture("packages/hud/hud_right.png"); //right HUD
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);   glVertex2f(w*1.2f-135, h*1.2f-207);
        glTexCoord2f(1.0f, 0.0f);   glVertex2f(w*1.2f,     h*1.2f-207);
        glTexCoord2f(1.0f, 1.0f);   glVertex2f(w*1.2f,     h*1.2f);
        glTexCoord2f(0.0f, 1.0f);   glVertex2f(w*1.2f-135, h*1.2f);
        glEnd();

        int maxammo = 0;

        switch(hudplayer()->gunselect)
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
                maxammo = 10;
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
            maxammo = 15;
            break;
        case GUN_CROSSBOW:
            maxammo = 10;
                    break;

            case GUN_PISTOL:
                maxammo = 150;
                break;
        }

        int curammo = (hudplayer()->ammo[hudplayer()->gunselect]*100)/maxammo,
            amh = (curammo*101)/100;

        float ams = (curammo*1.0f)/100;

        if     (camh>amh) camh -= max(1, ((camh-amh)/4));
        else if(camh<amh) camh += max(1, ((amh-camh)/4));
        if     (cams>ams) cams -= max(0.01f, ((cams-ams)/4));
        else if(cams<ams) cams += max(0.01f, ((ams-cams)/4));

        if(hudplayer()->ammo[hudplayer()->gunselect] > 0)
        {
            settexture("packages/hud/hud_health.png");  //Ammo bar
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f-cams);   glVertex2f(w*1.2f-47, h*1.2f-camh-56);
            glTexCoord2f(1.0f, 1.0f-cams);   glVertex2f(w*1.2f-97, h*1.2f-camh-56);
            glTexCoord2f(1.0f, 1.0f);       glVertex2f(w*1.2f-97, h*1.2f-57);
            glTexCoord2f(0.0f, 1.0f);       glVertex2f(w*1.2f-47, h*1.2f-57);
            glEnd();
        }

        glPopMatrix();
        glPushMatrix();

        glScalef(1/4.0f, 1/4.0f, 1);

        //Weapon icons

//        defformatstring(icon)("packages/hud/guns/%d.png", hudplayer()->gunselect);
//        settexture(icon);
//        glBegin(GL_QUADS);
//        glTexCoord2f(0.0f, 0.0f);   glVertex2f(w*4.0f-1162,    h*4.0f-350);
//        glTexCoord2f(1.0f, 0.0f);   glVertex2f(w*4.0f-650,     h*4.0f-350);
//        glTexCoord2f(1.0f, 1.0f);   glVertex2f(w*4.0f-650,     h*4.0f-50);
//        glTexCoord2f(0.0f, 1.0f);   glVertex2f(w*4.0f-1162,    h*4.0f-50);
//        glEnd();



        glPopMatrix();
    }

    void gameplayhud(int w, int h)
    {
        if(senewhud && player1->state!=CS_SPECTATOR) newhud(w, h);
        glPushMatrix();
        glScalef(h/1800.0f, h/1800.0f, 1);

        if(player1->state==CS_SPECTATOR)
        {
            int pw, ph, tw, th, fw, fh;
            text_bounds("  ", pw, ph);
            text_bounds("SPECTATOR", tw, th);
            th = max(th, ph);
            fpsent *f = followingplayer();
            text_bounds(f ? colorname(f) : " ", fw, fh);
            fh = max(fh, ph);
            draw_text("SPECTATOR", w*1800/h - tw - pw, 1650 - th - fh);
            if(f) draw_textf("%s(%d/%d)", w*900/h - fw - pw, 1650 - fh, colorname(f), f->health, f->armour); //1800
        }

        fpsent *d = hudplayer();
        if(d->state!=CS_EDITING)
        {
            if(d->state!=CS_SPECTATOR) drawhudicons(d);
            //if(cmode) cmode->drawhud(d, w, h);
            //newhud(w, h);
        }

        glPopMatrix();
    }

    int clipconsole(int w, int h)
    {
        if(cmode) return cmode->clipconsole(w, h);
        return 0;
    }

    VARP(teamcrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 425, 1000);

    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 2: return "data/hit.png";
            case 1: return "data/teammate.png";
            default: return "data/crosshair.png";
        }
    }

    VARP(seteamnames, 0, 1, 1);
    VARP(autofire, 0, 0, 1);
    VARP(isfiring, 0, 0, 1);
    VARP(triggerbot, 0, 0, 1);
    VARP(drawicons, 0, 0, 1);
    int selectcrosshair(float &r, float &g, float &b, int &w, int &h)
    {
        fpsent *d = hudplayer();
        if(d->state==CS_SPECTATOR) return -1;
        //draw_textf("%d", hudplayer()->vel.magnitude2(), w/2, h/3, 64, 64, 255);
        //if(d->state!=CS_ALIVE) return 0;

        int crosshair = 0;
        if(lasthit && lastmillis - lasthit < hitcrosshair) crosshair = 2;
        //else if(teamcrosshair)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);     
            if(o && o->type==ENT_PLAYER) //&& isteam(((fpsent *)o)->team, d->team))
            {
                if(isteam(((fpsent *)o)->team, d->team)) { crosshair = 1; r = g = 0;}
                else if(triggerbot)player1->attacking=true;
                if(seteamnames && !drawicons && isteam(((fpsent *)o)->team, d->team)) draw_text(((fpsent *)o)->name, w/2, h/3, 64, 64, 255);
                else if (seteamnames && !drawicons)draw_text(((fpsent *)o)->name, w/2, h/3, 255, 64, 64);
            }
        }
        if(drawicons)draw_text(guns[d->gunselect].name, w/2, h/3, 255, 255, 255);
        if(crosshair!=1 && !editmode && !m_insta)
        {
            if(d->health<=25) { r = 1.0f; g = b = 0; }
            else if(d->health<=50) { r = 1.0f; g = 0.5f; b = 0; }
        }
        //if(d->gunwait) { r *= 0.5f; g *= 0.5f; b *= 0.5f; }

        if(ffrag && totalmillis-when<=4000)
        {
            int tw, th;
            text_bounds(who6, tw, th);
            glPushMatrix();
            glScalef(1/2.5f, 1/2.5f, 1);
            draw_textf(who6, (w*2.5f - tw)/2, h*2.5f-500, who);
            glPopMatrix();
        }
        if(ffrag2 && totalmillis-when2<=4000)
        {
            int tw, th;
            text_bounds(who5, tw, th);
            glPushMatrix();
            glScalef(1/2.5f, 1/2.5f, 1);
            draw_textf(who5, (w*2.5f - tw)/2, h*2.5f-400, who);
            glPopMatrix();
        }

        return crosshair;
    }

    void lighteffects(dynent *e, vec &color, vec &dir)
    {
#if 0
        fpsent *d = (fpsent *)e;
        if(d->state!=CS_DEAD && d->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
    }

    bool serverinfostartcolumn(g3d_gui *g, int i)
    {
        static const char *names[] = { "ping ", "players ", "map ", "mode ", "master ", "host ", "port ", "description " };
        static const int struts[] =  { 0,       0,          12,     12,      8,         13,      6,       24 };
        if(size_t(i) >= sizeof(names)/sizeof(names[0])) return false;
        g->pushlist();
        g->text(names[i], 0xFFFF80, !i ? " " : NULL);
        if(struts[i]) g->strut(struts[i]);
        g->mergehits(true);
        return true;
    }

    void serverinfoendcolumn(g3d_gui *g, int i)
    {
        g->mergehits(false);
        g->poplist();
    }

    const char *mastermodecolor(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodecolors)/sizeof(mastermodecolors[0])) ? mastermodecolors[n-MM_START] : unknown;
    }

    const char *mastermodeicon(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodeicons)/sizeof(mastermodeicons[0])) ? mastermodeicons[n-MM_START] : unknown;
    }

    bool serverinfoentry(g3d_gui *g, int i, const char *name, int port, const char *sdesc, const char *map, int ping, const vector<int> &attr, int np)
    {
        if(ping < 0 || attr.empty() || attr[0]!=PROTOCOL_VERSION)
        {
            switch(i)
            {
                case 0:
                    if(g->button(" ", 0xFFFFDD, "serverunk")&G3D_UP) return true;
                    break;

                case 1:
                case 2:
                case 3:
                case 4:
                    if(g->button(" ", 0xFFFFDD)&G3D_UP) return true;
                    break;

                case 5:
                    if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
                    break;

                case 6:
                    if(g->buttonf("%d ", 0xFFFFDD, NULL, port)&G3D_UP) return true;
                    break;

                case 7:
                    if(ping < 0)
                    {
                        if(g->button(sdesc, 0xFFFFDD)&G3D_UP) return true;
                    }
                    else if(g->buttonf("[%s protocol] ", 0xFFFFDD, NULL, attr.empty() ? "unknown" : (attr[0] < PROTOCOL_VERSION ? "older" : "newer"))&G3D_UP) return true;
                    break;
            }
            return false;
        }

        switch(i)
        {
            case 0:
            {
                const char *icon = attr.inrange(3) && np >= attr[3] ? "serverfull" : (attr.inrange(4) ? mastermodeicon(attr[4], "serverunk") : "serverunk");
                if(g->buttonf("%d ", 0xFFFFDD, icon, ping)&G3D_UP) return true;
                break;
            }

            case 1:
                if(attr.length()>=4)
                {
                    if(g->buttonf(np >= attr[3] ? "\f3%d/%d " : "%d/%d ", 0xFFFFDD, NULL, np, attr[3])&G3D_UP) return true;
                }
                else if(g->buttonf("%d ", 0xFFFFDD, NULL, np)&G3D_UP) return true;
                break;

            case 2:
                if(g->buttonf("%.25s ", 0xFFFFDD, NULL, map)&G3D_UP) return true;
                break;

            case 3:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=2 ? server::modename(attr[1], "") : "")&G3D_UP) return true;
                break;

            case 4:
                if(g->buttonf("%s%s ", 0xFFFFDD, NULL, attr.length()>=5 ? mastermodecolor(attr[4], "") : "", attr.length()>=5 ? server::mastermodename(attr[4], "") : "")&G3D_UP) return true;
                break;

            case 5:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
                break;

            case 6:
                if(g->buttonf("%d ", 0xFFFFDD, NULL, port)&G3D_UP) return true;
                break;

            case 7:
                if(g->buttonf("%.25s", 0xFFFFDD, NULL, sdesc)&G3D_UP) return true;
                break;
        }
        return false;
    }

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}
    const char *gameident() { return "fps"; }
    const char *savedconfig()
    {
            //defformatstring(cfg)("profiles/%s/config.cfg", curprofile);
        return "config.cfg";
    }
    const char *restoreconfig() { return "restore.cfg"; }
    const char *defaultconfig() { return "data/defaults.cfg"; }
    const char *autoexec() { return "autoexec.cfg"; }
    const char *savedservers() { return "servers.cfg"; }

    void loadconfigs()
    {
        execfile("auth.cfg", false);
    }
}

