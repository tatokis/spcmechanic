// By Tasos Sahanidis <ggj@tasossah.com>, 2020
//
// faraway.xm
// https://commons.wikimedia.org/wiki/File:Andreas_Viklund_-_Faraway_Love.wav
//
// Sound effects
// https://opengameart.org/content/haydenwoffle-sfx-dump
//
// Gear
// glxgears
//
// Resistor
// KiCad
//
// Cursor
// DMZ Black
//
// Spaceship
// https://opengameart.org/content/pixel-spaceship

#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xmp.h>
#include <dmaKit.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <audsrv.h>
#include <gsKit.h>
#include <malloc.h>
#include <gsToolkit.h>
#include <gsFontM.h>
#include <libpad.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 1024*4

//#define NORETURN

#ifdef NORETURN
static void waste_time()
{
    while(1)
        sleep(1);
}
#define PGM_END(x) waste_time()
#else
#define PGM_END(x) return (x)
#endif

enum path_type
{
    PATH_OTHER,
    PATH_CDROM,
};

char _path_int[512] = { 0 };
char _cwd[256];
enum path_type ptype;

// WARNING: This is _NOT_ thread safe
char* fixPath(const char* p)
{
    if(*_path_int == 0)
    {
        ptype = PATH_OTHER;
        if(getcwd(_cwd, 255) == NULL)
        {
            _cwd[0] = '\0';
            puts("getcwd returned NULL");
        }

        // Check if the path is cdrom
        // We need this to swap the directory separators
        if(!strncmp(_cwd, "cdrom", 5))
            ptype = PATH_CDROM;
        printf("Inside getpath %s\n", _cwd);
    }

    // _cwd is half the size of _path_int so it should be fine
    strcpy(_path_int, _cwd);
    size_t sz = strlen(_path_int);
    //if(ptype == PATH_CDROM)
    //    _path_int[sz++] = '\\';

    // Append the path to the cwd
    strncpy(_path_int + sz, p, 511 - sz);

    // If we're reading from optical media, replace dir separators
    if(ptype == PATH_CDROM)
    {
        char* ptr = _path_int;
        while((ptr = strchr(ptr, '/')) != NULL)
                *ptr++ = '\\';
    }
    return _path_int;
}

// ADPCM
typedef struct _adpsamples {
    audsrv_adpcm_t fall, throw, enemy, hit, ui, success;
} adpsamples;

typedef struct _smpldescr {
    audsrv_adpcm_t* ptr;
    const char* path;
} smpldescr;

// GS
typedef struct _gsColours {
    u64 purple, black, white, whiter, whitenoa, trekieblue, pcbgreen, shmupbg, brown, red, orange, yellow, green, blue, violet, grey, gold, silver;
} gsColours;

typedef struct _gsTextures {
    GSTEXTURE* cursor;
    GSTEXTURE* sprites;
} gsTextures;


// Game functions

#define MAX_BULLETS 5
#define MAX_ENEMIES 8
#define MAX_RESISTORS 4
#define MAX_RES_SLN 24
//#define SPRITE_LEN 2
#define SPRITE_LEN 35

// Player
typedef struct _player
{
    float x, y;
    int w, h, score;
} player;

typedef struct _bullet
{
    int active; // Did this so that I only need to allocate this once
    float x, y;
} bullet;

void emitBullet(bullet* b, player* p, adpsamples* smpls)
{
    // Find an empty slot in the array and add it there
    for(int i = 0; i < MAX_BULLETS; i++)
    {
        //printf("Loop %d, %d\n", i, b[i].active);
        if(b[i].active)
            continue;

        // Create it and add it
        b[i].active = 1;
        b[i].x = p->x;
        b[i].y = p->y + 24.f; // FIXME, or maybe hack around. This is to centre vertically
        //printf("Emitting bullet %d\n", i);
        audsrv_play_adpcm(&smpls->throw);
        return;
    }
}
enum resistorband
{
    BAND_BLACK = 0,
    BAND_BROWN,
    BAND_RED,
    BAND_ORANGE,
    BAND_YELLOW,
    BAND_GREEN,
    BAND_BLUE,
    BAND_VIOLET,
    BAND_GREY,
    BAND_WHITE,
    BAND_GOLD,
    BAND_SILVER,
    BAND_MAX,
};

const char* bandToString(enum resistorband b)
{
    switch(b)
    {
        case BAND_BLACK:
            return "Black";
        case BAND_BROWN:
            return "Brown";
        case BAND_RED:
            return "Red";
        case BAND_ORANGE:
            return "Orange";
        case BAND_YELLOW:
            return "Yellow";
        case BAND_GREEN:
            return "Green";
        case BAND_BLUE:
            return "Blue";
        case BAND_VIOLET:
            return "Violet";
        case BAND_GREY:
            return "Grey";
        case BAND_WHITE:
            return "White";
        case BAND_GOLD:
            return "Gold";
        case BAND_SILVER:
            return "Silver";
        default:
            return "ERROR";
    }
}

typedef struct _res_solution
{
    const char* reqval;
    enum resistorband bands[3];
} res_solution;

typedef struct _resistor
{
    int active;
    //u64 value;
    const char* suffix;
    enum resistorband bands[3];
    /*enum _orientation
    {
        OR_HORIZONTAL,
        OR_VERTICAL,
    } orientation;*/
    float x, y;
    int w, h;
    unsigned char current_band;
    res_solution* solution;
    unsigned char zeroes;
    
} resistor;

