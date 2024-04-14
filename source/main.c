/**
 * @file source/main.c
 * @brief Rock Paper Scissors for PS3 using Tiny3D
 * @author FrontendTitan
 *
 *      This file contains the main function for the Rock Paper Scissors project.
 *      Do not remove any include statements; they are essential for the PS3 development environment.
 *      Maybe you might want to add some more functionality, so it's reccomended to leave them.
 *
 *      Make sure your development environment is set up correctly.
 *      If Using PSDK3v2, if you get errors for includes, you can either fix them in your
 *      IDE, or you can ignore the errors by right clicking then see if there is a "quick fix"
 *      available that will hide the errors
 *
 *      Good luck!
 */
#include <unistd.h>
#include <net/net.h>
#include <libfont.h>
#include <assert.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <io/pad.h>
#include <sysmodule/sysmodule.h>
#include <pngdec/pngdec.h>
#include <tiny3d.h>

#include <ppu-lv2.h>
#include <rsx/rsx.h>
#include <rsx/commands.h>
#include <lv2/spu.h>
#include <lv2/memory.h>
#include <lv2/thread.h>
#include <sysutil/sysutil.h>
#include <rsx/commands.h>
#include <spurs/types.h>
#include <sysutil/video.h>
#include <lv2/cond.h>
#include <io/pad.h>
#include <lv2/mutex.h>
#include <lv2/systime.h>
#include <lv2/syscalls.h>
#include <sysutil/msg.h>
#include <sysutil/sysutil.h>
#include <sysutil/osk.h>
#include <lv2/cond.h>
#include <lv2/mutex.h>

#include "dejavusans_ttf_bin.h"
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>

int ttf_inited = 0;

FT_Library freetype;
FT_Face face;

int TTFLoadFont(char *path, void *from_memory, int size_from_memory)
{

    if (!ttf_inited)
        FT_Init_FreeType(&freetype);
    ttf_inited = 1;

    if (path)
    {
        if (FT_New_Face(freetype, path, 0, &face))
            return -1;
    }
    else
    {
        if (FT_New_Memory_Face(freetype, from_memory, size_from_memory, 0, &face))
            return -1;
    }

    return 0;
}

void TTFUnloadFont()
{
    FT_Done_FreeType(freetype);
    ttf_inited = 0;
}

/**
 * To be honest, I got no idea what is going on here,
 * so if this breaks, i cant help, at all, but ChatGPT
 * knows.
 */
void TTF_to_Bitmap(u8 chr, u8 *bitmap, short *w, short *h, short *y_correction)
{
    FT_Set_Pixel_Sizes(face, (*w), (*h));

    FT_GlyphSlot slot = face->glyph;

    memset(bitmap, 0, (*w) * (*h));

    if (FT_Load_Char(face, (char)chr, FT_LOAD_RENDER))
    {
        (*w) = 0;
        return;
    }

    int n, m, ww;

    *y_correction = (*h) - 1 - slot->bitmap_top;

    ww = 0;

    for (n = 0; n < slot->bitmap.rows; n++)
    {
        for (m = 0; m < slot->bitmap.width; m++)
        {

            if (m >= (*w) || n >= (*h))
                continue;

            bitmap[m] = (u8)slot->bitmap.buffer[ww + m];
        }

        bitmap += *w;

        ww += slot->bitmap.width;
    }

    *w = ((slot->advance.x + 31) >> 6) + ((slot->bitmap_left < 0) ? -slot->bitmap_left : 0);
    *h = slot->bitmap.rows;
}

// draw one background color in virtual 2D coordinates
void DrawBackground2D(u32 rgba)
{
    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(0, 0, 65535);
    tiny3d_VertexColor(rgba);

    tiny3d_VertexPos(847, 0, 65535);

    tiny3d_VertexPos(847, 511, 65535);

    tiny3d_VertexPos(0, 511, 65535);
    tiny3d_End();
}

// Function to generate a random number between 1 and 100
int generateRandomNumber()
{
    srand(time(NULL));
    return rand() % 100 + 1;
}

// Function to handle the Guess the Number game
void displayInstructions(float x)
{
    DrawString(x, 0, "Welcome to Rock Paper Scissors!");

    // Set font color for Square (Rock)
    // SetFontColor(0xFFC0CBFF, 0x0); // Light Pink
    DrawString(x, 40, "Square = Rock");

    // Set font color for Triangle (Paper)
    // SetFontColor(0x008000FF, 0x0); // Dark Green
    DrawString(x, 80, "Triangle = Paper");

    // Set font color for Circle (Scissors)
    // SetFontColor(0xDC143CFF, 0x0); // Crimson Red
    DrawString(x, 120, "Circle = Scissors");

    // Set font color to white for subsequent drawings
    // SetFontColor(0xFFFFFFFF, 0x0); // White
}

