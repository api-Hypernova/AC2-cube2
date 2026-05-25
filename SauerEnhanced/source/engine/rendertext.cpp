#include "engine.h"

#pragma warning(push)
#pragma warning(disable: 4100 4244 4267 4456 4701 4702 4703 4127)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#pragma warning(pop)

static hashtable<const char *, font> fonts;
static font *fontdef = NULL;

font *curfont = NULL;

// ============================================================
// M18.5 — Runtime TTF font engine (stb_truetype)
// Supports UTF-8 multibyte sequences including Japanese/CJK.
// ============================================================

struct ttffont
{
    struct glyph
    {
        short ax, ay;       // atlas position (top-left of glyph bitmap)
        short bw, bh;       // bitmap dimensions
        short bx, by;       // bearing: pixel offset from cursor to glyph top-left
        float xadv;         // cursor advance
        float u0,v0,u1,v1;  // normalized atlas texcoords
    };

    stbtt_fontinfo info;
    uchar *filedata;
    float scale;
    int ascent, descent, linegap;
    int pixelsize;
    char *name;

    GLuint atlasTex;
    enum { AW = 1024, AH = 1024 };
    uchar *atlasData;   // RGBA, AW*AH*4
    int sx, sy, sh;     // shelf packer: current x, current shelf base-y, shelf height

    hashtable<int,glyph> cache;

    ttffont() : filedata(NULL), scale(1.0f), ascent(0), descent(0), linegap(0),
                pixelsize(24), name(NULL), atlasTex(0), atlasData(NULL), sx(0), sy(0), sh(0) {}
};

static hashtable<const char*, ttffont*> ttffonts;
ttffont *curttffont = NULL;

static int decode_utf8(const char *s, int &i)
{
    uchar c = (uchar)s[i];
    if(c < 0x80) { i++; return c; }
    if(c < 0xC0) { i++; return '?'; }
    if(c < 0xE0)
    {
        if(!s[i+1]) { i++; return '?'; }
        int cp = ((c & 0x1F) << 6) | ((uchar)s[i+1] & 0x3F);
        i += 2; return cp;
    }
    if(c < 0xF0)
    {
        if(!s[i+1] || !s[i+2]) { i++; return '?'; }
        int cp = ((c & 0x0F) << 12) | (((uchar)s[i+1] & 0x3F) << 6) | ((uchar)s[i+2] & 0x3F);
        i += 3; return cp;
    }
    if(!s[i+1] || !s[i+2] || !s[i+3]) { i++; return '?'; }
    int cp = ((c & 0x07) << 18) | (((uchar)s[i+1] & 0x3F) << 12) | (((uchar)s[i+2] & 0x3F) << 6) | ((uchar)s[i+3] & 0x3F);
    i += 4; return cp;
}

static ttffont::glyph *ttf_getglyph(ttffont *f, int cp)
{
    ttffont::glyph *g = f->cache.access(cp);
    if(g) return g;

    int adv, lsb;
    stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);

    int bx0, by0, bx1, by1;
    stbtt_GetCodepointBitmapBox(&f->info, cp, f->scale, f->scale, &bx0, &by0, &bx1, &by1);
    int bw = bx1 - bx0, bh = by1 - by0;

    ttffont::glyph &ng = f->cache[cp];
    ng.xadv = adv * f->scale;
    ng.bx = (short)bx0; ng.by = (short)by0;
    ng.bw = (short)bw;  ng.bh = (short)bh;
    ng.ax = 0; ng.ay = 0;
    ng.u0 = ng.v0 = ng.u1 = ng.v1 = 0;

    if(bw <= 0 || bh <= 0 || !f->atlasTex) return &ng;

    int pw = bw + 2, ph = bh + 2;
    if(f->sx + pw > ttffont::AW) { f->sx = 0; f->sy += f->sh + 1; f->sh = 0; }
    if(f->sy + ph > ttffont::AH) return &ng;  // atlas full

    int ax = f->sx + 1, ay = f->sy + 1;
    f->sx += pw;
    if(ph > f->sh) f->sh = ph;

    uchar *bmp = new uchar[bw * bh];
    stbtt_MakeCodepointBitmap(&f->info, bmp, bw, bh, bw, f->scale, f->scale, cp);

    for(int y = 0; y < bh; y++)
        for(int x = 0; x < bw; x++)
        {
            int idx = ((ay + y) * ttffont::AW + (ax + x)) * 4;
            f->atlasData[idx+0] = 255;
            f->atlasData[idx+1] = 255;
            f->atlasData[idx+2] = 255;
            f->atlasData[idx+3] = bmp[y*bw + x];
        }
    delete[] bmp;

    glBindTexture(GL_TEXTURE_2D, f->atlasTex);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, ttffont::AW);
    glTexSubImage2D(GL_TEXTURE_2D, 0, ax, ay, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE,
                    f->atlasData + (ay * ttffont::AW + ax) * 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    ng.ax = (short)ax; ng.ay = (short)ay;
    ng.u0 = (float)ax / ttffont::AW;   ng.v0 = (float)ay / ttffont::AH;
    ng.u1 = (float)(ax+bw) / ttffont::AW; ng.v1 = (float)(ay+bh) / ttffont::AH;

    return &ng;
}

