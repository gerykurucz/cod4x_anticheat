/*
 * ============================================================
 *  NEKTUM SHIELD - COD4X Anti-Cheat & Server Management Plugin
 *  Version: 2.8 Final
 *  Developer: Nobody
 * ============================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include "./pinc.h"

/* ========================= CONSTANTS ========================= */
#define MAXP 64
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WEBHOOK_URL "https://discord.com/api/webhooks/1521222250349727815/IBbRXnqCOIG67PGZwRgeJvwltDzfBFR0W1y6d7iw1yZYIHyDQbOYNvSK-AVOsk9GMJGa"

#define BAN_DVAR "cl_ext_status"
#define BAN_DVAR_VALUE 5281
#define STAT_CHEATER_MARK 5981
#define STAT_NEKTUM_ID 5982

#define MOD_PISTOL_BULLET 1
#define MOD_RIFLE_BULLET 2
#define MOD_HEAD_SHOT 6

#define MAX_DISCORD_QUEUE 16
#define MAX_BANS 2048
#define MAX_USERS 4096
#define MAX_MUTES 256
#define PERMA_BAN_MINUTES 5256000

#define BANLIST_FILE "nektumshield_banlist.txt"
#define USERS_FILE "nektumshield_users.txt"
#define MUTES_FILE "nektumshield_mutes.txt"
#define LOG_FILE "nektumshield.log"

extern char* Info_ValueForKey(const char *s, const char *key);

#ifndef VectorCopy
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#endif

typedef struct gclient_s gclient_t;

/* ========================= GLOBAL STATE ========================= */
static FILE *ac_log;
static ftRequest_t* activeWebhookRequest = NULL;
static int webhookStartTime = 0;

static char discordQueue[MAX_DISCORD_QUEUE][2200];
static int discordQueueHead = 0;
static int discordQueueTail = 0;
static int discordQueueCount = 0;

static uint64_t lastBlockedPID = 0;
static time_t lastBlockedTime = 0;
static char lastBlockedIP[64] = "";

/* ========================= PLAYER DATA ========================= */
typedef struct {
    int kills;
    int headshots;
    int hsStreak;
    int blStreak;
    int acScore;
    char lastReason[64];
    float lastScoreUpdate;
    vec3_t lastAngles;
    vec3_t lastOrigin;
    int lastCmdTime;
    float lastMoveTime;

    int firingFrames;
    int lastPitch;
    int lastYaw;
    int recoilZeroYawFrames;

    int perfectShots;
    float lastPerfectShotTime;
    float lastPerfectKillTime;
    int hipfireLongKills;
    float lastHipfireKillTime;
    float lastHipfireFlagTime;
    qboolean wasFiring;
    int lastCheckedClientCommand;

    float lastSnapAimTime;
    float lastNoRecoilTime;
    float lastHsRatioTime;
    float lastHsStreakTime;
    float lastBLStreakTime;
    int lastHsRatioCheckKills;

    int lastHitLoc;
    int sameHitStreak;
    float lastBodyLockTime;

    float pitchSum;
    float pitchSqSum;
    int recoilSamples;
    float lastMacroDetTime;

    qboolean pendingBan;
    char pendingReason[64];
    int scheduledDropTime;
    char dropReason[128];
    
    qboolean pendingNameCheck;
    int nameCheckStartTime;
    qboolean isMuted;

    float firstHsInStreakTime;
    float firstBlInStreakTime;
    float lastSnapTime;
    float firstSameHitTime;

    int movingHeadshotCount;
    float lastMovingHsTime;

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

    int silentAimCount;
    float lastSilentAimTime;
    int adsZeroMoveFrames;

    int noSpreadHitCount;
    float lastNoSpreadHitTime;
    vec3_t lastHitPositions[5];
    int hitPosIndex;

    int lastKillTime;
    vec3_t lastKillVictimOrigin;
    int rapidSwitchKills;

    vec3_t lastShotAngles;
    vec3_t lastShotOrigin;
    float lastShotTime;
    qboolean hasShotData;
    qboolean isADS;
    qboolean lastShotADS;
} acData_t;

static acData_t pData[MAXP];

/* ========================= DATABASE STRUCTURES ========================= */
typedef struct {
    uint64_t playerid;
    uint64_t steamid;
    char name[64];
    char subnet24[20];
    char reason[128];
    char admin[64];
    time_t banTime;
} nsBan_t;
static nsBan_t g_banlist[MAX_BANS];
static int g_numBans = 0;

typedef struct {
    uint64_t playerid;
    uint64_t steamid;
    char name[64];
    char subnet24[20];
    int nektumID;
} nsUser_t;
static nsUser_t g_users[MAX_USERS];
static int g_numUsers = 0;
static int g_nextNektumID = 10000;

typedef struct {
    uint64_t playerid;
    uint64_t steamid;
    char name[64];
    char admin[64];
    char reason[128];
    time_t muteTime;
} nsMute_t;
static nsMute_t g_mutedPlayers[MAX_MUTES];
static int g_numMutes = 0;

/* ========================= CVARS ========================= */
static CONVAR_T *cv_score_ban;
static CONVAR_T *cv_hs_ratio;
static CONVAR_T *cv_min_kills;
static CONVAR_T *cv_snap_threshold;
static CONVAR_T *cv_silent_aim_angle;
static CONVAR_T *cv_silent_aim_dist;
static CONVAR_T *cv_body_lock_streak;
static CONVAR_T *cv_perfect_shots;
static CONVAR_T *cv_perfect_yaw_pitch;
static CONVAR_T *cv_hipfire_kills;
static CONVAR_T *cv_hipfire_dist;
static CONVAR_T *cv_norecoil_threshold;
static CONVAR_T *cv_recoil_macro_var;
static CONVAR_T *cv_recoil_macro_mean;
static CONVAR_T *cv_recoil_samples;
static CONVAR_T *cv_alttab_move_dist;
static CONVAR_T *cv_alttab_move_time;
static CONVAR_T *cv_drop_delay_ms;
static CONVAR_T *cv_name_check_delay;

static CONVAR_T *cv_time_hs_streak;
static CONVAR_T *cv_time_hs_window;
static CONVAR_T *cv_time_bl_streak;
static CONVAR_T *cv_time_bl_window;
static CONVAR_T *cv_snap_kill_angle;
static CONVAR_T *cv_snap_kill_window;

static CONVAR_T *cv_moving_hs_dist;
static CONVAR_T *cv_moving_hs_streak;
static CONVAR_T *cv_moving_hs_window;

static CONVAR_T *cv_macro_logic_clicks;
static CONVAR_T *cv_macro_logic_window;
static CONVAR_T *cv_macro_logic_strikes;
static CONVAR_T *cv_macro_scroll_count;
static CONVAR_T *cv_macro_scroll_fast;
static CONVAR_T *cv_macro_scroll_reset;
static CONVAR_T *cv_macro_rate_count;
static CONVAR_T *cv_macro_rate_fast;
static CONVAR_T *cv_macro_consistency_count;
static CONVAR_T *cv_macro_consistency_gap;

static CONVAR_T *cv_silent_aim_threshold_hipfire;
static CONVAR_T *cv_silent_aim_threshold_ads;
static CONVAR_T *cv_silent_aim_streak;
static CONVAR_T *cv_ads_norecoil_frames;

static CONVAR_T *cv_nospread_max_deviation;
static CONVAR_T *cv_nospread_streak;

static CONVAR_T *cv_target_switch_time;
static CONVAR_T *cv_target_switch_angle;
static CONVAR_T *cv_target_switch_streak;

typedef struct {
    int score_ban;
    float hs_ratio;
    int min_kills;
    float snap_threshold;
    float silent_aim_angle;
    float silent_aim_dist;
    int body_lock_streak;
    int perfect_shots;
    float perfect_yaw_pitch;
    int hipfire_kills;
    float hipfire_dist;
    float norecoil_threshold;
    float recoil_macro_var;
    float recoil_macro_mean;
    int recoil_samples;
    float alttab_move_dist;
    float alttab_move_time;
    int drop_delay_ms;
    int name_check_delay;
    
    int time_hs_streak;
    float time_hs_window;
    int time_bl_streak;
    float time_bl_window;
    float snap_kill_angle;
    float snap_kill_window;
    
    float moving_hs_dist;
    int moving_hs_streak;
    float moving_hs_window;

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
    
    float silent_aim_threshold_hipfire;
    float silent_aim_threshold_ads;
    int silent_aim_streak;
    int ads_norecoil_frames;

    float nospread_max_deviation;
    int nospread_streak;
    float target_switch_time;
    float target_switch_angle;
    int target_switch_streak;

} acCvars_t;

static acCvars_t cvars;

/* ========================= HELPERS ========================= */
char* AC_va(const char* format, ...) {
    static char buffers[4][1024];
    static int idx = 0;
    char* buffer = buffers[idx];
    idx = (idx + 1) & 3;
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, 1024, format, argptr);
    va_end(argptr);
    return buffer;
}

static inline qboolean IsBot(client_t *cl) {
    if (!cl) return qtrue;
    if (cl->netchan.remoteAddress.type == NA_BOT) return qtrue;
    if (cl->steamid == 0 && cl->playerid == 0) return qtrue;
    return qfalse;
}

void StripColorCodes(const char* in, char* out, int outSize) {
    int i = 0, j = 0;
    while (in[i] && j < outSize - 1) {
        if (in[i] == '^' && in[i+1] != '\0') { i += 2; } 
        else { out[j++] = in[i++]; }
    }
    out[j] = '\0';
}

float GetYawDist(float yaw1, float yaw2) {
    float diff = fabsf(yaw1 - yaw2);
    if (diff > 180.0f) diff = 360.0f - diff;
    return diff;
}

float ShortToAngle(int s) {
    return (float)(s & 65535) * (360.0f / 65536.0f);
}

void GetSubnet24(const char* ip, char* out, int outSize) {
    strncpy(out, ip, outSize - 1);
    out[outSize - 1] = '\0';
    char* lastDot = strrchr(out, '.');
    if (lastDot) *lastDot = '\0';
    else out[0] = '\0';
}

static inline void ExtractSubnet(const char* query, char* subnet, int outSize) {
    int dots = 0;
    for (int i = 0; query[i]; i++) if (query[i] == '.') dots++;
    if (dots == 3) GetSubnet24(query, subnet, outSize);
    else snprintf(subnet, outSize, "%s", query);
}

static inline qboolean IsIPAddress(const char* str) {
    int dots = 0, nums = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '.') dots++;
        else if (str[i] >= '0' && str[i] <= '9') nums++;
        else return qfalse;
    }
    return ((dots == 2 || dots == 3) && nums >= 4);
}

static inline qboolean IsNumeric(const char* str) {
    if (!str[0]) return qfalse;
    int len = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9') return qfalse;
        len++;
    }
    return (len >= 5);
}

static inline qboolean IsCGNAT(const char* ip) {
    if (!ip || ip[0] == '\0') return qfalse;
    if (strncmp(ip, "100.", 4) == 0) {
        int second = atoi(ip + 4);
        if (second >= 64 && second <= 127) return qtrue;
    }
    if (strncmp(ip, "188.156.", 8) == 0 || strncmp(ip, "188.157.", 8) == 0 ||
        strncmp(ip, "176.63.", 7) == 0 || strncmp(ip, "176.64.", 7) == 0 ||
        strncmp(ip, "176.65.", 7) == 0) return qtrue;
    return qfalse;
}

static inline void SendResponse(int slot, const char* format, ...) {
    static char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    if (slot >= 0 && slot < MAXP) Plugin_ChatPrintf(slot, "%s", buffer);
    else Plugin_ChatPrintf(-1, "%s", buffer);
}

static inline qboolean IsSameIdentity(const nsUser_t* user, uint64_t pid, uint64_t sid, const char* subnet, int clientStat) {
    if (clientStat >= 10000 && user->nektumID == clientStat) return qtrue;
    if (user->playerid != 0 && user->playerid == pid) return qtrue;
    if (user->steamid != 0 && user->steamid == sid) return qtrue;
    if (user->subnet24[0] != '\0' && subnet[0] != '\0' && 
        !IsCGNAT(subnet) && strcmp(user->subnet24, subnet) == 0) return qtrue;
    return qfalse;
}

