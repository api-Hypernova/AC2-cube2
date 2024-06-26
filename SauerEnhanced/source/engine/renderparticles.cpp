// renderparticles.cpp

#include "engine.h"
#include "rendertarget.h"

Shader *particleshader = NULL, *particlenotextureshader = NULL;

VARP(particlesize, 20, 100, 500);
VARP(seweather, 0, 1, 1);

// Check emit_particles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VARP(emitmillis, 1, 17, 1000);
static int lastemitframe = 0, emitoffset = 0;
static bool canemit = false, regenemitters = false, canstep = false;

static bool emit_particles()
{
    if(reflecting || refracting) return false;
    return canemit || emitoffset;
}

VAR(dbgpseed, 0, 0, 1);

struct particleemitter
{
    extentity *ent;
    vec bbmin, bbmax;
    vec center;
    float radius;
    ivec bborigin, bbsize;
    int maxfade, lastemit, lastcull;

    particleemitter(extentity *ent)
        : ent(ent), bbmin(ent->o), bbmax(ent->o), maxfade(-1), lastemit(0), lastcull(0)
    {}

    void finalize()
    {
        center = vec(bbmin).add(bbmax).mul(0.5f);
        radius = bbmin.dist(bbmax)/2;
        bborigin = ivec(int(floor(bbmin.x)), int(floor(bbmin.y)), int(floor(bbmin.z)));
        bbsize = ivec(int(ceil(bbmax.x)), int(ceil(bbmax.y)), int(ceil(bbmax.z))).sub(bborigin);
        if(dbgpseed) conoutf(CON_DEBUG, "radius: %f, maxfade: %d", radius, maxfade);
    }

    void extendbb(const vec &o, float size = 0)
    {
        bbmin.x = min(bbmin.x, o.x - size);
        bbmin.y = min(bbmin.y, o.y - size);
        bbmin.z = min(bbmin.z, o.z - size);
        bbmax.x = max(bbmax.x, o.x + size);
        bbmax.y = max(bbmax.y, o.y + size);
        bbmax.z = max(bbmax.z, o.z + size);
    }

    void extendbb(float z, float size = 0)
    {
        bbmin.z = min(bbmin.z, z - size);
        bbmax.z = max(bbmax.z, z + size);
    }
};

static vector<particleemitter> emitters;
static particleemitter *seedemitter = NULL;

void clearparticleemitters()
{
    emitters.shrink(0);
    regenemitters = true;
}

void addparticleemitters()
{
    emitters.shrink(0);
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_PARTICLES) continue;
        emitters.add(particleemitter(&e));
    }
    regenemitters = false;
}

enum
{
    PT_PART = 0,
    PT_TAPE,
    PT_TRAIL,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS,
    PT_FIREBALL,
    PT_LIGHTNING,
    PT_FLARE,

    PT_MOD   = 1<<8,
    PT_RND4  = 1<<9,
    PT_LERP  = 1<<10, // use very sparingly - order of blending issues
    PT_TRACK = 1<<11,
    PT_GLARE = 1<<12,
    PT_SOFT  = 1<<13,
    PT_HFLIP = 1<<14,
    PT_VFLIP = 1<<15,
    PT_ROT   = 1<<16,
    PT_CULL  = 1<<17,
    PT_FEW   = 1<<18,
    PT_ICON  = 1<<19,
    PT_FLIP  = PT_HFLIP | PT_VFLIP | PT_ROT
};

const char *partnames[] = { "part", "tape", "trail", "text", "textup", "meter", "metervs", "fireball", "lightning", "flare" };

struct particle
{
    vec o, d;
    int gravity, fade, millis, grow;
    bvec color;
    uchar flags;
    float size;
    bool fastsplash, fixedfade;
    union
    {
        const char *text;         // will call delete[] on this only if it starts with an @
        float val;
        physent *owner;
        struct
        {
            uchar color2[3];
            uchar progress;
        };
    };
};

struct partvert
{
    vec pos;
    float u, v;
    bvec color;
    uchar alpha;
};

#define COLLIDERADIUS 8.0f
#define COLLIDEERROR 1.0f

struct partrenderer
{
    Texture *tex;
    const char *texname;
    int texclamp;
    uint type;
    int collide;

    partrenderer(const char *texname, int texclamp, int type, int collide = 0)
        : tex(NULL), texname(texname), texclamp(texclamp), type(type), collide(collide)
    {
    }
    partrenderer(int type, int collide = 0)
        : tex(NULL), texname(NULL), texclamp(0), type(type), collide(collide)
    {
    }
    virtual ~partrenderer()
    {
    }

    virtual void init(int n) { }
    virtual void reset() = 0;
    virtual void resettracked(physent *owner) { }
    virtual particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity = 0, int grow = 0) = NULL;
    virtual int adddepthfx(vec &bbmin, vec &bbmax) { return 0; }
    virtual void update() { }
    virtual void render() = 0;
    virtual bool haswork() = 0;
    virtual int count() = 0; //for debug
    virtual bool usesvertexarray() { return false; }
    virtual void cleanup() {}

    virtual void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
    }

    //blend = 0 => remove it
    void calc(particle *p, int &blend, int &ts, vec &o, vec &d, bool step = true)
    {
        o = p->o;
        d = p->d;
        float speed = gamespeed/100;
        if(type&PT_TRACK && p->owner) game::particletrack(p->owner, o, d);
        if(p->fade <= 5)
        {
            ts = 1;
            blend = 255;
        }
        else
        {
            if(p->grow)
            {
                switch(p->grow)
                {
                    case 1: //504
                        p->size += 0.01f*speed;
                        break;
                    case 2: //501
                        p->size += 0.05f*speed;
                        break;
                    case 3: //502
                        p->size += 0.1f*speed;
                        break;
                    case 4: //503
                        p->size += 2.0f*speed;
                        break;
                    case 5:
                        p->size += 16.0f*speed;
                        break;
                }
            }
            ts = lastmillis-p->millis;
            blend = max(255 - (ts<<8)/p->fade, 0);
            if(p->fastsplash)
            {
                if(ts > p->fade) ts = p->fade;
                float t = ts;
                o.add(vec(d).mul(t/500.0f));
                o.z -= t*t/(2.0f * 500.0f * 10000);
            }
            else if(p->gravity)
            {
                if(ts > p->fade) ts = p->fade;
                float t = ts;
                o.add(vec(d).mul(t/5000.0f));
                o.z -= t*t/(2.0f * 5000.0f * p->gravity);
            }
            if(o.z < p->val && step)
            {
                vec surface;
                float floorz = rayfloor(vec(o.x, o.y, p->val), surface, RAY_CLIPMAT, COLLIDERADIUS);
                float collidez = floorz<0 ? o.z-COLLIDERADIUS : p->val - rayfloor(vec(o.x, o.y, p->val), surface, RAY_CLIPMAT, COLLIDERADIUS);
                if(o.z >= collidez+COLLIDEERROR)
                    p->val = collidez+COLLIDEERROR;
                else
                {
                    if(collide && collide<77) adddecal(collide, vec(o.x, o.y, collidez), vec(p->o).sub(o).normalize(), 2*p->size, p->color, type&PT_RND4 ? (p->flags>>5)&3 : 0);
                    else if(collide && collide>=77 && o.dist(camera1->o)<=32) particle_explodesplash(o, 50, PART_BULLET, 0x3333FF, 0.1f, 250, 4); //rain splash
                    blend = 0;
                }
            }
        }
    }
};

struct listparticle : particle
{
    listparticle *next;
};

VARP(outlinemeters, 0, 0, 1);

