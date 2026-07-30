/*
 * ============================================================
 *  NEKTUM SHIELD - Admin Tools Module
 *  Banning system, admin commands, file management
 * ============================================================
 */

#ifndef NEKTUMSHIELD_ADMIN_H
#define NEKTUMSHIELD_ADMIN_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "./pinc.h"

/* ========================= FORWARD DECLARATIONS ========================= */
extern FILE *ac_log;
extern char discordQueue[][2200];
extern int discordQueueHead, discordQueueTail, discordQueueCount;
extern uint64_t lastBlockedPID;
extern time_t lastBlockedTime;
extern char lastBlockedIP[];

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

extern nsBan_t g_banlist[];
extern int g_numBans;

typedef struct {
    uint64_t playerid;
    uint64_t steamid;
    char name[64];
    char subnet24[20];
    int nektumID;
} nsUser_t;

extern nsUser_t g_users[];
extern int g_numUsers;
extern int g_nextNektumID;

typedef struct {
    uint64_t playerid;
    uint64_t steamid;
    char name[64];
    char admin[64];
    char reason[128];
    time_t muteTime;
} nsMute_t;

extern nsMute_t g_mutedPlayers[];
extern int g_numMutes;

/* ========================= FUNCTION DECLARATIONS ========================= */

// File management
void NS_LoadBans(void);
void NS_SaveBans(void);
void NS_LoadUsers(void);
void NS_SaveUsers(void);
void NS_LoadMutes(void);
void NS_SaveMutes(void);

// Ban management
void NS_AddBan(uint64_t pid, uint64_t sid, const char* name, const char* subnet24, const char* reason, const char* admin);
qboolean NS_IsPlayerMuted(uint64_t pid);
void ExecuteBan(int targetSlot, const char* reason, const char* adminName, int invokerSlot);
void ProcessBan(int invokerSlot, const char* query, const char* reason);
void ProcessUnban(int invokerSlot, const char* query);
void ProcessFindUser(int invokerSlot, const char* query);

// Admin commands
void Cmd_NS_Ban(void);
void Cmd_NS_Unban(void);
void Cmd_NS_FindUser(void);
void Cmd_NS_Mute(void);
void Cmd_NS_Unmute(void);

// Persistent ban system
void Nektum_PersistentBan(int clientNum, const char* reason);

// Name check
void PerformDelayedNameCheck(int id);

#endif /* NEKTUMSHIELD_ADMIN_H */