static inline float DotProduct(const vec3_t v1, const vec3_t v2) {
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

static inline void VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out) {
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static inline float VectorLength(const vec3_t v) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline void VectorNormalize(const vec3_t v, vec3_t out) {
    float len = VectorLength(v);
    if (len > 0.0001f) {
        out[0] = v[0] / len;
        out[1] = v[1] / len;
        out[2] = v[2] / len;
    } else {
        out[0] = out[1] = out[2] = 0.0f;
    }
}

static inline void AngleVectors(const vec3_t angles, vec3_t forward) {
    float angle;
    float sp, sy, cp, cy;
    
    // Pitch
    angle = angles[0] * (M_PI / 180.0f);
    sp = sinf(angle);
    cp = cosf(angle);
    
    // Yaw
    angle = angles[1] * (M_PI / 180.0f);
    sy = sinf(angle);
    cy = cosf(angle);
    
    if (forward) {
        forward[0] = cp * cy;  // X (Standard Quake3)
        forward[1] = cp * sy;  // Y (Standard Quake3)
        forward[2] = -sp;      // Z
    }
}

void RemoveFromNativeBanlist(uint64_t pid, const char* ip) {
    if (pid == 0 && (!ip || ip[0] == '\0')) return;
    char home[512];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    char pidStr[32] = "";
    if (pid != 0) snprintf(pidStr, sizeof(pidStr), "%llu", (unsigned long long)pid);
    const char* files[] = { "banlist_v2.dat", "banlist_v2.dat.tmp" };
    int totalRemoved = 0;
    for (int f = 0; f < 2; f++) {
        char path[600], tmpPath[600];
        snprintf(path, sizeof(path), "%s/%s", home, files[f]);
        snprintf(tmpPath, sizeof(tmpPath), "%s/%s_ns_tmp", home, files[f]);
        FILE* fin = fopen(path, "r");
        if (!fin) continue;
        FILE* fout = fopen(tmpPath, "w");
        if (!fout) { fclose(fin); continue; }
        char line[512];
        int removed = 0;
        while (fgets(line, sizeof(line), fin)) {
            qboolean skip = qfalse;
            if (pidStr[0] != '\0' && strstr(line, pidStr) != NULL) skip = qtrue;
            else if (ip && ip[0] != '\0' && strstr(line, "\\netadr\\") != NULL && strstr(line, ip) != NULL) skip = qtrue;
            if (skip) removed++;
            else fputs(line, fout);
        }
        fclose(fin); fclose(fout);
        if (removed > 0) {
            // JAVÍTVA: Biztonságos fájlkezelés
            if (remove(path) == 0) {
                if (rename(tmpPath, path) != 0) {
                    Plugin_Printf("^1[Nektum Shield] ^7Failed to rename %s to %s\n", tmpPath, path);
                    rename(path, tmpPath);
                } else {
                    Plugin_Printf("^2[Nektum Shield] ^7Removed %d entries from %s\n", removed, files[f]);
                    totalRemoved += removed;
                }
            } else {
                Plugin_Printf("^1[Nektum Shield] ^7Failed to remove %s\n", path);
                remove(tmpPath);
            }
        } else {
            remove(tmpPath);
        }
    }
    if (totalRemoved > 0) Plugin_Printf("^2[Nektum Shield] ^7Banlist files cleaned.\n");
}

/* ========================= DISCORD QUEUE ========================= */
void QueueDiscordMessage(const char* payload) {
    if (discordQueueCount >= MAX_DISCORD_QUEUE) {
        discordQueueHead = (discordQueueHead + 1) % MAX_DISCORD_QUEUE;
        discordQueueCount--;
    }
    
    // JAVÍTVA: Túlcsordulás védelem
    int payloadLen = strlen(payload);
    if (payloadLen >= 2200) {
        Plugin_Printf("^3[Nektum Shield] ^7Discord message truncated (%d chars)\n", payloadLen);
    }
    
    snprintf(discordQueue[discordQueueTail], 2200, "%s", payload);
    discordQueueTail = (discordQueueTail + 1) % MAX_DISCORD_QUEUE;
    discordQueueCount++;
}

/* ========================= LOGGING ========================= */
void AC_Log(const char *msg) {
    if (!ac_log) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(ac_log, "[%02d:%02d:%02d] %s\n", t->tm_hour, t->tm_min, t->tm_sec, msg);
    fflush(ac_log);
    if (strstr(msg, "[DET]") == NULL && strstr(msg, "[ADMIN") == NULL && strstr(msg, "[AUTO") == NULL) return;
    static char escapedMsg[2048];
    int j = 0;
    for (int i = 0; msg[i] != '\0' && j < (int)sizeof(escapedMsg) - 6; i++) {
        char c = msg[i];
        if (c == '"' || c == '\\') { escapedMsg[j++] = '\\'; escapedMsg[j++] = c; }
        else if (c == '\n') { escapedMsg[j++] = '\\'; escapedMsg[j++] = 'n'; }
        else if (c == '\r') { escapedMsg[j++] = '\\'; escapedMsg[j++] = 'r'; }
        else if (c == '\t') { escapedMsg[j++] = '\\'; escapedMsg[j++] = 't'; }
        else escapedMsg[j++] = c;
    }
    escapedMsg[j] = '\0';
    static char payload[2200];
    snprintf(payload, sizeof(payload),
        "{\"content\": \"**Nektum Shield: Log Webhook**\\n```[%02d:%02d:%02d] %s```\"}",
        t->tm_hour, t->tm_min, t->tm_sec, escapedMsg);
    QueueDiscordMessage(payload);
}

/* ========================= DATABASE I/O ========================= */
void NS_LoadBans(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" BANLIST_FILE, home);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[512]; g_numBans = 0;
    while (fgets(line, sizeof(line), f) && g_numBans < MAX_BANS) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;
        char* token = strtok(line, "|"); if (!token) continue; uint64_t pid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; uint64_t sid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; char r[128]; snprintf(r, sizeof(r), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; char a[64]; snprintf(a, sizeof(a), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; time_t bt = strtoll(token, NULL, 10);
        char n[64] = ""; token = strtok(NULL, "|"); if (token) snprintf(n, sizeof(n), "%s", token);
        char sn[20] = ""; token = strtok(NULL, "|"); if (token) snprintf(sn, sizeof(sn), "%s", token);
        g_banlist[g_numBans].playerid = pid; g_banlist[g_numBans].steamid = sid;
        snprintf(g_banlist[g_numBans].name, sizeof(g_banlist[g_numBans].name), "%s", n);
        snprintf(g_banlist[g_numBans].subnet24, sizeof(g_banlist[g_numBans].subnet24), "%s", sn);
        snprintf(g_banlist[g_numBans].reason, sizeof(g_banlist[g_numBans].reason), "%s", r);
        snprintf(g_banlist[g_numBans].admin, sizeof(g_banlist[g_numBans].admin), "%s", a);
        g_banlist[g_numBans].banTime = bt;
        g_numBans++;
    }
    fclose(f);
    Plugin_Printf("^2[Nektum Shield] ^7Loaded %d bans.\n", g_numBans);
}

void NS_SaveBans(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" BANLIST_FILE, home);
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < g_numBans; i++) {
        fprintf(f, "%llu|%llu|%s|%s|%lld|%s|%s\n",
            (unsigned long long)g_banlist[i].playerid, (unsigned long long)g_banlist[i].steamid,
            g_banlist[i].reason, g_banlist[i].admin, (long long)g_banlist[i].banTime,
            g_banlist[i].name, g_banlist[i].subnet24);
    }
    fclose(f);
}

void NS_AddBan(uint64_t pid, uint64_t sid, const char* name, const char* subnet24, const char* reason, const char* admin) {
    if (g_numBans >= MAX_BANS) { Plugin_Printf("^1[Nektum Shield] Banlist is full!\n"); return; }
    for (int i = 0; i < g_numBans; i++) if (g_banlist[i].playerid == pid && pid != 0) return;
    g_banlist[g_numBans].playerid = pid; g_banlist[g_numBans].steamid = sid;
    snprintf(g_banlist[g_numBans].name, sizeof(g_banlist[g_numBans].name), "%s", name ? name : "");
    snprintf(g_banlist[g_numBans].subnet24, sizeof(g_banlist[g_numBans].subnet24), "%s", subnet24 ? subnet24 : "");
    snprintf(g_banlist[g_numBans].reason, sizeof(g_banlist[g_numBans].reason), "%s", reason);
    snprintf(g_banlist[g_numBans].admin, sizeof(g_banlist[g_numBans].admin), "%s", admin);
    g_banlist[g_numBans].banTime = Plugin_GetRealtime();
    g_numBans++;
    NS_SaveBans();
}

typedef enum { NS_NOT_BANNED = 0, NS_BANNED_BY_ID = 1, NS_BANNED_BY_NAME = 2 } nsBanStatus_t;

nsBanStatus_t NS_CheckBanStatus(uint64_t pid, uint64_t sid, const char* name, const char* subnet, char* reasonOut, int reasonLen) {
    for (int i = 0; i < g_numBans; i++) {
        if ((g_banlist[i].playerid != 0 && g_banlist[i].playerid == pid) ||
            (g_banlist[i].steamid != 0 && g_banlist[i].steamid == sid)) {
            if (reasonOut) snprintf(reasonOut, reasonLen, "%s", g_banlist[i].reason);
            return NS_BANNED_BY_ID;
        }
    }
    if (name && name[0] != '\0') {
        char chk[64]; StripColorCodes(name, chk, sizeof(chk));
        for (int k = 0; chk[k]; k++) chk[k] = tolower((unsigned char)chk[k]);
        for (int i = 0; i < g_numBans; i++) {
            if (g_banlist[i].name[0] == '\0') continue;
            char bn[64]; StripColorCodes(g_banlist[i].name, bn, sizeof(bn));
            for (int k = 0; bn[k]; k++) bn[k] = tolower((unsigned char)bn[k]);
            if (strcmp(bn, chk) == 0) {
                if (strlen(bn) >= 8) {
                    if (reasonOut) snprintf(reasonOut, reasonLen, "Connection refused.\n^7Contact admins at: ^2www.discord.yob.at");
                    return NS_BANNED_BY_ID;
                } else {
                    if (reasonOut) snprintf(reasonOut, reasonLen, "%s", g_banlist[i].reason);
                    return NS_BANNED_BY_NAME;
                }
            }
        }
    }
    if (subnet && subnet[0] != '\0') {
        for (int i = 0; i < g_numBans; i++) {
            if (g_banlist[i].subnet24[0] != '\0' && strcmp(g_banlist[i].subnet24, subnet) == 0 && g_banlist[i].playerid == 0) {
                if (reasonOut) snprintf(reasonOut, reasonLen, "Connection refused.\n^7Contact admins at: ^2www.discord.yob.at");
                return NS_BANNED_BY_ID;
            }
        }
    }
    return NS_NOT_BANNED;
}

void NS_LoadUsers(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" USERS_FILE, home);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[512]; g_numUsers = 0; g_nextNektumID = 10000;
    while (fgets(line, sizeof(line), f) && g_numUsers < MAX_USERS) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;
        char* token = strtok(line, "|"); if (!token) continue; uint64_t pid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; uint64_t sid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; char n[64]; snprintf(n, sizeof(n), "%s", token);
        char sn[20] = ""; token = strtok(NULL, "|"); if (token) snprintf(sn, sizeof(sn), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; int nid = atoi(token);
        g_users[g_numUsers].playerid = pid; g_users[g_numUsers].steamid = sid;
        snprintf(g_users[g_numUsers].name, sizeof(g_users[g_numUsers].name), "%s", n);
        snprintf(g_users[g_numUsers].subnet24, sizeof(g_users[g_numUsers].subnet24), "%s", sn);
        g_users[g_numUsers].nektumID = nid;
        if (nid >= g_nextNektumID) g_nextNektumID = nid + 1;
        g_numUsers++;
    }
    fclose(f);
    Plugin_Printf("^2[Nektum Shield] ^7Loaded %d users^2. ^7Next NektumID: %d\n", g_numUsers, g_nextNektumID);
}

void NS_SaveUsers(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" USERS_FILE, home);
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < g_numUsers; i++) {
        fprintf(f, "%llu|%llu|%s|%s|%d\n",
            (unsigned long long)g_users[i].playerid, (unsigned long long)g_users[i].steamid,
            g_users[i].name, g_users[i].subnet24, g_users[i].nektumID);
    }
    fclose(f);
}

void NS_LoadMutes(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" MUTES_FILE, home);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[512]; g_numMutes = 0;
    while (fgets(line, sizeof(line), f) && g_numMutes < MAX_MUTES) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;
        char* token = strtok(line, "|"); if (!token) continue; uint64_t pid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; uint64_t sid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; char n[64]; snprintf(n, sizeof(n), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; char a[64]; snprintf(a, sizeof(a), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; char r[128]; snprintf(r, sizeof(r), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; time_t mt = strtoll(token, NULL, 10);
        g_mutedPlayers[g_numMutes].playerid = pid; g_mutedPlayers[g_numMutes].steamid = sid;
        snprintf(g_mutedPlayers[g_numMutes].name, sizeof(g_mutedPlayers[g_numMutes].name), "%s", n);
        snprintf(g_mutedPlayers[g_numMutes].admin, sizeof(g_mutedPlayers[g_numMutes].admin), "%s", a);
        snprintf(g_mutedPlayers[g_numMutes].reason, sizeof(g_mutedPlayers[g_numMutes].reason), "%s", r);
        g_mutedPlayers[g_numMutes].muteTime = mt;
        g_numMutes++;
    }
    fclose(f);
    Plugin_Printf("^2[Nektum Shield] ^7Loaded %d active mutes.\n", g_numMutes);
}

void NS_SaveMutes(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" MUTES_FILE, home);
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < g_numMutes; i++) {
        fprintf(f, "%llu|%llu|%s|%s|%s|%lld\n",
            (unsigned long long)g_mutedPlayers[i].playerid, (unsigned long long)g_mutedPlayers[i].steamid,
            g_mutedPlayers[i].name, g_mutedPlayers[i].admin,
            g_mutedPlayers[i].reason, (long long)g_mutedPlayers[i].muteTime);
    }
    fclose(f);
}

qboolean NS_IsPlayerMuted(uint64_t pid) {
    for (int i = 0; i < g_numMutes; i++) if (g_mutedPlayers[i].playerid == pid) return qtrue;
    return qfalse;
}

/* ========================= BAN EXECUTION ========================= */
void ExecuteBan(int targetSlot, const char* reason, const char* adminName, int invokerSlot) {
    client_t* cl = Plugin_GetClientForClientNum(targetSlot);
    if (!cl || IsBot(cl)) return;
    if (cl->playerid == 0 && cl->steamid == 0) {
        SendResponse(invokerSlot, "^1[Nektum Shield] ^7Unable to ban. No user found.");
        return;
    }
    char cleanName[64]; StripColorCodes(cl->name, cleanName, sizeof(cleanName));
    if (cleanName[0] == '\0') snprintf(cleanName, sizeof(cleanName), "%s", cl->name);
    netadr_t adr = cl->netchan.remoteAddress;
    char ipBuf[64]; Plugin_NET_AdrToStringMT(&adr, ipBuf, sizeof(ipBuf));
    char subnet[20]; GetSubnet24(ipBuf, subnet, sizeof(subnet));
    qboolean isCGNAT = IsCGNAT(ipBuf);
    if (isCGNAT) {
        NS_AddBan(cl->playerid, cl->steamid, cleanName, "", reason, adminName);
        SendResponse(invokerSlot, "^2[Nektum Shield] ^1CGNAT DETECTED: ^7Subnet ban disabled.");
        AC_Log(AC_va("[CGNAT] Admin: %s banned player: %s (PID: %llu | IP: %s) - Subnet skipped", adminName, cl->name, (unsigned long long)cl->playerid, ipBuf));
    } else {
        NS_AddBan(cl->playerid, cl->steamid, cleanName, subnet, reason, adminName);
        AC_Log(AC_va("[ADMIN BAN] Admin: %s banned player: %s (Slot %d | PID: %llu | Subnet: %s)", adminName, cl->name, targetSlot, (unsigned long long)cl->playerid, subnet));
    }
    Plugin_SetStat(targetSlot, STAT_CHEATER_MARK, 1);
    Plugin_ChatPrintf(-1, "^2[Nektum Shield] ^7Admin: ^2%s ^7permanently banned %s: %s", adminName, cl->name, reason);
    pData[targetSlot].scheduledDropTime = Plugin_Milliseconds() + cvars.drop_delay_ms;
    snprintf(pData[targetSlot].dropReason, sizeof(pData[targetSlot].dropReason), "%s", reason);
}

/* ========================= UNIVERSAL COMMAND PROCESSORS ========================= */
void ProcessBan(int invokerSlot, const char* query, const char* reason) {
    char admin[64] = "Console";
    if (invokerSlot >= 0) {
        client_t* cl = Plugin_GetClientForClientNum(invokerSlot);
        if (cl && !IsBot(cl)) StripColorCodes(cl->name, admin, sizeof(admin));
    }
    if (IsIPAddress(query)) {
        char subnet[20]; ExtractSubnet(query, subnet, sizeof(subnet));
        qboolean isCGNAT = IsCGNAT(query);
        if (isCGNAT) {
            SendResponse(invokerSlot, "^2[Nektum Shield] ^1CGNAT WARNING: ^7Subnet ban BLOCKED.");
            AC_Log(AC_va("[CGNAT] Admin: %s attempted Subnet ban on CGNAT IP: %s", admin, query));
        } else {
            NS_AddBan(0, 0, "", subnet, reason, admin);
            SendResponse(invokerSlot, "^2[Nektum Shield] ^7Subnet ^2%s ^7banned.", subnet);
            AC_Log(AC_va("[ADMIN BAN] Admin: %s banned Subnet: %s", admin, subnet));
        }
        return;
    }
    if (IsNumeric(query)) {
        uint64_t num = strtoull(query, NULL, 10);
        if (num > 1000000) {
            char dbName[64] = "", dbSubnet[20] = ""; uint64_t dbSid = 0;
            for (int i = 0; i < g_numUsers; i++) {
                if (g_users[i].playerid == num) {
                    snprintf(dbName, sizeof(dbName), "%s", g_users[i].name);
                    snprintf(dbSubnet, sizeof(dbSubnet), "%s", g_users[i].subnet24);
                    dbSid = g_users[i].steamid; break;
                }
            }
            qboolean isCGNAT = (dbSubnet[0] != '\0') ? IsCGNAT(dbSubnet) : qfalse;
            NS_AddBan(num, dbSid, dbName, isCGNAT ? "" : dbSubnet, reason, admin);
            SendResponse(invokerSlot, "^2[Nektum Shield] ^7PlayerID ^2%llu ^7banned.", (unsigned long long)num);
            AC_Log(AC_va("[ADMIN BAN] Admin: %s banned PID: %llu", admin, (unsigned long long)num));
        } else {
            qboolean foundNID = qfalse;
            for (int i = 0; i < g_numUsers; i++) {
                if (g_users[i].nektumID == (int)num) {
                    qboolean isCGNAT = IsCGNAT(g_users[i].subnet24);
                    NS_AddBan(g_users[i].playerid, g_users[i].steamid, g_users[i].name, isCGNAT ? "" : g_users[i].subnet24, reason, admin);
                    SendResponse(invokerSlot, "^2[Nektum Shield] ^7User ^2%s ^7(NID: %d) banned.", g_users[i].name, g_users[i].nektumID);
                    foundNID = qtrue; break;
                }
            }
            if (!foundNID) {
                char numStr[32]; snprintf(numStr, sizeof(numStr), "%llu", (unsigned long long)num);
                level_locals_t *level = Plugin_GetLevelBase();
                if (level) {
                    for (int i = 0; i < level->maxclients; i++) {
                        client_t* cl = Plugin_GetClientForClientNum(i);
                        if (cl && cl->state == CS_ACTIVE && !IsBot(cl)) {
                            char cn[64]; StripColorCodes(cl->name, cn, sizeof(cn));
                            if (strcmp(cn, numStr) == 0) { ExecuteBan(i, reason, admin, invokerSlot); return; }
                        }
                    }
                }
                qboolean foundOffline = qfalse;
                for (int i = 0; i < g_numUsers; i++) {
                    char rn[64]; StripColorCodes(g_users[i].name, rn, sizeof(rn));
                    if (strcmp(rn, numStr) == 0) {
                        qboolean isCGNAT = IsCGNAT(g_users[i].subnet24);
                        NS_AddBan(g_users[i].playerid, g_users[i].steamid, g_users[i].name, isCGNAT ? "" : g_users[i].subnet24, reason, admin);
                        SendResponse(invokerSlot, "^2[Nektum Shield] ^7Offline user ^2%s ^7banned.", g_users[i].name);
                        foundOffline = qtrue; break;
                    }
                }
                if (!foundOffline) SendResponse(invokerSlot, "^1[Nektum Shield] ^7NID or name '^1%s^7' not found.", numStr);
            }
        }
        return;
    }
    level_locals_t *level = Plugin_GetLevelBase();
    if (level) {
        char tgt[64]; snprintf(tgt, sizeof(tgt), "%s", query);
        for (int k = 0; tgt[k]; k++) tgt[k] = tolower((unsigned char)tgt[k]);
        for (int i = 0; i < level->maxclients; i++) {
            client_t* cl = Plugin_GetClientForClientNum(i);
            if (cl && cl->state == CS_ACTIVE && !IsBot(cl)) {
                char cn[64]; StripColorCodes(cl->name, cn, sizeof(cn));
                char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cn);
                for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
                if (strstr(lcn, tgt)) { ExecuteBan(i, reason, admin, invokerSlot); return; }
            }
        }
    }
    char tgt[64]; snprintf(tgt, sizeof(tgt), "%s", query);
    for (int k = 0; tgt[k]; k++) tgt[k] = tolower((unsigned char)tgt[k]);
    int matchCount = 0, lastMatch = -1;
    for (int i = 0; i < g_numUsers; i++) {
        char rn[64]; StripColorCodes(g_users[i].name, rn, sizeof(rn));
        char lrn[64]; snprintf(lrn, sizeof(lrn), "%s", rn);
        for (int k = 0; lrn[k]; k++) lrn[k] = tolower((unsigned char)lrn[k]);
        if (strstr(lrn, tgt)) { matchCount++; lastMatch = i; }
    }
    if (matchCount == 1) {
        qboolean isCGNAT = IsCGNAT(g_users[lastMatch].subnet24);
        NS_AddBan(g_users[lastMatch].playerid, g_users[lastMatch].steamid, g_users[lastMatch].name, isCGNAT ? "" : g_users[lastMatch].subnet24, reason, admin);
        SendResponse(invokerSlot, "^2[Nektum Shield] ^7Offline player: ^2%s ^7banned.", g_users[lastMatch].name);
    } else if (matchCount > 1) {
        SendResponse(invokerSlot, "^1[Nektum Shield] ^7Multiple matches. Be more specific.");
    } else {
        SendResponse(invokerSlot, "^1[Nektum Shield] ^7Player not found.");
    }
}