// Slight cheating because calculations aren't accurate
res_solution sln[MAX_RES_SLN] = {
    {"1 KOhm", {BAND_BROWN, BAND_BLACK, BAND_RED}},
    {"2.2 KOhm", {BAND_RED, BAND_RED, BAND_RED}},
    {"10 KOhm", {BAND_BROWN, BAND_BLACK, BAND_ORANGE}},
    {"47 Ohm", {BAND_YELLOW, BAND_VIOLET, BAND_BLACK}},
    {"3.3 MOhm", {BAND_ORANGE, BAND_ORANGE, BAND_GREEN}},
    {"720 Ohm", {BAND_VIOLET, BAND_RED, BAND_BROWN}},
    {"330 Ohm", {BAND_ORANGE, BAND_ORANGE, BAND_BROWN}},
    {"220 Ohm", {BAND_RED, BAND_RED, BAND_BROWN}},
    {"22 Ohm", {BAND_RED, BAND_RED, BAND_BLACK}},
    {"10 Ohm", {BAND_BROWN, BAND_BLACK, BAND_BLACK}},
    {"15 KOhm", {BAND_BROWN, BAND_GREEN, BAND_ORANGE}},
    {"1.5 KOhm", {BAND_BROWN, BAND_GREEN, BAND_RED}},
    {"12 KOhm", {BAND_BROWN, BAND_RED, BAND_ORANGE}},
    {"150 KOhm", {BAND_BROWN, BAND_GREEN, BAND_YELLOW}},
    {"100 KOhm", {BAND_BROWN, BAND_BLACK, BAND_YELLOW}},
    {"1 MOhm", {BAND_BROWN, BAND_BLACK, BAND_GREEN}},
    {"820 Ohm", {BAND_GREY, BAND_RED, BAND_BROWN}},
    {"750 Ohm", {BAND_VIOLET, BAND_GREEN, BAND_BROWN}},
    {"470 Ohm", {BAND_YELLOW, BAND_VIOLET, BAND_BROWN}},
    {"4.7 KOhm", {BAND_YELLOW, BAND_VIOLET, BAND_RED}},
    {"1 Ohm", {BAND_BROWN, BAND_BLACK, BAND_GOLD}},
    {"4.7 Ohm", {BAND_YELLOW, BAND_VIOLET, BAND_GOLD}},
    {"1.2 Ohm", {BAND_BROWN, BAND_RED, BAND_GOLD}},
    {"56 Ohm", {BAND_GREEN, BAND_BLUE, BAND_BLACK}},
};

static inline unsigned int resistorToOhmsNoMul(resistor* r)
{
    return ((r->bands[0] * 10) + r->bands[1]);
}

static inline float resistorValueSuffix(resistor* r)
{
    unsigned int rval = resistorToOhmsNoMul(r);
    switch(r->bands[2])
    {
        case BAND_WHITE:
            r->zeroes = 9;
            r->suffix = "GOhm";
            return rval;
        case BAND_GREY:
            r->zeroes = 8;
            r->suffix = "GOhm";
            return rval / 10.f;
        case BAND_VIOLET:
            r->zeroes = 7;
            r->suffix = "MOhm";
            return rval * 10;
        case BAND_BLUE:
            r->zeroes = 6;
            r->suffix = "MOhm";
            return rval;
        case BAND_GREEN:
            r->zeroes = 5;
            r->suffix = "MOhm";
            return rval / 10.f;
        case BAND_YELLOW:
            r->zeroes = 4;
            r->suffix = "KOhm";
            return rval * 10;
        case BAND_ORANGE:
            r->zeroes = 3;
            r->suffix = "KOhm";
            return rval;
        case BAND_RED:
            r->zeroes = 2;
            r->suffix = "KOhm";
            return rval / 10.f;
        case BAND_BROWN:
            r->zeroes = 1;
            r->suffix = "Ohm";
            return rval * 10;
        case BAND_GOLD:
            r->suffix = "Ohm";
            return rval / 10.f;
        case BAND_SILVER:
            r->suffix = "Ohm";
            return rval / 100.f;
        case BAND_BLACK:
        default:
            r->zeroes = 0;
            r->suffix = "Ohm";
            return rval;
    }
}

void generateResistors(GSGLOBAL* g, resistor* r, GSTEXTURE* t, unsigned int* ticks_until_game_over)
{
    int resistorcount = rand() % MAX_RESISTORS;
    *ticks_until_game_over = 20 * (resistorcount + 1);
    for(int i = 0; i < resistorcount + 1; i++)
    {
        int seprand = rand();
        printf("Generating resistor %d\n", i);
        (r + i)->active = 1;

        (r + i)->x = 70 + (seprand % 100);
        (r + i)->y = 130 + (70 * i);

        (r + i)->w = t->Width;
        (r + i)->h = t->Height;
        (r + i)->suffix = "Ohm";

        // Pick a random solution
        (r + i)->solution = sln + (rand() % MAX_RES_SLN);

        // First band can't be 0
        (r + i)->bands[0] = BAND_BROWN;
    }
}

typedef struct _enemy
{
    int active;
    float x, y;
    int w, h;
   /* enum path_type
    {
        PATH_LINE,
        PATH_ANGLEUP,
        PATH_ANGLEDOWN,
        //PATH_ANGLERETUP,
        //PATH_ANGLERETDOWN,
        PATH_ENUM_MAX,
    } path;*/
    size_t path_step;
    enum accel_type
    {
        ACCEL_NONE,
        ACCEL_VAR,
        ACCEL_MAX,
    } acceleration;
    unsigned char speed_modifier;
} enemy;

int enemyIntersect(enemy* e, float px, float py, int pw, int ph)
{
    //printf("Bullet %d, %d, %d, %d\n", 

    if(e->x < px + pw
        && e->x + e->w > px
        && e->y < py + ph
        && e->y + e->h > py)
        return 1;
    return 0;
}