struct listrenderer : partrenderer
{
    static listparticle *parempty;
    listparticle *list;

    listrenderer(const char *texname, int texclamp, int type, int collide = 0)
        : partrenderer(texname, texclamp, type, collide), list(NULL)
    {
    }
    listrenderer(int type, int collide = 0)
        : partrenderer(type, collide), list(NULL)
    {
    }

    virtual ~listrenderer()
    {
    }

    virtual void cleanup(listparticle *p)
    {
    }

    void reset()
    {
        if(!list) return;
        listparticle *p = list;
        for(;;)
        {
            cleanup(p);
            if(p->next) p = p->next;
            else break;
        }
        p->next = parempty;
        parempty = list;
        list = NULL;
    }

    void resettracked(physent *owner)
    {
        if(!(type&PT_TRACK)) return;
        for(listparticle **prev = &list, *cur = list; cur; cur = *prev)
        {
            if(!owner || cur->owner==owner)
            {
                *prev = cur->next;
                cur->next = parempty;
                parempty = cur;
            }
            else prev = &cur->next;
        }
    }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity, int grow)
    {
        if(!parempty)
        {
            listparticle *ps = new listparticle[256];
            loopi(255) ps[i].next = &ps[i+1];
            ps[255].next = parempty;
            parempty = ps;
        }
        listparticle *p = parempty;
        parempty = p->next;
        p->next = list;
        list = p;
        p->o = o;
        p->d = d;
        p->gravity = gravity;
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec(color>>16, (color>>8)&0xFF, color&0xFF);
        p->size = size;
        p->owner = NULL;
        p->flags = 0;
        return p;
    }

    int count()
    {
        int num = 0;
        listparticle *lp;
        for(lp = list; lp; lp = lp->next) num++;
        return num;
    }

    bool haswork()
    {
        return (list != NULL);
    }

    virtual void startrender() = 0;
    virtual void endrender() = 0;
    virtual void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts, uchar *color) = 0;

    void render()
    {
        startrender();
        if(texname)
        {
            if(!tex) tex = textureload(texname, texclamp);
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }

        for(listparticle **prev = &list, *p = list; p; p = *prev)
        {
            vec o, d;
            int blend, ts;
            calc(p, blend, ts, o, d, canstep);
            if(blend > 0)
            {
                renderpart(p, o, d, blend, ts, p->color.v);

                if(p->fade > 5 || !canstep)
                {
                    prev = &p->next;
                    continue;
                }
            }
            //remove
            *prev = p->next;
            p->next = parempty;
            cleanup(p);
            parempty = p;
        }

        endrender();
    }
};

listparticle *listrenderer::parempty = NULL;

struct meterrenderer : listrenderer
{
    meterrenderer(int type)
        : listrenderer(type)
    {}

    void startrender()
    {
         glDisable(GL_BLEND);
         glDisable(GL_TEXTURE_2D);
         particlenotextureshader->set();
    }

    void endrender()
    {
         glEnable(GL_BLEND);
         glEnable(GL_TEXTURE_2D);
         particleshader->set();
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts, uchar *color)
    {
        int basetype = type&0xFF;

        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        glRotatef(camera1->yaw, 0, 0, 1);
        glRotatef(camera1->pitch-90, 1, 0, 0);

        float scale = p->size/80.0f;
        glScalef(-scale, scale, -scale);

        float right = 8*FONTH, left = p->progress/100.0f*right;
        glTranslatef(-right/2.0f, 0, 0);

        if(outlinemeters)
        {
            glColor3f(0, 0.8f, 0);
            glBegin(GL_TRIANGLE_STRIP);
            loopk(10)
            {
                float c = (0.5f + 0.1f)*sinf(k/9.0f*M_PI), s = 0.5f - (0.5f + 0.1f)*cosf(k/9.0f*M_PI);
                glVertex2f(-c*FONTH, s*FONTH);
                glVertex2f(right + c*FONTH, s*FONTH);
            }
            glEnd();
        }

        if(basetype==PT_METERVS) glColor3ubv(p->color2);
        else glColor3f(0, 0, 0);
        glBegin(GL_TRIANGLE_STRIP);
        loopk(10)
        {
            float c = 0.5f*sinf(k/9.0f*M_PI), s = 0.5f - 0.5f*cosf(k/9.0f*M_PI);
            glVertex2f(left + c*FONTH, s*FONTH);
            glVertex2f(right + c*FONTH, s*FONTH);
        }
        glEnd();

        if(outlinemeters)
        {
            glColor3f(0, 0.8f, 0);
            glBegin(GL_TRIANGLE_FAN);
            loopk(10)
            {
                float c = (0.5f + 0.1f)*sinf(k/9.0f*M_PI), s = 0.5f - (0.5f + 0.1f)*cosf(k/9.0f*M_PI);
                glVertex2f(left + c*FONTH, s*FONTH);
            }
            glEnd();
        }

        glColor3ubv(color);
        glBegin(GL_TRIANGLE_STRIP);
        loopk(10)
        {
            float c = 0.5f*sinf(k/9.0f*M_PI), s = 0.5f - 0.5f*cosf(k/9.0f*M_PI);
            glVertex2f(-c*FONTH, s*FONTH);
            glVertex2f(left + c*FONTH, s*FONTH);
        }
        glEnd();


        glPopMatrix();
    }
};
static meterrenderer meters(PT_METER|PT_LERP), metervs(PT_METERVS|PT_LERP);

struct textrenderer : listrenderer
{
    textrenderer(int type)
        : listrenderer(type)
    {}

    void startrender()
    {
    }

    void endrender()
    {
    }

    void cleanup(listparticle *p)
    {
        if(p->text && p->flags&1) delete[] p->text;
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts, uchar *color)
    {
        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);

        glRotatef(camera1->yaw, 0, 0, 1);
        glRotatef(camera1->pitch-90, 1, 0, 0);

        float scale = p->size/80.0f;
        glScalef(-scale, scale, -scale);

        float xoff = -text_width(p->text)/2;
        float yoff = 0;
        if((type&0xFF)==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); } //@TODO instead in worldspace beforehand?
        glTranslatef(xoff, yoff, 50);

        draw_text(p->text, 0, 0, color[0], color[1], color[2], blend);

        glPopMatrix();
    }
};
static textrenderer texts(PT_TEXT|PT_LERP);

template<int T>
static inline void modifyblend(const vec &o, int &blend)
{
    blend = min(blend<<2, 255);
}

template<>
inline void modifyblend<PT_TAPE>(const vec &o, int &blend)
{
}

template<int T>
static inline void genpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs)
{
    vec udir = vec(camup).sub(camright).mul(size);
    vec vdir = vec(camup).add(camright).mul(size);
    vs[0].pos = vec(o.x + udir.x, o.y + udir.y, o.z + udir.z);
    vs[1].pos = vec(o.x + vdir.x, o.y + vdir.y, o.z + vdir.z);
    vs[2].pos = vec(o.x - udir.x, o.y - udir.y, o.z - udir.z);
    vs[3].pos = vec(o.x - vdir.x, o.y - vdir.y, o.z - vdir.z);
}

template<>
inline void genpos<PT_TAPE>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec dir1 = d, dir2 = d, c;
    dir1.sub(o);
    dir2.sub(camera1->o);
    c.cross(dir2, dir1).normalize().mul(size);
    vs[0].pos = vec(d.x-c.x, d.y-c.y, d.z-c.z);
    vs[1].pos = vec(o.x-c.x, o.y-c.y, o.z-c.z);
    vs[2].pos = vec(o.x+c.x, o.y+c.y, o.z+c.z);
    vs[3].pos = vec(d.x+c.x, d.y+c.y, d.z+c.z);
}

