#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/BenGui/HudEditor.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Rando/MiscBehavior/ClockShuffle.h"
#include "2s2h/CustomMessage/CustomMessage.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "assets/interface/week_static/week_static.h"
#include "assets/archives/icon_item_static/icon_item_static_yar.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "assets/interface/message_static/message_static.h"
#include "assets/interface/nes_font_static/nes_font_static.h"
}

#define CVAR_NAME "gEnhancements.Songs.BetterSongOfDoubleTime"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

// Normalize's time so that 6am is considered 0, and anything before rolls over u16
#define CLOCK_TIME_NORMALIZED(time) ((u16)(time - CLOCK_TIME(6, 0)))
#define CLOCK_TIME_MINUTE_F CLOCK_TIME_F(0, 1)

#define COL_CHAN_MIX(c1, c2, m) (c1 - (s32)(c2 * m)) & 0xFF

typedef enum AdjustDirection {
    ADJUST_DIRECTION_NONE,
    ADJUST_DIRECTION_REVERSE,
    ADJUST_DIRECTION_FORWARD,
} AdjustDirection;

static bool sActivelyChangingTime = false;
static u16 sOriginalTime = CLOCK_TIME(0, 0);
static s32 sOriginalDay = 0;
static u16 sSelectedTime = CLOCK_TIME(0, 0);
static s32 sSelectedDay = 0;

extern void UpdateGameTime(u16 gameTime);

static const char* sDoWeekTableCopy[] = {
    gClockDay1stTex,
    gClockDay2ndTex,
    gClockDayFinalTex,
};

static HOOK_ID onPlayerUpdateHookId = 0;
static HOOK_ID onEnTest6KillHookId = 0;
static HOOK_ID onPlayDestroyHookId = 0;

Color_RGBA8 sArrowAnimColor = {};
static f32 sArrowAnimTween = 0.0f;
static f32 sStickAnimTween = 0.0f;
static s16 sArrowAnimState = 0;
static s16 sStickAnimState = 0;

// Tracks animation state for the arrows and control sticks
void UpdateStickDirectionPromptAnim() {
    f32 arrowAnimTween = sArrowAnimTween;
    f32 stickAnimTween = sStickAnimTween;

    if (sArrowAnimState == 0) {
        arrowAnimTween += 0.05f;
        if (arrowAnimTween > 1.0f) {
            arrowAnimTween = 1.0f;
            sArrowAnimState = 1;
        }
    } else {
        arrowAnimTween -= 0.05f;
        if (arrowAnimTween < 0.0f) {
            arrowAnimTween = 0.0f;
            sArrowAnimState = 0;
        }
    }
    sArrowAnimTween = arrowAnimTween;

    if (sStickAnimState == 0) {
        stickAnimTween += 0.1f;
        if (stickAnimTween > 1.0f) {
            stickAnimTween = 1.0f;
            sStickAnimState = 1;
        }
    } else {
        stickAnimTween = 0.0f;
        sStickAnimState = 0;
    }
    sStickAnimTween = stickAnimTween;

    sArrowAnimColor.r = COL_CHAN_MIX(255, 155.0f, arrowAnimTween);
    sArrowAnimColor.g = COL_CHAN_MIX(255, 155.0f, arrowAnimTween);
    sArrowAnimColor.b = COL_CHAN_MIX(0, -100.0f, arrowAnimTween);
    sArrowAnimColor.a = COL_CHAN_MIX(200, 50.0f, arrowAnimTween);
}