void drawScene()
{
    float x = 0.0;

    // Change to 2D context (remember it works with 848 x 512 as virtual coordinates)
    tiny3d_Project2D();

    DrawBackground2D(0x000000ff); // Black with full opacity
    SetFontSize(18, 32);

    // Add the Guess the Number game instructions
    displayInstructions(x);
}

void LoadTexture()
{
    u32 *texture_mem = tiny3d_AllocTexture(64 * 1024 * 1024); // allocate 64MB of space for textures (this pointer can be global)
    u32 *texture_pointer;                                     // use to asign texture space without changes texture_mem

    if (!texture_mem)
        return;

    texture_pointer = texture_mem;

    ResetFont();

    TTFLoadFont(NULL, (void *)dejavusans_ttf_bin, dejavusans_ttf_bin_size);
    texture_pointer = (u32 *)AddFontFromTTF((u8 *)texture_pointer, 32, 255, 32, 32, TTF_to_Bitmap);
    TTFUnloadFont();
}

padInfo padinfo;
padData paddata;

int playerChoice = 0; // 1 for Rock, 2 for Paper, 3 for Scissors
int winner = 0;       // 0 for no winner, 1 for player, 2 for computer, 3 for tie

void computeChoice(int computerChoice)
{
    if (playerChoice == computerChoice)
        winner = 3; // Tie
    else if ((playerChoice == 1 && computerChoice == 3) ||
             (playerChoice == 2 && computerChoice == 1) ||
             (playerChoice == 3 && computerChoice == 2))
        winner = 1; // Player wins
    else
        winner = 2; // Computer wins
}

s32 main(s32 argc, const char *argv[])
{
    tiny3d_Init(1024 * 1024);
    ioPadInit(7);
    LoadTexture();

    // Ok, everything is set up. Now for the main loop.
    // We'll only execute the loop once to get the user's input.
    int userChoice = 0; // 1 for Rock, 2 for Paper, 3 for Scissors

    /* DRAWING STARTS HERE */
    // Clear the screen, buffer Z, and initialize the environment to 2D
    tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

    // Enable alpha Test
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

    // Enable alpha blending.
    tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                     TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                     TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

    drawScene();
    tiny3d_Flip(); // Call it once after drawing the initial scene

    int eventHandled = 0;

    while (!eventHandled)
    {
        // Check for events
        ioPadGetInfo(&padinfo);

        int i;
        for (i = 0; i < MAX_PADS; i++)
        {
            if (padinfo.status[i])
            {
                ioPadGetData(i, &paddata);

                if (paddata.BTN_TRIANGLE)
                {
                    userChoice = 2;   // User selected Paper
                    eventHandled = 1; // Event handled, exit the loop
                }
                else if (paddata.BTN_SQUARE)
                {
                    userChoice = 1;   // User selected Rock
                    eventHandled = 1; // Event handled, exit the loop
                }
                else if (paddata.BTN_CIRCLE)
                {
                    userChoice = 3;   // User selected Scissors
                    eventHandled = 1; // Event handled, exit the loop
                }
            }
        }

        // Introduce a short delay to avoid excessive CPU usage
        sleep(0.010); // 10 milliseconds
    }

    // Randomly generated choice for the computer
    int computerChoice = rand() % 3 + 1;

    // Draw the result of the game
    drawResult(userChoice, computerChoice);

    sleep(3);

    return 0;
}

void drawResult(int userChoice, int computerChoice)
{
    float x = 0.0;

    // Clear the screen, buffer Z, and initialize the environment to 2D
    tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

    // Enable alpha Test
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

    // Enable alpha blending.
    tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                     TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                     TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

    // Draw the initial instructions again
    drawScene();

    // Draw the result of the game
    if (userChoice == computerChoice)
    {
        x = DrawString(x, 200, "It's a Tie");
    }
    else if ((userChoice == 1 && computerChoice == 3) ||
             (userChoice == 2 && computerChoice == 1) ||
             (userChoice == 3 && computerChoice == 2))
    {
        x = DrawString(x, 200, "You Win!");
    }
    else
    {
        x = DrawString(x, 200, "You Lost");
    }

    tiny3d_Flip(); // Flip the screen after drawing the result
}