void renderBullets(GSGLOBAL* gs, bullet* b, gsColours* c, enemy* e, player* p, adpsamples* s)
{
    for(int i = 0; i < MAX_BULLETS; i++)
    {
        // Ignore inactive bullets
        if(!(b+i)->active)
            continue;

        (b+i)->x += 6.f;
        // Remove it if it went off screen. Else render it
        if((b+i)->x >= gs->Width)
           (b+i)->active = 0;
        else
        {
            gsKit_prim_sprite(gs, (b+i)->x, (b+i)->y, (b+i)->x + 15.f, (b+i)->y + 7.f, 3, c->whitenoa);
            for(int j = 0; j < MAX_ENEMIES; j++)
            {
                //printf("Active %d\n", (e+j)->active);
                if(!(e+j)->active)
                    continue;
                //printf("Bullet x %f, y %f, w %f, h %f\n", (b+i)->x, (b+i)->y, 15.f, 7.f);
                //printf("Enemy #%d x %f, y %f, w %d, h %d\n\n", j, (e+j)->x, (e+j)->y, (e+j)->w, (e+j)->h);
                if(enemyIntersect(e+j, (b+i)->x, (b+i)->y, 15, 7))
                {
                    //printf("Kaboom!\n");
                    audsrv_play_adpcm(&s->hit);
                    // Remove the enemy and the bullet
                    (b+i)->active = (e+j)->active = 0;
                    // Increment the score
                    p->score++;
                }
            }
        }
    }
}

static inline int generateEnemy(GSGLOBAL* gs, enemy* e, GSTEXTURE* Tex)
{
    for(int i = 0; i < MAX_ENEMIES; i++)
    {
        if((e+i)->active)
            continue;

        int rndval = rand();
        (e+i)->active = 1;
        (e+i)->w = Tex->Width;
        (e+i)->h = Tex->Height;
        (e+i)->x = gs->Width + (e+i)->w;
        (e+i)->y = rndval % gs->Width;
        if((e+i)->y < 20.f)
            (e+i)->y = 20.f;
        else if((e+i)->y > gs->Height - 40.f)
            (e+i)->y = gs->Height - 40.f;
        (e+i)->speed_modifier = (rndval % 3) + 1;
        (e+i)->acceleration = (rand() % ACCEL_MAX);
        return 1;
    }
    // If we get here, that means there are already too many enemies on screen
    // It's fine if we don't generate any more
    return 0;
}

void renderEnemies(GSGLOBAL* gs, GSTEXTURE* Tex, gsColours* c, enemy* e, unsigned char* count, player* p, adpsamples* s, volatile int* allow_pad)
{
    for(int i = 0; i < MAX_ENEMIES; i++)
    {
        if(!(e + i)->active)
            continue;

        // Check for collision with player
        // Else, draw the enemy
        if(enemyIntersect((e + i), p->x, p->y + 10.f, p->w, p->h - 10.f))
        {
            if(*allow_pad)
            {
                puts("RIP Player");
                audsrv_play_adpcm(&s->fall);
                *allow_pad = 0;
                (e + i)->active = 0;
                p->score = 0;
            }
        }
        // Move them along the path
        if(!(*count % 2))
        {
            if((e + i)->acceleration == ACCEL_VAR)
                (e + i)->x -= 3.f + *count / 100.f;
            else
                (e + i)->x -= 3.f;
        }

        // Remove them if they went offscreen, and decrement the score
        if((e + i)->x < 0.f)
        {
            (e + i)->active = 0;
            p->score--;
        }

        GSTEXTURE* spr = Tex + ((*count / (e + i)->speed_modifier) % SPRITE_LEN);

        gsKit_prim_sprite_texture(gs, spr, (e + i)->x, (e + i)->y, 0, 0,
            (e + i)->x + (e + i)->w, (e + i)->y + (e + i)->h, (e + i)->w, (e + i)->h, 4, c->white);
    }
}

// Scene

enum scene
{
    SCENE_SHMUP,
    SCENE_REPAIR,
    SCENE_PRE_REPAIR,
    SCENE_GAME_OVER,
};

static enum scene scn;

// Misc
extern void *_gp;
#define STACK_SIZE 16384


// Audio thread
static int audsrv_play;
static int fillbuffer_sema;
void audioThread()
{
    puts("Hello from audio thread");
    char* modfile = fixPath("ASSETS/MODULES/FARAWAY.XM");
    //const char* modfile = "bzl-zeroplex.xm";

    puts("Past init");
	xmp_context ctx = xmp_create_context();
    
    printf("Loading %s into memory\n", modfile);
    FILE* f = fopen(modfile, "rb");
    fseek(f, 0, SEEK_END);
    size_t modsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("Size %u\n", modsize);
    void* data = malloc(modsize);
    printf("Malloc returned %p\n", data);
    fread(data, modsize, 1, f);
    fclose(f);
    int ret;
    if ((ret = xmp_load_module_from_memory(ctx, data, modsize)) < 0) {
        printf("error loading %s: %d\n", modfile + 5, ret);
        return;
    }
    free(data);
    data = NULL;

    puts("Past load module");
    if (xmp_start_player(ctx, 44100, 0)) {
        puts("Error initialising audio");
        return;
    }

    // Playback
    struct xmp_frame_info fi;
    // Audio buffer
    char buf[BUFFER_SIZE];
    
    // Fill buffer initially
    xmp_play_buffer(ctx, buf, BUFFER_SIZE, 0);
    while(audsrv_play)
    {
        if((ret = xmp_play_buffer(ctx, buf, BUFFER_SIZE, 0)))
            printf("Something happened %d\n", ret);
        else
        {
            xmp_get_frame_info(ctx, &fi);
            //if (fi.loop_count > 0)
            //    break;

            WaitSema(fillbuffer_sema);
            audsrv_play_audio(buf, BUFFER_SIZE);
        }
    }
    xmp_end_player(ctx);

    xmp_release_module(ctx);
    printf("\n");

	xmp_free_context(ctx);
}

static ee_thread_t threadAttr = { 0 };
s32 audioThreadID;

static void initAudioThread()
{
    threadAttr.func = (void*)audioThread;
	threadAttr.stack = (void*)malloc(STACK_SIZE);
	if (threadAttr.stack == NULL)
        puts("Thread stack malloc error. Something went really wrong.\n");
    threadAttr.stack_size = STACK_SIZE;
	threadAttr.gp_reg = (void *)&_gp;
	threadAttr.initial_priority = 0x17;

    audioThreadID = CreateThread(&threadAttr);
    StartThread(audioThreadID, NULL);
}