int getfonth()
{
    if(curttffont) return curttffont->ascent - curttffont->descent + curttffont->linegap;
    if(curfont) return curfont->defaulth;
    return 16;
}

static void text_bounds_ttf(ttffont *f, const char *str, int &width, int &height, int maxwidth = -1)
{
    int x = 0, y = 0;
    int lineH = f->ascent - f->descent + f->linegap;
    int tabW  = max(1, 4 * f->pixelsize / 2);
    int maxX  = 0;

    for(int i = 0; str[i]; )
    {
        uchar c0 = (uchar)str[i];
        if(c0 == '\f') { if(str[i+1]) i += 2; else i++; continue; }
        if(c0 == '\t') { x = ((x + tabW) / tabW) * tabW; i++; continue; }
        if(c0 == '\n') { if(x > maxX) maxX = x; x = 0; y += lineH; i++; continue; }

        int cp = decode_utf8(str, i);
        ttffont::glyph *g = ttf_getglyph(f, cp);
        if(!g) continue;

        if(maxwidth != -1 && x + (int)g->xadv > maxwidth && x > 0)
        {
            if(x > maxX) maxX = x;
            x = 0; y += lineH;
        }
        x += (int)g->xadv + 1;
    }
    if(x > maxX) maxX = x;
    width  = maxX;
    height = y + lineH;
}

static void ttf_applycolor(char c, char *stack, int size, int &sp, int r, int g, int b, int a)
{
    xtraverts += varray::end();
    if(c == 's') { if(sp < size-1) stack[++sp] = stack[sp]; varray::begin(GL_QUADS); return; }
    if(c == 'r') c = stack[(sp > 0) ? --sp : sp];
    else stack[sp] = c;
    int cr = r, cg = g, cb = b;
    switch(c)
    {
        case '0': cr=64;  cg=255; cb=128; break;
        case '1': cr=96;  cg=160; cb=255; break;
        case '2': cr=255; cg=192; cb=64;  break;
        case '3': cr=255; cg=64;  cb=64;  break;
        case '4': cr=128; cg=128; cb=128; break;
        case '5': cr=192; cg=64;  cb=192; break;
        case '6': cr=255; cg=128; cb=0;   break;
        case '7': cr=255; cg=255; cb=255; break;
    }
    glColor4ub(cr, cg, cb, a);
    varray::begin(GL_QUADS);
}