void ProcessUnban(int invokerSlot, const char* query) {
    char admin[64] = "Console";
    if (invokerSlot >= 0) {
        client_t* cl = Plugin_GetClientForClientNum(invokerSlot);
        if (cl && !IsBot(cl)) StripColorCodes(cl->name, admin, sizeof(admin));
    }
    qboolean found = qfalse; int unbanCount = 0;
    if (IsIPAddress(query)) {
        char subnet[20]; ExtractSubnet(query, subnet, sizeof(subnet));
        for (int i = g_numBans - 1; i >= 0; i--) {
            if (g_banlist[i].subnet24[0] != '\0' && strcmp(g_banlist[i].subnet24, subnet) == 0) {
                if (g_banlist[i].playerid != 0) {
                    char fakeIP[64]; snprintf(fakeIP, sizeof(fakeIP), "%s.1", g_banlist[i].subnet24);
                    RemoveFromNativeBanlist(g_banlist[i].playerid, fakeIP);
                }
                for (int j = i; j < g_numBans - 1; j++) g_banlist[j] = g_banlist[j+1];
                g_numBans--; unbanCount++; found = qtrue;
            }
        }
        if (found) {
            NS_SaveBans();
            SendResponse(invokerSlot, "^2[Nektum Shield] ^7Unbanned %d entries for subnet ^2%s^7.", unbanCount, subnet);
            AC_Log(AC_va("[ADMIN UNBAN] Admin: %s unbanned Subnet: %s", admin, subnet));
            NS_LoadBans();
        } else SendResponse(invokerSlot, "^1[Nektum Shield] ^7No entries found for subnet ^2%s^7.", subnet);
        return;
    }
    if (IsNumeric(query)) {
        uint64_t num = strtoull(query, NULL, 10);
        uint64_t targetPID = 0, targetSteamID = 0; char targetIP[64] = "";
        if (num < 1000000) {
            for (int i = 0; i < g_numUsers; i++) {
                if (g_users[i].nektumID == (int)num) {
                    targetPID = g_users[i].playerid; targetSteamID = g_users[i].steamid;
                    if (g_users[i].subnet24[0] != '\0') snprintf(targetIP, sizeof(targetIP), "%s.1", g_users[i].subnet24);
                    break;
                }
            }
            if (targetPID == 0) {
                SendResponse(invokerSlot, "^1[Nektum Shield] ^7NektumID ^1%llu ^7not found.", (unsigned long long)num);
                return;
            }
        } else targetPID = num;
        for (int i = 0; i < g_numBans; i++) {
            if (g_banlist[i].playerid == targetPID || g_banlist[i].steamid == targetSteamID) {
                RemoveFromNativeBanlist(g_banlist[i].playerid, targetIP);
                for (int j = i; j < g_numBans - 1; j++) g_banlist[j] = g_banlist[j+1];
                g_numBans--; found = qtrue; break;
            }
        }
        if (found) {
            NS_SaveBans();
            SendResponse(invokerSlot, "^2[Nektum Shield] ^7Unbanned: ^2%llu^7", (unsigned long long)num);
            AC_Log(AC_va("[ADMIN UNBAN] Admin: %s unbanned: %llu", admin, (unsigned long long)num));
            NS_LoadBans();
        } else SendResponse(invokerSlot, "^1[Nektum Shield] ^7No ban entries found.");
        return;
    }
    char tgt[64]; snprintf(tgt, sizeof(tgt), "%s", query);
    for (int k = 0; tgt[k]; k++) tgt[k] = tolower((unsigned char)tgt[k]);
    for (int i = g_numBans - 1; i >= 0; i--) {
        char bn[64]; StripColorCodes(g_banlist[i].name, bn, sizeof(bn));
        char lbn[64]; snprintf(lbn, sizeof(lbn), "%s", bn);
        for (int k = 0; lbn[k]; k++) lbn[k] = tolower((unsigned char)lbn[k]);
        if (strstr(lbn, tgt)) {
            char targetIP[64] = "";
            if (g_banlist[i].subnet24[0] != '\0') snprintf(targetIP, sizeof(targetIP), "%s.1", g_banlist[i].subnet24);
            if (g_banlist[i].playerid != 0) RemoveFromNativeBanlist(g_banlist[i].playerid, targetIP);
            for (int j = i; j < g_numBans - 1; j++) g_banlist[j] = g_banlist[j+1];
            g_numBans--; unbanCount++; found = qtrue;
        }
    }
    if (found) {
        NS_SaveBans();
        SendResponse(invokerSlot, "^2[Nektum Shield] ^7Unbanned %d entries matching '^2%s^7'.", unbanCount, query);
        AC_Log(AC_va("[ADMIN UNBAN] Admin: %s unbanned matching: %s", admin, query));
        NS_LoadBans();
    } else SendResponse(invokerSlot, "^1[Nektum Shield] ^7No entries found matching: ^1%s", query);
}

void ProcessFindUser(int invokerSlot, const char* query) {
    int matchCount = 0;
    if (IsIPAddress(query)) {
        char subnet[20]; ExtractSubnet(query, subnet, sizeof(subnet));
        for (int i = 0; i < g_numUsers; i++) {
            if (g_users[i].subnet24[0] != '\0' && strcmp(g_users[i].subnet24, subnet) == 0) {
                SendResponse(invokerSlot, "^2[Nektum] ^7User: ^2%s ^7| PID: ^2%llu ^7| NID: ^5%d",
                    g_users[i].name, (unsigned long long)g_users[i].playerid, g_users[i].nektumID);
                if (++matchCount >= 5) { SendResponse(invokerSlot, "^1[Nektum Shield] ^7Too many matches."); return; }
            }
        }
    } else if (IsNumeric(query)) {
        uint64_t num = strtoull(query, NULL, 10);
        for (int i = 0; i < g_numUsers; i++) {
            if (g_users[i].playerid == num || g_users[i].nektumID == (int)num) {
                SendResponse(invokerSlot, "^2[Nektum] ^7User: ^2%s ^7| PID: ^2%llu ^7| NID: ^2%d",
                    g_users[i].name, (unsigned long long)g_users[i].playerid, g_users[i].nektumID);
                matchCount++;
            }
        }
    } else {
        char tgt[64]; snprintf(tgt, sizeof(tgt), "%s", query);
        for (int k = 0; tgt[k]; k++) tgt[k] = tolower((unsigned char)tgt[k]);
        for (int i = 0; i < g_numUsers; i++) {
            char rn[64]; StripColorCodes(g_users[i].name, rn, sizeof(rn));
            char lrn[64]; snprintf(lrn, sizeof(lrn), "%s", rn);
            for (int k = 0; lrn[k]; k++) lrn[k] = tolower((unsigned char)lrn[k]);
            if (strstr(lrn, tgt)) {
                SendResponse(invokerSlot, "^2[Nektum Shield] ^7[DB] ^2%s ^7| PID: ^2%llu ^7| NID: ^2%d",
                    g_users[i].name, (unsigned long long)g_users[i].playerid, g_users[i].nektumID);
                if (++matchCount >= 5) { SendResponse(invokerSlot, "^1[Nektum Shield] ^7Too many matches."); return; }
            }
        }
        level_locals_t *level = Plugin_GetLevelBase();
        if (level) {
            for (int i = 0; i < level->maxclients; i++) {
                client_t* cl = Plugin_GetClientForClientNum(i);
                if (cl && cl->state == CS_ACTIVE && !IsBot(cl)) {
                    char cn[64]; StripColorCodes(cl->name, cn, sizeof(cn));
                    char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cn);
                    for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
                    if (strstr(lcn, tgt)) {
                        char ipBuf[64]; Plugin_NET_AdrToStringMT(&cl->netchan.remoteAddress, ipBuf, sizeof(ipBuf));
                        SendResponse(invokerSlot, "^2[Nektum Shield] ^7[Online] ^2%s ^7| PID: ^2%llu ^7| Slot: ^2%d ^7| IP: ^2%s",
                            cl->name, (unsigned long long)cl->playerid, i, ipBuf);
                        if (++matchCount >= 5) { SendResponse(invokerSlot, "^1[Nektum Shield] ^7Too many matches."); return; }
                    }
                }
            }
        }
    }
    if (matchCount == 0) SendResponse(invokerSlot, "^1[Nektum Shield] ^7No player found matching: ^1%s", query);
}

/* ========================= COMMANDS ========================= */
void Cmd_NS_Ban(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 80) { SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); return; }
    int adminSlot = invoker, argStart = 1;
    if (invoker < 0 && Plugin_Cmd_Argc() >= 2) { adminSlot = atoi(Plugin_Cmd_Argv(1)); argStart = 2; }
    if (Plugin_Cmd_Argc() < argStart + 2) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Usage: bb <query> <reason>"); return; }
    const char* query = Plugin_Cmd_Argv(argStart);
    char reason[128] = {0};
    for (int i = argStart + 1; i < Plugin_Cmd_Argc(); i++) {
        int len = (int)strlen(reason);
        snprintf(reason + len, sizeof(reason) - len, "%s%s", (i > argStart + 1) ? " " : "", Plugin_Cmd_Argv(i));
    }
    ProcessBan(adminSlot, query, reason);
}

void Cmd_NS_Unban(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 80) { SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); return; }
    int adminSlot = invoker, argStart = 1;
    if (invoker < 0 && Plugin_Cmd_Argc() >= 2) { adminSlot = atoi(Plugin_Cmd_Argv(1)); argStart = 2; }
    if (Plugin_Cmd_Argc() < argStart + 1) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Usage: ub <query>"); return; }
    ProcessUnban(adminSlot, Plugin_Cmd_Argv(argStart));
}

void Cmd_NS_FindUser(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 80) { SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); return; }
    int adminSlot = invoker, argStart = 1;
    if (invoker < 0 && Plugin_Cmd_Argc() >= 2) { adminSlot = atoi(Plugin_Cmd_Argv(1)); argStart = 2; }
    if (Plugin_Cmd_Argc() < argStart + 1) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Usage: fu <query>"); return; }
    ProcessFindUser(adminSlot, Plugin_Cmd_Argv(argStart));
}