template<>
inline void genpos<PT_TRAIL>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec e = d;
    if(grav) e.z -= float(ts)/grav;
    e.div(grav==201?-200.0f:100.0f).add(o);
    genpos<PT_TAPE>(o, e, size, ts, grav, vs);
}

template<int T>
static inline void genrotpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    genpos<T>(o, d, size, grav, ts, vs);
}

#define ROTCOEFFS(n) { \
    vec(-1,  1, 0).rotate_around_z(n*2*M_PI/32.0f), \
    vec( 1,  1, 0).rotate_around_z(n*2*M_PI/32.0f), \
    vec( 1, -1, 0).rotate_around_z(n*2*M_PI/32.0f), \
    vec(-1, -1, 0).rotate_around_z(n*2*M_PI/32.0f) \
}
static const vec rotcoeffs[32][4] =
{
    ROTCOEFFS(0),  ROTCOEFFS(1),  ROTCOEFFS(2),  ROTCOEFFS(3),  ROTCOEFFS(4),  ROTCOEFFS(5),  ROTCOEFFS(6),  ROTCOEFFS(7),
    ROTCOEFFS(8),  ROTCOEFFS(9),  ROTCOEFFS(10), ROTCOEFFS(11), ROTCOEFFS(12), ROTCOEFFS(13), ROTCOEFFS(14), ROTCOEFFS(15),
    ROTCOEFFS(16), ROTCOEFFS(17), ROTCOEFFS(18), ROTCOEFFS(19), ROTCOEFFS(20), ROTCOEFFS(21), ROTCOEFFS(22), ROTCOEFFS(7),
    ROTCOEFFS(24), ROTCOEFFS(25), ROTCOEFFS(26), ROTCOEFFS(27), ROTCOEFFS(28), ROTCOEFFS(29), ROTCOEFFS(30), ROTCOEFFS(31),
};

template<>
inline void genrotpos<PT_PART>(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    const vec *coeffs = rotcoeffs[rot];
    (vs[0].pos = o).add(vec(camright).mul(coeffs[0].x*size)).add(vec(camup).mul(coeffs[0].y*size));
    (vs[1].pos = o).add(vec(camright).mul(coeffs[1].x*size)).add(vec(camup).mul(coeffs[1].y*size));
    (vs[2].pos = o).add(vec(camright).mul(coeffs[2].x*size)).add(vec(camup).mul(coeffs[2].y*size));
    (vs[3].pos = o).add(vec(camright).mul(coeffs[3].x*size)).add(vec(camup).mul(coeffs[3].y*size));
}

template<int T>
static inline void seedpos(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    if(grav)
    {
        vec end(o);
        float t = fade;
        end.add(vec(d).mul(t/5000.0f));
        end.z -= t*t/(2.0f * 5000.0f * grav);
        pe.extendbb(end, size);

        float tpeak = d.z*grav;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }
}

template<>
inline void seedpos<PT_TAPE>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    pe.extendbb(d, size);
}

template<>
inline void seedpos<PT_TRAIL>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    vec e = d;
    if(grav) e.z -= float(fade)/grav;
    e.div(-200.0f).add(o);
    pe.extendbb(e, size);
}

template<int T>
struct varenderer : partrenderer
{
    partvert *verts;
    particle *parts;
    int maxparts, numparts, lastupdate, rndmask;

    varenderer(const char *texname, int type, int collide = 0)
        : partrenderer(texname, 3, type, collide),
          verts(NULL), parts(NULL), maxparts(0), numparts(0), lastupdate(-1), rndmask(0)
    {
        if(type & PT_HFLIP) rndmask |= 0x01;
        if(type & PT_VFLIP) rndmask |= 0x02;
        if(type & PT_ROT) rndmask |= 0x1F<<2;
        if(type & PT_RND4) rndmask |= 0x03<<5;
    }

    void init(int n)
    {
        DELETEA(parts);
        DELETEA(verts);
        parts = new particle[n];
        verts = new partvert[n*4];
        maxparts = n;
        numparts = 0;
        lastupdate = -1;
    }

    void reset()
    {
        numparts = 0;
        lastupdate = -1;
    }

    void resettracked(physent *owner)
    {
        if(!(type&PT_TRACK)) return;
        loopi(numparts)
        {
            particle *p = parts+i;
            if(!owner || (p->owner == owner)) p->fade = -1;
        }
        lastupdate = -1;
    }

    int count()
    {
        return numparts;
    }

    bool haswork()
    {
        return (numparts > 0);
    }

    bool usesvertexarray() { return true; }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity, int grow = 0)
    {
        particle *p = parts + (numparts < maxparts ? numparts++ : rnd(maxparts)); //next free slot, or kill a random kitten
        p->o = o;
        p->d = d;
        p->gravity = gravity;
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec(color>>16, (color>>8)&0xFF, color&0xFF);
        p->size = size;
        p->owner = NULL;
        p->flags = 0x80 | (rndmask ? rnd(0x80) & rndmask : 0);
        p->grow = grow;
        lastupdate = -1;
        return p;
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
        pe.maxfade = max(pe.maxfade, fade);
        size *= SQRT2;
        pe.extendbb(o, size);

        seedpos<T>(pe, o, d, fade, size, gravity);
        if(!gravity) return;

        vec end(o);
        float t = fade;
        end.add(vec(d).mul(t/5000.0f));
        end.z -= t*t/(2.0f * 5000.0f * gravity);
        pe.extendbb(end, size);

        float tpeak = d.z*gravity;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }

    void genverts(particle *p, partvert *vs, bool regen)
    {
        vec o, d;
        int blend, ts;

        calc(p, blend, ts, o, d);
        if(blend <= 1 || p->fade <= 5) p->fade = -1; //mark to remove on next pass (i.e. after render)

        modifyblend<T>(o, blend);

        if(regen)
        {
            p->flags &= ~0x80;

            #define SETTEXCOORDS(u1c, u2c, v1c, v2c, body) \
            { \
                float u1 = u1c, u2 = u2c, v1 = v1c, v2 = v2c; \
                body; \
                vs[0].u = u1; \
                vs[0].v = v1; \
                vs[1].u = u2; \
                vs[1].v = v1; \
                vs[2].u = u2; \
                vs[2].v = v2; \
                vs[3].u = u1; \
                vs[3].v = v2; \
            }
            if(type&PT_RND4)
            {
                float tx = 0.5f*((p->flags>>5)&1), ty = 0.5f*((p->flags>>6)&1);
                SETTEXCOORDS(tx, tx + 0.5f, ty, ty + 0.5f,
                {
                    if(p->flags&0x01) swap(u1, u2);
                    if(p->flags&0x02) swap(v1, v2);
                });
            }
            else if(type&PT_ICON)
            {
                float tx = 0.25f*(p->flags&3), ty = 0.25f*((p->flags>>2)&3);
                SETTEXCOORDS(tx, tx + 0.25f, ty, ty + 0.25f, {});
            }
            else SETTEXCOORDS(0, 1, 0, 1, {});

            #define SETCOLOR(r, g, b, a) \
            do { \
                uchar col[4] = { r, g, b, a }; \
                loopi(4) memcpy(vs[i].color.v, col, sizeof(col)); \
            } while(0)
            #define SETMODCOLOR SETCOLOR((p->color[0]*blend)>>8, (p->color[1]*blend)>>8, (p->color[2]*blend)>>8, 255)
            if(type&PT_MOD) SETMODCOLOR;
            else SETCOLOR(p->color[0], p->color[1], p->color[2], blend);
        }
        else if(type&PT_MOD) SETMODCOLOR;
        else loopi(4) vs[i].alpha = blend;

        if(type&PT_ROT) genrotpos<T>(o, d, p->size, ts, p->gravity, vs, (p->flags>>2)&0x1F);
        else genpos<T>(o, d, p->size, ts, p->gravity, vs);
    }

    void update()
    {
        if(lastmillis == lastupdate) return;
        lastupdate = lastmillis;

        loopi(numparts)
        {
            particle *p = &parts[i];
            partvert *vs = &verts[i*4];
            if(p->fade < 0)
            {
                do
                {
                    --numparts;
                    if(numparts <= i) return;
                }
                while(parts[numparts].fade < 0);
                *p = parts[numparts];
                genverts(p, vs, true);
            }
            else genverts(p, vs, (p->flags&0x80)!=0);
        }
    }

    void render()
    {
        if(!tex) tex = textureload(texname, texclamp);
        if(!strcmp(texname, "packages/particles/glow.png") && parts->size != 5.1f ) glDisable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glVertexPointer(3, GL_FLOAT, sizeof(partvert), &verts->pos);
        glTexCoordPointer(2, GL_FLOAT, sizeof(partvert), &verts->u);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(partvert), &verts->color);
        glDrawArrays(GL_QUADS, 0, numparts*4);
        if(!strcmp(texname, "packages/particles/glow.png") && parts->size != 5.1f) glEnable(GL_DEPTH_TEST);
    }
};
typedef varenderer<PT_PART> quadrenderer;
typedef varenderer<PT_TAPE> taperenderer;
typedef varenderer<PT_TRAIL> trailrenderer;