void OnPlayerUpdate(Actor* actor) {
    if (!sActivelyChangingTime) {
        S2H::GameHooks::UnregisterForID<GameInteractor::OnActorUpdate>(onPlayerUpdateHookId);
        onPlayerUpdateHookId = 0;
        return;
    }

    UpdateStickDirectionPromptAnim();

    // #678: this TU is in the single-exe link from now on, so PR #673's
    // criterion applies to every play-state deref below it -- the same #516
    // SIGSEGV class the Before/AfterInterfaceClockDraw pair was guarded for.
    // Placed AFTER the sActivelyChangingTime early-out above on purpose: that
    // block is this hook's own self-unregistration and must keep running with
    // or without a play state. Everything from here down reads or writes
    // through MM_gPlayState.
    RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);

    MM_gPlayState->interfaceCtx.bAlpha = 255;

    Input* input = &MM_gPlayState->state.input[0];

    // Pressing B should cancel the song
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        Audio_PlaySfx_MessageCancel();
        MM_gPlayState->msgCtx.ocarinaMode = OCARINA_MODE_END;
        sActivelyChangingTime = false;
        return;
    }

    // Pressing A should confirm the song
    if (CHECK_BTN_ALL(input->press.button, BTN_A) && MM_gPlayState->msgCtx.msgMode == MSGMODE_NONE) {
        // Check if the selected time is owned in ClockShuffle mode
        if (!Rando::ClockShuffle::IsTimeOwnedForClockShuffle(sSelectedDay, sSelectedTime)) {
            // Play error sound
            Audio_PlaySfx(NA_SE_SY_OCARINA_ERROR);

            // Get time description for the error message
            std::string timeDescription =
                Rando::ClockShuffle::GetTimeDescriptionForMessage(sSelectedDay, sSelectedTime);

            // Show message FIRST
            CustomMessage::StartTextbox(timeDescription + " is beyond your reach!");
            return;
        }

        Audio_PlaySfx_MessageDecide();
        MM_gPlayState->msgCtx.ocarinaMode = OCARINA_MODE_APPLY_DOUBLE_SOT;
        sActivelyChangingTime = false;

        // Use a hook for when the song of double time cutscene is finished to reload the scene via a respawn
        // This is to ensure that no actors or scripts are processed with the new time on the fly,
        // and everything is reloaded in a fresh state
        onEnTest6KillHookId = S2H::GameHooks::RegisterForID<GameInteractor::OnActorKill>(
            ACTOR_EN_TEST6, [](Actor* actor) {
                // #678, per PR #673's criterion. GET_PLAYER walks
                // play->actorCtx, so this is a deref on the first line.
                // Returning early leaves the hook REGISTERED rather than
                // unregistering it at the bottom, which is the right trade:
                // the only thing it would miss is one respawn, and it fires
                // again on the next EN_TEST6 kill.
                RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
                Player* player = GET_PLAYER(MM_gPlayState);

                MM_gPlayState->nextEntrance = gSaveContext.save.entrance;
                MM_gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                MM_gPlayState->transitionType = TRANS_TYPE_FADE_BLACK_FAST;

                MM_Play_SetRespawnData(MM_gPlayState, RESPAWN_MODE_RETURN, gSaveContext.save.entrance,
                                    MM_gPlayState->roomCtx.curRoom.num, PLAYER_PARAMS(0xFF, PLAYER_START_MODE_B),
                                    &player->actor.world.pos, player->actor.world.rot.y);
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
                gSaveContext.respawnFlag = 2;

                // Stop BGM so that new day sequences can play
                gSaveContext.seqId = NA_BGM_DISABLED;

                S2H::GameHooks::UnregisterForID<GameInteractor::OnActorKill>(onEnTest6KillHookId);
                onEnTest6KillHookId = 0;
            });

        // Use a hook to apply the new day and time before respawning
        onPlayDestroyHookId = S2H::GameHooks::Register<GameInteractor::OnPlayDestroy>([]() {
            gSaveContext.save.day = sSelectedDay;
            gSaveContext.save.time = sSelectedTime;

            S2H::GameHooks::Unregister<GameInteractor::OnPlayDestroy>(onPlayDestroyHookId);
            onPlayDestroyHookId = 0;
        });

        return;
    }

    AdjustDirection adjustMode = ADJUST_DIRECTION_NONE;
    f32 interval = (CLOCK_TIME_MINUTE_F * 30);

    static s8 sDPadRepeatState = 0;
    static s8 sDPadRepeatTimer = 0;

    // Check for DPad movement first, inheriting full speed
    if (CHECK_BTN_ALL(input->cur.button, BTN_DLEFT)) {
        if (sDPadRepeatState == -1) {
            sDPadRepeatTimer--;
            if (sDPadRepeatTimer < 0) {
                // Allow the input to register and apply the delay for all subsequent repeated inputs
                sDPadRepeatTimer = 2;
                adjustMode = ADJUST_DIRECTION_REVERSE;
            }
        } else {
            // Allow the input to register and apply the delay for the first repeated input
            sDPadRepeatTimer = 10;
            sDPadRepeatState = -1;
            adjustMode = ADJUST_DIRECTION_REVERSE;
        }
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_DRIGHT)) {
        if (sDPadRepeatState == 1) {
            sDPadRepeatTimer--;
            if (sDPadRepeatTimer < 0) {
                // Allow the input to register and apply the delay for all subsequent repeated inputs
                sDPadRepeatTimer = 2;
                adjustMode = ADJUST_DIRECTION_FORWARD;
            }
        } else {
            // Allow the input to register and apply the delay for the first repeated input
            sDPadRepeatTimer = 10;
            sDPadRepeatState = 1;
            adjustMode = ADJUST_DIRECTION_FORWARD;
        }
    } else {
        sDPadRepeatState = 0;
    }

    // Then analog stick direction, clamped to 30 minutes
    if (input->rel.stick_x < -5) {
        adjustMode = ADJUST_DIRECTION_REVERSE;
        interval = CLOCK_TIME_MINUTE_F * CLAMP_MIN(input->rel.stick_x / -2, -30);
    } else if (input->rel.stick_x > 5) {
        adjustMode = ADJUST_DIRECTION_FORWARD;
        interval = CLOCK_TIME_MINUTE_F * CLAMP_MAX(input->rel.stick_x / 2, 30);
    }

    if (CHECK_BTN_ALL(input->cur.button, BTN_Z)) { // Holding Z slows the interval
        interval /= 6;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_R)) { // Holding R speeds up the interval
        interval *= 2;
    }

    if (adjustMode == ADJUST_DIRECTION_FORWARD) { // Advance time
        u16 newTime = sSelectedTime + interval;
        if (sSelectedDay == 3 && sSelectedTime < CLOCK_TIME(6, 0) && newTime > CLOCK_TIME(6, 0) - CLOCK_TIME_HOUR) {
            newTime = CLOCK_TIME(6, 0) - CLOCK_TIME_HOUR;

            // If BSoDT was played while already in the final hour, don't allow the clamp to place the new time behind
            // the original time
            if (sOriginalDay == 3 && CLOCK_TIME_NORMALIZED(newTime) < CLOCK_TIME_NORMALIZED(sOriginalTime)) {
                newTime = sOriginalTime;
            }
        }
        // Day incrementing
        if (newTime > CLOCK_TIME(6, 0) && sSelectedTime < CLOCK_TIME(6, 0)) {
            sSelectedDay = CLAMP(sSelectedDay + 1, sOriginalDay, 3);
        }
        sSelectedTime = newTime;
    } else if (adjustMode == ADJUST_DIRECTION_REVERSE) { // Reverse time
        u16 newTime = sSelectedTime - interval;
        if (sSelectedDay == sOriginalDay && (CLOCK_TIME_NORMALIZED(newTime) < CLOCK_TIME_NORMALIZED(sOriginalTime) ||
                                             interval > CLOCK_TIME_NORMALIZED(sSelectedTime))) {
            newTime = sOriginalTime;
        }
        // Day decrementing
        if (newTime < CLOCK_TIME(6, 0) && sSelectedTime > CLOCK_TIME(6, 0)) {
            sSelectedDay = CLAMP(sSelectedDay - 1, sOriginalDay, 3);
        }
        sSelectedTime = newTime;
    }
}