void Cmd_NS_Mute(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 60) { SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); return; }
    int adminSlot = invoker, argStart = 1;
    if (invoker < 0 && Plugin_Cmd_Argc() >= 2) { adminSlot = atoi(Plugin_Cmd_Argv(1)); argStart = 2; }
    
    if (Plugin_Cmd_Argc() < argStart + 1) { 
        SendResponse(adminSlot, "^1[Nektum Shield] ^7Usage: mute <player> [reason]"); 
        return; 
    }
    
    const char* targetQuery = Plugin_Cmd_Argv(argStart);
    char reason[128] = {0};
    
    for (int i = argStart + 1; i < Plugin_Cmd_Argc(); i++) {
        int len = (int)strlen(reason);
        snprintf(reason + len, sizeof(reason) - len, "%s%s", (i > argStart + 1) ? " " : "", Plugin_Cmd_Argv(i));
    }
    
    level_locals_t *level = Plugin_GetLevelBase();
    if (!level) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Level not available."); return; }
    
    char tgt[64]; snprintf(tgt, sizeof(tgt), "%s", targetQuery);
    for (int k = 0; tgt[k]; k++) tgt[k] = tolower((unsigned char)tgt[k]);
    
    for (int i = 0; i < level->maxclients; i++) {
        client_t* cl = Plugin_GetClientForClientNum(i);
        if (cl && cl->state == CS_ACTIVE && !IsBot(cl)) {
            char cn[64]; StripColorCodes(cl->name, cn, sizeof(cn));
            char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cn);
            for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
            if (strstr(lcn, tgt)) {
                if (NS_IsPlayerMuted(cl->playerid)) { 
                    SendResponse(adminSlot, "^1[Nektum Shield] ^7Player ^2%s ^7already muted.", cn); 
                    return; 
                }
                if (g_numMutes >= MAX_MUTES) { 
                    SendResponse(adminSlot, "^1[Nektum Shield] ^7Mute list full!"); 
                    return; 
                }
                
                char adminName[64] = "Console";
                if (adminSlot >= 0) {
                    client_t* admin = Plugin_GetClientForClientNum(adminSlot);
                    if (admin) StripColorCodes(admin->name, adminName, sizeof(adminName));
                }
                
                g_mutedPlayers[g_numMutes].playerid = cl->playerid;
                g_mutedPlayers[g_numMutes].steamid = cl->steamid;
                snprintf(g_mutedPlayers[g_numMutes].name, sizeof(g_mutedPlayers[g_numMutes].name), "%s", cn);
                snprintf(g_mutedPlayers[g_numMutes].admin, sizeof(g_mutedPlayers[g_numMutes].admin), "%s", adminName);
                
                snprintf(g_mutedPlayers[g_numMutes].reason, sizeof(g_mutedPlayers[g_numMutes].reason), "%s", 
                         reason[0] ? reason : "No reason specified");
                
                g_mutedPlayers[g_numMutes].muteTime = Plugin_GetRealtime();
                g_numMutes++; 
                NS_SaveMutes();
                pData[i].isMuted = qtrue;
                
                if (reason[0]) {
                    SendResponse(adminSlot, "^2[Nektum Shield] ^7Player ^2%s ^7MUTED. Reason: ^2%s", cn, reason);
                    Plugin_ChatPrintf(i, "^1[Nektum Shield] ^7You have been MUTED by admin ^2%s^7. Reason: ^2%s", adminName, reason);
                } else {
                    SendResponse(adminSlot, "^2[Nektum Shield] ^7Player ^2%s ^7MUTED.", cn);
                    Plugin_ChatPrintf(i, "^1[Nektum Shield] ^7You have been MUTED by admin ^2%s^7.", adminName);
                }
                
                AC_Log(AC_va("[ADMIN MUTE] Admin: %s muted: %s (PID: %llu) | Reason: %s", 
                             adminName, cn, (unsigned long long)cl->playerid, 
                             reason[0] ? reason : "No reason specified"));
                return;
            }
        }
    }
    SendResponse(adminSlot, "^1[Nektum Shield] ^7Player not found.");
}

void Cmd_NS_Unmute(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 60) { SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); return; }
    int adminSlot = invoker, argStart = 1;
    if (invoker < 0 && Plugin_Cmd_Argc() >= 2) { adminSlot = atoi(Plugin_Cmd_Argv(1)); argStart = 2; }
    if (Plugin_Cmd_Argc() < argStart + 1) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Usage: unmute <player>"); return; }
    const char* targetQuery = Plugin_Cmd_Argv(argStart);
    level_locals_t *level = Plugin_GetLevelBase();
    if (!level) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Level not available."); return; }
    char tgt[64]; snprintf(tgt, sizeof(tgt), "%s", targetQuery);
    for (int k = 0; tgt[k]; k++) tgt[k] = tolower((unsigned char)tgt[k]);
    for (int i = 0; i < level->maxclients; i++) {
        client_t* cl = Plugin_GetClientForClientNum(i);
        if (cl && cl->state == CS_ACTIVE && !IsBot(cl)) {
            char cn[64]; StripColorCodes(cl->name, cn, sizeof(cn));
            char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cn);
            for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
            if (strstr(lcn, tgt)) {
                qboolean found = qfalse;
                for (int j = 0; j < g_numMutes; j++) {
                    if (g_mutedPlayers[j].playerid == cl->playerid) {
                        for (int k = j; k < g_numMutes - 1; k++) g_mutedPlayers[k] = g_mutedPlayers[k+1];
                        g_numMutes--; found = qtrue; break;
                    }
                }
                if (!found) { SendResponse(adminSlot, "^1[Nektum Shield] ^7Player ^2%s ^7not muted.", cn); return; }
                NS_SaveMutes(); pData[i].isMuted = qfalse;
                char adminName[64] = "Console";
                if (adminSlot >= 0) {
                    client_t* admin = Plugin_GetClientForClientNum(adminSlot);
                    if (admin) StripColorCodes(admin->name, adminName, sizeof(adminName));
                }
                SendResponse(adminSlot, "^2[Nektum Shield] ^7Player ^2%s ^7UNMUTED.", cn);
                Plugin_ChatPrintf(i, "^2[Nektum Shield] ^7You have been UNMUTED by admin ^2%s^7.", adminName);
                AC_Log(AC_va("[ADMIN UNMUTE] Admin: %s unmuted: %s (PID: %llu)", adminName, cn, (unsigned long long)cl->playerid));
                return;
            }
        }
    }
    SendResponse(adminSlot, "^1[Nektum Shield] ^7Player not found.");
}

void Nektum_PersistentBan(int clientNum, const char* reason) {
    pData[clientNum].pendingBan = qtrue;
    snprintf(pData[clientNum].pendingReason, sizeof(pData[clientNum].pendingReason), "%s", reason);
}

/* ========================= CALLBACKS ========================= */
PCL void OnMessageSent(char* message, int slot, qboolean *show, int mode) {
    (void)mode;
    if (!message || slot < 0 || slot >= MAXP) return;
    if (pData[slot].isMuted) {
        *show = qfalse;
        Plugin_ChatPrintf(slot, "^1[Nektum Shield] ^7You are muted.");
        return;
    }
    client_t* admin = Plugin_GetClientForClientNum(slot);
    if (!admin || IsBot(admin) || admin->power < 80) return;
    if (strncmp(message, "!bb ", 4) == 0) {
        *show = qfalse;
        char* args = message + 4; while (*args == ' ') args++;
        char* space = strchr(args, ' ');
        if (!space) { Plugin_ChatPrintf(slot, "^1[Nektum Shield] ^7Usage: !bb <query> <reason>"); return; }
        char query[64] = {0};
        int qLen = (int)(space - args);
        if (qLen >= (int)sizeof(query)) qLen = sizeof(query) - 1;
        snprintf(query, sizeof(query), "%.*s", qLen, args);
        char* reason = space + 1; while (*reason == ' ') reason++;
        ProcessBan(slot, query, reason);
    } else if (strncmp(message, "!ub ", 4) == 0) {
        *show = qfalse;
        char* args = message + 4; while (*args == ' ') args++;
        ProcessUnban(slot, args);
    } else if (strncmp(message, "!fu ", 4) == 0) {
        *show = qfalse;
        char* args = message + 4; while (*args == ' ') args++;
        ProcessFindUser(slot, args);
    }
}

PCL void OnPlayerGotAuthInfo(netadr_t* from, uint64_t* playerid, uint64_t* steamid, char* rejectmsg, qboolean* returnNow, client_t* cl) {
    if( IsBot(cl)) return;
    char cleanName[64] = "";
    if (cl && cl->name[0]) StripColorCodes(cl->name, cleanName, sizeof(cleanName));
    else if (cl) { const char* n = Info_ValueForKey(cl->userinfo, "name"); if (n) StripColorCodes(n, cleanName, sizeof(cleanName)); }
    char ipBuf[64]; Plugin_NET_AdrToStringMT(from, ipBuf, sizeof(ipBuf));
    char currentSubnet[20]; GetSubnet24(ipBuf, currentSubnet, sizeof(currentSubnet));
    time_t now_t = time(NULL);
    if (*playerid == lastBlockedPID && (now_t - lastBlockedTime) < 30) {
        *returnNow = qtrue; snprintf(rejectmsg, 1023, "Connection refused.\nContact admins at: www.discord.yob.at"); return;
    }
    if (strcmp(ipBuf, lastBlockedIP) == 0 && (now_t - lastBlockedTime) < 30) {
        *returnNow = qtrue; snprintf(rejectmsg, 1023, "Connection refused.\nContact admins at: www.discord.yob.at"); return;
    }
    int clientNektumID = 0;
    if (cl && cl->gentity) clientNektumID = Plugin_GetStat(cl->gentity->s.clientNum, STAT_NEKTUM_ID);
    char reason[128] = {0};
    nsBanStatus_t status = NS_CheckBanStatus(*playerid, *steamid, cleanName, currentSubnet, reason, sizeof(reason));
    int nameLen = (int)strlen(cleanName);
    qboolean isCGNAT = IsCGNAT(ipBuf);
    if (status == NS_BANNED_BY_ID || status == NS_BANNED_BY_NAME) {
        if (nameLen >= 7) {
            NS_AddBan(*playerid, *steamid, cleanName, isCGNAT ? "" : currentSubnet, reason, "Nektum Shield");
        }
    }
    if (currentSubnet[0] != '\0' && !IsCGNAT(ipBuf)) {
        qboolean evasion = qfalse;
        for (int i = 0; i < g_numBans; i++) {
            if (g_banlist[i].subnet24[0] != '\0' && strcmp(g_banlist[i].subnet24, currentSubnet) == 0) {
                if (g_banlist[i].playerid != *playerid && g_banlist[i].playerid != 0) { evasion = qtrue; break; }
            }
        }
        if (evasion && nameLen >= 7) {
            NS_AddBan(*playerid, *steamid, cleanName, currentSubnet, "Ban Evasion (Subnet Match)", "Nektum Shield");
        }
    }
    if (*playerid != 0 && cleanName[0] != '\0') {
        char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cleanName);
        for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
        int uidx = -1;
        for (int i = 0; i < g_numUsers; i++) if (g_users[i].playerid == *playerid) { uidx = i; break; }
        if (uidx != -1) {
            qboolean changed = qfalse;
            char rn[64]; StripColorCodes(g_users[uidx].name, rn, sizeof(rn));
            char lrn[64]; snprintf(lrn, sizeof(lrn), "%s", rn);
            for (int k = 0; lrn[k]; k++) lrn[k] = tolower((unsigned char)lrn[k]);
            if (strcmp(lrn, lcn) != 0) { snprintf(g_users[uidx].name, sizeof(g_users[uidx].name), "%s", cleanName); changed = qtrue; }
            if (currentSubnet[0] != '\0' && strcmp(g_users[uidx].subnet24, currentSubnet) != 0) {
                snprintf(g_users[uidx].subnet24, sizeof(g_users[uidx].subnet24), "%s", currentSubnet); changed = qtrue;
            }
            if (changed) {
                NS_SaveUsers();
                AC_Log(AC_va("[REG] Updated user PID: %llu | Name: '%s'%s", (unsigned long long)*playerid, cleanName, isCGNAT ? " [CGNAT]" : ""));
            }
        } else {
            int nameIdx = -1;
            for (int i = 0; i < g_numUsers; i++) {
                char rn[64]; StripColorCodes(g_users[i].name, rn, sizeof(rn));
                char lrn[64]; snprintf(lrn, sizeof(lrn), "%s", rn);
                for (int k = 0; lrn[k]; k++) lrn[k] = tolower((unsigned char)lrn[k]);
                if (strcmp(lrn, lcn) == 0) { nameIdx = i; break; }
            }
            if (nameIdx != -1) {
                if (IsSameIdentity(&g_users[nameIdx], *playerid, *steamid, currentSubnet, clientNektumID)) {
                    int preservedNektumID = g_users[nameIdx].nektumID;
                    g_users[nameIdx].playerid = *playerid;
                    g_users[nameIdx].steamid = *steamid;
                    if (!isCGNAT) snprintf(g_users[nameIdx].subnet24, sizeof(g_users[nameIdx].subnet24), "%s", currentSubnet);
                    NS_SaveUsers();
                    AC_Log(AC_va("[REG] Identity verified! Preserved NID: %d for '%s' (New PID: %llu)", preservedNektumID, cleanName, (unsigned long long)*playerid));
                } else {
                    AC_Log(AC_va("[KICK PREP] Name theft! New PID: %llu trying to steal '%s' (Original NID: %d)", (unsigned long long)*playerid, cleanName, g_users[nameIdx].nektumID));
                }
            } else {
                if (g_numUsers < MAX_USERS) {
                    g_users[g_numUsers].playerid = *playerid;
                    g_users[g_numUsers].steamid = *steamid;
                    snprintf(g_users[g_numUsers].name, sizeof(g_users[g_numUsers].name), "%s", cleanName);
                    snprintf(g_users[g_numUsers].subnet24, sizeof(g_users[g_numUsers].subnet24), "%s", currentSubnet);
                    g_users[g_numUsers].nektumID = g_nextNektumID++;
                    g_numUsers++; NS_SaveUsers();
                    AC_Log(AC_va("[REG] New user! PID: %llu | Name: %s | NID: %d%s", (unsigned long long)*playerid, cleanName, g_users[g_numUsers-1].nektumID, isCGNAT ? " [CGNAT]" : ""));
                }
            }
        }
    }
}