#include "depthfx.h"
#include "explosion.h"
#include "lensflare.h"
#include "lightning.h"

struct softquadrenderer : quadrenderer
{
    softquadrenderer(const char *texname, int type, int collide = 0)
        : quadrenderer(texname, type|PT_SOFT, collide)
    {
    }

    int adddepthfx(vec &bbmin, vec &bbmax)
    {
        if(!depthfxtex.highprecision() && !depthfxtex.emulatehighprecision()) return 0;
        int numsoft = 0;
        loopi(numparts)
        {
            particle &p = parts[i];
            float radius = p.size*SQRT2;
            vec o, d;
            int blend, ts;
            calc(&p, blend, ts, o, d, false);
            if(!isfoggedsphere(radius, p.o) && (depthfxscissor!=2 || depthfxtex.addscissorbox(p.o, radius)))
            {
                numsoft++;
                loopk(3)
                {
                    bbmin[k] = min(bbmin[k], o[k] - radius);
                    bbmax[k] = max(bbmax[k], o[k] + radius);
                }
            }
        }
        return numsoft;
    }
};

static partrenderer *parts[] =
{
    new quadrenderer("<grey>packages/particles/blood.png", PT_PART|PT_FLIP|PT_MOD|PT_RND4, DECAL_BLOOD), // blood spats (note: rgb is inverted)
    new trailrenderer("packages/particles/base.png", PT_TRAIL|PT_LERP),                            // water, entity
    new quadrenderer("<grey>packages/particles/smoke.png", PT_PART|PT_FLIP|PT_LERP),                     // smoke
    new quadrenderer("<grey>packages/particles/steam.png", PT_PART|PT_FLIP),                             // steam
    new quadrenderer("<grey>packages/particles/flames.png", PT_PART|PT_HFLIP|PT_RND4|PT_GLARE),          // flame on - no flipping please, they have orientation
    new quadrenderer("packages/particles/ball1.png", PT_PART|PT_FEW|PT_GLARE),                     // fireball1
    new quadrenderer("packages/particles/ball2.png", PT_PART|PT_FEW|PT_GLARE),                     // fireball2
    new quadrenderer("packages/particles/ball3.png", PT_PART|PT_FEW|PT_GLARE),                     // fireball3
    new taperenderer("packages/particles/flare.jpg", PT_TAPE|PT_GLARE),                            // streak
    &lightnings,                                                                                   // lightning
    &fireballs,                                                                                    // explosion fireball
    &bluefireballs,                                                                                // bluish explosion fireball
    &plasmafireballs,
    new quadrenderer("packages/particles/spark.png", PT_PART|PT_FLIP|PT_GLARE),                    // sparks
    new quadrenderer("packages/particles/base.png",  PT_PART|PT_FLIP|PT_GLARE),                    // edit mode entities
    new quadrenderer("packages/particles/muzzleflash1.jpg", PT_PART|PT_FEW|PT_FLIP|PT_TRACK|PT_RND4), // muzzle flash
    new quadrenderer("packages/particles/muzzleflash2.jpg", PT_PART|PT_FEW|PT_FLIP|PT_GLARE|PT_TRACK), // muzzle flash
    new quadrenderer("packages/particles/muzzleflash3.jpg", PT_PART|PT_FEW|PT_FLIP|PT_GLARE|PT_TRACK), // muzzle flash
    new quadrenderer("packages/particles/muzzleflash4.jpg", PT_PART|PT_FEW|PT_TRACK), // muzzle flash
    new quadrenderer("packages/hud/items.png", PT_PART|PT_FEW|PT_ICON),                            // hud icon
    new quadrenderer("<colorify:1/1/1>packages/hud/items.png", PT_PART|PT_FEW|PT_ICON),            // grey hud icon
    &texts,                                                                                        // text
    &meters,                                                                                       // meter
    &metervs,                                                                                      // meter vs.
    &flares,
    new quadrenderer("packages/particles/flame1.png", PT_PART|PT_FLIP),
    new quadrenderer("packages/particles/flame2.png", PT_PART|PT_FLIP),
    new quadrenderer("packages/particles/flame3.png", PT_PART|PT_FLIP),
    new quadrenderer("packages/particles/flame4.png", PT_PART|PT_FLIP),
    new quadrenderer("packages/particles/snow.png", PT_PART|PT_GLARE|PT_FLIP), // snow
    new trailrenderer("packages/particles/rain.png", PT_TRAIL|PT_LERP, 77),
    new trailrenderer("packages/particles/flare.png", PT_TRAIL),
    new trailrenderer("packages/particles/lightflare.png", PT_TRAIL|PT_GLARE),
    new trailrenderer("packages/particles/tele.png", PT_TRAIL),                  // teleport
    new quadrenderer("packages/particles/glow.png", PT_PART, 78),
    new taperenderer("packages/particles/lightflare.png", PT_TAPE|PT_GLARE),
    new quadrenderer("packages/particles/bubble.jpg", PT_PART|PT_GLARE),
    new quadrenderer("packages/particles/explode.jpg", PT_PART|PT_GLARE),
    new taperenderer("packages/particles/smoketrail.png", PT_TAPE|PT_GLARE),                                                                                        // lens flares - must be done last
    new taperenderer("packages/particles/railtrail.jpg", PT_TAPE|PT_GLARE),
    new quadrenderer("<grey>packages/particles/rail.png", PT_PART|PT_FLIP|PT_LERP)
};