static void draw_text_ttf(ttffont *f, const char *str, int left, int top, int r, int g, int b, int a)
{
    int baseline = top + f->ascent;
    int x = 0, y = 0;
    int lineH = f->ascent - f->descent + f->linegap;
    int tabW  = max(1, 4 * f->pixelsize / 2);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, f->atlasTex);
    glColor4ub(r, g, b, a);

    varray::enable();
    varray::defattrib(varray::ATTRIB_VERTEX,    2, GL_FLOAT);
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
    varray::begin(GL_QUADS);

    char colorstack[10];
    int colorpos = 0;
    colorstack[0] = 'c';

    for(int i = 0; str[i]; )
    {
        uchar c0 = (uchar)str[i];
        if(c0 == '\f')
        {
            if(str[i+1]) { i++; ttf_applycolor(str[i], colorstack, sizeof(colorstack), colorpos, r, g, b, a); i++; }
            else i++;
            continue;
        }
        if(c0 == '\t') { x = ((x + tabW) / tabW) * tabW; i++; continue; }
        if(c0 == '\n') { x = 0; y += lineH; i++; continue; }

        int cp = decode_utf8(str, i);
        ttffont::glyph *gp = ttf_getglyph(f, cp);
        if(!gp) continue;
        if(gp->bw > 0 && gp->bh > 0)
        {
            float px = (float)(left + x + gp->bx);
            float py = (float)(baseline + y + gp->by);
            float pw = (float)gp->bw;
            float ph = (float)gp->bh;
            varray::attrib<float>(px,    py   ); varray::attrib<float>(gp->u0, gp->v0);
            varray::attrib<float>(px+pw, py   ); varray::attrib<float>(gp->u1, gp->v0);
            varray::attrib<float>(px+pw, py+ph); varray::attrib<float>(gp->u1, gp->v1);
            varray::attrib<float>(px,    py+ph); varray::attrib<float>(gp->u0, gp->v1);
        }
        x += (int)gp->xadv + 1;
    }

    xtraverts += varray::end();
    varray::disable();
}

static void loadttf(const char *name, const char *path, int *psize)
{
    stream *fs = openfile(path, "rb");
    if(!fs) { conoutf(CON_ERROR, "TTF: could not open \"%s\"", path); return; }
    int fsz = (int)fs->size();
    uchar *fdata = new uchar[fsz];
    fs->read(fdata, fsz);
    delete fs;

    ttffont *tf = new ttffont();
    tf->filedata = fdata;
    if(!stbtt_InitFont(&tf->info, fdata, stbtt_GetFontOffsetForIndex(fdata, 0)))
    {
        delete[] fdata; delete tf;
        conoutf(CON_ERROR, "TTF: failed to parse \"%s\"", path); return;
    }
    tf->pixelsize = *psize;
    tf->scale = stbtt_ScaleForPixelHeight(&tf->info, (float)*psize);

    int asc, desc, lg;
    stbtt_GetFontVMetrics(&tf->info, &asc, &desc, &lg);
    tf->ascent  = (int)(asc  * tf->scale + 0.5f);
    tf->descent = (int)(desc * tf->scale - 0.5f);
    tf->linegap = (int)(lg   * tf->scale + 0.5f);
    if(tf->linegap < tf->pixelsize / 6) tf->linegap = tf->pixelsize / 6;

    tf->name = newstring(name);

    tf->atlasData = new uchar[ttffont::AW * ttffont::AH * 4]();
    glGenTextures(1, &tf->atlasTex);
    glBindTexture(GL_TEXTURE_2D, tf->atlasTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ttffont::AW, ttffont::AH, 0, GL_RGBA, GL_UNSIGNED_BYTE, tf->atlasData);

    ttffonts[tf->name] = tf;
    conoutf("TTF: loaded \"%s\" from \"%s\" at %dpx  ascent=%d lineh=%d",
            name, path, *psize, tf->ascent, tf->ascent - tf->descent + tf->linegap);
}

COMMAND(loadttf, "ssi");

void newfont(char *name, char *tex, int *defaultw, int *defaulth, int *offsetx, int *offsety, int *offsetw, int *offseth)
{
    font *f = fonts.access(name);
    if(!f)
    {
        name = newstring(name);
        f = &fonts[name];
        f->name = name;
    }

    f->tex = textureload(tex);
    f->chars.shrink(0);
    f->charoffset = '!';
    f->defaultw = *defaultw;
    f->defaulth = *defaulth;
    f->offsetx = *offsetx;
    f->offsety = *offsety;
    f->offsetw = *offsetw;
    f->offseth = *offseth;

    fontdef = f;
}

void fontoffset(char *c)
{
    if(!fontdef) return;
    
    fontdef->charoffset = c[0];
}