void PerformDelayedNameCheck(int id) {
    client_t* client = Plugin_GetClientForClientNum(id);
    if (!client || !client->gentity || IsBot(client)) return;
    char cleanName[64] = ""; StripColorCodes(client->name, cleanName, sizeof(cleanName));
    char ipBuf[64]; Plugin_NET_AdrToStringMT(&client->netchan.remoteAddress, ipBuf, sizeof(ipBuf));
    char currentSubnet[20]; GetSubnet24(ipBuf, currentSubnet, sizeof(currentSubnet));
    int clientNektumID = Plugin_GetStat(id, STAT_NEKTUM_ID);
    if (client->playerid != 0 && cleanName[0] != '\0') {
        char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cleanName);
        for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
        for (int i = 0; i < g_numUsers; i++) {
            if (g_users[i].playerid != client->playerid) {
                char rn[64]; StripColorCodes(g_users[i].name, rn, sizeof(rn));
                char lrn[64]; snprintf(lrn, sizeof(lrn), "%s", rn);
                for (int k = 0; lrn[k]; k++) lrn[k] = tolower((unsigned char)lrn[k]);
                if (strcmp(lrn, lcn) == 0) {
                    if (IsSameIdentity(&g_users[i], client->playerid, client->steamid, currentSubnet, clientNektumID)) {
                        AC_Log(AC_va("[REG] Identity verified! Player: %s (NID: %d)", cleanName, g_users[i].nektumID));
                        break;
                    } else {
                        AC_Log(AC_va("[KICK] Name theft! Dropping: %s (New PID: %llu) | Original NID: %d", cleanName, (unsigned long long)client->playerid, g_users[i].nektumID));
                        Plugin_DropClient(id, "English: Your NAME is already used by someone else. Please provide us a unique name!\nGerman: Ihr NAME wird bereits von jemand anderem verwendet. Bitte geben Sie einen eindeutigen Namen an!");
                        return;
                    }
                }
            }
        }
    }
}

PCL void OnClientEnterWorld(client_t* client) {
    if (!client || !client->gentity || IsBot(client)) return;
    int id = client->gentity->s.clientNum;
    if (id < 0 || id >= MAXP) return;
    memset(&pData[id], 0, sizeof(acData_t));

    int initTime = Plugin_Milliseconds();
    pData[id].logicWindowStart = initTime;
    pData[id].lastScrollTime = initTime;
    pData[id].lastRateTime = initTime;
    pData[id].lastConsistencyTime = initTime;

    char cleanName[64] = ""; StripColorCodes(client->name, cleanName, sizeof(cleanName));
    char ipBuf[64]; Plugin_NET_AdrToStringMT(&client->netchan.remoteAddress, ipBuf, sizeof(ipBuf));
    char currentSubnet[20]; GetSubnet24(ipBuf, currentSubnet, sizeof(currentSubnet));
    char reason[128] = {0};
    nsBanStatus_t status = NS_CheckBanStatus(client->playerid, client->steamid, cleanName, currentSubnet, reason, sizeof(reason));
    if (status == NS_BANNED_BY_ID) {
        if (Plugin_GetStat(id, STAT_CHEATER_MARK) != 1) Plugin_SetStat(id, STAT_CHEATER_MARK, 1);
        Nektum_PersistentBan(id, reason);
        return;
    }
    if (status == NS_BANNED_BY_NAME) {
        AC_Log(AC_va("[KICK] Banned name on enter! Dropping: %s (PID: %llu)", client->name, (unsigned long long)client->playerid));
        Plugin_DropClient(id, "Choose a different and unique name with at least 7 characters.");
        return;
    }
    level_locals_t *level = Plugin_GetLevelBase();
    if (level && cleanName[0] != '\0') {
        char lcn[64]; snprintf(lcn, sizeof(lcn), "%s", cleanName);
        for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
        for (int i = 0; i < level->maxclients; i++) {
            if (i == id) continue;
            client_t* oc = Plugin_GetClientForClientNum(i);
            if (oc && oc->state == CS_ACTIVE && !IsBot(oc)) {
                char on[64]; StripColorCodes(oc->name, on, sizeof(on));
                char lon[64]; snprintf(lon, sizeof(lon), "%s", on);
                for (int k = 0; lon[k]; k++) lon[k] = tolower((unsigned char)lon[k]);
                if (strcmp(lon, lcn) == 0) {
                    AC_Log(AC_va("[KICK] Name conflict! Dropping: %s | Conflicts with: %s", cleanName, on));
                    Plugin_DropClient(id, "Choose a different and unique name with at least 7 characters.");
                    return;
                }
            }
        }
    }
    pData[id].pendingNameCheck = qtrue;
    pData[id].nameCheckStartTime = Plugin_Milliseconds();
    if (Plugin_GetStat(id, STAT_CHEATER_MARK) == 1) {
        Plugin_SetStat(id, STAT_CHEATER_MARK, 0);
        AC_Log(AC_va("[HEAL] Profile healed: %s (PID: %llu)", client->name, (unsigned long long)client->playerid));
    }
    if (NS_IsPlayerMuted(client->playerid)) pData[id].isMuted = qtrue;
    if (client->playerid != 0) {
        for (int i = 0; i < g_numUsers; i++) {
            if (g_users[i].playerid == client->playerid) { Plugin_SetStat(id, STAT_NEKTUM_ID, g_users[i].nektumID); break; }
        }
    }
    const char* val = Info_ValueForKey(client->userinfo, BAN_DVAR);
    if (val && atoi(val) == BAN_DVAR_VALUE) Nektum_PersistentBan(id, "Nektum Shield got you!");
}

PCL void OnClientMoveCommand(client_t* client, usercmd_t* ucmd) {
    if (!client || !client->gentity || IsBot(client)) return;
    int id = client->gentity->s.clientNum;
    if (id < 0 || id >= MAXP || pData[id].pendingBan || pData[id].scheduledDropTime > 0) return;
    gclient_t *gcl = client->gentity->client;
    if (gcl->sess.sessionState != STATE_PLAYING) return;
    level_locals_t* level = Plugin_GetLevelBase();
    float now = level ? (level->time / 1000.0f) : (ucmd->serverTime / 1000.0f);
    vec3_t currentOrigin;
    VectorCopy(client->gentity->r.currentOrigin, currentOrigin);
    
    // JAVÍTVA: sqrtf() optimalizálás - moveDistSq használata
    float dx = currentOrigin[0] - pData[id].lastOrigin[0];
    float dy = currentOrigin[1] - pData[id].lastOrigin[1];
    float dz = currentOrigin[2] - pData[id].lastOrigin[2];
    float moveDistSq = dx*dx + dy*dy + dz*dz;
    float thresholdSq = cvars.alttab_move_dist * cvars.alttab_move_dist;
    if (moveDistSq > thresholdSq) pData[id].lastMoveTime = now;
    
    qboolean isMoving = (now - pData[id].lastMoveTime) < cvars.alttab_move_time;
    VectorCopy(currentOrigin, pData[id].lastOrigin);
    vec3_t ca; ca[0] = ShortToAngle(ucmd->angles[0]); ca[1] = ShortToAngle(ucmd->angles[1]); ca[2] = 0.0f;
    
    if (pData[id].lastAngles[1] != 0.0f) {
        float yc = GetYawDist(ca[1], pData[id].lastAngles[1]);
        int dt = ucmd->serverTime - pData[id].lastCmdTime;
        
        if (isMoving && dt >= 15 && dt <= 100 && yc > cvars.snap_threshold && now - pData[id].lastSnapAimTime > 5.0f) {
            pData[id].acScore += 10; 
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aimbot");
            pData[id].lastScoreUpdate = now; 
            pData[id].lastSnapAimTime = now;
            AC_Log(AC_va("[DET] Aimbot | Snap %.1f deg in %dms | P: %s", yc, dt, client->name));
        }
        
        if (isMoving && dt >= 15 && dt <= 100 && yc > cvars.snap_kill_angle) {
            pData[id].lastSnapTime = now;
        }
    }
    
    int cp = ucmd->angles[0] & 65535, cy = ucmd->angles[1] & 65535;
    qboolean firing = (ucmd->buttons & 1) ? qtrue : qfalse;
    int nowMs = Plugin_Milliseconds();
    qboolean currentADS = (ucmd->buttons & 256) ? qtrue : qfalse;
    pData[id].isADS = currentADS;

    if (firing && !pData[id].wasFiring) {
        if (client->gentity && client->gentity->client) {
            playerState_t* ps = &client->gentity->client->ps;
            
            pData[id].lastShotAngles[0] = ps->viewangles[0]; // Pitch (világ-szög)
            pData[id].lastShotAngles[1] = ps->viewangles[1]; // Yaw (világ-szög)
            pData[id].lastShotAngles[2] = 0.0f;
            
            VectorCopy(currentOrigin, pData[id].lastShotOrigin);
            pData[id].lastShotTime = now;
            pData[id].hasShotData = qtrue;
            pData[id].lastShotADS = pData[id].isADS;
        }
        
        if (nowMs - pData[id].logicWindowStart >= cvars.macro_logic_window) {
            pData[id].logicWindowStart = nowMs;
            pData[id].logicClickCount = 1;
        } else {
            pData[id].logicClickCount++;
        }
        
        if (pData[id].logicClickCount >= cvars.macro_logic_clicks) {
            pData[id].logicStrikes += 1.0f;
            if (pData[id].logicStrikes >= cvars.macro_logic_strikes) {
                pData[id].logicStrikes = 0.0f;
                pData[id].acScore += 5;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "Macro (Logic)");
                pData[id].lastScoreUpdate = now;
                AC_Log(AC_va("[DET] MACRO LOGIC | %d clicks in %dms | Strikes reached | P: %s", 
                    pData[id].logicClickCount, cvars.macro_logic_window, client->name));
                Plugin_ChatPrintf(-1, "^1[Nektum Shield] ^7Do not use macro^1 %s! ^7It's not allowed and you will get banned^1!", client->name);
            } else {
                AC_Log(AC_va("[DET] MACRO LOGIC | %d clicks in %dms | Strike %.1f/%.1f | P: %s", 
                    pData[id].logicClickCount, cvars.macro_logic_window,
                    pData[id].logicStrikes, cvars.macro_logic_strikes, client->name));
                Plugin_ChatPrintf(-1, "^1[Nektum Shield] ^7Do not use macro^1 %s! ^7It's not allowed and you will get banned^1!", client->name);
            }
            pData[id].logicClickCount = 0;
            pData[id].logicWindowStart = nowMs;
        } else {
            if (pData[id].logicStrikes > 0.0f && pData[id].logicClickCount == 1) {
                pData[id].logicStrikes -= 0.2f;
                if (pData[id].logicStrikes < 0.0f) pData[id].logicStrikes = 0.0f;
            }
        }

        int scrollDiff = nowMs - pData[id].lastScrollTime;
        if (scrollDiff < cvars.macro_scroll_fast) {
            pData[id].scrollFastCount++;
        } else if (scrollDiff >= cvars.macro_scroll_reset) {
            pData[id].scrollFastCount = 0;
        }
        
        if (pData[id].scrollFastCount >= cvars.macro_scroll_count) {
            pData[id].scrollFastCount = 0;
            pData[id].acScore += 5;
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "Macro (Scroll)");
            pData[id].lastScoreUpdate = now;
            AC_Log(AC_va("[DET] MACRO SCROLL | %d clicks <%dms | P: %s", 
                cvars.macro_scroll_count, cvars.macro_scroll_fast, client->name));
            Plugin_ChatPrintf(-1, "^1[Nektum Shield] ^7Do not use macro^1 %s! ^7It's not allowed and you will get banned^1!", client->name);
        }
        pData[id].lastScrollTime = nowMs;

        int rateDiff = nowMs - pData[id].lastRateTime;
        if (rateDiff < cvars.macro_rate_fast) {
            pData[id].rateFastCount++;
        } else {
            if (pData[id].rateFastCount > 0) pData[id].rateFastCount--;
        }
        
        if (pData[id].rateFastCount >= cvars.macro_rate_count) {
            pData[id].rateFastCount = 0;
            pData[id].acScore += 5;
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "Macro (Rate)");
            pData[id].lastScoreUpdate = now;
            AC_Log(AC_va("[DET] MACRO RATE | %d clicks <%dms | P: %s", 
                cvars.macro_rate_count, cvars.macro_rate_fast, client->name));
            Plugin_ChatPrintf(-1, "^1[Nektum Shield] ^7Do not use macro^1 %s! ^7It's not allowed and you will get banned^1!", client->name);
        }
        pData[id].lastRateTime = nowMs;

        int consistencyGap = nowMs - pData[id].lastConsistencyTime;
        if (consistencyGap < cvars.macro_consistency_gap) {
            if (consistencyGap == pData[id].lastConsistencyGap && consistencyGap > 0) {
                pData[id].consistencyMatchCount++;
            } else {
                if (pData[id].consistencyMatchCount > 0) pData[id].consistencyMatchCount--;
            }
            
            if (pData[id].consistencyMatchCount >= cvars.macro_consistency_count) {
                pData[id].consistencyMatchCount = 0;
                pData[id].acScore += 5;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "Macro (Consistency)");
                pData[id].lastScoreUpdate = now;
                AC_Log(AC_va("[DET] MACRO CONSISTENCY | %d identical gaps (%dms) | P: %s", 
                    cvars.macro_consistency_count, consistencyGap, client->name));
                Plugin_ChatPrintf(-1, "^1[Nektum Shield] ^7Do not use macro^1 %s! ^7It's not allowed and you will get banned^1!", client->name);
            }
        }
        pData[id].lastConsistencyGap = consistencyGap;
        pData[id].lastConsistencyTime = nowMs;
    }

    qboolean wasFiringPrev = pData[id].wasFiring;
    pData[id].wasFiring = firing;

    if (firing) {
        if (wasFiringPrev) {
            pData[id].firingFrames++;
            int dp = cp - pData[id].lastPitch;
            int dy = cy - pData[id].lastYaw;
            
            if (dp > 32768) dp -= 65536;
            if (dp < -32768) dp += 65536;
            
            if (dy > 32768) dy -= 65536;
            if (dy < -32768) dy += 65536;
            
            float pd = ShortToAngle(cp); if (pd > 180.0f) pd -= 360.0f;
            if (pd <= 80.0f && pd >= -80.0f && dy > -3 && dy < 3) pData[id].recoilZeroYawFrames++;
            float dpf = (float)dp;

            if (pData[id].isADS) {
                if (abs(dp) <= 8 && abs(dy) <= 8) {
                    pData[id].adsZeroMoveFrames++;
                } else {
                    pData[id].adsZeroMoveFrames = 0;
                }
            } else {
                pData[id].adsZeroMoveFrames = 0;
            }
            pData[id].pitchSum += dpf;
            pData[id].pitchSqSum += dpf * dpf;
            pData[id].recoilSamples++;
        } else { 
            pData[id].firingFrames = 1; 
            pData[id].recoilZeroYawFrames = 0;
            pData[id].adsZeroMoveFrames = 0;
        }
    } else {
        if (wasFiringPrev && pData[id].recoilSamples > cvars.recoil_samples) {
            float meanPitch = pData[id].pitchSum / (float)pData[id].recoilSamples;
            float varPitch = (pData[id].pitchSqSum / (float)pData[id].recoilSamples) - (meanPitch * meanPitch);
            if (varPitch < cvars.recoil_macro_var && meanPitch < cvars.recoil_macro_mean && now - pData[id].lastMacroDetTime > 10.0f) {
                pData[id].acScore += 10;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "No-Recoil Macro");
                pData[id].lastScoreUpdate = now; pData[id].lastMacroDetTime = now;
                AC_Log(AC_va("[DET] No-Recoil | Pitch Var: %.2f, Mean: %.2f | P: %s", varPitch, meanPitch, client->name));
            }
            float ys = (float)pData[id].recoilZeroYawFrames / (float)pData[id].firingFrames;
            if (ys > cvars.norecoil_threshold && now - pData[id].lastNoRecoilTime > 10.0f) {
                pData[id].acScore += 10; snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "No Recoil (Static)");
                pData[id].lastScoreUpdate = now; pData[id].lastNoRecoilTime = now;
                AC_Log(AC_va("[DET] No Recoil | DESC: %.1f%% stability | P: %s", ys * 100.0f, client->name));
            }
            
            float zeroMoveRatio = (float)pData[id].adsZeroMoveFrames / (float)pData[id].firingFrames;
            
            if (pData[id].adsZeroMoveFrames >= cvars.ads_norecoil_frames  // ✅ CVAR használata
                && pData[id].firingFrames >= 120                           // ✅ Legalább 100 frame (2 mp)
                && zeroMoveRatio >= 0.80f                                  // ✅ Legalább 70% zero-move
                && now - pData[id].lastNoRecoilTime > 30.0f) {
                pData[id].acScore += 20;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "No-Recoil");
                pData[id].lastScoreUpdate = now;
                pData[id].lastNoRecoilTime = now;
                AC_Log(AC_va("[DET] No-Recoil ADS | %d zero-move frames / %d total frames (%.1f%%) | P: %s", 
                    pData[id].adsZeroMoveFrames, pData[id].firingFrames,
                    zeroMoveRatio * 100.0f,
                    client->name));
            }
        }
        pData[id].firingFrames = 0; 
        pData[id].recoilZeroYawFrames = 0; 
        pData[id].adsZeroMoveFrames = 0;
        pData[id].pitchSum = 0.0f; 
        pData[id].pitchSqSum = 0.0f; 
        pData[id].recoilSamples = 0;
    }
    
    pData[id].lastPitch = cp; pData[id].lastYaw = cy;
    VectorCopy(ca, pData[id].lastAngles); pData[id].lastCmdTime = ucmd->serverTime;
    if (pData[id].acScore >= cvars.score_ban && !pData[id].pendingBan) {
        pData[id].pendingBan = qtrue;
        snprintf(pData[id].pendingReason, sizeof(pData[id].pendingReason), "%s", pData[id].lastReason[0] ? pData[id].lastReason : "Nektum Shield: You've been banned from this server.");
    }
}