void UpdateDayTexture(PlayState* play, s16 day) {
    s16 i = day - 1;

    // i is used to store dayMinusOne
    if ((i < 0) || (i >= 3)) {
        i = 0;
    }

    play->interfaceCtx.doActionSegment[DO_ACTION_SEG_CLOCK].mainTex = (char*)sDoWeekTableCopy[i];
}

Gfx* DrawTexRectI4(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                   s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy) {
    gDPLoadTextureBlock_4b(gfx++, texture, G_IM_FMT_I, textureWidth, textureHeight, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPWideTextureRectangle(gfx++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                            (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, dsdx, dtdy);

    return gfx;
}

Gfx* DrawTexRectIA8(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                    s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy) {
    gDPLoadTextureBlock(gfx++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, textureWidth, textureHeight, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 4, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPWideTextureRectangle(gfx++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                            (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, dsdx, dtdy);

    return gfx;
}

void DrawIndicators() {
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);

    Gfx_SetupDL39_Overlay(MM_gPlayState->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sArrowAnimColor.r, sArrowAnimColor.g, sArrowAnimColor.b, sArrowAnimColor.a);

    // Render the left/right arrows
    OVERLAY_DISP =
        DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gArrowCursorTex, 16, 24, 26, 191, 16, 24, -1 << 10, 1 << 10);
    OVERLAY_DISP =
        DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gArrowCursorTex, 16, 24, 278, 191, 16, 24, 1 << 10, 1 << 10);

    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 200, 200, 180);

    // Render the control stick icon with movement applied
    OVERLAY_DISP = DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gControlStickTex, 16, 16, 42 - (8.0f * sStickAnimTween),
                                  195, 16, 16, -1 << 10, 1 << 10);
    OVERLAY_DISP = DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gControlStickTex, 16, 16, 262 + (8.0f * sStickAnimTween),
                                  195, 16, 16, 1 << 10, 1 << 10);

    // Render two small black squares behind the Z/R buttons to fill in the cutout with black
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, 255);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPFillRectangle(OVERLAY_DISP++, 58 + 2, 196 + 2, 58 + 2 + 9, 196 + 2 + 10);
    gDPFillRectangle(OVERLAY_DISP++, 248 + 2, 196 + 2, 248 + 2 + 9, 196 + 2 + 10);

    Gfx_SetupDL39_Overlay(MM_gPlayState->state.gfxCtx);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetAlphaCompare(OVERLAY_DISP++, G_AC_NONE);
    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);

    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);

    // Render a Z and R button on the left/right side
    OVERLAY_DISP =
        DrawTexRectI4(OVERLAY_DISP, (TexturePtr)gMsgCharB5ButtonZTex, 16, 16, 58, 196, 16, 16, 1 << 10, 1 << 10);
    OVERLAY_DISP =
        DrawTexRectI4(OVERLAY_DISP, (TexturePtr)gMsgCharB4ButtonRTex, 16, 16, 248, 196, 16, 16, 1 << 10, 1 << 10);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
}