void fontchar(int *x, int *y, int *w, int *h)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
}

COMMANDN(font, newfont, "ssiiiiii");
COMMAND(fontoffset, "s");
COMMAND(fontchar, "iiii");

bool setfont(const char *name)
{
    ttffont **tf = ttffonts.access(name);
    if(tf) { curttffont = *tf; return true; }
    curttffont = NULL;
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
}

static vector<font *> fontstack;
static vector<ttffont *> ttffontstack;

void pushfont()
{
    fontstack.add(curfont);
    ttffontstack.add(curttffont);
}

bool popfont()
{
    if(fontstack.empty()) return false;
    curfont    = fontstack.pop();
    curttffont = ttffontstack.pop();
    return true;
}

void gettextres(int &w, int &h)
{
    if(w < MINRESW || h < MINRESH)
    {
        if(MINRESW > w*MINRESH/h)
        {
            h = h*MINRESW/w;
            w = MINRESW;
        }
        else
        {
            w = w*MINRESH/h;
            h = MINRESH;
        }
    }
}

#define PIXELTAB (4*curfont->defaultw)

int text_width(const char *str) { //@TODO deprecate in favour of text_bounds(..)
    int width, height;
    text_bounds(str, width, height);
    return width;
}

void tabify(const char *str, int *numtabs)
{
    vector<char> tabbed;
    tabbed.put(str, strlen(str));
    int w = text_width(str), tw = max(*numtabs, 0)*PIXELTAB;
    while(w < tw)
    {
        tabbed.add('\t');
        w = ((w+PIXELTAB)/PIXELTAB)*PIXELTAB;
    }
    tabbed.add('\0');
    result(tabbed.getbuf());
}

COMMAND(tabify, "si");
    
void draw_textf(const char *fstr, int left, int top, ...)
{
    defvformatstring(str, top, fstr);
    draw_text(str, left, top);
}

static int draw_char(int c, int x, int y)
{
    font::charinfo &info = curfont->chars[c-curfont->charoffset];
    float tc_left    = (info.x + curfont->offsetx) / float(curfont->tex->xs);
    float tc_top     = (info.y + curfont->offsety) / float(curfont->tex->ys);
    float tc_right   = (info.x + info.w + curfont->offsetw) / float(curfont->tex->xs);
    float tc_bottom  = (info.y + info.h + curfont->offseth) / float(curfont->tex->ys);

    varray::attrib<float>(x,          y         ); varray::attrib<float>(tc_left,  tc_top   );
    varray::attrib<float>(x + info.w, y         ); varray::attrib<float>(tc_right, tc_top   );
    varray::attrib<float>(x + info.w, y + info.h); varray::attrib<float>(tc_right, tc_bottom);
    varray::attrib<float>(x,          y + info.h); varray::attrib<float>(tc_left,  tc_bottom);

    return info.w;
}

//stack[sp] is current color index
static void text_color(char c, char *stack, int size, int &sp, bvec color, int a) 
{
    if(c=='s') // save color
    {   
        c = stack[sp];
        if(sp<size-1) stack[++sp] = c;
    }
    else
    {
        xtraverts += varray::end();
        if(c=='r') c = stack[(sp > 0) ? --sp : sp]; // restore color
        else stack[sp] = c;
        switch(c)
        {
            case '0': color = bvec( 64, 255, 128); break;   // green: player talk
            case '1': color = bvec( 96, 160, 255); break;   // blue: "echo" command
            case '2': color = bvec(255, 192,  64); break;   // yellow: gameplay messages 
            case '3': color = bvec(255,  64,  64); break;   // red: important errors
            case '4': color = bvec(128, 128, 128); break;   // gray
            case '5': color = bvec(192,  64, 192); break;   // magenta
            case '6': color = bvec(255, 128,   0); break;   // orange
            case '7': color = bvec(255, 255, 255); break;   // white
            // provided color: everything else
        }
        glColor4ub(color.x, color.y, color.z, a);
    } 
}