PCL void OnPlayerKilled(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int meansOfDeath, int iWeapon, hitLocation_t hitLocation) {
    (void)inflictor; (void)damage; (void)iWeapon;
    if (!self || !attacker || !attacker->client || attacker == self) return;
    team_t at = attacker->client->sess.cs.team;
    team_t vt = (self->client) ? self->client->sess.cs.team : TEAM_FREE;
    if (at != TEAM_FREE && at == vt) return;
    int id = attacker->s.clientNum;
    if (id < 0 || id >= MAXP) return;
    client_t* cl = Plugin_GetClientForClientNum(id);
    if (!cl || IsBot(cl) || pData[id].pendingBan || pData[id].scheduledDropTime > 0) return;
    gclient_t *gcl = attacker->client;
    if (gcl->sess.sessionState != STATE_PLAYING) return;
    if (meansOfDeath != MOD_PISTOL_BULLET && meansOfDeath != MOD_RIFLE_BULLET && meansOfDeath != MOD_HEAD_SHOT) return;
    level_locals_t* level = Plugin_GetLevelBase();
    float now = level ? (level->time / 1000.0f) : 0.0f;
    
    pData[id].lastScoreUpdate = now; 
    pData[id].kills++;
    
    int nowMs = Plugin_Milliseconds();
    
    if (pData[id].lastKillTime > 0 && (nowMs - pData[id].lastKillTime) < 5000) {
        float timeBetweenKills = (float)(nowMs - pData[id].lastKillTime);
        
        vec3_t dir1, dir2;
        VectorSubtract(pData[id].lastKillVictimOrigin, attacker->r.currentOrigin, dir1);
        VectorSubtract(self->r.currentOrigin, attacker->r.currentOrigin, dir2);
        VectorNormalize(dir1, dir1);
        VectorNormalize(dir2, dir2);
        float dot = DotProduct(dir1, dir2);
        
        if (dot > 1.0f) dot = 1.0f;
        if (dot < -1.0f) dot = -1.0f;
        
        float angleBetweenVictims = acosf(dot) * (180.0f / (float)M_PI);
        
        if (timeBetweenKills > 50.0f && timeBetweenKills < (cvars.target_switch_time * 1000.0f) && angleBetweenVictims > cvars.target_switch_angle) {
            pData[id].rapidSwitchKills++;
            AC_Log(AC_va("[DET] Target Switch | 2 kills in %.0fms, %.1f° apart | P: %s", 
                timeBetweenKills, angleBetweenVictims, cl->name));
            
            if (pData[id].rapidSwitchKills >= cvars.target_switch_streak) {
                pData[id].acScore += 20;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "Aim Assist (Target Switching)");
                pData[id].lastScoreUpdate = now;
                pData[id].rapidSwitchKills = 0;
            }
        } else {
            pData[id].rapidSwitchKills = 0;
        }
    }
    
    pData[id].lastKillTime = nowMs;
    VectorCopy(self->r.currentOrigin, pData[id].lastKillVictimOrigin);
    
    if (hitLocation == HITLOC_HEAD || hitLocation == HITLOC_HELMET || hitLocation == HITLOC_NECK || hitLocation == HITLOC_TORSO_UPR) {
        pData[id].headshots++; 
        pData[id].hsStreak++;
        if (pData[id].hsStreak == 1) pData[id].firstHsInStreakTime = now;
    } else {
        pData[id].hsStreak = 0;
    }
    
    if (hitLocation == HITLOC_TORSO_UPR || hitLocation == HITLOC_TORSO_LWR) {
        pData[id].blStreak++;
        if (pData[id].blStreak == 1) pData[id].firstBlInStreakTime = now;
    } else {
        pData[id].blStreak = 0;
    }

    if (pData[id].kills >= cvars.min_kills) {
        int killsSinceLastCheck = pData[id].kills - pData[id].lastHsRatioCheckKills;
        if (killsSinceLastCheck >= cvars.min_kills) {
            float hr = (float)pData[id].headshots / (float)pData[id].kills;
            if (hr > cvars.hs_ratio) {
                pData[id].acScore += 20;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aimbot");
                pData[id].lastScoreUpdate = now;
                pData[id].lastHsRatioCheckKills = pData[id].kills;
                AC_Log(AC_va("[DET] Aimbot | HS ratio %.2f (%d/%d) | P: %s", hr, pData[id].headshots, pData[id].kills, cl->name));
            } else { 
                pData[id].lastHsRatioCheckKills = pData[id].kills; 
            }
        }
    }
    
    if (pData[id].hsStreak >= cvars.time_hs_streak) {
        if (now - pData[id].firstHsInStreakTime <= cvars.time_hs_window) {
            pData[id].acScore += 30; 
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aimbot");
            pData[id].lastScoreUpdate = now;
            AC_Log(AC_va("[DET] Time HS Streak | %d HS in %.1fs | P: %s", pData[id].hsStreak, now - pData[id].firstHsInStreakTime, cl->name));
            pData[id].hsStreak = 0;
        }
    }
    
    if (pData[id].blStreak >= cvars.time_bl_streak) {
        if (now - pData[id].firstBlInStreakTime <= cvars.time_bl_window) {
            pData[id].acScore += 30; 
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aim Assist");
            pData[id].lastScoreUpdate = now;
            AC_Log(AC_va("[DET] Time BL Streak | %d BL in %.1fs | P: %s", pData[id].blStreak, now - pData[id].firstBlInStreakTime, cl->name));
            pData[id].blStreak = 0;
        }
    }
    
    if (hitLocation != HITLOC_NONE) {
        if (hitLocation == pData[id].lastHitLoc) { 
            pData[id].sameHitStreak++; 
        } else { 
            pData[id].sameHitStreak = 1; 
            pData[id].lastHitLoc = hitLocation;
            pData[id].firstSameHitTime = now;
        }
        
        if (pData[id].sameHitStreak >= cvars.body_lock_streak && (now - pData[id].firstSameHitTime) <= cvars.time_bl_window && (now - pData[id].lastBodyLockTime) > 15.0f) {
            pData[id].acScore += 30;
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aim Assist");
            pData[id].lastBodyLockTime = now;
            pData[id].lastScoreUpdate = now;
            const char* locName = "Unknown";
            if (hitLocation == 1) locName = "Head";
            else if (hitLocation == 2) locName = "Neck";
            else if (hitLocation == 3) locName = "Torso Upper";
            else if (hitLocation == 4) locName = "Torso Lower";
            else if (hitLocation >= 5 && hitLocation <= 16) locName = "Limbs";
            AC_Log(AC_va("[DET] Time Body Lock | %d consecutive hits on %s in %.1fs | P: %s", 
                pData[id].sameHitStreak, locName, now - pData[id].firstSameHitTime, cl->name));
            pData[id].sameHitStreak = 0;
        }
    }
    
    if (pData[id].lastSnapTime > 0.0f && (now - pData[id].lastSnapTime) <= cvars.snap_kill_window) {
        pData[id].acScore += 8;
        snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aimbot");
        pData[id].lastScoreUpdate = now;
        float timeSinceSnap = now - pData[id].lastSnapTime;
        pData[id].lastSnapTime = 0.0f;
        AC_Log(AC_va("[DET] Snap + Kill | %.1f deg snap followed by kill in %.1fs | P: %s", 
            cvars.snap_kill_angle, timeSinceSnap, cl->name));
    }

    float dx = self->r.currentOrigin[0] - attacker->r.currentOrigin[0];
    float dy = self->r.currentOrigin[1] - attacker->r.currentOrigin[1];
    float dist = sqrtf(dx*dx + dy*dy);
    
    qboolean isMovingWhileShooting = qfalse;
    if (cl->lastUsercmd.forwardmove != 0 || cl->lastUsercmd.rightmove != 0) {
        isMovingWhileShooting = qtrue;
    }
    
    if (isMovingWhileShooting && dist > cvars.moving_hs_dist) {
        if (hitLocation == HITLOC_HEAD || hitLocation == HITLOC_HELMET || hitLocation == HITLOC_NECK) {
            pData[id].movingHeadshotCount++;
            
            if (pData[id].movingHeadshotCount >= cvars.moving_hs_streak && (now - pData[id].lastMovingHsTime) <= cvars.moving_hs_window) {
                pData[id].acScore += 25;
                snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aimbot");
                pData[id].lastScoreUpdate = now;
                AC_Log(AC_va("[DET] Aimbot | %d HS while strafing at %.0f units | P: %s", 
                    pData[id].movingHeadshotCount, dist, cl->name));
                pData[id].movingHeadshotCount = 0;
            }
            pData[id].lastMovingHsTime = now;
        }
    } else {
        pData[id].movingHeadshotCount = 0;
    }

    if (dist > cvars.silent_aim_dist) {
        float ay = ShortToAngle(cl->lastUsercmd.angles[1]);
        float ap = ShortToAngle(cl->lastUsercmd.angles[0]); if (ap > 180.0f) ap -= 360.0f;
        float iy = 0, ip = 0; qboolean cs = qfalse;
        if (hitLocation == HITLOC_HEAD || hitLocation == HITLOC_HELMET || hitLocation == HITLOC_NECK || hitLocation == HITLOC_TORSO_UPR) {
            float dz = self->r.currentOrigin[2] - attacker->r.currentOrigin[2];
            iy = atan2f(dy, dx) * (180.0f / (float)M_PI); if (iy < 0) iy += 360.0f;
            ip = -atan2f(dz, dist) * (180.0f / (float)M_PI); cs = qtrue;
        } else if (hitLocation == HITLOC_TORSO_UPR || hitLocation == HITLOC_TORSO_LWR) {
            float dz = (self->r.currentOrigin[2] + 45.0f) - (attacker->r.currentOrigin[2] + 60.0f);
            iy = atan2f(dy, dx) * (180.0f / (float)M_PI); if (iy < 0) iy += 360.0f;
            ip = -atan2f(dz, dist) * (180.0f / (float)M_PI); cs = qtrue;
        }
        if (cs) {
            float yawDiff = GetYawDist(ay, iy);
            float pitchDiff = fabsf(ap - ip);
            if (yawDiff < cvars.perfect_yaw_pitch && pitchDiff < cvars.perfect_yaw_pitch) {
                if (pData[id].perfectShots > 0 && now - pData[id].lastPerfectKillTime > 15.0f) pData[id].perfectShots = 0;
                pData[id].perfectShots++; pData[id].lastPerfectKillTime = now;
            } else if (yawDiff > cvars.silent_aim_angle || pitchDiff > cvars.silent_aim_angle) {
                pData[id].perfectShots = 0;
            }
        } else {
            pData[id].perfectShots = 0;
        }
    } else {
        pData[id].perfectShots = 0;
    }
    
    if (pData[id].perfectShots >= cvars.perfect_shots && now - pData[id].lastPerfectShotTime > 60.0f) {
        pData[id].acScore += 20; 
        snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Aim Assist");
        pData[id].lastPerfectShotTime = now;
        AC_Log(AC_va("[DET] Soft Aimbot | %d perfect hitbox center shots | P: %s", pData[id].perfectShots, cl->name));
        pData[id].perfectShots = 0;
    }
    
    qboolean ads = (cl->lastUsercmd.buttons & 256) ? qtrue : qfalse;
    if (!ads && dist > cvars.hipfire_dist) {
        if (pData[id].hipfireLongKills > 0 && now - pData[id].lastHipfireKillTime > 10.0f) pData[id].hipfireLongKills = 0;
        pData[id].hipfireLongKills++; 
        pData[id].lastHipfireKillTime = now;
        if (pData[id].hipfireLongKills >= cvars.hipfire_kills && now - pData[id].lastHipfireFlagTime > 60.0f) {
            pData[id].acScore += 25; 
            snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "NS | Auto Detection: Silent Aim");
            pData[id].lastHipfireFlagTime = now;
            AC_Log(AC_va("[DET] Silent Aim | %d hipfire kills | P: %s", pData[id].hipfireLongKills, cl->name));
            pData[id].hipfireLongKills = 0;
        }
    }

    qboolean isBulletMod = (meansOfDeath == MOD_PISTOL_BULLET || meansOfDeath == MOD_RIFLE_BULLET || meansOfDeath == MOD_HEAD_SHOT);
    
    if (isBulletMod && dist > 600.0f && pData[id].hasShotData) {
        if ((now - pData[id].lastShotTime) <= 2.0f) {
            
            qboolean wasADS = pData[id].lastShotADS;
            
            float threshold;
            if (wasADS) {
                threshold = cvars.silent_aim_threshold_ads;
            } else {
                threshold = cvars.silent_aim_threshold_hipfire;
            }
            
            vec3_t attackerOrigin;
            attackerOrigin[0] = pData[id].lastShotOrigin[0];
            attackerOrigin[1] = pData[id].lastShotOrigin[1];
            
            float eyeHeight = 55.0f;
            if (cl->gentity->client) {
                eyeHeight = cl->gentity->client->ps.viewHeightCurrent;
                if (eyeHeight < 10.0f) eyeHeight = 55.0f;
            }
            attackerOrigin[2] = pData[id].lastShotOrigin[2] + eyeHeight;
            
            vec3_t victimOrigin;
            victimOrigin[0] = self->r.currentOrigin[0];
            victimOrigin[1] = self->r.currentOrigin[1];
            
            float victimEyeHeight = 55.0f;
            if (self->client) {
                victimEyeHeight = self->client->ps.viewHeightCurrent;
                if (victimEyeHeight < 10.0f) victimEyeHeight = 55.0f;
            }
            victimOrigin[2] = self->r.currentOrigin[2] + victimEyeHeight;
            
            vec3_t attackerAngles;
            attackerAngles[0] = pData[id].lastShotAngles[0];
            attackerAngles[1] = pData[id].lastShotAngles[1];
            attackerAngles[2] = 0.0f;
            
            vec3_t forward;
            AngleVectors(attackerAngles, forward);
            
            vec3_t dirToTarget;
            VectorSubtract(victimOrigin, attackerOrigin, dirToTarget);
            VectorNormalize(dirToTarget, dirToTarget);
            
            float dot = DotProduct(forward, dirToTarget);
            
            if (dot < threshold) {
                pData[id].silentAimCount++;
                
                if (now - pData[id].lastSilentAimTime > 30.0f) {
                    pData[id].silentAimCount = 1;
                }
                pData[id].lastSilentAimTime = now;
                
                if (pData[id].silentAimCount >= cvars.silent_aim_streak) {
                    pData[id].acScore += 20;
                    snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "Silent Aim (Redirection)");
                    pData[id].lastScoreUpdate = now;
                    AC_Log(AC_va("[DET] Silent Aim | %d impossible hits (Dot: %.3f, Threshold: %.2f, %s, Dist: %.0f) | P: %s", 
                        pData[id].silentAimCount, dot, threshold, wasADS ? "ADS" : "Hipfire", dist, cl->name));
                    pData[id].silentAimCount = 0;
                }
            }
        }
        
        pData[id].hasShotData = qfalse;
    }

    qboolean isSpreadWeapon = (meansOfDeath == MOD_RIFLE_BULLET || meansOfDeath == MOD_PISTOL_BULLET);
    qboolean isADS_Current = (cl->lastUsercmd.buttons & 256) ? qtrue : qfalse;
    qboolean suspiciousCondition = (dist > 600.0f && (!isADS_Current || isMovingWhileShooting));
    
    if (isSpreadWeapon && suspiciousCondition) {
        VectorCopy(self->r.currentOrigin, pData[id].lastHitPositions[pData[id].hitPosIndex]);
        pData[id].hitPosIndex = (pData[id].hitPosIndex + 1) % 5;
        
        if (now - pData[id].lastNoSpreadHitTime < 1.5f) {
            float maxDeviation = 0.0f;
            for (int i = 0; i < 4; i++) {
                for (int j = i + 1; j < 4; j++) {
                    int idx1 = (pData[id].hitPosIndex - 1 - i + 5) % 5;
                    int idx2 = (pData[id].hitPosIndex - 1 - j + 5) % 5;
                    vec3_t diff;
                    VectorSubtract(pData[id].lastHitPositions[idx1], pData[id].lastHitPositions[idx2], diff);
                    float d = VectorLength(diff);
                    if (d > maxDeviation) maxDeviation = d;
                }
            }
            
            if (maxDeviation < cvars.nospread_max_deviation) {
                pData[id].noSpreadHitCount++;
                if (pData[id].noSpreadHitCount >= cvars.nospread_streak) {
                    pData[id].acScore += 25;
                    snprintf(pData[id].lastReason, sizeof(pData[id].lastReason), "No-Spread (Impossible Accuracy)");
                    pData[id].lastScoreUpdate = now;
                    AC_Log(AC_va("[DET] No-Spread | %d hits within %.1f units at %.0f range | P: %s", 
                        pData[id].noSpreadHitCount, maxDeviation, dist, cl->name));
                    pData[id].noSpreadHitCount = 0;
                }
            } else {
                pData[id].noSpreadHitCount = 0;
            }
        }
        pData[id].lastNoSpreadHitTime = now;
    } else {
        pData[id].noSpreadHitCount = 0;
    }
    
    if (pData[id].acScore >= cvars.score_ban && !pData[id].pendingBan) {
        pData[id].pendingBan = qtrue;
        snprintf(pData[id].pendingReason, sizeof(pData[id].pendingReason), "%s", pData[id].lastReason[0] ? pData[id].lastReason : "Nektum Shield: You've been banned from this server.");
    }
}