void finddepthfxranges()
{
    depthfxmin = vec(1e16f, 1e16f, 1e16f);
    depthfxmax = vec(0, 0, 0);
    numdepthfxranges = fireballs.finddepthfxranges(depthfxowners, depthfxranges, 0, MAXDFXRANGES, depthfxmin, depthfxmax);
    numdepthfxranges = bluefireballs.finddepthfxranges(depthfxowners, depthfxranges, numdepthfxranges, MAXDFXRANGES, depthfxmin, depthfxmax);
    numdepthfxranges = plasmafireballs.finddepthfxranges(depthfxowners, depthfxranges, numdepthfxranges, MAXDFXRANGES, depthfxmin, depthfxmax);
    loopk(3)
    {
        depthfxmin[k] -= depthfxmargin;
        depthfxmax[k] += depthfxmargin;
    }
    if(depthfxparts)
    {
        loopi(sizeof(parts)/sizeof(parts[0]))
        {
            partrenderer *p = parts[i];
            if(p->type&PT_SOFT && p->adddepthfx(depthfxmin, depthfxmax))
            {
                if(!numdepthfxranges)
                {
                    numdepthfxranges = 1;
                    depthfxowners[0] = NULL;
                    depthfxranges[0] = 0;
                }
            }
        }
    }
    if(depthfxscissor<2 && numdepthfxranges>0) depthfxtex.addscissorbox(depthfxmin, depthfxmax);
}

VARFP(maxparticles, 10, 4000, 40000, particleinit());
VARFP(fewparticles, 10, 100, 40000, particleinit());

void particleinit()
{
    if(!particleshader) particleshader = lookupshaderbyname("particle");
    if(!particlenotextureshader) particlenotextureshader = lookupshaderbyname("particlenotexture");
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->init(parts[i]->type&PT_FEW ? min(fewparticles, maxparticles) : maxparticles);
}

void clearparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->reset();
    clearparticleemitters();
}

void cleanupparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->cleanup();
}

void removetrackedparticles(physent *owner)
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->resettracked(owner);
}

VARP(particleglare, 0, 2, 100);

VAR(debugparticles, 0, 0, 1);

void renderparticles(bool mainpass, bool glowonly)
{
    canstep = mainpass;
    //want to debug BEFORE the lastpass render (that would delete particles)
    if(debugparticles && !glaring && !reflecting && !refracting)
    {
        int n = sizeof(parts)/sizeof(parts[0]);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, FONTH*n*2*screen->w/float(screen->h), FONTH*n*2, 0, -1, 1); //squeeze into top-left corner
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        defaultshader->set();
        loopi(n)
        {
            if(glowonly) continue;
            int type = parts[i]->type;
            const char *title = parts[i]->texname ? strrchr(parts[i]->texname, '/')+1 : NULL;
            string info = "";
            if(type&PT_GLARE) concatstring(info, "g,");
            if(type&PT_LERP) concatstring(info, "l,");
            if(type&PT_MOD) concatstring(info, "m,");
            if(type&PT_RND4) concatstring(info, "r,");
            if(type&PT_TRACK) concatstring(info, "t,");
            if(type&PT_FLIP) concatstring(info, "f,");
            if(parts[i]->collide) concatstring(info, "c,");
            if(info[0]) info[strlen(info)-1] = '\0';
            defformatstring(ds)("%d\t%s(%s) %s", parts[i]->count(), partnames[type&0xFF], info, title ? title : "");
            draw_text(ds, FONTH, (i+n/2)*FONTH);
        }
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    if(glaring && !particleglare) return;

    loopi(sizeof(parts)/sizeof(parts[0]))
    {

        if(glaring && !(parts[i]->type&PT_GLARE)) continue;
        if(glowonly && parts[i]->collide!=78) continue;
        if(!glowonly && parts[i]->collide==78) continue;
        parts[i]->update();
    }

    static float zerofog[4] = { 0, 0, 0, 1 };
    float oldfogc[4];
    bool rendered = false;
    uint lastflags = PT_LERP, flagmask = PT_LERP|PT_MOD;

    if(binddepthfxtex()) flagmask |= PT_SOFT;

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        partrenderer *p = parts[i];
        if(glaring && !(p->type&PT_GLARE)) continue;
        if(!p->haswork()) continue;
        if(glowonly && p->collide!=78) continue;
        if(!glowonly && p->collide==78) continue;

        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if(glaring) setenvparamf("colorscale", SHPARAM_VERTEX, 4, particleglare, particleglare, particleglare, 1);
            else setenvparamf("colorscale", SHPARAM_VERTEX, 4, 1, 1, 1, 1);

            particleshader->set();
            glGetFloatv(GL_FOG_COLOR, oldfogc);
        }

        uint flags = p->type & flagmask;
        if(p->usesvertexarray()) flags |= 0x01; //0x01 = VA marker
        uint changedbits = (flags ^ lastflags);
        if(changedbits != 0x0000)
        {
            if(changedbits&0x01)
            {
                if(flags&0x01)
                {
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glEnableClientState(GL_COLOR_ARRAY);
                }
                else
                {
                    glDisableClientState(GL_VERTEX_ARRAY);
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                    glDisableClientState(GL_COLOR_ARRAY);
                }
            }
            if(changedbits&PT_LERP) glFogfv(GL_FOG_COLOR, (flags&PT_LERP) ? oldfogc : zerofog);
            if(changedbits&(PT_LERP|PT_MOD))
            {
                if(flags&PT_LERP) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                else if(flags&PT_MOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            if(changedbits&PT_SOFT)
            {
                if(flags&PT_SOFT)
                {
                    if(depthfxtex.target==GL_TEXTURE_RECTANGLE_ARB)
                    {
                        if(!depthfxtex.highprecision()) SETSHADER(particlesoft8rect);
                        else SETSHADER(particlesoftrect);
                    }
                    else
                    {
                        if(!depthfxtex.highprecision()) SETSHADER(particlesoft8);
                        else SETSHADER(particlesoft);
                    }

                    binddepthfxparams(depthfxpartblend);
                }
                else particleshader->set();
            }
            lastflags = flags;
        }
        p->render();
    }

    if(rendered)
    {
        if(lastflags&(PT_LERP|PT_MOD)) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        if(!(lastflags&PT_LERP)) glFogfv(GL_FOG_COLOR, oldfogc);
        if(lastflags&0x01)
        {
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
        }
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

static int addedparticles = 0;

particle *newparticle(const vec &o, const vec &d, int fade, int type, int color, float size, int gravity = 0, int grow = 0)
{
    static particle dummy;
    if(seedemitter)
    {
        parts[type]->seedemitter(*seedemitter, o, d, fade, size, gravity);
        return &dummy;
    }
    if(fade + emitoffset < 0) return &dummy;
    addedparticles++;
    return parts[type]->addpart(o, d, fade, color, size, gravity, grow);
}

#define maxparticledistance 4096

static void splash(int type, int color, int radius, int num, int fade, const vec &p, float size, int gravity, bool regfade, int flag, bool fastsplash, int grow)
{
    if(camera1->o.dist(p) > maxparticledistance && !seedemitter) return;
    float collidez = parts[type]->collide ? p.z - raycube(p, vec(0, 0, -1), COLLIDERADIUS, RAY_CLIPMAT) + COLLIDEERROR : -1;
    int fmin = 1;
    int fmax = fade*3;
    loopi(num)
    {
        int x, y, z;
        switch(flag)
        {
            case 1:
                do
                {
                    x = (rnd(radius*2)-radius)/4;
                    y = (rnd(radius*2)-radius)/4;
                    z = 40-(rnd(10));
                }
                while(x*x+y*y+z*z>radius*radius);
                break;
            case 2:
                do
                {
                    x = (rnd(radius*2)-radius)/4;
                    y = (rnd(radius*2)-radius)/4;
                    z = 20-(rnd(5));
                }
                while(x*x+y*y+z*z>radius*radius);
                break;
            default:
                do
                {
                    x = rnd(radius*2)-radius;
                    y = rnd(radius*2)-radius;
                    z = rnd(radius*2)-radius;
                }
                while(x*x+y*y+z*z>radius*radius);
                break;
        }
    	vec tmp = vec((float)x, (float)y, (float)z);
        int f = (num < 10) ? (fmin + rnd(fmax)) : (fmax - (i*(fmax-fmin))/(num-1)); //help deallocater by using fade distribution rather than random
        particle *np = newparticle(p, tmp, regfade?fade:f, type, color, size, gravity);
        np->val = collidez;
        np->fastsplash = fastsplash;
        np->grow = grow;
    }
}

static void regularsplash(int type, int color, int radius, int num, int fade, const vec &p, float size, int gravity, int delay = 0, bool regfade = false, int flag = 0, int grow = 0)
{
    if(!emit_particles() /*|| (delay > 0 && rnd(delay) != 0))*/) return;
    splash(type, color, radius, num, fade, p, size, gravity, regfade, flag, false, grow);
}

void regularshape(int type, int radius, int color, int dir, int num, int fade, const vec &p, float size, int gravity, float vel, vec windoffset, bool weather, bool weather2, bool b);

void particle_explodesplash(const vec &o, int fade, int type, int color, float size, int gravity, int num)
{
    regularshape(type, 16, color, 22, num, fade, o, size, gravity, 0, vec(0, 0, 0), false, false, false);
}

void particle_flying_flare(const vec &o, const vec &d, int fade, int type, int color, float size, int gravity)
{
    newparticle(o, d, fade, type, color, size, gravity);
}

bool canaddparticles()
{
    return !renderedgame && !shadowmapping;
}

void regular_particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int gravity, int delay, bool hover, int grow)
{
    if(!canaddparticles()) return;
    if(hover)
        splash(type, color, radius, num, fade, p, size, gravity, false, 2, false, grow);
    else
        regularsplash(type, color, radius, num, fade, p, size, gravity, delay, false, 0, grow);
}

void particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int gravity, bool regfade, int flag, bool fastsplash, int grow)
{
    if(!canaddparticles()) return;
    splash(type, color, radius, num, fade, p, size, gravity, regfade, flag, fastsplash, grow);
}