#define TEXTSKELETON \
    int y = 0, x = 0;\
    int i;\
    for(i = 0; str[i]; i++)\
    {\
        TEXTINDEX(i)\
        int c = str[i];\
        if(c=='\t')      { x = ((x+PIXELTAB)/PIXELTAB)*PIXELTAB; TEXTWHITE(i) }\
        else if(c==' ')  { x += curfont->defaultw; TEXTWHITE(i) }\
        else if(c=='\n') { TEXTLINE(i) x = 0; y += FONTH; }\
        else if(c=='\f') { if(str[i+1]) { i++; TEXTCOLOR(i) }}\
        else if(curfont->chars.inrange(c-curfont->charoffset))\
        {\
            if(maxwidth != -1)\
            {\
                int j = i;\
                int w = curfont->chars[c-curfont->charoffset].w;\
                for(; str[i+1]; i++)\
                {\
                    int c = str[i+1];\
                    if(c=='\f') { if(str[i+2]) i++; continue; }\
                    if(i-j > 16) break;\
                    if(!curfont->chars.inrange(c-curfont->charoffset)) break;\
                    int cw = curfont->chars[c-curfont->charoffset].w + 1;\
                    if(w + cw >= maxwidth) break;\
                    w += cw;\
                }\
                if(x + w >= maxwidth && j!=0) { TEXTLINE(j-1) x = 0; y += FONTH; }\
                TEXTWORD\
            }\
            else\
            { TEXTCHAR(i) }\
        }\
    }

//all the chars are guaranteed to be either drawable or color commands
#define TEXTWORDSKELETON \
                for(; j <= i; j++)\
                {\
                    TEXTINDEX(j)\
                    int c = str[j];\
                    if(c=='\f') { if(str[j+1]) { j++; TEXTCOLOR(j) }}\
                    else { TEXTCHAR(j) }\
                }

int text_visible(const char *str, int hitx, int hity, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->charoffset].w+1; TEXTWHITE(idx)
    #define TEXTWORD TEXTWORDSKELETON
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
    return i;
}

//inverse of text_visible
void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth) 
{
    #define TEXTINDEX(idx) if(idx == cursor) { cx = x; cy = y; break; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->charoffset].w + 1;
    #define TEXTWORD TEXTWORDSKELETON if(i >= cursor) break;
    cx = INT_MIN;
    cy = 0;
    TEXTSKELETON
    if(cx == INT_MIN) { cx = x; cy = y; }
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void text_bounds(const char *str, int &width, int &height, int maxwidth)
{
    if(curttffont) { text_bounds_ttf(curttffont, str, width, height, maxwidth); return; }
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->charoffset].w + 1;
    #define TEXTWORD x += w + 1;
    width = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void draw_text(const char *str, int left, int top, int r, int g, int b, int a, int cursor, int maxwidth)
{
    if(curttffont) { draw_text_ttf(curttffont, str, left, top, r, g, b, a); return; }
    #define TEXTINDEX(idx) if(idx == cursor) { cx = x; cy = y; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) 
    #define TEXTCOLOR(idx) text_color(str[idx], colorstack, sizeof(colorstack), colorpos, color, a);
    #define TEXTCHAR(idx) x += draw_char(c, left+x, top+y)+1;
    #define TEXTWORD TEXTWORDSKELETON
    char colorstack[10];
    bvec color(r, g, b);
    int colorpos = 0, cx = INT_MIN, cy = 0;
    colorstack[0] = 'c'; //indicate user color
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, curfont->tex->id);
    glColor4ub(color.x, color.y, color.z, a);
    varray::enable();
    varray::defattrib(varray::ATTRIB_VERTEX, 2, GL_FLOAT);
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
    varray::begin(GL_QUADS);
    TEXTSKELETON
    xtraverts += varray::end();
    if(cursor >= 0 && (totalmillis/250)&1)
    {
        glColor4ub(r, g, b, a);
        if(cx == INT_MIN) { cx = x; cy = y; }
        if(maxwidth != -1 && cx >= maxwidth) { cx = 0; cy += FONTH; }
        draw_char('_', left+cx, top+cy);
        xtraverts += varray::end();
    }
    varray::disable();
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        if(!reloadtexture(*f.tex)) fatal("failed to reload font texture");
    );
}