PCL void OnPlayerDC(client_t* client, const char* reason) {
    (void)reason;
    if (!client || !client->gentity || IsBot(client)) return;
    int id = client->gentity->s.clientNum;
    if (id >= 0 && id < MAXP) memset(&pData[id], 0, sizeof(acData_t));
}

PCL void OnSpawnServer(void) {
    for (int i = 0; i < MAXP; i++) memset(&pData[i], 0, sizeof(acData_t));
}

PCL void OnFrame(void) {
    if (activeWebhookRequest) {
        Plugin_HTTP_SendReceiveData(activeWebhookRequest);
        if (activeWebhookRequest->complete) {
            Plugin_HTTP_FreeObj(activeWebhookRequest); activeWebhookRequest = NULL;
        } 
        else if (Plugin_Milliseconds() - webhookStartTime > 10000)
        {
            Plugin_HTTP_FreeObj(activeWebhookRequest); 
            activeWebhookRequest = NULL;
        }
    }
    if (!activeWebhookRequest && discordQueueCount > 0) {
        activeWebhookRequest = Plugin_HTTP_MakeHttpRequest(WEBHOOK_URL, "POST",
            (byte*)discordQueue[discordQueueHead], (int)strlen(discordQueue[discordQueueHead]),
            "Content-Type: application/json\r\n");
        if (activeWebhookRequest) webhookStartTime = Plugin_Milliseconds();
        discordQueueHead = (discordQueueHead + 1) % MAX_DISCORD_QUEUE;
        discordQueueCount--;
    }
    
    level_locals_t* level = Plugin_GetLevelBase();
    if (!level) return;
    int mc = level->maxclients;
    if (mc <= 0 || mc > MAXP) mc = MAXP;
    int now = Plugin_Milliseconds();
    for (int i = 0; i < mc; i++) {
        client_t* cl = Plugin_GetClientForClientNum(i);
        if (pData[i].pendingBan && pData[i].scheduledDropTime == 0) {
            if (cl && !IsBot(cl)) {
                Plugin_SetStat(i, STAT_CHEATER_MARK, 1);
                uint64_t pid = cl->playerid, sid = cl->steamid;
                netadr_t adr = cl->netchan.remoteAddress;
                char ib[64]; Plugin_NET_AdrToStringMT(&adr, ib, sizeof(ib));
                char subnet[20]; GetSubnet24(ib, subnet, sizeof(subnet));
                char cn[64]; StripColorCodes(cl->name, cn, sizeof(cn));
                if (cn[0] == '\0') snprintf(cn, sizeof(cn), "%s", cl->name);
                qboolean isCGNAT = IsCGNAT(ib);
                if (isCGNAT) {
                    NS_AddBan(pid, sid, cn, "", pData[i].pendingReason, "Nektum Shield Detection");
                    AC_Log(AC_va("[CGNAT AUTO BAN] IP: %s | PID: %llu | Reason: %s | P: %s", ib, (unsigned long long)pid, pData[i].pendingReason, cl->name));
                } else {
                    NS_AddBan(pid, sid, cn, subnet, pData[i].pendingReason, "Nektum Shield Detection");
                    AC_Log(AC_va("[AUTO BAN] IP: %s | Subnet: %s | PID: %llu | Reason: %s | P: %s", ib, subnet, (unsigned long long)pid, pData[i].pendingReason, cl->name));
                }
            }
            pData[i].scheduledDropTime = now + cvars.drop_delay_ms;
            snprintf(pData[i].dropReason, sizeof(pData[i].dropReason), "%s", pData[i].pendingReason);
            pData[i].pendingBan = qfalse;
        }
        if (pData[i].scheduledDropTime > 0 && now >= pData[i].scheduledDropTime) {
            Plugin_DropClient(i, AC_va("You've been banned from this server.\nReason: %s\nAppeal on discord.", pData[i].dropReason));
            pData[i].scheduledDropTime = 0;
            continue;
        }
    }
}

PCL void OnOneSecond(void) {
    level_locals_t* level = Plugin_GetLevelBase();
    if (!level || !level->clients || !level->gentities) return;
    
    cvars.score_ban = Plugin_Cvar_GetInteger(cv_score_ban);
    cvars.hs_ratio = Plugin_Cvar_GetValue(cv_hs_ratio);
    cvars.min_kills = Plugin_Cvar_GetInteger(cv_min_kills);
    cvars.snap_threshold = Plugin_Cvar_GetValue(cv_snap_threshold);
    cvars.silent_aim_angle = Plugin_Cvar_GetValue(cv_silent_aim_angle);
    cvars.silent_aim_dist = Plugin_Cvar_GetValue(cv_silent_aim_dist);
    cvars.body_lock_streak = Plugin_Cvar_GetInteger(cv_body_lock_streak);
    cvars.perfect_shots = Plugin_Cvar_GetInteger(cv_perfect_shots);
    cvars.perfect_yaw_pitch = Plugin_Cvar_GetValue(cv_perfect_yaw_pitch);
    cvars.hipfire_kills = Plugin_Cvar_GetInteger(cv_hipfire_kills);
    cvars.hipfire_dist = Plugin_Cvar_GetValue(cv_hipfire_dist);
    cvars.norecoil_threshold = Plugin_Cvar_GetValue(cv_norecoil_threshold);
    cvars.recoil_macro_var = Plugin_Cvar_GetValue(cv_recoil_macro_var);
    cvars.recoil_macro_mean = Plugin_Cvar_GetValue(cv_recoil_macro_mean);
    cvars.recoil_samples = Plugin_Cvar_GetInteger(cv_recoil_samples);
    cvars.alttab_move_dist = Plugin_Cvar_GetValue(cv_alttab_move_dist);
    cvars.alttab_move_time = Plugin_Cvar_GetValue(cv_alttab_move_time);
    cvars.drop_delay_ms = Plugin_Cvar_GetInteger(cv_drop_delay_ms);
    cvars.name_check_delay = Plugin_Cvar_GetInteger(cv_name_check_delay);
    
    cvars.time_hs_streak = Plugin_Cvar_GetInteger(cv_time_hs_streak);
    cvars.time_hs_window = Plugin_Cvar_GetValue(cv_time_hs_window);
    cvars.time_bl_streak = Plugin_Cvar_GetInteger(cv_time_bl_streak);
    cvars.time_bl_window = Plugin_Cvar_GetValue(cv_time_bl_window);
    cvars.snap_kill_angle = Plugin_Cvar_GetValue(cv_snap_kill_angle);
    cvars.snap_kill_window = Plugin_Cvar_GetValue(cv_snap_kill_window);
    
    cvars.moving_hs_dist = Plugin_Cvar_GetValue(cv_moving_hs_dist);
    cvars.moving_hs_streak = Plugin_Cvar_GetInteger(cv_moving_hs_streak);
    cvars.moving_hs_window = Plugin_Cvar_GetValue(cv_moving_hs_window);

    cvars.macro_logic_clicks = Plugin_Cvar_GetInteger(cv_macro_logic_clicks);
    cvars.macro_logic_window = Plugin_Cvar_GetInteger(cv_macro_logic_window);
    cvars.macro_logic_strikes = Plugin_Cvar_GetValue(cv_macro_logic_strikes);
    cvars.macro_scroll_count = Plugin_Cvar_GetInteger(cv_macro_scroll_count);
    cvars.macro_scroll_fast = Plugin_Cvar_GetInteger(cv_macro_scroll_fast);
    cvars.macro_scroll_reset = Plugin_Cvar_GetInteger(cv_macro_scroll_reset);
    cvars.macro_rate_count = Plugin_Cvar_GetInteger(cv_macro_rate_count);
    cvars.macro_rate_fast = Plugin_Cvar_GetInteger(cv_macro_rate_fast);
    cvars.macro_consistency_count = Plugin_Cvar_GetInteger(cv_macro_consistency_count);
    cvars.macro_consistency_gap = Plugin_Cvar_GetInteger(cv_macro_consistency_gap);

    cvars.silent_aim_threshold_hipfire = Plugin_Cvar_GetValue(cv_silent_aim_threshold_hipfire);
    cvars.silent_aim_threshold_ads = Plugin_Cvar_GetValue(cv_silent_aim_threshold_ads);
    cvars.silent_aim_streak = Plugin_Cvar_GetInteger(cv_silent_aim_streak);
    cvars.ads_norecoil_frames = Plugin_Cvar_GetInteger(cv_ads_norecoil_frames);

    cvars.nospread_max_deviation = Plugin_Cvar_GetValue(cv_nospread_max_deviation);
    cvars.nospread_streak = Plugin_Cvar_GetInteger(cv_nospread_streak);
    cvars.target_switch_time = Plugin_Cvar_GetValue(cv_target_switch_time);
    cvars.target_switch_angle = Plugin_Cvar_GetValue(cv_target_switch_angle);
    cvars.target_switch_streak = Plugin_Cvar_GetInteger(cv_target_switch_streak);
    
    float now = level->time / 1000.0f;
    int mc = level->maxclients;
    if (mc <= 0 || mc > MAXP) mc = MAXP;
    qboolean ib[MAXP]; client_t* cls[MAXP];
    for (int i = 0; i < mc; i++) { cls[i] = Plugin_GetClientForClientNum(i); ib[i] = IsBot(cls[i]); }
    for (int i = 0; i < mc; i++) {
        if (ib[i] || pData[i].pendingBan || pData[i].scheduledDropTime > 0) continue;
        client_t* cl = cls[i];
        if (!cl || !cl->gentity || !cl->gentity->client) continue;
        gclient_t* gcl = cl->gentity->client;
        if (gcl->sess.sessionState != STATE_PLAYING) continue;
        if (pData[i].acScore > 0 && now - pData[i].lastScoreUpdate > 15.0f) {
            pData[i].acScore -= 2;
            if (pData[i].acScore < 0) pData[i].acScore = 0;
            pData[i].lastScoreUpdate = now;
        }
    }
    for (int i = 0; i < mc; i++) {
        if (pData[i].pendingNameCheck && pData[i].nameCheckStartTime > 0) {
            int elapsed = (Plugin_Milliseconds() - pData[i].nameCheckStartTime) / 1000;
            if (elapsed >= cvars.name_check_delay) {
                PerformDelayedNameCheck(i);
                pData[i].pendingNameCheck = qfalse;
                pData[i].nameCheckStartTime = 0;
            }
        }
    }
}