VARP(maxtrail, 1, 500, 10000);

void particle_trail(int type, int fade, const vec &s, const vec &e, int color, float size, int gravity, bool bubbles)
{
    if(!canaddparticles()) return;
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*2), 1, maxtrail);
    v.div(steps);
    vec p = s;
    loopi(steps)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        if(lookupmaterial(p)==MAT_WATER && (bubbles || type==PART_SMOKE))
            newparticle(p, tmp, rnd(250)+250, PART_BUBBLE, 0xFFFFFF, 0.1f, 500);
        else if(!bubbles && lookupmaterial(p)==MAT_WATER && type==PART_SMOKE)
            continue;
        else if(!bubbles)
            newparticle(p, tmp, rnd(fade)+fade, type, color, size, gravity);
    }
}

VARP(particletext, 0, 1, 1);
VARP(maxparticletextdistance, 0, 128, 10000);

void particle_text(const vec &s, const char *t, int type, int fade, int color, float size, int gravity)
{
    if(!canaddparticles()) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->text = t;
}

void particle_textcopy(const vec &s, const char *t, int type, int fade, int color, float size, int gravity)
{
    if(!canaddparticles()) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->text = newstring(t);
    p->flags = 1;
}

void particle_icon(const vec &s, int ix, int iy, int type, int fade, int color, float size, int gravity)
{
    if(!canaddparticles()) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->flags |= ix | (iy<<2);
}

void particle_meter(const vec &s, float val, int type, int fade, int color, int color2, float size)
{
    if(!canaddparticles()) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size);
    p->color2[0] = color2>>16;
    p->color2[1] = (color2>>8)&0xFF;
    p->color2[2] = color2&0xFF;
    p->progress = clamp(int(val*100), 0, 100);
}

void particle_flare(const vec &p, const vec &dest, int fade, int type, int color, float size, physent *owner, int grow)
{
    //if(!canaddparticles()) return;
    particle *np = newparticle(p, dest, fade, type, color, size);
    np->owner = owner;
    np->grow = grow;
    np->fastsplash = false;
}

void particle_fireball(const vec &dest, float maxsize, int type, int fade, int color, float size)
{
    if(!canaddparticles()) return;
    float growth = maxsize - size;
    if(fade < 0) fade = int(growth*25);
    newparticle(dest, vec(0, 0, 1), fade, type, color, size)->val = growth;
}

