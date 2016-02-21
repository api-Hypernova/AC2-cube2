SauerEnchanced

v1.5.5 BETA (SVN)

Source code is included. Check 'SauerEnhanced/src'.

Thanks to Quaker66 for help.

--BUGS--

If sometimes sound will not be played that probably means game is trying to play too much sounds at once. Try "/maxsoundsatonce 10" or higher value.
NOTE: 3D menus doesn't work anymore. Please be patient before i'll fix it...

--CHANGES--

FOR LATEST SVN CHANGES SEE changelog.txt

History:

v1.48 - Patch:
	-Show info about checking for updates when loading
	-Fixed glow particles visible thru models
	-More weather improvements
	-Added weapon icons to HUD
	-Some more ambient sounds
	-More fx for more maps (.SE files)
	-Some other HUD rework
	-Minor fixes
------
v1.47 - Patch:
	-Rewrote weather system code. Rain and snow won't be laggy anymore.
	-Moved some functions to main menu when connected for easier use.
	-More map effects (SE files).
------
v1.46 - Patch:
	-Fixed some vars not saving
	-Colour master modes in server list
------
v1.45 - Patch:
	-Allow to disable glow particles
	-Allow to enable old-style rifle trail.
------
v1.44 - Patch:
	-Delete unnecessary files after analysys
	-Downloaded files will be saved in Sauerbraten/seDownloads
------
v1.43 - BETA:
	Added "Check-For-New-Version".
	Added profile system.
	Added working news system.
	New, realistic bullet trails.
	Screenshots are saved in "screenshots" folder now and movies in "videos".
	Disabled annoying auto-sort for servers. Press "Sort servers" button to resort.
	Added heartbeat sound and bloody-screen when health is less than 26.
	Added bullet nearmiss sounds.
	Improved rain and snow.
	Removed screen fx.
	Remade teleport effect.
	Nicer spark effect.
	Light rays aka "Godrays".
	Full UI rework: HUD, GUI, loading screen and UI sounds.
	Rewrote SE file functions.
	Added frag notify sounds (Normal kill, nice-shot kill, team-kill).
	Added glow particles.

--EXTRA MAP EFFECTS--
How to use additional map effects and .se ent storage files.

RAIN AND SNOW:
Snow: 		/newent particles 77 <windX> <windY> <colour 0xRGB> <density>
Rain: 		/newent particles 78 <windX> <windY> <colour 0xRGB> <density>
Glow: 		/newent particles 79 <colour 0xRGB> <size> <flicker 0/1>
Rain fog: 	/newent particles 80 <radius> <colour 0xRGB> <size>

If there's default flare-rain on a map, SauerEnhanced automaticly replaces it with enhanced one.

ENTITY STACKING:
Allows to stack entites regulary
/stackent <number> <spacing> <direction>
Can bo undone (supports undo)

SOUND MUTERS:

Sound muters are entities that can mute ambient sounds with the same ID.
The linked mapsound's volume gets lower as you get closer to the muter.
Mapsound's ID is stored in fourth argument: /newent sound 255 255 255 <ID HERE> 1
Muter: /newent muter <silence radius> <fade radius> <id>

ADDING SOUNDS:

Just do /newent sound <SoundNum> <Radius1> <Radius2> <ID> 1

If 5th attribute wont higher than 0, sound entity won't be saved to .SE file.
You can also disable panning for that sound by setting that attriubute to 2.

To save extra map fx (sounds and particles): /writesefile

For debugging SE file saving/reading use the /sefiledbg var
You can save particles to SE file only if attr1 is higher than 76

You can edit SE files manually:
Just go to SauerEnhanced/ents/ and open .se file for specific map with Notepad or WordPad

Scheme:

ENT_# type attr1 attr2 attr3 attr4 attr5 posX posY posZ

types are:

5 - particles
6 - sound
8 - muter


--CUBESCRIPT EXTENSIONS--

(lastsaid)		- eg. /echo (lastsaid)	-this will return latest chat message
(lastsaidteam)	- eg. /echo (lastsaidteam)	-this will return latest teamchat message
(getprofile) 	- eg. /echo (getprofile)	-this will return current profile loaded
(getprofiles) 	- eg. /echo (getprofiles)	-this will list available profiles
(getprofilenum)	- eg. /echo (getprofilenum)	-this will return ammount of available profiles
(onprofile)	- eg. /echo (onprofile)		-this will return 1 if profile is loaded or 0 if not
(stats #) 	- eg. /echo (stats 15)		-this will return stats from slot #
(getseversion) 	- eg. /echo (getseversion	-this will return current SauerEnhanced version running
writeinfile <file> <text> <append? 1/0>	- eg. /writeinfile "blah.txt" "this is first line^nand second one"	-this will create a file and put/append text in there

END...


For full history check the 'changelog.txt' file.

--------------------------------Q009----