/* ========================= LIFECYCLE ========================= */
PCL int OnInit(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" LOG_FILE, home);
    ac_log = fopen(path, "a+");
    Plugin_Cvar_Set("sv_kickbantime", "5256000");
    NS_LoadBans(); NS_LoadUsers(); NS_LoadMutes();
    Plugin_AddCommand("bb", Cmd_NS_Ban, 80);
    Plugin_AddCommand("ub", Cmd_NS_Unban, 80);
    Plugin_AddCommand("fu", Cmd_NS_FindUser, 80);
    Plugin_AddCommand("mute", Cmd_NS_Mute, 60);
    Plugin_AddCommand("unmute", Cmd_NS_Unmute, 60);

    cv_score_ban = Plugin_Cvar_RegisterInt("ac_score_ban", 30, 10, 100, 0, "Ban threshold");
    cv_min_kills = Plugin_Cvar_RegisterInt("ac_min_kills", 40, 5, 200, 0, "Min kills for HS check");
    cv_body_lock_streak = Plugin_Cvar_RegisterInt("ac_body_lock_streak", 15, 3, 20, 0, "Same hit location streak");
    cv_perfect_shots = Plugin_Cvar_RegisterInt("ac_perfect_shots", 7, 3, 15, 0, "Perfect center shots limit");
    cv_hipfire_kills = Plugin_Cvar_RegisterInt("ac_hipfire_kills", 10, 3, 20, 0, "Hipfire long range kills");
    cv_recoil_samples = Plugin_Cvar_RegisterInt("ac_recoil_samples", 7, 5, 30, 0, "Recoil minimum samples");
    cv_drop_delay_ms = Plugin_Cvar_RegisterInt("ac_drop_delay_ms", 5000, 1000, 30000, 0, "Ban kick delay (ms)");
    cv_name_check_delay = Plugin_Cvar_RegisterInt("ac_name_check_delay", 3, 1, 10, 0, "Name theft check delay (sec)");

    cv_hs_ratio = Plugin_Cvar_RegisterFloat("ac_hs_ratio", 0.8f, 0.1f, 1.0f, 0, "Max HS ratio");
    cv_snap_threshold = Plugin_Cvar_RegisterFloat("ac_snap_threshold", 150.0f, 50.0f, 180.0f, 0, "Snap aimbot angle");
    cv_silent_aim_angle = Plugin_Cvar_RegisterFloat("ac_silent_aim_angle", 3.5f, 1.0f, 45.0f, 0, "Silent aim angle threshold");
    cv_silent_aim_dist = Plugin_Cvar_RegisterFloat("ac_silent_aim_dist", 500.0f, 400.0f, 2000.0f, 0, "Silent aim min distance");
    cv_perfect_yaw_pitch = Plugin_Cvar_RegisterFloat("ac_perfect_yaw_pitch", 2.5f, 1.0f, 10.0f, 0, "Perfect shot yaw/pitch");
    cv_hipfire_dist = Plugin_Cvar_RegisterFloat("ac_hipfire_dist", 4000.0f, 1000.0f, 8000.0f, 0, "Hipfire distance threshold");
    cv_norecoil_threshold = Plugin_Cvar_RegisterFloat("ac_norecoil_threshold", 0.998f, 0.99f, 1.0f, 0, "No recoil threshold");
    cv_recoil_macro_var = Plugin_Cvar_RegisterFloat("ac_recoil_macro_var", 0.3f, 0.05f, 1.0f, 0, "No-recoil macro variance");
    cv_recoil_macro_mean = Plugin_Cvar_RegisterFloat("ac_recoil_macro_mean", -1.0f, -5.0f, 0.0f, 0, "No-recoil macro mean pitch");
    cv_alttab_move_dist = Plugin_Cvar_RegisterFloat("ac_alttab_move_dist", 80.0f, 1.0f, 80.0f, 0, "Alt-Tab movement distance");
    cv_alttab_move_time = Plugin_Cvar_RegisterFloat("ac_alttab_move_time", 15.0f, 0.5f, 15.0f, 0, "Alt-Tab movement time (sec)");
    
    cv_time_hs_streak = Plugin_Cvar_RegisterInt("ac_time_hs_streak", 6, 3, 20, 0, "Time-based HS streak limit");
    cv_time_hs_window = Plugin_Cvar_RegisterFloat("ac_time_hs_window", 10.0f, 2.0f, 30.0f, 0, "Time window for HS streak (seconds)");
    cv_time_bl_streak = Plugin_Cvar_RegisterInt("ac_time_bl_streak", 10, 3, 20, 0, "Time-based BL streak limit");
    cv_time_bl_window = Plugin_Cvar_RegisterFloat("ac_time_bl_window", 10.0f, 2.0f, 30.0f, 0, "Time window for BL streak (seconds)");
    cv_snap_kill_angle = Plugin_Cvar_RegisterFloat("ac_snap_kill_angle", 70.0f, 20.0f, 150.0f, 0, "Snap angle to track for follow-up kill");
    cv_snap_kill_window = Plugin_Cvar_RegisterFloat("ac_snap_kill_window", 1.0f, 0.5f, 5.0f, 0, "Time window for snap + kill (seconds)");
    
    cv_moving_hs_dist = Plugin_Cvar_RegisterFloat("ac_moving_hs_dist", 1500.0f, 500.0f, 5000.0f, 0, "Min distance for moving HS detection");
    cv_moving_hs_streak = Plugin_Cvar_RegisterInt("ac_moving_hs_streak", 5, 3, 15, 0, "Consecutive moving headshots to flag");
    cv_moving_hs_window = Plugin_Cvar_RegisterFloat("ac_moving_hs_window", 15.0f, 5.0f, 30.0f, 0, "Time window for moving HS streak (seconds)");

    cv_macro_logic_clicks = Plugin_Cvar_RegisterInt("ac_macro_logic_clicks", 5, 3, 20, 0, "Logic: clicks per window to trigger");
    cv_macro_logic_window = Plugin_Cvar_RegisterInt("ac_macro_logic_window", 300, 100, 1000, 0, "Logic: time window (ms)");
    cv_macro_logic_strikes = Plugin_Cvar_RegisterFloat("ac_macro_logic_strikes", 3.0f, 1.0f, 15.0f, 0, "Logic: strikes before ban score");
    cv_macro_scroll_count = Plugin_Cvar_RegisterInt("ac_macro_scroll_count", 4, 2, 15, 0, "Scroll: consecutive fast clicks");
    cv_macro_scroll_fast = Plugin_Cvar_RegisterInt("ac_macro_scroll_fast", 90, 30, 200, 0, "Scroll: fast click threshold (ms)");
    cv_macro_scroll_reset = Plugin_Cvar_RegisterInt("ac_macro_scroll_reset", 150, 100, 300, 0, "Scroll: reset threshold (ms)");
    cv_macro_rate_count = Plugin_Cvar_RegisterInt("ac_macro_rate_count", 7, 3, 20, 0, "Rate: consecutive fast clicks");
    cv_macro_rate_fast = Plugin_Cvar_RegisterInt("ac_macro_rate_fast", 120, 50, 200, 0, "Rate: fast click threshold (ms)");
    cv_macro_consistency_count = Plugin_Cvar_RegisterInt("ac_macro_consistency_count", 5, 3, 20, 0, "Consistency: matching gaps to trigger");
    cv_macro_consistency_gap = Plugin_Cvar_RegisterInt("ac_macro_consistency_gap", 150, 50, 300, 0, "Consistency: max gap to consider (ms)");

    cv_silent_aim_threshold_hipfire = Plugin_Cvar_RegisterFloat("ac_silent_aim_threshold_hipfire", 0.40f, 0.10f, 0.99f, 0, "Silent Aim: Hipfire max dot product");
    cv_silent_aim_threshold_ads = Plugin_Cvar_RegisterFloat("ac_silent_aim_threshold_ads", 0.85f, 0.50f, 0.99f, 0, "Silent Aim: ADS max dot product");
    cv_silent_aim_streak = Plugin_Cvar_RegisterInt("ac_silent_aim_streak", 4, 2, 10, 0, "Silent Aim: Impossible hits to trigger ban");
    cv_ads_norecoil_frames = Plugin_Cvar_RegisterInt("ac_ads_norecoil_frames", 130, 50, 150, 0, "No-Recoil: ADS zero-move frames to trigger (50fps)");

    cv_nospread_max_deviation = Plugin_Cvar_RegisterFloat("ac_nospread_max_deviation", 20.0f, 10.0f, 50.0f, 0, "No-Spread: Max deviation between consecutive hits");
    cv_nospread_streak = Plugin_Cvar_RegisterInt("ac_nospread_streak", 4, 3, 8, 0, "No-Spread: Consecutive hits to trigger");
    cv_target_switch_time = Plugin_Cvar_RegisterFloat("ac_target_switch_time", 0.15f, 0.10f, 0.40f, 0, "Target Switch: Max time between 2 kills (seconds)");
    cv_target_switch_angle = Plugin_Cvar_RegisterFloat("ac_target_switch_angle", 70.0f, 30.0f, 90.0f, 0, "Target Switch: Min angle between 2 victims");
    cv_target_switch_streak = Plugin_Cvar_RegisterInt("ac_target_switch_streak", 3, 2, 5, 0, "Target Switch: Consecutive impossible switches");

    cvars.score_ban = Plugin_Cvar_GetInteger(cv_score_ban);
    cvars.hs_ratio = Plugin_Cvar_GetValue(cv_hs_ratio);
    cvars.min_kills = Plugin_Cvar_GetInteger(cv_min_kills);
    cvars.snap_threshold = Plugin_Cvar_GetValue(cv_snap_threshold);
    cvars.silent_aim_angle = Plugin_Cvar_GetValue(cv_silent_aim_angle);
    cvars.silent_aim_dist = Plugin_Cvar_GetValue(cv_silent_aim_dist);
    cvars.body_lock_streak = Plugin_Cvar_GetInteger(cv_body_lock_streak);
    cvars.perfect_shots = Plugin_Cvar_GetInteger(cv_perfect_shots);
    cvars.perfect_yaw_pitch = Plugin_Cvar_GetValue(cv_perfect_yaw_pitch);
    cvars.hipfire_kills = Plugin_Cvar_GetInteger(cv_hipfire_kills);
    cvars.hipfire_dist = Plugin_Cvar_GetValue(cv_hipfire_dist);
    cvars.norecoil_threshold = Plugin_Cvar_GetValue(cv_norecoil_threshold);
    cvars.recoil_macro_var = Plugin_Cvar_GetValue(cv_recoil_macro_var);
    cvars.recoil_macro_mean = Plugin_Cvar_GetValue(cv_recoil_macro_mean);
    cvars.recoil_samples = Plugin_Cvar_GetInteger(cv_recoil_samples);
    cvars.alttab_move_dist = Plugin_Cvar_GetValue(cv_alttab_move_dist);
    cvars.alttab_move_time = Plugin_Cvar_GetValue(cv_alttab_move_time);
    cvars.drop_delay_ms = Plugin_Cvar_GetInteger(cv_drop_delay_ms);
    cvars.name_check_delay = Plugin_Cvar_GetInteger(cv_name_check_delay);
    
    cvars.time_hs_streak = Plugin_Cvar_GetInteger(cv_time_hs_streak);
    cvars.time_hs_window = Plugin_Cvar_GetValue(cv_time_hs_window);
    cvars.time_bl_streak = Plugin_Cvar_GetInteger(cv_time_bl_streak);
    cvars.time_bl_window = Plugin_Cvar_GetValue(cv_time_bl_window);
    cvars.snap_kill_angle = Plugin_Cvar_GetValue(cv_snap_kill_angle);
    cvars.snap_kill_window = Plugin_Cvar_GetValue(cv_snap_kill_window);
    
    cvars.moving_hs_dist = Plugin_Cvar_GetValue(cv_moving_hs_dist);
    cvars.moving_hs_streak = Plugin_Cvar_GetInteger(cv_moving_hs_streak);
    cvars.moving_hs_window = Plugin_Cvar_GetValue(cv_moving_hs_window);

    cvars.macro_logic_clicks = Plugin_Cvar_GetInteger(cv_macro_logic_clicks);
    cvars.macro_logic_window = Plugin_Cvar_GetInteger(cv_macro_logic_window);
    cvars.macro_logic_strikes = Plugin_Cvar_GetValue(cv_macro_logic_strikes);
    cvars.macro_scroll_count = Plugin_Cvar_GetInteger(cv_macro_scroll_count);
    cvars.macro_scroll_fast = Plugin_Cvar_GetInteger(cv_macro_scroll_fast);
    cvars.macro_scroll_reset = Plugin_Cvar_GetInteger(cv_macro_scroll_reset);
    cvars.macro_rate_count = Plugin_Cvar_GetInteger(cv_macro_rate_count);
    cvars.macro_rate_fast = Plugin_Cvar_GetInteger(cv_macro_rate_fast);
    cvars.macro_consistency_count = Plugin_Cvar_GetInteger(cv_macro_consistency_count);
    cvars.macro_consistency_gap = Plugin_Cvar_GetInteger(cv_macro_consistency_gap);

    cvars.silent_aim_threshold_hipfire = Plugin_Cvar_GetValue(cv_silent_aim_threshold_hipfire);
    cvars.silent_aim_threshold_ads = Plugin_Cvar_GetValue(cv_silent_aim_threshold_ads);
    cvars.silent_aim_streak = Plugin_Cvar_GetInteger(cv_silent_aim_streak);
    cvars.ads_norecoil_frames = Plugin_Cvar_GetInteger(cv_ads_norecoil_frames);

    cvars.nospread_max_deviation = Plugin_Cvar_GetValue(cv_nospread_max_deviation);
    cvars.nospread_streak = Plugin_Cvar_GetInteger(cv_nospread_streak);
    cvars.target_switch_time = Plugin_Cvar_GetValue(cv_target_switch_time);
    cvars.target_switch_angle = Plugin_Cvar_GetValue(cv_target_switch_angle);
    cvars.target_switch_streak = Plugin_Cvar_GetInteger(cv_target_switch_streak);

    Plugin_Printf("\n^2Nektum Shield has been started!\n");
    return 0;
}

PCL void OnInfoRequest(pluginInfo_t* info) {
    info->handlerVersion.major = PLUGIN_HANDLER_VERSION_MAJOR;
    info->handlerVersion.minor = PLUGIN_HANDLER_VERSION_MINOR;
    info->pluginVersion.major = 2;
    info->pluginVersion.minor = 8;
    snprintf(info->fullName, sizeof(info->fullName), "Nektum Shield");
    snprintf(info->shortDescription, sizeof(info->shortDescription), "COD4X Anti-Cheat & Identity Management");
}

PCL void OnTerminate(void) {
    if (activeWebhookRequest) { Plugin_HTTP_FreeObj(activeWebhookRequest); activeWebhookRequest = NULL; }
    if (ac_log) { fclose(ac_log); ac_log = NULL; }
}