static int fillbuffer(void *arg)
{
	iSignalSema((int)arg);
	return 0;
}

#define SAMPLE_CNT 6
static void loadSamples(adpsamples* smpls)
{
    smpldescr smplstr[SAMPLE_CNT] =  {
        {&smpls->fall, "ASSETS/ADPCM/FALL.SAD"},
        {&smpls->throw, "ASSETS/ADPCM/THROW.SAD"},
        {&smpls->enemy, "ASSETS/ADPCM/ENEMY.SAD"},
        {&smpls->hit, "ASSETS/ADPCM/HIT.SAD"},
        {&smpls->ui, "ASSETS/ADPCM/UI.SAD"},
        {&smpls->success, "ASSETS/ADPCM/SUCCESS.SAD"},
    };

    for(int i = 0; i < SAMPLE_CNT; i++)
    {
        char* fixedpath = fixPath(smplstr[i].path);
        FILE* fsmpl = fopen(fixedpath, "rb");
        if(!fsmpl)
        {
            printf("Error opening %s\n", fixedpath);
            continue;
        }
        fseek(fsmpl, 0, SEEK_END);
        size_t smplsize = ftell(fsmpl);
        fseek(fsmpl, 0, SEEK_SET);

        char* sbuf = malloc(smplsize);
        fread(sbuf, smplsize, 1, fsmpl);

        printf("Loading %s\n", fixedpath);
        audsrv_load_adpcm(smplstr[i].ptr, sbuf, smplsize);

        free(sbuf);
        fclose(fsmpl);
    }
}

// Pad
extern int port, slot;
extern int pad_init();
u32 old_pad = 0;

static int dismiss_explanation;
static int break_from_main;

// This function does much more than just "read the pad", but it's too late at this point
static void readPad(GSGLOBAL* gsGlobal, player* p, adpsamples* smpls, bullet* b, resistor* r, resistor** selected_resistor)
{
	int ret;

	do {
		ret=padGetState(port, slot);
	} while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1));

    struct padButtonStatus buttons;
	ret = padRead(port, slot, &buttons);

	if (!ret)
        return;
    //printf("%u, %u\n", buttons.ljoy_h, buttons.ljoy_v);

    // Read analog sticks (and apply a deadzone)
    // Only for cursor/ship
    if(!(*selected_resistor))
    {
        if(buttons.ljoy_h > 160)
            p->x += (buttons.ljoy_h - 160) / 11;
        else if(buttons.ljoy_h < 90)
            p->x += (buttons.ljoy_h - 90) / 11;

        if(buttons.ljoy_v > 160)
            p->y += (buttons.ljoy_v - 160) / 11;
        else if(buttons.ljoy_v < 90)
            p->y += (buttons.ljoy_v - 90) / 11;
    }
    
    u32 paddata = 0xffff ^ buttons.btns;
    u32 new_pad = paddata & ~old_pad;
    old_pad = paddata;

    if(paddata)
    {
        //printf("paddata = 0x%04x\n", paddata);     
        if(*selected_resistor)
        {
            if(new_pad & PAD_LEFT && (*selected_resistor)->current_band > 0)
                (*selected_resistor)->current_band -= 1;

            if(new_pad & PAD_RIGHT && (*selected_resistor)->current_band < 2)
                (*selected_resistor)->current_band +=1;
            
            if(new_pad & PAD_DOWN && (*selected_resistor)->bands[(*selected_resistor)->current_band] < BAND_MAX - 1) // -3 so that the last two can't be chosen, not true anymore
            {
                // Don't allow bands 0 and 1 to go above BAND_WHITE
                if(!(((*selected_resistor)->current_band == 0 || (*selected_resistor)->current_band == 1) && (*selected_resistor)->bands[(*selected_resistor)->current_band] >= BAND_WHITE))
                    (*selected_resistor)->bands[(*selected_resistor)->current_band] += 1;
            }

            if(new_pad & PAD_UP && (*selected_resistor)->bands[(*selected_resistor)->current_band] > 0)
            {
                // Don't allow the the first band to be black
                // It's both not valid and will mess up our hacky value calculations
                if(!((*selected_resistor)->current_band == 0 && (*selected_resistor)->bands[(*selected_resistor)->current_band] == BAND_BROWN))
                    (*selected_resistor)->bands[(*selected_resistor)->current_band] -= 1;
            }
        }
        else
        {
            // Change X
            if(paddata & PAD_LEFT)
                p->x -= 8.f;

            if(paddata & PAD_RIGHT)
                p->x += 8.f;

            // Change Y
            if(paddata & PAD_DOWN) 
                p->y += 8.f;

            if(paddata & PAD_UP)
                p->y -= 8.f;
        }
    }

    // Mute applies for all scenes
    if(new_pad & PAD_SELECT)
        audsrv_set_volume(0);
    // Following ones are single shot
    if(scn == SCENE_SHMUP)
    {
        if(new_pad & PAD_CROSS)
            emitBullet(b, p, smpls);
    }
    else if(scn == SCENE_REPAIR)
    {
        // Select the resistor under the cursor
        if(new_pad & PAD_CROSS)
        {
            if(*selected_resistor)
            {
                (*selected_resistor) = NULL;
                return;
            }
            for(int i = 0; i < MAX_RESISTORS; i++)
            {
                if(!(r+i)->active)
                    break;
                if((r+i)->x < p->x + p->w/2
                    && (r+i)->x + (r+i)->w > p->x
                    && (r+i)->y < p->y + p->h/2
                    && (r+i)->y + (r+i)->h > p->y)
                {
                    audsrv_play_adpcm(&smpls->ui);
                    *selected_resistor = r+i;
                    (r+i)->current_band = 0;
                    break;
                }
            }
        }
        
        // Deselect the resistor
        if(new_pad & PAD_TRIANGLE)
        {
            *selected_resistor = NULL;
        }
        
        if(new_pad & PAD_START)
        {
            //printf("START\n");
            int validated_resistors = 0;
            int i = 0;
            // Validate the results
            for(; i < MAX_RESISTORS; i++)
            {
                //printf("Resistor %d\n", i);
                if(!(r+i)->active)
                    break;
                                
                int j = 0;
                for(; j < 3; j++)
                {
                    //printf("Band %d\n", j);
                    //printf("Comparing %d with %d\n", (r+i)->solution->bands[j], (r+i)->bands[j]);
                    if((r+i)->solution->bands[j] != (r+i)->bands[j])
                        break;
                }
                
                if(j == 3)
                    validated_resistors++;

            }
            //printf("Validated resistors %d, i %d\n", validated_resistors, i);
            if(validated_resistors == i)
            {
                // Success
                scn = SCENE_SHMUP;

                // Reset everything
                *selected_resistor = NULL;
                memset(r, 0, sizeof(resistor) * MAX_RESISTORS);
                memset(b, 0, sizeof(bullet) * MAX_BULLETS);

                audsrv_play_adpcm(&smpls->success);
            }
            else
                audsrv_play_adpcm(&smpls->ui);
            
        }
    }
    else if(scn == SCENE_PRE_REPAIR)
    {
        if(new_pad & PAD_START)
        {
            dismiss_explanation = 1;
            scn = SCENE_REPAIR;
        }
    }
    else if(scn == SCENE_GAME_OVER)
    {
        if(new_pad & PAD_START)
        {
            scn = SCENE_SHMUP;
        }
        if(new_pad & PAD_TRIANGLE)
        {
            break_from_main = 1;
            audsrv_set_volume(0);
            return;
        }
    }
    else
    {
        puts("Unknown scene in getpad");
    }
}

