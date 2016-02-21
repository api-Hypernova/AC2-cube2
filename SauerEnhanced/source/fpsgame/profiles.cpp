#include "game.h"
#include "engine.h"

int lasttimeupdate = 0;

ICOMMAND(wut, "", (), conoutf(game::savedconfig()));

namespace game
{
    bool profileloaded = false;
    string curprofile;
    int stats[24];

/**
STATS LIST

0: total shoots
1: total damage dealt
2: total jumps
3: avg accuracy
4: total deaths
6: bots killed
7: unnameds killed
8: players killed (total)
9: flags scored (ctf)
10: flags scored (protect)
11: avg. acc. factor
12: times lol'd
13: quad damages taken
14: health boosts taken
15: ammo packs taken
16: health kits taken
17: players killed with chainsaw
18: times suiced

Total play time:

19: secs
20: mins
21: hours
22: days
23: weeks
**/

    void writestats()
    {
        if(!game::profileloaded) return;
        defformatstring(file)("profiles/%s/stats", game::curprofile);
        stream *f = openfile(file, "w");
        if(!f)
        {
            conoutf("error creating stats file");
            return;
        }
        loopi(24)
        {
            f->printf("stats[%d] = %d\n", i, game::stats[i]);
        }
        f->close();
    }

    void dotime()   //Time updating for 'Total play time' stats
    {
        if(totalmillis >= lasttimeupdate+1000)    //1 second interval
        {
            game::stats[19]++;  //Add one second

            if(game::stats[19] >= 60)   //After 60 secs
            {
                game::stats[19] = 0;    //Reset seconds
                game::stats[20]++;      //Add one minute
            }

            if(game::stats[20] >= 60)   //After 60 mins
            {
                game::stats[20] = 0;    //Reset mins
                game::stats[21]++;      //Add one hour
            }

            if(game::stats[21] >= 24)   //After 24 hours
            {
                game::stats[21] = 0;    //Reset hours
                game::stats[22]++;      //Add one day
            }

            if(game::stats[22] >= 7)    //After 7 days
            {
                game::stats[22] = 0;    //Reset days
                game::stats[23]++;      //Add one week
            }
            lasttimeupdate = totalmillis;
        }
    }
}

static string profilelist =           "profiles/list.cfg";

void createprofile(const char *name)
{
    if( strstr(name, "\\")  ||      //Check for invalid characters
        strstr(name, "/")   ||
        strstr(name, ":")   ||
        strstr(name, "*")   ||
        strstr(name, "?")   ||
        strstr(name, "\"")  ||
        strstr(name, "<")   ||
        strstr(name, ">")   ||
        strstr(name, "|") )
        {
            conoutf("\f2You used invalid characters! Please type another name in...");
            return;
        }

    defformatstring(profiledir   )("SauerEnhanced/profiles/%s",             name);
    defformatstring(videodir     )("SauerEnhanced/profiles/%s/videos",      name);
    defformatstring(screenshotdir)("SauerEnhanced/profiles/%s/screenshots", name);
    defformatstring(demodir      )("SauerEnhanced/profiles/%s/demos",       name);

    path(profilelist  );
    path(profiledir   );
    path(videodir     );
    path(screenshotdir);
    path(demodir      );

    createdir(profiledir);
    createdir(videodir);
    createdir(screenshotdir);
    createdir(demodir);

    stream *f = openfile(profilelist, "a");
    if(!f)
	{
		conoutf("Error while creating/opening profile list file.");
		return;
	}
	f->printf("%s\n", name);
	f->close();

    printf("created profile: %s\n", name);
    copystring(game::curprofile, name);
    filtertext(game::player1->name, name, false, MAXNAMELEN);
    writecfg();
    execfile(game::savedconfig());
    execfile("data/defaultexec.cfg");
    game::profileloaded = true;
    checkfornewseversion();
}

string profiles;
int numprofiles = 0;

void readprofilelist()
{
    char buf[32];
    stream *f = openfile(profilelist, "r");
    while(f->getline(buf, sizeof(buf)))
    {
        formatstring(profiles)("%s %s", profiles, buf);
    }
    f->close();
}

void readprofilenum()
{
    char buf[32];
    stream *f = openfile(profilelist, "r");
    if(!f)
    {
        numprofiles = 0;
    }
    else
    {
        while(f->getline(buf, sizeof(buf)))
        {
            numprofiles++;
        }
        f->close();
    }
}

void readstats();

void readprofile(const char *name)
{
    copystring(game::curprofile, name);
    execfile(game::savedconfig());
    execfile("data/defaultexec.cfg");
    game::profileloaded = true;
    printf("loaded profile: %s\n", name);
    readstats();
    checkfornewseversion();
}

void readstats()
{
    if(!game::profileloaded) return;
    char buf[64];   //Stats might be long after longer play and i'm afraid of that 32 won't be engough in that situtation
    int statsdata[2];
    defformatstring(file)("profiles/%s/stats", game::curprofile);
    stream *f = openfile(file, "r");
    if(!f)
    {
        conoutf("error opening stats file");
        return;
    }
    while(f->getline(buf, sizeof(buf)))
    {
        sscanf(buf, "stats[%d] = %d", &statsdata[0], &statsdata[1]);
        game::stats[statsdata[0]] = statsdata[1];
    }
    f->close();
}


COMMAND(createprofile, "s");
COMMAND(readprofile, "s");
ICOMMAND(getprofiles, "", (), { readprofilelist(); result(profiles); copystring(profiles, ""); } );
ICOMMAND(getprofilenum, "", (), { readprofilenum(); intret(numprofiles); numprofiles = 0; } );
ICOMMAND(onprofile, "", (), { intret(game::profileloaded?1:0); } );
ICOMMAND(loadedprofile, "", (), { result(game::curprofile); } );
ICOMMAND(getprofile, "", (), game::profileloaded?result(game::curprofile):result("no profile loaded"));
ICOMMAND(stats, "i", (int *i), intret(game::stats[*i]));