void RegisterBetterSongOfDoubleTime() {
    COND_HOOK(OnSceneInit, CVAR, [](s8 sceneId, s8 spawnNum) {
        // In case we didn't properly reset our variables
        sActivelyChangingTime = false;
        sOriginalTime = CLOCK_TIME(0, 0);
        sOriginalDay = 0;
        sSelectedTime = CLOCK_TIME(0, 0);
        sSelectedDay = 0;

        S2H::GameHooks::UnregisterForID<GameInteractor::OnActorKill>(onEnTest6KillHookId);
        S2H::GameHooks::Unregister<GameInteractor::OnPlayDestroy>(onPlayDestroyHookId);

        onEnTest6KillHookId = 0;
        onPlayDestroyHookId = 0;
    });

    // Hijack the time and day values on the save before drawing the clock so that it renders our selected time
    COND_HOOK(BeforeInterfaceClockDraw, CVAR, []() {
        if (sActivelyChangingTime) {
            // #438: guard landed with the Before/AfterInterfaceClockDraw
            // dispatch, which was wired as a pair. UpdateDayTexture writes
            // play->interfaceCtx.doActionSegment[...].mainTex, so a NULL
            // MM_gPlayState here is a write through a null pointer, not just a
            // read -- the #516 SIGSEGV class with a store. sActivelyChangingTime
            // is a file static that no play teardown clears, so reaching this
            // line without a play state is possible rather than theoretical.
            //
            // Returning early is the correct behaviour and not a partial one:
            // the pair's contract is that After undoes exactly what Before did,
            // and the same guard on the After leg keeps them symmetric. The
            // save-field writes are deliberately left INSIDE the guard for the
            // same reason -- writing time/day here with no clock draw to consume
            // them would leave the player's real time overwritten, which is the
            // half-wired failure the pair exists to prevent.
            //
            // Expands to nothing outside the single exe; see GameInteractor.h
            // for why it is a macro and not an #ifdef.
            RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
            gSaveContext.save.time = sSelectedTime;
            gSaveContext.save.day = sSelectedDay;
            UpdateDayTexture(MM_gPlayState, CURRENT_DAY);

            HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_VANILLA);
        }
    });

    // Return everything back to normal after drawing the clock
    COND_HOOK(AfterInterfaceClockDraw, CVAR, []() {
        if (sActivelyChangingTime) {
            // The symmetric half of the guard on the Before leg above (#438).
            // If Before returned early it wrote nothing, so skipping the restore
            // here is what keeps the pair balanced.
            RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
            gSaveContext.save.time = sOriginalTime;
            gSaveContext.save.day = sOriginalDay;
            UpdateDayTexture(MM_gPlayState, CURRENT_DAY);

            HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_NONE);
        }
    });

    COND_VB_SHOULD(VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT, CVAR, {
        // #678, per PR #673's criterion. Every branch below writes through
        // MM_gPlayState (msgCtx.ocarinaMode, MM_Message_StartTextbox,
        // Interface_SetAButtonDoAction).
        //
        // ORDER IS LOAD-BEARING: this sits BEFORE `*should = false`, so a
        // NULL play state leaves the vanilla verdict untouched and z_message.c
        // shows the stock prompt. Guarding after the assignment would suppress
        // vanilla and then substitute nothing, which is the half-wired failure
        // -- the player would play the song and get no prompt at all.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);

        *should = false;

        if (gSaveContext.save.day >= 4) {
            // On 4th day and beyond, just display the "can't go further" text
            MM_Message_StartTextbox(MM_gPlayState, 0x1B94, NULL);
            MM_gPlayState->msgCtx.ocarinaMode = OCARINA_MODE_END;
            return;
        } else if (gSaveContext.save.day <= 0) {
            // On 0th day, display the "notes echoed" text
            MM_Message_StartTextbox(MM_gPlayState, 0x1B95, NULL);
            MM_gPlayState->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
            return;
        }

        Interface_SetAButtonDoAction(MM_gPlayState, DO_ACTION_DECIDE);
        Interface_SetHudVisibility(HUD_VISIBILITY_A_B);

        MM_gPlayState->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_DOUBLE_TIME;
        sActivelyChangingTime = true;
        sOriginalTime = CURRENT_TIME;
        sOriginalDay = gSaveContext.save.day;
        sSelectedTime = CURRENT_TIME;
        sSelectedDay = gSaveContext.save.day;

        onPlayerUpdateHookId = S2H::GameHooks::RegisterForID<GameInteractor::OnActorUpdate>(
            ACTOR_PLAYER, OnPlayerUpdate);
    });

    COND_VB_SHOULD(VB_PREVENT_CLOCK_DISPLAY, CVAR, {
        if (!sActivelyChangingTime) {
            return;
        }

        // #678, per PR #673's criterion: DrawIndicators opens display lists on
        // MM_gPlayState->state.gfxCtx. This body never writes *should, so the
        // early return is behaviour-neutral -- it draws nothing, exactly as it
        // does when sActivelyChangingTime is false. sActivelyChangingTime is a
        // file static no play teardown clears, so reaching here with no play
        // state is possible rather than theoretical (the same reasoning as the
        // clock-draw pair above).
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);

        DrawIndicators();
    });

    COND_VB_SHOULD(VB_ALLOW_SONG_DOUBLE_TIME_ON_FINAL_NIGHT, CVAR, { *should = true; });
}

static RegisterShipInitFunc initFunc(RegisterBetterSongOfDoubleTime, { CVAR_NAME });