static void showLoadingScreen(GSGLOBAL* gs, GSFONTM* gsFont, gsColours* c)
{
    float centrex = gs->Width / 2.f;
    float centrey = gs->Height / 2.f;

    for(int i = 0; i < 2; i++)
    {
        gsKit_clear(gs, c->black);
        gsKit_fontm_print(gs, gsFont, centrex - 10*12, centrey - 12, 0, c->purple, "Loading...");
        gsKit_queue_exec(gs);
        gsKit_queue_reset(gs->Per_Queue);
        gsKit_sync_flip(gs);
    }
}

void wakeup(s32 alarm_id, u16 time, void* common)
{
    (void)alarm_id;
    (void)time;
    iWakeupThread(*(int*)common);
    ExitHandler();
}

int main(int argc, char **argv)
{
    srand((unsigned int)time(NULL)); // Note, doesn't return time since epoch
    puts("Init RPC");
    SifInitRpc(0);

    int ret;
    // DMA init
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    // Graphics init
    GSGLOBAL* gs = gsKit_init_global();
    GSFONTM* gsFont = gsKit_init_fontm();
    //GSFONTM* gsFontTable = gsKit_init_fontm();

    gsColours c = {
        .purple = GS_SETREG_RGBAQ(0x55, 0x1a, 0x8b, 0x80, 0x00),
        .black = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00),
        .white = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00),
        .whiter = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00),
        .whitenoa = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x00, 0x00),
        .trekieblue = GS_SETREG_RGBAQ(0x31, 0x8B, 0xFD, 0x80, 0x00),
        .pcbgreen = GS_SETREG_RGBAQ(0x12, 0x7D, 0x15, 0x80, 0x00),
        .shmupbg = GS_SETREG_RGBAQ(0x10, 0x10, 0x10, 0x00, 0x00),
        // resistor colours
        .brown = GS_SETREG_RGBAQ(0x66, 0x32, 0x34, 0x80, 0x00),
        .red = GS_SETREG_RGBAQ(0xFF, 0x00, 0x00, 0x80, 0x00),
        .orange = GS_SETREG_RGBAQ(0xFF, 0x66, 0x00, 0x80, 0x00),
        .yellow = GS_SETREG_RGBAQ(0xFF, 0xFF, 0x00, 0x80, 0x00),
        .green = GS_SETREG_RGBAQ(0x34, 0xCD, 0x32, 0x80, 0x00),
        .blue = GS_SETREG_RGBAQ(0x66, 0x66, 0xFF, 0x80, 0x00),
        .violet = GS_SETREG_RGBAQ(0xCD, 0x66, 0xFF, 0x80, 0x00),
        .grey = GS_SETREG_RGBAQ(0x93, 0x93, 0x93, 0x80, 0x00),
        .gold = GS_SETREG_RGBAQ(0xCD, 0x99, 0x32, 0x80, 0x00),
        .silver = GS_SETREG_RGBAQ(0xCD, 0xCD, 0xCD, 0x80, 0x00),
    };

    // more graphics init
    gs->PrimAlphaEnable = GS_SETTING_ON;
    gs->PrimAlpha = GS_BLEND_FRONT2BACK;
    gs->PSM = GS_PSM_CT32;
    gs->PSMZ = GS_PSMZ_16S;
    gs->ZBuffering = GS_SETTING_ON;
    gs->DoubleBuffering = GS_SETTING_ON;

    // Force PAL for now
    gs->Mode = GS_MODE_PAL;
    gs->Width = 640;
    gs->Height = 512;

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_PERSISTENT);
    

    gsKit_fontm_upload(gs, gsFont);
    //gsKit_fontm_upload(gs, gsFontTable);
    gsFont->Spacing = 0.7f;

    // Show loading screen
    showLoadingScreen(gs, gsFont, &c);

    // Init pad
    pad_init();

    GSTEXTURE Tex[SPRITE_LEN];
    memset(&Tex, 0, sizeof(GSTEXTURE) * SPRITE_LEN);
    puts("Loading sprites");
    // Load the sprites
    for(int i = 0; i < SPRITE_LEN; i++)
    {
        char path[50] = { 0 };
        sprintf(path, "ASSETS/GEAR/%u.PNG", (i + 43));
        ret = gsKit_texture_png(gs, Tex + i, fixPath(path));
        printf("load for %s ret %d\n", path, ret);

        int txsize = gsKit_texture_size(Tex[i].Width, Tex[i].Height, Tex[i].PSM);
        printf("Texture %u VRAM Range = 0x%X - 0x%X Size = %d\n\n", i, Tex[i].Vram, Tex[i].Vram + txsize - 1, txsize);
    }

    GSTEXTURE t = { 0 };
    ret = gsKit_texture_png(gs, &t, fixPath("ASSETS/UI/CURSOR.PNG"));
    printf("load for cursor.png ret %d\n", ret);
    int txsize = gsKit_texture_size(t.Width, t.Height, t.PSM);
    printf("Cursor VRAM Range = 0x%X - 0x%X Size = %d\n\n", t.Vram, t.Vram + txsize - 1, txsize);

    
    GSTEXTURE shipt = { 0 };
    ret = gsKit_texture_png(gs, &shipt, fixPath("ASSETS/SHIP/SHIP.PNG"));
    printf("load for ship.png ret %d\n", ret);
    txsize = gsKit_texture_size(shipt.Width, shipt.Height, shipt.PSM);
    printf("Ship VRAM Range = 0x%X - 0x%X Size = %d\n\n", t.Vram, t.Vram + txsize - 1, txsize);

    GSTEXTURE restexture = { 0 };
    ret = gsKit_texture_png(gs, &restexture, fixPath("ASSETS/RESISTOR/RESISTOR.PNG"));
    printf("load for resistor.png ret %d\n", ret);
    txsize = gsKit_texture_size(restexture.Width, restexture.Height, restexture.PSM);
    printf("resistor VRAM Range = 0x%X - 0x%X Size = %d\n\n", restexture.Vram, restexture.Vram + txsize - 1, txsize);

    puts("Create sema");

    ee_sema_t sema;
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    fillbuffer_sema = CreateSema(&sema);

    puts("Audsrv init");
    struct audsrv_fmt_t format;
    format.bits = 16;
    format.freq = 44100;
    format.channels = 2;

    ret = SifLoadModule("rom0:LIBSD", 0, NULL);
    printf("libsd loadmodule %d\n", ret);
    
    ret = SifLoadModule(fixPath("AUDSRV.IRX"), 0, NULL);
    printf("audsrv loadmodule %d\n", ret);

    ret = audsrv_init();
    if (ret != 0)
    {
        printf("sample: failed to initialize audsrv\n");
        printf("audsrv returned error string: %s\n", audsrv_get_error_string());
        PGM_END(1);
    }

    ret = audsrv_on_fillbuf(BUFFER_SIZE, fillbuffer, (void*)fillbuffer_sema);
    if (ret != 0)
    {
        printf("audsrv on fillbuff %d\n", ret);
        PGM_END(1);
    }

    ret = audsrv_set_format(&format);
    printf("set format returned %d\n", ret);

    audsrv_set_volume(75);
    
    puts("Audsrv adpcm");
    adpsamples samples;
    audsrv_adpcm_init();
    loadSamples(&samples);

    puts("Audsrv init end");

	int row;
    
    puts("Starting Audio Thread");
    initAudioThread();
    audsrv_play = 1;

    row = -1;

    unsigned char count = 0;
    unsigned int next_enemy_in = 0;
    float prev_x = -1.f;

    int thread = GetThreadId();
    
    // Set up game objects
    bullet bullets[MAX_BULLETS];
    memset(bullets, 0, sizeof(bullets));
    
    enemy enemies[MAX_ENEMIES];
    memset(enemies, 0, sizeof(enemies));

    player plr = {
        .x = 0.f,
        .y = gs->Height / 2,
        .w = t.Width,
        .h = t.Height,
        .score = 0,
    };
    
    volatile int allow_user_control = 1;
    unsigned int fall_counter = 0;

    // at least "Score: " + 11 (max int 32)
    char scorestr[50] = "Score: ";
    char resvalstr[200] = "Value: ";
    char ticksleftstr[50] = "Time Left: ";
    
    resistor resistors[MAX_RESISTORS];
    memset(resistors, 0, sizeof(resistors));
    resistor* selected_resistor = NULL;
    
    u64* resistorcolours[12] = 
    {
        &c.black,
        &c.brown,
        &c.red,
        &c.orange,
        &c.yellow,
        &c.green,
        &c.blue,
        &c.violet,
        &c.grey,
        &c.white,
        &c.gold,
        &c.silver,
    };
    
    unsigned int ticks_until_game_over = 10;

    while (1)
    {
        if(allow_user_control)
        {
            prev_x = plr.x;
            readPad(gs, &plr, &samples, bullets, resistors, &selected_resistor);
            if(break_from_main)
                break;

            // Limit cursor
            if(plr.x > gs->Width)
                plr.x = gs->Width;

            if(plr.x < 0.f)
                plr.x = 0.f;

            if(plr.y + plr.h > gs->Height)
                plr.y = gs->Height - plr.h;

            if(plr.y < 0.f)
                plr.y = 0;
        }
        else
        {
            // If it fell down on the ground
            if(plr.y > gs->Height)
            {
                allow_user_control = 1;
                plr.x = plr.y = 150.f;
                scn = (dismiss_explanation ? SCENE_REPAIR : SCENE_PRE_REPAIR);

                fall_counter = 0;
                generateResistors(gs, resistors, &restexture, &ticks_until_game_over);
                // Reset these for later
                next_enemy_in = 0;
                memset(enemies, 0, sizeof(enemies));

                gsKit_queue_exec(gs);
                gsKit_queue_reset(gs->Per_Queue);
                gsKit_sync_flip(gs);

                continue;
            }

            fall_counter++;
            count = 1;

            // Fall to the right, else to the left
            if(plr.x > prev_x)
                plr.x += fall_counter/4.f + 8;
            else
            {
                plr.x -= fall_counter/4.f + 8;
            }

            plr.y += fall_counter/2.f;
        }

        // Start drawing
        // We always start with this since scn is 0 (static)
        if(scn == SCENE_SHMUP)
        {
            gsKit_set_primalpha(gs, GS_BLEND_BACK2FRONT, 0);
            gsKit_set_test(gs, GS_ATEST_OFF);

            gsKit_clear(gs, c.shmupbg);
            gsKit_set_test(gs, GS_ATEST_ON);

            sprintf(scorestr + 7, "%d", plr.score);
            // Every time the counter reaches 0 (it wraps around), check to see how many more we have left until the next enemy spawn
            if(!(count % 31))
            {
                if(!next_enemy_in)
                {
                    int rnd = rand();
                    next_enemy_in = (rnd % 2) + 1;
                    //printf("Next in %d, rnd %d\n", next_enemy_in, rnd);
                    // spawn this many enemies at once
                    int ret = 0;
                    for(int i = 0; i < rnd % 3; i++)
                        ret += generateEnemy(gs, enemies, Tex);
                    // Play enemy spawned sound
                    if(ret)
                        audsrv_play_adpcm(&samples.enemy);
                }
                next_enemy_in--;
            }
            // Print score

            gsKit_fontm_print(gs, gsFont, (gs->Width / 2) - 10*12, 12.f, 2, c.purple, scorestr);

            // Draw ship
            gsKit_prim_sprite_texture(gs, &shipt, plr.x, plr.y, 0, 0,
                plr.x + shipt.Width, plr.y + shipt.Height, shipt.Width, shipt.Height, 4, c.white);

            renderBullets(gs, bullets, &c, enemies, &plr, &samples);
            gsKit_set_test(gs, GS_ATEST_ON);
            renderEnemies(gs, Tex, &c, enemies, &count, &plr, &samples, &allow_user_control);
            if(plr.score < -4)
            {
                scn = SCENE_GAME_OVER;
                // Reset things for next run
                memset(enemies, 0, sizeof(enemies));
                memset(bullets, 0, sizeof(bullet) * MAX_BULLETS);
                plr.x = 0.f;
                plr.y = gs->Height / 2;
                plr.score = 0;
            }

        } 
        else if (scn == SCENE_PRE_REPAIR)
        {
            gsKit_set_test(gs, GS_ATEST_OFF);
            gsKit_clear(gs, c.shmupbg);
            gsKit_set_test(gs, GS_ATEST_ON);
            gsKit_set_primalpha(gs, GS_BLEND_BACK2FRONT, 0);
            gsKit_fontm_print_scaled(gs, gsFont, 15.f, 15.f, 1, 0.6f, c.whiter, "You crashed your spaceship.\n"
            "Luckily you have the required tools\nand components to repair it!\n\n"
            "You will need to replace the blown resistors with\nnew ones in order to proceed.\n"
            "Be sure to act quick though, as your oxygen supply\nis running low.\n\n"
            "Press start to begin\n\n\n\n\n\n\n\n(The Select button also mutes the music)");
        }
        else if (scn == SCENE_GAME_OVER)
        {
            gsKit_set_test(gs, GS_ATEST_OFF);
            gsKit_clear(gs, c.shmupbg);
            gsKit_set_test(gs, GS_ATEST_ON);
            gsKit_set_primalpha(gs, GS_BLEND_BACK2FRONT, 0);
            gsKit_fontm_print_scaled(gs, gsFont, 130.f, 15.f, 1, 0.8f, c.whiter, "       GAME OVER!\n\nPress start to try again\nor Triangle to Quit\n");
        }
        else // SCENE_REPAIR
        {
            gsKit_set_test(gs, GS_ATEST_OFF);
            gsKit_set_primalpha(gs, GS_SETREG_ALPHA(2, 2, 2, 0, 0), 0);
            gsKit_clear(gs, c.shmupbg);            
            // Draw PCB
            gsKit_prim_sprite(gs, 10.f, 10.f, gs->Width / 2, gs->Height - 10.f, 0, c.pcbgreen);


            // Draw Resistor Table            
            gsKit_prim_sprite(gs, gs->Width / 2 + 10.f, 10.f, gs->Width - 10.f, gs->Height - 10.f, 0, c.black);

            // Draw table cells
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 26.f, gs->Width - 12.f, 26.f + 38.f, 1, c.black);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 64.f, gs->Width - 12.f, 64.f + 38.f, 1, c.brown);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 104.f, gs->Width - 12.f, 104.f + 38.f, 1, c.red);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 144.f, gs->Width - 12.f, 144.f + 38.f, 1, c.orange);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 184.f, gs->Width - 12.f, 184.f + 38.f, 1, c.yellow);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 224.f, gs->Width - 12.f, 224.f + 38.f, 1, c.green);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 264.f, gs->Width - 12.f, 264.f + 38.f, 1, c.blue);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 304.f, gs->Width - 12.f, 304.f + 38.f, 1, c.violet);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 344.f, gs->Width - 12.f, 344.f + 38.f, 1, c.grey);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 384.f, gs->Width - 12.f, 384.f + 38.f, 1, c.whiter);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 424.f, gs->Width - 12.f, 424.f + 38.f, 1, c.gold);
            gsKit_prim_sprite(gs, gs->Width / 2 + 12.f, 464.f, gs->Width - 12.f, 464.f + 38.f, 1, c.silver);

            gsKit_set_test(gs, GS_ATEST_ON);
            gsKit_set_primalpha(gs, GS_BLEND_BACK2FRONT, 0);

            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 15.f, 2, 0.6f, c.whiter, "Bands 1, 2   *   3rd Band");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 38.f, 2, 0.6f, c.whiter, "      0, 0          1 Ohm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 76.f, 2, 0.6f, c.black,  "      1, 1         10 Ohm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 116.f, 2, 0.6f, c.black, "      2, 2        100 Ohm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 156.f, 2, 0.6f, c.black, "      3, 3         1 KOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 196.f, 2, 0.6f, c.black, "      4, 4        10 KOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 236.f, 2, 0.6f, c.black, "      5, 5       100 KOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 276.f, 2, 0.6f, c.black, "      6, 6         1 MOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 316.f, 2, 0.6f, c.black, "      7, 7        10 MOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 356.f, 2, 0.6f, c.black, "      8, 8       100 MOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 396.f, 2, 0.6f, c.black, "      9, 9         1 GOhm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 436.f, 2, 0.6f, c.black, "                  0.1 Ohm");
            gsKit_fontm_print_scaled(gs, gsFont, gs->Width / 2 + 12.f, 476.f, 2, 0.6f, c.black, "                 0.01 Ohm");

            // Draw the current value text over the pcb
            if(selected_resistor)
            {
                // Print everything until the value in Ohms (without printing it)
                char* pstr = resvalstr + 7;
                float resistorval = resistorValueSuffix(selected_resistor);
                pstr += sprintf(pstr, "%g %s\n(", resistorval, selected_resistor->suffix);
                // If multiplier is gold or silver, then print the value as-is
                if(selected_resistor->bands[2] >= BAND_GOLD)
                {
                    pstr += sprintf(pstr, "%g", resistorval);
                }
                else
                {
                    int vlcnt = sprintf(pstr, "%u", resistorToOhmsNoMul(selected_resistor));
                    // Place pcnt to the end of the string
                    pstr += vlcnt;
                    // Append the correct number of zeroes to the string
                    // Needs to be done this way because the numbers are too big to fit/calculate reliably
                    vlcnt = selected_resistor->zeroes;
                    if(vlcnt > 0)
                    {
                        while(vlcnt--)
                        {
                            *pstr++ = '0';
                        }
                    }
                }
                // Continue printing the rest
                sprintf(pstr, " Ohm)\n%s %s %s", bandToString(selected_resistor->bands[0]), bandToString(selected_resistor->bands[1]), bandToString(selected_resistor->bands[2]));
                gsKit_fontm_print_scaled(gs, gsFont, 22.f, 15.f, 1, 0.6f, c.whiter, resvalstr);
            }
            else
            {
                gsKit_fontm_print_scaled(gs, gsFont, 22.f, 15.f, 1, 0.6f, c.whiter, "Use the cursor to select\nresistors and the X button\nto select a value.\nPress start when finished.");
            }

            // Draw resistors
            for(int i = 0; i < MAX_RESISTORS; i++)
            {
                // Resistors are always in series (no pun intended) in the array
                if(!(resistors + i)->active)
                    break;
                gsKit_prim_sprite_texture(gs, &restexture, (resistors + i)->x, (resistors + i)->y, 0, 0,
                    (resistors + i)->x + (resistors + i)->w, (resistors + i)->y + (resistors + i)->h, (resistors + i)->w, (resistors + i)->h, 1, c.white);

                // Draw the bands on top
                gsKit_set_test(gs, GS_ATEST_OFF);
                gsKit_set_primalpha(gs, GS_SETREG_ALPHA(2, 2, 2, 0, 0), 0);

                gsKit_prim_sprite(gs, (resistors + i)->x + 36.f, (resistors + i)->y + 5.f, (resistors + i)->x + 36.f + 4.f, (resistors + i)->y + (resistors + i)->h - 4.f, 1, **(resistorcolours + (resistors + i)->bands[0]));
                gsKit_prim_sprite(gs, (resistors + i)->x + 44.f, (resistors + i)->y + 5.f, (resistors + i)->x + 44.f + 4.f, (resistors + i)->y + (resistors + i)->h - 4.f, 1, **(resistorcolours + (resistors + i)->bands[1]));
                gsKit_prim_sprite(gs, (resistors + i)->x + 52.f, (resistors + i)->y + 5.f, (resistors + i)->x + 52.f + 4.f, (resistors + i)->y + (resistors + i)->h - 4.f, 1, **(resistorcolours + (resistors + i)->bands[2]));
                gsKit_set_test(gs, GS_ATEST_ON);
                gsKit_set_primalpha(gs, GS_BLEND_BACK2FRONT, 0);
                
                // Draw the text above the resistor
                gsKit_fontm_print_scaled(gs, gsFont, (resistors + i)->x + 15.f, (resistors + i)->y - 14.f, 1, 0.6f, c.whiter, (resistors + i)->solution->reqval);
                
                // Draw timeout
                gsKit_fontm_print_scaled(gs, gsFont, 22.f, gs->Height - 30.f, 1, 0.6f, c.whiter, ticksleftstr);
            }

            // Finally, draw the cursor, only if we don't have a selected resistor
            if(!selected_resistor)
                gsKit_prim_sprite_texture(gs, &t, plr.x, plr.y, 0, 0,
                    plr.x + t.Width, plr.y + t.Height, t.Width, t.Height, 4, c.white);

            if(!(count % 64))
            {
                // Decrement timeout
                ticks_until_game_over--;
                sprintf(ticksleftstr + 11, "%u", ticks_until_game_over);
                //printf("Tick %u\n", ticks_until_game_over);
            }
            if(!ticks_until_game_over)
            {
                scn = SCENE_GAME_OVER;
                selected_resistor = NULL;
                // Clear resistors and player data
                memset(resistors, 0, sizeof(resistor) * MAX_RESISTORS);
                plr.x = 0.f;
                plr.y = gs->Height / 2;
                plr.score = 0;
            }

        }

		gsKit_queue_exec(gs);
		gsKit_queue_reset(gs->Per_Queue);
        gsKit_sync_flip(gs);

        SetAlarm(150, &wakeup, &thread);
        SleepThread();
        count++;
    }

    audsrv_quit();

    free(threadAttr.stack);

    PGM_END(0);

	return 0;
}
