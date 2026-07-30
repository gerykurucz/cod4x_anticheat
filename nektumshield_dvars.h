/*
 * ============================================================
 *  NEKTUM SHIELD - DVar Definitions
 *  Version: 3.0 Refactored
 *  Developer: Nobody
 * ============================================================
 */

#ifndef NEKTUMSHIELD_DVARS_H
#define NEKTUMSHIELD_DVARS_H

#include "./pinc.h"

/* ========================= CVAR POINTERS ========================= */
extern CONVAR_T *cv_score_ban;
extern CONVAR_T *cv_hs_ratio;
extern CONVAR_T *cv_min_kills;
extern CONVAR_T *cv_snap_threshold;
extern CONVAR_T *cv_silent_aim_angle;
extern CONVAR_T *cv_silent_aim_dist;
extern CONVAR_T *cv_body_lock_streak;
extern CONVAR_T *cv_perfect_shots;
extern CONVAR_T *cv_perfect_yaw_pitch;
extern CONVAR_T *cv_hipfire_kills;
extern CONVAR_T *cv_hipfire_dist;
extern CONVAR_T *cv_norecoil_threshold;
extern CONVAR_T *cv_recoil_macro_var;
extern CONVAR_T *cv_recoil_macro_mean;
extern CONVAR_T *cv_recoil_samples;
extern CONVAR_T *cv_alttab_move_dist;
extern CONVAR_T *cv_alttab_move_time;
extern CONVAR_T *cv_drop_delay_ms;
extern CONVAR_T *cv_name_check_delay;

extern CONVAR_T *cv_time_hs_streak;
extern CONVAR_T *cv_time_hs_window;
extern CONVAR_T *cv_time_bl_streak;
extern CONVAR_T *cv_time_bl_window;
extern CONVAR_T *cv_snap_kill_angle;
extern CONVAR_T *cv_snap_kill_window;

extern CONVAR_T *cv_moving_hs_dist;
extern CONVAR_T *cv_moving_hs_streak;
extern CONVAR_T *cv_moving_hs_window;

extern CONVAR_T *cv_macro_logic_clicks;
extern CONVAR_T *cv_macro_logic_window;
extern CONVAR_T *cv_macro_logic_strikes;
extern CONVAR_T *cv_macro_scroll_count;
extern CONVAR_T *cv_macro_scroll_fast;
extern CONVAR_T *cv_macro_scroll_reset;
extern CONVAR_T *cv_macro_rate_count;
extern CONVAR_T *cv_macro_rate_fast;
extern CONVAR_T *cv_macro_consistency_count;
extern CONVAR_T *cv_macro_consistency_gap;

extern CONVAR_T *cv_silent_aim_threshold_hipfire;
extern CONVAR_T *cv_silent_aim_threshold_ads;
extern CONVAR_T *cv_silent_aim_streak;
extern CONVAR_T *cv_ads_norecoil_frames;

extern CONVAR_T *cv_nospread_max_deviation;
extern CONVAR_T *cv_nospread_streak;

extern CONVAR_T *cv_target_switch_time;
extern CONVAR_T *cv_target_switch_angle;
extern CONVAR_T *cv_target_switch_streak;

/* ========================= CVAR VALUES STRUCTURE ========================= */
typedef struct {
    /* Core detection thresholds */
    int score_ban;
    float hs_ratio;
    int min_kills;
    
    /* Snap detection */
    float snap_threshold;
    float snap_kill_angle;
    float snap_kill_window;
    
    /* Silent aim detection */
    float silent_aim_angle;
    float silent_aim_dist;
    float silent_aim_threshold_hipfire;
    float silent_aim_threshold_ads;
    int silent_aim_streak;
    
    /* Body lock detection */
    int body_lock_streak;
    
    /* Perfect shot detection */
    int perfect_shots;
    float perfect_yaw_pitch;
    
    /* Hipfire detection */
    int hipfire_kills;
    float hipfire_dist;
    
    /* Recoil detection */
    float norecoil_threshold;
    float recoil_macro_var;
    float recoil_macro_mean;
    int recoil_samples;
    
    /* Alt-tab detection */
    float alttab_move_dist;
    float alttab_move_time;
    
    /* Timing controls */
    int drop_delay_ms;
    int name_check_delay;
    
    /* Time-based streak detection */
    int time_hs_streak;
    float time_hs_window;
    int time_bl_streak;
    float time_bl_window;
    
    /* Moving headshot detection */
    float moving_hs_dist;
    int moving_hs_streak;
    float moving_hs_window;
    
    /* Macro detection */
    int macro_logic_clicks;
    int macro_logic_window;
    float macro_logic_strikes;
    int macro_scroll_count;
    int macro_scroll_fast;
    int macro_scroll_reset;
    int macro_rate_count;
    int macro_rate_fast;
    int macro_consistency_count;
    int macro_consistency_gap;
    
    /* ADS no-recoil detection */
    int ads_norecoil_frames;
    
    /* No-spread detection */
    float nospread_max_deviation;
    int nospread_streak;
    
    /* Target switch detection */
    float target_switch_time;
    float target_switch_angle;
    int target_switch_streak;
    
} acCvars_t;

extern acCvars_t cvars;

/* ========================= FUNCTIONS ========================= */

/**
 * Initialize all console variables with default values
 */
void AC_InitCvars(void);

/**
 * Update cvar values from server cvars
 * Should be called periodically to sync with server
 */
void AC_UpdateCvars(void);

#endif /* NEKTUMSHIELD_DVARS_H */
