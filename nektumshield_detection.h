/*
 * ============================================================
 *  NEKTUM SHIELD - Detection Functions
 *  Version: 3.0 Refactored
 *  Developer: Nobody
 * ============================================================
 */

#ifndef NEKTUMSHIELD_DETECTION_H
#define NEKTUMSHIELD_DETECTION_H

#include "./pinc.h"
#include "nektumshield_dvars.h"

/* ========================= DETECTION TYPES ========================= */
typedef enum {
    DETECT_SNAP_AIM = 0,
    DETECT_SILENT_AIM,
    DETECT_BODY_LOCK,
    DETECT_PERFECT_SHOT,
    DETECT_HIPFIRE_KILL,
    DETECT_NORECOIL,
    DETECT_RECOIL_MACRO,
    DETECT_MOVING_HEADSHOT,
    DETECT_MACRO_CLICK,
    DETECT_MACRO_SCROLL,
    DETECT_MACRO_RATE,
    DETECT_MACRO_CONSISTENCY,
    DETECT_ADS_NORECOIL,
    DETECT_NOSPREAD,
    DETECT_TARGET_SWITCH,
    DETECT_ALT_TAB,
    DETECT_COUNT
} acDetectionType_t;

/* ========================= DETECTION RESULTS ========================= */
typedef struct {
    qboolean detected;
    float confidence;      /* 0.0 - 1.0 */
    int violationCount;    /* Number of violations in current window */
    char reason[64];       /* Detection reason string */
} acDetectionResult_t;

/* ========================= PLAYER DATA ========================= */
typedef struct {
    /* Basic stats */
    int kills;
    int headshots;
    int hsStreak;
    int blStreak;
    int acScore;
    char lastReason[64];
    float lastScoreUpdate;
    
    /* Position tracking */
    vec3_t lastAngles;
    vec3_t lastOrigin;
    int lastCmdTime;
    float lastMoveTime;
    
    /* Firing tracking */
    int firingFrames;
    int lastPitch;
    int lastYaw;
    int recoilZeroYawFrames;
    
    /* Perfect shot tracking */
    int perfectShots;
    float lastPerfectShotTime;
    float lastPerfectKillTime;
    int hipfireLongKills;
    float lastHipfireKillTime;
    float lastHipfireFlagTime;
    qboolean wasFiring;
    int lastCheckedClientCommand;
    
    /* Timing flags */
    float lastSnapAimTime;
    float lastNoRecoilTime;
    float lastHsRatioTime;
    float lastHsStreakTime;
    float lastBLStreakTime;
    int lastHsRatioCheckKills;
    
    /* Hit location tracking */
    int lastHitLoc;
    int sameHitStreak;
    float lastBodyLockTime;
    
    /* Recoil macro tracking */
    float pitchSum;
    float pitchSqSum;
    int recoilSamples;
    float lastMacroDetTime;
    
    /* Ban scheduling */
    qboolean pendingBan;
    char pendingReason[64];
    int scheduledDropTime;
    char dropReason[128];
    
    /* Name check */
    qboolean pendingNameCheck;
    int nameCheckStartTime;
    
    /* Mute status */
    qboolean isMuted;
    
    /* Streak timing */
    float firstHsInStreakTime;
    float firstBlInStreakTime;
    float lastSnapTime;
    float firstSameHitTime;
    
    /* Moving headshot tracking */
    int movingHeadshotCount;
    float lastMovingHsTime;
    
    /* Macro detection tracking */
    int logicClickCount;
    int logicWindowStart;
    float logicStrikes;
    int scrollFastCount;
    int lastScrollTime;
    int rateFastCount;
    int lastRateTime;
    int consistencyMatchCount;
    int lastConsistencyGap;
    int lastConsistencyTime;
    
    /* Silent aim tracking */
    int silentAimCount;
    float lastSilentAimTime;
    int adsZeroMoveFrames;
    
    /* No-spread tracking */
    int noSpreadHitCount;
    float lastNoSpreadHitTime;
    vec3_t lastHitPositions[5];
    int hitPosIndex;
    
    /* Kill tracking */
    int lastKillTime;
    vec3_t lastKillVictimOrigin;
    int rapidSwitchKills;
    
    /* Shot data */
    vec3_t lastShotAngles;
    vec3_t lastShotOrigin;
    float lastShotTime;
    qboolean hasShotData;
    qboolean isADS;
    qboolean lastShotADS;
    
} acPlayerData_t;

/* ========================= FUNCTIONS ========================= */

/**
 * Initialize player anti-cheat data
 * @param pData Pointer to player data structure
 */
void AC_InitPlayerData(acPlayerData_t* pData);

/**
 * Reset player anti-cheat data (on spawn/reconnect)
 * @param pData Pointer to player data structure
 */
void AC_ResetPlayerData(acPlayerData_t* pData);

/**
 * Check for snap aim behavior
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectSnapAim(client_t* cl, acPlayerData_t* pData);

/**
 * Check for silent aim behavior
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectSilentAim(client_t* cl, acPlayerData_t* pData);

/**
 * Check for body lock behavior
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectBodyLock(client_t* cl, acPlayerData_t* pData);

/**
 * Check for perfect shot patterns
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectPerfectShot(client_t* cl, acPlayerData_t* pData);

/**
 * Check for suspicious hipfire kills
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectHipfireKill(client_t* cl, acPlayerData_t* pData);

/**
 * Check for no-recoil behavior
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectNoRecoil(client_t* cl, acPlayerData_t* pData);

/**
 * Check for recoil macro patterns
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectRecoilMacro(client_t* cl, acPlayerData_t* pData);

/**
 * Check for moving headshot patterns
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectMovingHeadshot(client_t* cl, acPlayerData_t* pData);

/**
 * Check for macro click patterns
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectMacroClick(client_t* cl, acPlayerData_t* pData);

/**
 * Check for macro scroll patterns
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectMacroScroll(client_t* cl, acPlayerData_t* pData);

/**
 * Check for ADS no-recoil behavior
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectAdsNoRecoil(client_t* cl, acPlayerData_t* pData);

/**
 * Check for no-spread behavior
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectNoSpread(client_t* cl, acPlayerData_t* pData);

/**
 * Check for rapid target switching
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectTargetSwitch(client_t* cl, acPlayerData_t* pData);

/**
 * Check for alt-tab movement anomalies
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return Detection result
 */
acDetectionResult_t AC_DetectAltTab(client_t* cl, acPlayerData_t* pData);

/**
 * Process all detections for a player
 * @param cl Client pointer
 * @param pData Player anti-cheat data
 * @return qtrue if ban triggered, qfalse otherwise
 */
qboolean AC_ProcessDetections(client_t* cl, acPlayerData_t* pData);

#endif /* NEKTUMSHIELD_DETECTION_H */
