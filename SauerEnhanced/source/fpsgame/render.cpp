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
        { "mrfixit", "mrfixit/blue", "mrfixit/red", "mrfixit/blue_nohead", "mrfixit/red_nohead", "snoutx10k/hudguns", NULL, "mrfixit/horns", {"mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow"}, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
        { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", "snoutx10k/blue_nohead", "snoutx10k/red_nohead", "snoutx10k/hudguns", NULL, "snoutx10k/wings", { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "snoutx10k", "snoutx10k_blue", "snoutx10k_red", true, true },
        { "mrfixit", "mrfixit/blue", "mrfixit/red", "mrfixit/blue_nohead", "mrfixit/red_nohead", "snoutx10k/hudguns", NULL, "mrfixit/horns", {"mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow"}, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
        { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", "snoutx10k/blue_nohead", "snoutx10k/red_nohead", "snoutx10k/hudguns", NULL, "snoutx10k/wings", { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "snoutx10k", "snoutx10k_blue", "snoutx10k_red", true, true },
        //sorry ogro :){ "ogro/green", "ogro/blue", "ogro/red", "mrfixit/hudguns", "ogro/vwep", NULL, { NULL, NULL, NULL }, "ogro", "ogro_blue", "ogro_red", false, false },
        { "mrfixit", "mrfixit/blue", "mrfixit/red", "mrfixit/blue_nohead", "mrfixit/red_nohead", "snoutx10k/hudguns", NULL, "mrfixit/horns", {"mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow"}, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
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
            case 1: mdlname = d->diedbyheadshot ? mdl.blueheadless : mdl.blueteam; break;
            case 2: mdlname = d->diedbyheadshot ? mdl.redheadless : mdl.redteam; break;
        }
        if (d->quadmillis) {
            mdlname = "mapmodels/sitters/vehicles/heli";
        }

        /*if(d->health>-40 ||d->health<-3000 || guns[d->diedgun].splash <= 30)*/
        if(!d->quadmillis || d->state!=CS_DEAD)renderclient(d, mdlname, a[0].tag ? a : NULL, hold, attack, delay, lastaction, intermission && d->state != CS_DEAD ? 0 : d->lastpain, fade, ragdoll && mdl.ragdoll);
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
            fpsent *s = ragdolls[i];
            if(s->diedgun!=GUN_TELEKENESIS && s->diedgun!=GUN_TELEKENESIS2  && s->diedgun!=GUN_FIST && s->o.dist(to) <= 10 && !player1->isholdingprop && !player1->isholdingnade && !player1->isholdingorb && !player1->isholdingbarrel && !player1->isholdingshock){
                s->holdingweapon=0;
                int type;
                if(s->diedgun==GUN_CG || s->gunselect==GUN_CG2)type=I_MINIGUN;
                else if(s->diedgun==GUN_CROSSBOW)type=I_CROSSBOW;
                else if(s->diedgun==GUN_ELECTRO || s->diedgun==GUN_ELECTRO2)type=I_ELECTRO;
                else if(s->diedgun==GUN_HANDGRENADE)type=I_GRENADE;
                else if(s->diedgun==GUN_MAGNUM)type=I_MAGNUM;
                else if(s->diedgun==GUN_PISTOL)type=I_PISTOLAMMO;
                else if(s->diedgun==GUN_RL)type=I_RPG;
                else if(s->diedgun==GUN_SG || s->diedgun==GUN_SHOTGUN2)type=I_SHOTGUN;
                else if(s->diedgun==GUN_SMG|| s->diedgun==GUN_SMG2)type=I_SMGAMMO;
                if(type && player1->canpickup(type))player1->pickup(type);
            }
        }
    }

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);
    VARP(teamhudguns, 0, 1, 1);
    VARP(chainsawhudgun, 0, 1, 1);
    VAR(testhudgun, 0, 0, 1);

    FVAR(swaystep, 30.f, 30.f, 30.f);
    FVAR(swayside, 0.1f, 0.1f, 0.1f);
    FVAR(swayup, 0.1f, 0.1f, 0.1f);

    float swayfade = 0, swayspeed = 0, swaydist = 0;
    vec swaydir(0, 0, 0);

    // ============================================================================
    // NEXT-GEN WEAPON MOVEMENT SYSTEM - POSITION-BASED LAG
    // ============================================================================
    // Ultra-dynamic weapon movement with TRUE position lag
    // Weapon POSITION lags 500ms behind camera, but ALWAYS points forward
    // Animations slowed 2x for smoother transitions
    // ============================================================================
    
    // Camera-based movement (position lag with history)
    const int WEAPON_LAG_DURATION_MS = 200;        // How far back in time weapon position is (200ms)
    const float WEAPON_LAG_AMOUNT = 0.375f;        // Position offset multiplier (HALVED again from 0.75)
    
    // Position offsets (height/lateral)
    const float WEAPON_JUMP_RISE = 1.6f;           // Jump rise (DOUBLED from 0.8)
    const float WEAPON_LAND_DROP = 2.6f;           // Landing drop (DOUBLED from 1.3)
    const float WEAPON_SPRINT_BOB_INTENSITY = 0.8f; // Sprint bob
    const float WEAPON_SPRINT_BOB_SPEED = 1.8f;    // Sprint bob speed
    const float WEAPON_WALK_BOB_INTENSITY = 0.35f; // Walk bob
    const float WEAPON_WALK_BOB_SPEED = 1.2f;      // Walk bob speed
    
    // Rotation effects (for strafe tilt only - NO pitch offset)
    const float WEAPON_STRAFE_TILT = 0.027f;       // Subtle strafe tilt
    const float WEAPON_TURN_ROLL = 0.04f;          // Subtle turn roll
    
    // Smoothing - 2X SLOWER than before for smoother animations
    const float WEAPON_SMOOTH_SPEED = 0.03f;       // Ultra slow smoothing (was 0.06, now HALF)
    const float WEAPON_RECOVERY_SPEED = 0.015f;    // Ultra slow recovery (was 0.03, now HALF)
    
    // Movement tracking
    float prevyaw = 0, prevpitch = 0;
    
    // Camera history for position-based lag (200ms of history at 60fps = 12 frames, but we keep 30 for safety)
    const int CAMERA_HISTORY_SIZE = 30;
    struct CameraState {
        float yaw, pitch;
        int timestamp;
        CameraState() : yaw(0), pitch(0), timestamp(0) {}
    } camerahistory[CAMERA_HISTORY_SIZE];
    int camerahistoryindex = 0;
    
    float weaponoffsetx = 0, weaponoffsety = 0, weaponoffsetz = 0; // Position offsets (lateral, up/down from mouse, vertical from jump/land)
    float weaponoffsetforward = 0;                 // Forward/backward offset from movement
    float weaponoffsetz_movement = 0;              // Vertical offset from forward/backward movement
    float weaponroll = 0;                          // Weapon roll angle
    float landingimpact = 0;                       // Landing impact timer
    float jumpboost = 0;                           // Jump boost timer
    int lastphysstate = 0;
    float sprintbobphase = 0;                      // Sprint bob animation phase
    float prevvelx = 0, prevvely = 0, prevvelz = 0; // Previous velocity for acceleration detection

    void swayhudgun(int curtime)
    {
        fpsent *d = hudplayer();
        if(d->state == CS_SPECTATOR) return;
        
        float frametime = curtime / 1000.0f;
        float smoothfactor = pow(WEAPON_SMOOTH_SPEED, frametime * 60.0f); // Frame-rate independent (2x slower)
        float recoveryfactor = pow(WEAPON_RECOVERY_SPEED, frametime * 60.0f); // (2x slower)
        
        // ============================================================================
        // CAMERA RECOIL RECOVERY (for automatic weapons)
        // ============================================================================
        if(d->recoilPitchAccum != 0.0f || d->recoilYawAccum != 0.0f) {
            // Determine recovery rate based on current weapon
            float recoveryRate = 0.01f; // default
            if(d->gunselect == GUN_SMG) recoveryRate = 0.027f;
            else if(d->gunselect == GUN_CG) recoveryRate = 0.0216f;
            
            // Calculate recovery amount (degrees per frame)
            float recoveryAmount = recoveryRate * curtime;
            
            // Apply recovery to accumulated recoil (bring towards 0)
            if(d->recoilPitchAccum > 0) {
                float recovery = min(recoveryAmount, d->recoilPitchAccum);
                d->recoilPitchAccum -= recovery;
                d->pitch -= recovery;
            } else if(d->recoilPitchAccum < 0) {
                float recovery = min(recoveryAmount, -d->recoilPitchAccum);
                d->recoilPitchAccum += recovery;
                d->pitch += recovery;
            }
            
            if(d->recoilYawAccum > 0) {
                float recovery = min(recoveryAmount, d->recoilYawAccum);
                d->recoilYawAccum -= recovery;
                d->yaw -= recovery;
            } else if(d->recoilYawAccum < 0) {
                float recovery = min(recoveryAmount, -d->recoilYawAccum);
                d->recoilYawAccum += recovery;
                d->yaw += recovery;
            }
            
            // Clamp to zero when very close
            if(fabs(d->recoilPitchAccum) < 0.01f) d->recoilPitchAccum = 0.0f;
            if(fabs(d->recoilYawAccum) < 0.01f) d->recoilYawAccum = 0.0f;
        }
        
        // ============================================================================
        // CLASSIC SWAY SYSTEM (velocity-based)
        // ============================================================================
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
        
        // ============================================================================
        // RECORD CAMERA HISTORY (for position-based lag)
        // ============================================================================
        camerahistoryindex = (camerahistoryindex + 1) % CAMERA_HISTORY_SIZE;
        camerahistory[camerahistoryindex].yaw = d->yaw;
        camerahistory[camerahistoryindex].pitch = d->pitch;
        camerahistory[camerahistoryindex].timestamp = lastmillis;
        
        // ============================================================================
        // FIND CAMERA STATE FROM 200MS AGO (position lag)
        // ============================================================================
        int targettime = lastmillis - WEAPON_LAG_DURATION_MS;
        float pastyaw = d->yaw;
        float pastpitch = d->pitch;
        
        // Search backwards through history to find closest timestamp
        int bestidx = camerahistoryindex;
        int mindiff = abs(camerahistory[camerahistoryindex].timestamp - targettime);
        
        for(int i = 0; i < CAMERA_HISTORY_SIZE; i++)
        {
            if(camerahistory[i].timestamp == 0) continue; // Not initialized yet
            int diff = abs(camerahistory[i].timestamp - targettime);
            if(diff < mindiff)
            {
                mindiff = diff;
                bestidx = i;
            }
        }
        
        pastyaw = camerahistory[bestidx].yaw;
        pastpitch = camerahistory[bestidx].pitch;
        
        // ============================================================================
        // CALCULATE POSITION OFFSET FROM LAG (current vs past)
        // ============================================================================
        float deltayaw = d->yaw - pastyaw;
        float deltapitch = d->pitch - pastpitch;
        
        // Handle yaw wraparound
        if(deltayaw > 180.0f) deltayaw -= 360.0f;
        if(deltayaw < -180.0f) deltayaw += 360.0f;
        
        // Target offset based on how much camera has moved since past
        // Weapon position is "behind" where we were looking
        float targetoffsetx = -deltayaw * WEAPON_LAG_AMOUNT;  // Horizontal offset
        float targetoffsety = -deltapitch * WEAPON_LAG_AMOUNT; // Vertical offset (up/down, NOT pitch!)
        
        // Smooth interpolation (slow for visible drag)
        weaponoffsetx = weaponoffsetx * smoothfactor + targetoffsetx * (1.0f - smoothfactor);
        weaponoffsety = weaponoffsety * smoothfactor + targetoffsety * (1.0f - smoothfactor);
        
        // ============================================================================
        // MOVEMENT-BASED EFFECTS (Velocity & acceleration)
        // ============================================================================
        float horizontalspeed = sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y);
        float verticalspeed = d->vel.z;
        
        // Calculate acceleration for dynamic effects
        float accelx = (d->vel.x - prevvelx) / max(frametime, 0.001f);
        float accely = (d->vel.y - prevvely) / max(frametime, 0.001f);
        float accelz = (d->vel.z - prevvelz) / max(frametime, 0.001f);
        float accelhoriz = sqrtf(accelx*accelx + accely*accely);
        
        // ============================================================================
        // MOVEMENT-BASED POSITION OFFSETS (Forward/backward, strafe)
        // ============================================================================
        vec viewdir, rightdir;
        vecfromyawpitch(d->yaw, 0, 1, 0, viewdir);
        rightdir = vec(-viewdir.y, viewdir.x, 0);
        
        float strafevel = rightdir.dot(vec(d->vel.x, d->vel.y, 0));
        float forwardvel = viewdir.dot(vec(d->vel.x, d->vel.y, 0));
        
        // Target forward/backward offset based on movement (horizontal)
        // Only apply when moving BACKWARD (negative velocity)
        float targetforward = forwardvel < 0 ? forwardvel * 0.015f : 0;
        weaponoffsetforward = weaponoffsetforward * smoothfactor + targetforward * (1.0f - smoothfactor);
        
        // Target vertical offset based on backward movement (LOWERS weapon)
        // Only apply when moving BACKWARD (negative velocity) - weapon dips down
        float targetz_movement = forwardvel < 0 ? forwardvel * 0.02f : 0;
        weaponoffsetz_movement = weaponoffsetz_movement * smoothfactor + targetz_movement * (1.0f - smoothfactor);
        
        // Target roll based on strafe
        float targetroll = -strafevel * WEAPON_STRAFE_TILT;
        
        // Add roll for fast camera turns
        targetroll += deltayaw * WEAPON_TURN_ROLL;
        
        // Smooth interpolation (2x slower)
        weaponroll = weaponroll * smoothfactor + targetroll * (1.0f - smoothfactor);
        
        // ============================================================================
        // SPRINT BOB (up/down motion - added to position offset)
        // ============================================================================
        bool isMoving = horizontalspeed > 10.0f;
        bool isSprinting = isMoving && horizontalspeed > d->maxspeed * 0.7f;
        
        float targetbobz = 0;
        
        if(d->physstate >= PHYS_SLOPE && isMoving)
        {
            // Update bob phase
            float bobspeed = isSprinting ? WEAPON_SPRINT_BOB_SPEED : WEAPON_WALK_BOB_SPEED;
            sprintbobphase += frametime * horizontalspeed * 0.03f * bobspeed;
            sprintbobphase = fmod(sprintbobphase, M_PI * 2.0f);
            
            float bobintensity = isSprinting ? WEAPON_SPRINT_BOB_INTENSITY : WEAPON_WALK_BOB_INTENSITY;
            
            // Vertical bob pattern (up/down)
            targetbobz = sin(sprintbobphase) * bobintensity;
        }
        else
        {
            sprintbobphase *= 0.9f; // Gradually reset phase
        }
        
        // Apply bob to Z offset with 2x slower smoothing
        float bobtarget = targetbobz;
        static float currentbob = 0;
        currentbob = currentbob * smoothfactor + bobtarget * (1.0f - smoothfactor);
        weaponoffsetz += currentbob; // Add to existing Z offset (jump/land)
        
        // ============================================================================
        // JUMP & LANDING EFFECTS (vertical position offset)
        // ============================================================================
        
        // Detect jump (going from ground to air with upward velocity)
        if(lastphysstate >= PHYS_SLOPE && d->physstate < PHYS_SLOPE && verticalspeed > 50.0f)
        {
            // JUMP! Weapon raises up
            jumpboost = WEAPON_JUMP_RISE;
        }
        
        // Detect landing (going from air to ground)
        if(lastphysstate < PHYS_SLOPE && d->physstate >= PHYS_SLOPE)
        {
            // LAND! Weapon drops down
            float fallspeed = max(0.0f, -prevvelz);
            landingimpact = min(fallspeed * 0.015f, 1.0f) * WEAPON_LAND_DROP;
        }
        
        // Smooth recovery from jump boost (3x slower than original)
        if(jumpboost > 0)
        {
            jumpboost *= pow(0.95f, frametime * 60.0f); // Much slower (was 0.92, original 0.85)
            if(jumpboost < 0.01f) jumpboost = 0;
        }
        
        // Smooth recovery from landing impact (3x slower than original)
        if(landingimpact > 0)
        {
            landingimpact *= pow(0.96f, frametime * 60.0f); // Much slower (was 0.94, original 0.88)
            if(landingimpact < 0.01f) landingimpact = 0;
        }
        
        // Apply vertical offset (jump raises, landing lowers) - this is a BASE offset
        weaponoffsetz = jumpboost - landingimpact;
        
        // ============================================================================
        // STORE STATE FOR NEXT FRAME
        // ============================================================================
        prevyaw = d->yaw;
        prevpitch = d->pitch;
        prevvelx = d->vel.x;
        prevvely = d->vel.y;
        prevvelz = d->vel.z;
        lastphysstate = d->physstate;
    }

    struct hudent : dynent
    {
        hudent() { type = ENT_CAMERA; }
    } guninterp;

    SVARP(hudgunsdir, "");
    int hudgunpitch;
    int jump;
    int dir;
    float hudgunoffset = 0.0f; // Backward recoil offset for weapon model
    
    void drawhudmodel(fpsent *d, int anim, float speed = 0, int base = 0)
    {
        if(d->gunselect>GUN_PISTOL) return;

        // ============================================================================
        // WEAPON POSITIONING - Sway + Lag + Jump/Land
        // ============================================================================
        
        // Start with classic sway
        vec sway;
        vecfromyawpitch(d->yaw, 0, 0, 1, sway);
        float steps = swaydist/swaystep*M_PI;
        sway.mul(swayside*cosf(steps));
        sway.z = swayup*(fabs(sinf(steps)) - 1);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway) sway = d->o;
        
        vec weaponpos = sway;
        
        if(hudgunsway)
        {
            // ============================================================================
            // POSITION-BASED LAG - Weapon position offset from camera movement
            // ============================================================================
            vec rightdir, updir, forwarddir;
            vecfromyawpitch(d->yaw + 90, 0, 1, 0, rightdir);  // Right vector
            vecfromyawpitch(d->yaw, 0, 1, 0, forwarddir);      // Forward vector (horizontal)
            updir = vec(0, 0, 1); // World up
            
            // === HORIZONTAL LAG (Left/Right from mouse movement) ===
            weaponpos.add(vec(rightdir).mul(weaponoffsetx * 0.02f));
            
            // === VERTICAL LAG (Up/Down from mouse movement) ===
            weaponpos.add(vec(updir).mul(weaponoffsety * 0.02f));
            
            // === FORWARD/BACKWARD OFFSET (from movement velocity) ===
            weaponpos.add(vec(forwarddir).mul(-weaponoffsetforward * 0.3f)); // Negative = pushed back when moving forward
            
            // === VERTICAL POSITION (Jump/Landing/Bob + Movement) ===
            weaponpos.z += weaponoffsetz * 0.3f;                    // Jump/landing/bob
            weaponpos.z += weaponoffsetz_movement * 0.5f;           // Forward movement raises weapon
        }
        
        // Apply backward recoil offset for SMG only (not Pulse Rifle)
        if(d->gunselect == GUN_SMG || d->gunselect == GUN_SMG2) {
            vec recoilOffset;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, recoilOffset);
            recoilOffset.mul(hudgunoffset);
            weaponpos.add(recoilOffset);
        }
        
        // Slight base offset (right and forward for proper weapon positioning)
        vec right, forward;
        vecfromyawpitch(d->yaw + 90, 0, 1, 0, right);
        vecfromyawpitch(d->yaw, 0, 1, 0, forward);
        weaponpos.add(vec(right).mul(0.5f));
        weaponpos.add(vec(forward).mul(0.2f));

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
        
        // Backward recoil animation for SMG only (not Pulse Rifle)
        if(lastmillis-d->lastaction<3 && (d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2)) {
            // Kick gun backward on shot - subtle but visible
            float kickAmount = -1.5f; // Halved again (negative = backward)
            hudgunoffset += kickAmount;
            // Clamp maximum backward displacement
            if(hudgunoffset < -6.0f) hudgunoffset = -6.0f; // Halved - prevents disappearing
        }
        // Smooth forward recovery - ULTRA SLOW for beautiful, deliberate animation
        if(hudgunoffset < 0.0f) {
            hudgunoffset += 0.1f; // Return forward at 0.1 units per frame (very slow!)
            if(hudgunoffset > 0.0f) hudgunoffset = 0.0f;
        }
        // Reset to normal position after not firing
        if(lastmillis-d->lastaction>200 && lastmillis-d->lastswitch>200 && (d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2)) {
            hudgunoffset = 0.0f;
        }
        
        if(lastmillis-d->lastaction<3 && (d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_CG || d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2 || d->gunselect==GUN_ELECTRO) && (lastmillis-d->lastaction)<10)jump=1;
        // SMG - subtle upward jump with backward slide
        // CG - 2X upward jump (no backward slide)
        // Shotguns - SAME HEIGHT but slower animation
        if(jump && (d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_CG || d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2 || d->gunselect==GUN_ELECTRO))hudgunpitch+=(d->gunselect==GUN_SMG||d->gunselect==GUN_SMG2)?0.5:(d->gunselect==GUN_CG?1.0:(d->gunselect==GUN_SHOTGUN2?10:(d->gunselect==GUN_SG?8:1)));
        if((d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_CG || d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2 || d->gunselect==GUN_ELECTRO) && jump && hudgunpitch>d->pitch+((d->gunselect==GUN_SMG||d->gunselect==GUN_SMG2)?2.5:(d->gunselect==GUN_CG?5.0:(d->gunselect==GUN_SHOTGUN2?36:(d->gunselect==GUN_SG?32:20)))))jump=0;
        // During backward recoil animation, always snap pitch instantly to camera
        if(hudgunoffset < 0.0f && (d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2)) {
            // Backward recoil is active - weapon pitch should ALWAYS track camera instantly
            hudgunpitch = d->pitch;
        }
        else if(hudgunpitch<d->pitch-2 && !jump)hudgunpitch+=2; //raise hudgun to normal pos in case player looked up during firing
        else if(hudgunpitch>d->pitch+2 && !jump)hudgunpitch-=(d->gunselect==GUN_SMG||d->gunselect==GUN_SMG2)?1.5:(d->gunselect==GUN_CG?3.0:(d->gunselect==GUN_SG||d->gunselect==GUN_SHOTGUN2?0.1875:2));
        // Constantly track towards current pitch during sustained fire to fix model position bug
        if(lastmillis-d->lastaction<100&&lastmillis-d->lastswitch>200&&(d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_CG || d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2 || d->gunselect==GUN_ELECTRO)) {
            if(hudgunpitch < d->pitch - 2) hudgunpitch += 1;
            else if(hudgunpitch > d->pitch + 2) hudgunpitch -= 1;
        }
        if(lastmillis-d->lastaction>350&&lastmillis-d->lastswitch>200&&(d->gunselect==GUN_MAGNUM || d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2 || d->gunselect==GUN_CG || d->gunselect==GUN_SG || d->gunselect==GUN_SHOTGUN2 || d->gunselect==GUN_ELECTRO))hudgunpitch=d->pitch;
        if(lastmillis-d->lastswitch>200&&(d->gunselect!=GUN_MAGNUM && d->gunselect!=GUN_SMG && d->gunselect!=GUN_SMG2 && d->gunselect!=GUN_CG && d->gunselect!=GUN_SG && d->gunselect!=GUN_SHOTGUN2 && d->gunselect!=GUN_ELECTRO))hudgunpitch=d->pitch;
        if(lastmillis-d->lastreload<1300)dir=30;
        else dir=90;
        
        // ============================================================================
        // WEAPON ROTATION - Preserves recoil animation!
        // ============================================================================
        // Start with hudgunpitch which contains recoil animation
        float finalPitch = testhudgun ? 0 : ((hudgunpitch<d->pitch - 2 || hudgunpitch>d->pitch + 2) ? hudgunpitch : d->pitch);
        float finalYaw = testhudgun ? 0 : d->yaw + dir;
        
        // At the EXACT moment of firing (first frame only), snap to aim for perfect accuracy
        // But PRESERVE hudgunpitch so recoil animation can play immediately after!
        bool isFirstFrameOfFire = lastmillis - d->lastaction < 3; // Only first 3ms (basically 1 frame)
        
        if(isFirstFrameOfFire)
        {
            // Firing frame: weapon points exactly at camera for perfect initial accuracy
            // This corrects any movement-based position offsets for the firing moment
            finalPitch = testhudgun ? 0 : d->pitch;
            finalYaw = testhudgun ? 0 : d->yaw + dir;
            // Recoil will kick in on subsequent frames via hudgunpitch
        }
        else if(hudgunsway && !testhudgun)
        {
            // NOT firing: apply weapon roll (strafe tilt + turn roll)
            // hudgunpitch is already in finalPitch, so recoil animation plays
            float rollEffect = weaponroll * 6.0f; // Convert to degrees
            finalPitch += rollEffect * 0.4f; // Roll affects pitch slightly
            finalYaw += rollEffect * 0.25f;  // Roll affects yaw slightly
        }
        
        rendermodel(NULL, gunname, anim, weaponpos, finalYaw, finalPitch, MDL_LIGHT, interp, a, base, (int)ceil(speed));
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
        if(!(d->mods & MOD_LIGHTNINGGUN) && d->gunselect == GUN_MAGNUM)rtime = 0;
        if(d->mods & MOD_LIGHTNINGGUN && d->gunselect == GUN_MAGNUM)rtime = 1300;
        if(d->gunselect==GUN_SHOTGUN2)rtime=250;
        if(d->gunselect==GUN_SMG || d->gunselect==GUN_SMG2)rtime=0;
        if(d->gunselect==GUN_ELECTRO2 || d->gunselect==GUN_ELECTRO)rtime=75;
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