//dir = 0..6 where 0=up
static inline vec offsetvec(vec o, int dir, int dist)
{
    vec v = vec(o);
    v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

//converts a 16bit color to 24bit
static inline int colorfromattr(int attr)
{
    return (((attr&0xF)<<4) | ((attr&0xF0)<<8) | ((attr&0xF00)<<12)) + 0x0F0F0F;
}

VAR(debugweather, 0, 0, 1);

/* Experiments in shapes...
 * dir: (where dir%3 is similar to offsetvec with 0=up)
 * 0..2 circle
 * 3.. 5 cylinder shell
 * 6..11 cone shell
 * 12..14 plane volume
 * 15..20 line volume, i.e. wall
 * 21 sphere
 * +32 to inverse direction
 */
void regularshape(int type, int radius, int color, int dir, int num, int fade, const vec &p, float size, int gravity, float vel = 0, vec windoffset = vec(0, 0, 0), bool weather = false, bool weather2 = false, bool b = false)
{
    if(!emit_particles()) return;

    int basetype = parts[type]->type&0xFF;
    bool flare = (basetype == PT_TAPE) || (basetype == PT_LIGHTNING),
         inv = (dir&0x20)!=0, taper = (dir&0x40)!=0 && !seedemitter;
    dir &= 0x1F;
    loopi(num)
    {
        vec to, from;
        if(dir < 12)
        {
            float a = PI2*float(rnd(1000))/1000.0;
            to[dir%3] = sinf(a)*radius;
            to[(dir+1)%3] = cosf(a)*radius;
            to[(dir+2)%3] = 0.0;
            to.add(p);
            if(dir < 3) //circle
                from = p;
            else if(dir < 6) //cylinder
            {
                from = to;
                to[(dir+2)%3] += radius;
                from[(dir+2)%3] -= radius;
            }
            else //cone
            {
                from = p;
                to[(dir+2)%3] += (dir < 9)?radius:(-radius);
            }
        }
        else if(dir < 15) //plane
        {
            vec camadd;
            if(weather && b)
            {
                camadd = vec(camera1->o.x, camera1->o.y, camera1->o.z+512);
                radius = 128;

            }

            to[dir%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            to[(dir+1)%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            to[(dir+2)%3] = radius;
            to.add(b ? camadd : p);
            from = to;
            from[(dir+2)%3] -= 2*radius;
        }
        else if(dir < 21) //line
        {
            if(dir < 18)
            {
                to[dir%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
                to[(dir+1)%3] = 0.0;
            }
            else
            {
                to[dir%3] = 0.0;
                to[(dir+1)%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            }
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
            to[(dir+2)%3] += radius;
        }
        else //sphere
        {
            to = vec(PI2*float(rnd(1000))/1000.0, PI*float(rnd(1000)-500)/1000.0).mul(radius);
            to.add(p);
            from = p;
        }


        if(weather)
        {
            //The new, optimized weather code! :D --Q009

            vec toz(to.x, to.y, camera1->o.z);
            if(camera1->o.dist(toz) > 128 && !seedemitter) continue;

            vec c = camera1->o;
            float z = c.z+32;
            vec bottom(c.x, c.y, c.z+177);
            vec spawnz(to.x, to.y, z);

            float distToFloor = raycube(to, vec(0, 0, -1), 0, RAY_CLIPMAT);
            float floorHeight = (to.z - distToFloor);

            if(debugweather) conoutf("to.z: %f | floorH: %f | dist: %f", to.z, floorHeight, distToFloor);

            if(z<=floorHeight) continue;

            vec d(spawnz);
            d.sub(bottom);
            float totalvel = type==PART_RAIN?(vel + -camera1->falling.z*4):vel;

            if(windoffset.x)
            {
                if(windoffset.x>0)
                    d.add(vec(windoffset.x/2 + rnd(int(windoffset.x)), 0, 0));
                else if(windoffset.x<0)
                {
                    d.sub(vec(-windoffset.x/2 + rnd(int(-windoffset.x)), 0, 0));
                }
            }

            if(windoffset.y)
            {
                if(windoffset.y>0)
                    d.add(vec(0, windoffset.y/2 + rnd(int(windoffset.y)), 0));
                else if(windoffset.y<0)
                    d.sub(vec(0, -windoffset.y/2 + rnd(int(-windoffset.y)), 0));
            }

            d.normalize().mul(to.dist(camera1->o)<=radius?-totalvel:totalvel);
            particle *np = newparticle(spawnz, d, type==PART_RAIN?1000:5000, type, color, size, gravity);
            np->fixedfade = true;
            np->val = floorHeight;
        }
        else if(weather2)
        {
            int random = rnd(400);
            if(random == 250)
            {
                vec pos;
                pos.x = (p.x-(radius/2))+rndscale(radius);
                pos.y = (p.y-(radius/2))+rndscale(radius);
                pos.z = (p.z-(radius/2))+rndscale(radius);
                newparticle(pos, pos, fade, type, color, size, gravity);
            }
        }
        else
        {
            if(taper)
            {
                vec o = inv ? to : from;
                o.sub(camera1->o);
                float dist = clamp(sqrtf(o.x*o.x + o.y*o.y)/maxparticledistance, 0.0f, 1.0f);
                if(dist > 0.2f)
                {
                    dist = 1 - (dist - 0.2f)/0.8f;
                    if(rnd(0x10000) > dist*dist*0xFFFF) continue;
                }
            }

            if(flare)
                newparticle(inv?to:from, inv?from:to, rnd(fade*3)+1, type, color, size, gravity)->fastsplash = false;
            else
            {
                vec d(to);
                d.sub(from);
                d.normalize().mul(inv ? -200.0f : 200.0f); //velocity
                newparticle(inv?to:from, d, rnd(fade*3)+1, type, color, size, gravity);
            }
        }
    }
}

static void regularflame(int type, const vec &p, float radius, float height, int color, int density = 3, float scale = 2.0f, float speed = 200.0f, float fade = 600.0f, int gravity = -15)
{
    if(!emit_particles()) return;

    float size = scale * min(radius, height);
    vec v(0, 0, min(1.0f, height)*speed);
    loopi(density)
    {
        vec s = p;
        s.x += rndscale(radius*2.0f)-radius;
        s.y += rndscale(radius*2.0f)-radius;
        newparticle(s, v, rnd(max(int(fade*height), 1))+1, type, color, size, gravity);
    }
}

VARP(seglow, 0, 1, 1);

void regular_particle_flame(int type, const vec &p, float radius, float height, int color, int density, float scale, float speed, float fade, int gravity)
{
    if(!canaddparticles()) return;
    regularflame(type, p, radius, height, color, density, scale, speed, fade, gravity);
}

static void makeparticles(entity &e)
{
    switch(e.attr1)
    {
        int type;
        float size;
        int gravity;
        int dir;
        case 0: //fire and smoke -  <radius> <height> <rgb> - 0 values default to compat for old maps
        {
            //regularsplash(PART_FIREBALL1, 0xFFC8C8, 150, 1, 40, e.o, 4.8f);
            //regularsplash(PART_SMOKE, 0x897661, 50, 1, 200,  vec(e.o.x, e.o.y, e.o.z+3.0f), 2.4f, -20, 3);
            float radius = e.attr2 ? float(e.attr2)/100.0f : 1.5f,
                  height = e.attr3 ? float(e.attr3)/100.0f : radius/3;
            regularflame(PART_FLAME, e.o, radius, height, e.attr4 ? colorfromattr(e.attr4) : 0x903020, 3, 2.0f);
            regularflame(PART_SMOKE, vec(e.o.x, e.o.y, e.o.z + 4.0f*min(radius, height)), radius, height, 0x303020, 1, 4.0f, 100.0f, 2000.0f, -20);

            vec occlusioncheck;
            vec pos(e.o.x, e.o.y, e.o.z+(e.attr3/14));
            if(raycubelos(pos, camera1->o, occlusioncheck) && seglow )
                particle_flare(pos, pos, 1, PART_GLOW, 0x903020, (radius*8)+rndscale(5), NULL);
            break;
        }
        case 1: //steam vent - <dir>
            regularsplash(PART_STEAM, 0x897661, 50, 1, 200, offsetvec(e.o, e.attr2, rnd(10)), 2.4f, -20);
            break;
        case 2: //water fountain - <dir>
        {
            int color = (int(waterfallcolor[0])<<16) | (int(waterfallcolor[1])<<8) | int(waterfallcolor[2]);
            if(!color) color = (int(watercolor[0])<<16) | (int(watercolor[1])<<8) | int(watercolor[2]);
            regularsplash(PART_WATER, color, 150, 4, 200, offsetvec(e.o, e.attr2, rnd(10)), 0.6f, 2);
            break;
        }
        case 3: //fire ball - <size> <rgb>
            {
            newparticle(e.o, vec(0, 0, 1), 1, PART_EXPLOSION, colorfromattr(e.attr3), 4.0f)->val = 1+e.attr2;
            break;
        }
        case 4:  //tape - <dir> <length> <rgb>
		{
            static const int typemap2[]   = { PART_STREAK, -1, -1, PART_LIGHTNING, -1, PART_STEAM, PART_WATER };
            static const float sizemap2[] = { 0.28f, 0.0f, 0.0f, 0.28f, 0.0f, 2.4f, 0.60f };
            static const int gravmap2[] = { 0, 0, 0, 0, 0, -20, 2 };
            type = typemap2[e.attr1-4];
            size = sizemap2[e.attr1-4];
            gravity = gravmap2[e.attr1-4];
            dir=e.attr2;
            dir &= 0x1F;
            if(dir < 15 && dir > 6 && seweather && e.attr3 >= 32)
            {
                regularshape(PART_RAIN,
                             e.attr3,
                             colorfromattr(!e.attr4 ? e.attr4=0xFFF : e.attr4),
                             44,
                             64,
                             0,
                             e.o,
                             0.1f,
                             201,
                             1000,
                             vec(20, 20, 0),
                             true);
            }
            else if(e.attr2 >= 256) regularshape(type, max(1+e.attr3, 1), colorfromattr(e.attr4), e.attr2-256, 5, 200, e.o, size, gravity);
            else newparticle(e.o, offsetvec(e.o, e.attr2, max(1+e.attr3, 0)), 1, type, colorfromattr(e.attr4), size, gravity);
            break;
		}
        case 7:  //lightning
        case 9:  //steam
        case 10: //water
        {
            static const int typemap[]   = { PART_STREAK, -1, -1, PART_LIGHTNING, -1, PART_STEAM, PART_WATER };
            static const float sizemap[] = { 0.28f, 0.0f, 0.0f, 1.0f, 0.0f, 2.4f, 0.60f };
            static const int gravmap[] = { 0, 0, 0, 0, 0, -20, 2 };
            type = typemap[e.attr1-4];
            size = sizemap[e.attr1-4];
            gravity = gravmap[e.attr1-4];
            if(e.attr2 >= 256) regularshape(type, max(1+e.attr3, 1), colorfromattr(e.attr4), e.attr2-256, 5, 200, e.o, size, gravity);
            else newparticle(e.o, offsetvec(e.o, e.attr2, max(1+e.attr3, 0)), 1, type, colorfromattr(e.attr4), size, gravity);
            break;
        }
        case 5: //meter, metervs - <percent> <rgb> <rgb2>
        case 6:
        {
            particle *p = newparticle(e.o, vec(0, 0, 1), 1, e.attr1==5 ? PART_METER : PART_METER_VS, colorfromattr(e.attr3), 2.0f);
            int color2 = colorfromattr(e.attr4);
            p->color2[0] = color2>>16;
            p->color2[1] = (color2>>8)&0xFF;
            p->color2[2] = color2&0xFF;
            p->progress = clamp(int(e.attr2), 0, 100);
            break;
        }
        case 11: // flame <radius> <height> <rgb> - radius=100, height=100 is the classic size
        {
            float radius = e.attr2 ? float(e.attr2)/100.0f : 1.5f;
            regularflame(PART_FLAME, e.o, float(e.attr2)/100.0f, float(e.attr3)/100.0f, colorfromattr(e.attr4), 3, 2.0f);
            vec occlusioncheck;
            vec pos(e.o.x, e.o.y, e.o.z+(e.attr3/14));
            if(raycubelos(pos, camera1->o, occlusioncheck) && seglow )
                particle_flare(pos, pos, 1, PART_GLOW, colorfromattr(e.attr4), (radius*8)+rndscale(5), NULL);
            break;
        }
        case 12: // smoke plume <radius> <height> <rgb>
            regularflame(PART_SMOKE, e.o, float(e.attr2)/100.0f, float(e.attr3)/100.0f, colorfromattr(e.attr4), 1, 4.0f, 100.0f, 2000.0f, -20);
            break;
        case 32: //lens flares - plain/sparkle/sun/sparklesun <red> <green> <blue>
        case 33:
        case 34:
        case 35:
            flares.addflare(e.o, e.attr2, e.attr3, e.attr4, (e.attr1&0x02)!=0, (e.attr1&0x01)!=0);
            break;
        case 77:    //snow
        case 78:    //rain
        {
            if(seweather)
            {
                regularshape(e.attr1==77 ? PART_SNOW : PART_RAIN,
                             0,
                             colorfromattr(e.attr4),
                             44,
                             e.attr5,
                             0,
                             e.o,
                             e.attr1 == 77 ? 0.25f : 0.1f,
                             e.attr1 == 77 ? 200   : 201,
                             e.attr1 == 77 ? 350   : 1000,
                             vec(e.attr2, e.attr3, 0),
                             true,
                             false,
                             true);
            }
            break;
        }
        case 79:    //glow
        {
            vec occlusioncheck;
            if(raycubelos(e.o, camera1->o, occlusioncheck) && seglow )
                particle_flare(e.o, e.o, 1, PART_GLOW, colorfromattr(e.attr2), e.attr4?(e.attr3+rndscale(5)):e.attr3, NULL);
            break;
        }
        case 80:    //rain fog
        {
            regularshape(PART_STEAM, e.attr2, colorfromattr(e.attr3), 0, 5, 5000+rnd(500), e.o, e.attr4, 0, NULL, vec(0, 0, 0), NULL, true);
            break;
        }
        default:
            if(editmode)    //SauerEnhanced: show this annoying text inside editmode instead of outside
            {
                defformatstring(ds)("particles %d?", e.attr1);
                particle_textcopy(e.o, ds, PART_TEXT, 1, 0x6496FF, 2.0f);
            }
            break;
    }
}

bool printparticles(extentity &e, char *buf)
{
    switch(e.attr1)
    {
        case 0: case 4: case 7: case 8: case 9: case 10: case 11: case 12: case 77: case 78:
            formatstring(buf)("%s %d %d %d 0x%.3hX %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        case 3: case 80:
            formatstring(buf)("%s %d %d 0x%.3hX %d %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        case 5: case 6:
            formatstring(buf)("%s %d %d 0x%.3hX 0x%.3hX %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        case 79:
            formatstring(buf)("%s %d 0x%.3hX %d %d %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
    }
    return false;
}

VARP(showparticles, 0, 1, 1);
VAR(cullparticles, 0, 1, 1);
VAR(replayparticles, 0, 1, 1);
VARN(seedparticles, seedmillis, 0, 3000, 10000);
VAR(dbgpcull, 0, 0, 1);

void seedparticles()
{
    renderprogress(0, "seeding particles");
    addparticleemitters();
    canemit = true;
    loopv(emitters)
    {
        particleemitter &pe = emitters[i];
        extentity &e = *pe.ent;
        seedemitter = &pe;
        for(int millis = 0; millis < seedmillis; millis += min(emitmillis, seedmillis/10))
            makeparticles(e);
        seedemitter = NULL;
        pe.lastemit = -seedmillis;
        pe.finalize();
    }
}

void updateparticles()
{
    if(regenemitters) addparticleemitters();

    if(lastmillis - lastemitframe >= emitmillis)
    {
        canemit = true;
        lastemitframe = lastmillis - (lastmillis%emitmillis);
    }
    else canemit = false;

    flares.makelightflares();

    if(!editmode || showparticles)
    {
        int emitted = 0, replayed = 0;
        addedparticles = 0;
        loopv(emitters)
        {
            particleemitter &pe = emitters[i];
            extentity &e = *pe.ent;
            if(e.o.dist(camera1->o) > maxparticledistance) { pe.lastemit = lastmillis; continue; }
            if(cullparticles && e.attr1 != 77 && e.attr1 != 78 && pe.maxfade >= 0)    //dont cull weather (SE)
            {
                if(isfoggedsphere(pe.radius, pe.center)) { pe.lastcull = lastmillis; continue; }
                if(pvsoccluded(pe.bborigin, pe.bbsize)) { pe.lastcull = lastmillis; continue; }
            }
            makeparticles(e);
            emitted++;
            if(replayparticles && pe.maxfade > 5 && pe.lastcull > pe.lastemit)
            {
                for(emitoffset = max(pe.lastemit + emitmillis - lastmillis, -pe.maxfade); emitoffset < 0; emitoffset += emitmillis)
                {
                    makeparticles(e);
                    replayed++;
                }
                emitoffset = 0;
            }
            pe.lastemit = lastmillis;
        }
        if(dbgpcull && (canemit || replayed) && addedparticles) conoutf(CON_DEBUG, "%d emitters, %d particles", emitted, addedparticles);
    }
    if(editmode) // show sparkly thingies for map entities in edit mode
    {
        const vector<extentity *> &ents = entities::getents();
        // note: order matters in this case as particles of the same type are drawn in the reverse order that they are added
        loopv(entgroup)
        {
            entity &e = *ents[entgroup[i]];
            particle_textcopy(e.o, entname(e), PART_TEXT, 1, 0xFF4B19, 2.0f);
        }
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type==ET_EMPTY) continue;
            particle_textcopy(e.o, entname(e), PART_TEXT, 1, 0x1EC850, 2.0f);
            regular_particle_splash(PART_EDIT, 2, 40, e.o, 0x3232FF, 0.32f*particlesize/100.0f);
        }
    }
}
