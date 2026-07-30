/*
 * ============================================================
 *  NEKTUM SHIELD - Admin Tools Implementation
 *  Banning system, admin commands, file management
 * ============================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "./pinc.h"
#include "nektumshield_helpers.h"
#include "nektumshield_dvars.h"
#include "nektumshield_admin.h"
#include "nektumshield_detection.h"

/* ========================= GLOBAL STATE ========================= */
FILE *ac_log = NULL;
char discordQueue[MAX_DISCORD_QUEUE][2200];
int discordQueueHead = 0;
int discordQueueTail = 0;
int discordQueueCount = 0;

uint64_t lastBlockedPID = 0;
time_t lastBlockedTime = 0;
char lastBlockedIP[64] = "";

nsBan_t g_banlist[MAX_BANS];
int g_numBans = 0;

nsUser_t g_users[MAX_USERS];
int g_numUsers = 0;
int g_nextNektumID = 10000;

nsMute_t g_mutedPlayers[MAX_MUTES];
int g_numMutes = 0;

/* ========================= DISCORD QUEUE ========================= */
void QueueDiscordMessage(const char* payload) {
    if (discordQueueCount >= MAX_DISCORD_QUEUE) {
        discordQueueHead = (discordQueueHead + 1) % MAX_DISCORD_QUEUE;
        discordQueueCount--;
    }
    
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
}

/* ========================= FILE MANAGEMENT ========================= */
void NS_LoadBans(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" BANLIST_FILE, home);
    FILE* f = fopen(path, "r");
    if (!f) return;
    
    char line[512]; 
    g_numBans = 0;
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
        
        g_banlist[g_numBans].playerid = pid; 
        g_banlist[g_numBans].steamid = sid;
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

void NS_LoadUsers(void) {
    char home[512], path[600];
    Plugin_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
    snprintf(path, sizeof(path), "%s/" USERS_FILE, home);
    FILE* f = fopen(path, "r");
    if (!f) { g_numUsers = 0; g_nextNektumID = 10000; return; }
    
    char line[512]; 
    g_numUsers = 0; 
    g_nextNektumID = 10000;
    while (fgets(line, sizeof(line), f) && g_numUsers < MAX_USERS) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;
        
        char* token = strtok(line, "|"); if (!token) continue; uint64_t pid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; uint64_t sid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; char n[64]; snprintf(n, sizeof(n), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; char sn[20]; snprintf(sn, sizeof(sn), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; int nid = atoi(token);
        
        g_users[g_numUsers].playerid = pid; 
        g_users[g_numUsers].steamid = sid;
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
    if (!f) { g_numMutes = 0; return; }
    
    char line[512]; 
    g_numMutes = 0;
    while (fgets(line, sizeof(line), f) && g_numMutes < MAX_MUTES) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;
        
        char* token = strtok(line, "|"); if (!token) continue; uint64_t pid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; uint64_t sid = strtoull(token, NULL, 10);
        token = strtok(NULL, "|"); if (!token) continue; char n[64]; snprintf(n, sizeof(n), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; char a[64]; snprintf(a, sizeof(a), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; char r[128]; snprintf(r, sizeof(r), "%s", token);
        token = strtok(NULL, "|"); if (!token) continue; time_t mt = strtoll(token, NULL, 10);
        
        g_mutedPlayers[g_numMutes].playerid = pid; 
        g_mutedPlayers[g_numMutes].steamid = sid;
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
            g_mutedPlayers[i].name, g_mutedPlayers[i].admin, g_mutedPlayers[i].reason,
            (long long)g_mutedPlayers[i].muteTime);
    }
    fclose(f);
}

/* ========================= BAN MANAGEMENT ========================= */
void NS_AddBan(uint64_t pid, uint64_t sid, const char* name, const char* subnet24, const char* reason, const char* admin) {
    if (g_numBans >= MAX_BANS) { 
        Plugin_Printf("^1[Nektum Shield] Banlist is full!\n"); 
        return; 
    }
    
    for (int i = 0; i < g_numBans; i++) {
        if (g_banlist[i].playerid == pid && pid != 0) return;
    }
    
    g_banlist[g_numBans].playerid = pid; 
    g_banlist[g_numBans].steamid = sid;
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

qboolean NS_IsPlayerMuted(uint64_t pid) {
    for (int i = 0; i < g_numMutes; i++) {
        if (g_mutedPlayers[i].playerid == pid && pid != 0) return qtrue;
        if (g_mutedPlayers[i].steamid == pid && pid != 0) return qtrue;
    }
    return qfalse;
}

void ExecuteBan(int targetSlot, const char* reason, const char* adminName, int invokerSlot) {
    client_t* cl = Plugin_GetClientForClientNum(targetSlot);
    if (!cl || !cl->gentity) return;
    
    netadr_t adr; 
    adr = cl->netchan.remoteAddress;
    char ipBuf[64]; 
    Plugin_NET_AdrToStringMT(&adr, ipBuf, sizeof(ipBuf));
    char subnet24[20]; 
    GetSubnet24(ipBuf, subnet24, sizeof(subnet24));
    
    NS_AddBan(cl->playerid, cl->steamid, cl->name, subnet24, reason, adminName);
    
    char cleanName[64]; StripColorCodes(cl->name, cleanName, sizeof(cleanName));
    AC_Log(AC_va("[ADMIN] %s banned %s (PID: %llu | SID: %llu | IP: %s) - Reason: %s", 
        adminName, cleanName, (unsigned long long)cl->playerid, (unsigned long long)cl->steamid, ipBuf, reason));
    
    Plugin_SetStat(targetSlot, STAT_CHEATER_MARK, 1);
    Plugin_ChatPrintf(-1, "^2[Nektum Shield] ^7Admin: ^2%s ^7permanently banned %s: %s", adminName, cl->name, reason);
    pData[targetSlot].scheduledDropTime = Plugin_Milliseconds() + cvars.drop_delay_ms;
}

void ProcessBan(int invokerSlot, const char* query, const char* reason) {
    char adminName[64] = "Console";
    int adminSlot = invokerSlot;
    
    if (invokerSlot >= 0) {
        client_t* cl = Plugin_GetClientForClientNum(invokerSlot);
        if (cl) snprintf(adminName, sizeof(adminName), "%s", cl->name);
    }
    
    if (IsIPAddress(query) || IsNumeric(query)) {
        level_locals_t *level = Plugin_GetLevelBase();
        if (!level) return;
        
        for (int i = 0; i < level->maxclients; i++) {
            client_t* cl = Plugin_GetClientForClientNum(i);
            if (!cl || cl->state != CS_ACTIVE || IsBot(cl)) continue;
            
            char ipBuf[64]; Plugin_NET_AdrToStringMT(&cl->netchan.remoteAddress, ipBuf, sizeof(ipBuf));
            char subnet24[20]; GetSubnet24(ipBuf, subnet24, sizeof(subnet24));
            
            qboolean match = qfalse;
            if (IsIPAddress(query) && strstr(ipBuf, query) != NULL) match = qtrue;
            else if (IsNumeric(query) && (cl->playerid == strtoull(query, NULL, 10) || cl->steamid == strtoull(query, NULL, 10))) match = qtrue;
            
            if (match) {
                ExecuteBan(i, reason, adminName, invokerSlot);
                SendResponse(adminSlot, "^2[Nektum Shield] ^7Banned %s", cl->name);
                return;
            }
        }
        SendResponse(adminSlot, "^1[Nektum Shield] ^7Player not found!");
        return;
    }
    
    level_locals_t *level = Plugin_GetLevelBase();
    if (!level) return;
    
    for (int i = 0; i < level->maxclients; i++) {
        client_t* cl = Plugin_GetClientForClientNum(i);
        if (!cl || cl->state != CS_ACTIVE || IsBot(cl)) continue;
        
        char cleanName[64]; StripColorCodes(cl->name, cleanName, sizeof(cleanName));
        char lcn[64], lqn[64];
        snprintf(lcn, sizeof(lcn), "%s", cleanName);
        snprintf(lqn, sizeof(lqn), "%s", query);
        for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
        for (int k = 0; lqn[k]; k++) lqn[k] = tolower((unsigned char)lqn[k]);
        
        if (strstr(lcn, lqn) != NULL) {
            ExecuteBan(i, reason, adminName, invokerSlot);
            SendResponse(adminSlot, "^2[Nektum Shield] ^7Banned %s", cl->name);
            return;
        }
    }
    
    char subnetQuery[20]; ExtractSubnet(query, subnetQuery, sizeof(subnetQuery));
    if (subnetQuery[0] != '\0') {
        for (int i = 0; i < g_numUsers; i++) {
            if (g_users[i].subnet24[0] != '\0' && strcmp(g_users[i].subnet24, subnetQuery) == 0 && !IsCGNAT(subnetQuery)) {
                NS_AddBan(0, 0, "", subnetQuery, AC_va("Subnet ban by %s: %s", adminName, reason), adminName);
                SendResponse(adminSlot, "^2[Nektum Shield] ^7Subnet %s banned", subnetQuery);
                AC_Log(AC_va("[ADMIN] %s banned subnet %s - Reason: %s", adminName, subnetQuery, reason));
                return;
            }
        }
    }
    
    SendResponse(adminSlot, "^1[Nektum Shield] ^7No matching player or subnet found!");
}

void ProcessUnban(int invokerSlot, const char* query) {
    char adminName[64] = "Console";
    if (invokerSlot >= 0) {
        client_t* cl = Plugin_GetClientForClientNum(invokerSlot);
        if (cl) snprintf(adminName, sizeof(adminName), "%s", cl->name);
    }
    
    if (g_numBans == 0) {
        SendResponse(invokerSlot, "^1[Nektum Shield] ^7Banlist is empty!");
        return;
    }
    
    int removed = -1;
    if (IsNumeric(query)) {
        uint64_t id = strtoull(query, NULL, 10);
        for (int i = 0; i < g_numBans; i++) {
            if (g_banlist[i].playerid == id || g_banlist[i].steamid == id) {
                removed = i;
                break;
            }
        }
    }
    
    if (removed == -1) {
        char lq[64]; snprintf(lq, sizeof(lq), "%s", query);
        for (int k = 0; lq[k]; k++) lq[k] = tolower((unsigned char)lq[k]);
        
        for (int i = 0; i < g_numBans; i++) {
            if (g_banlist[i].name[0] == '\0') continue;
            char bn[64]; StripColorCodes(g_banlist[i].name, bn, sizeof(bn));
            for (int k = 0; bn[k]; k++) bn[k] = tolower((unsigned char)bn[k]);
            if (strstr(bn, lq) != NULL) { removed = i; break; }
        }
    }
    
    if (removed == -1) {
        SendResponse(invokerSlot, "^1[Nektum Shield] ^7Ban not found!");
        return;
    }
    
    char removedName[64], removedReason[128];
    snprintf(removedName, sizeof(removedName), "%s", g_banlist[removed].name);
    snprintf(removedReason, sizeof(removedReason), "%s", g_banlist[removed].reason);
    
    for (int i = removed; i < g_numBans - 1; i++) {
        g_banlist[i] = g_banlist[i + 1];
    }
    g_numBans--;
    NS_SaveBans();
    
    SendResponse(invokerSlot, "^2[Nektum Shield] ^7Unbanned %s (Reason: %s)", removedName, removedReason);
    AC_Log(AC_va("[ADMIN] %s unbanned %s - Original reason: %s", adminName, removedName, removedReason));
}

void ProcessFindUser(int invokerSlot, const char* query) {
    if (g_numUsers == 0) {
        SendResponse(invokerSlot, "^1[Nektum Shield] ^7User database is empty!");
        return;
    }
    
    int foundCount = 0;
    char response[2048] = "^2[Nektum Shield] ^7Found:\n";
    
    if (IsNumeric(query)) {
        uint64_t id = strtoull(query, NULL, 10);
        for (int i = 0; i < g_numUsers; i++) {
            if (g_users[i].playerid == id || g_users[i].steamid == id || g_users[i].nektumID == atoi(query)) {
                snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                    "^2%s^7 (NID: %d | PID: %llu | SID: %llu)\n", 
                    g_users[i].name, g_users[i].nektumID, 
                    (unsigned long long)g_users[i].playerid, (unsigned long long)g_users[i].steamid);
                foundCount++;
                if (foundCount >= 10) break;
            }
        }
    } else {
        char lq[64]; snprintf(lq, sizeof(lq), "%s", query);
        for (int k = 0; lq[k]; k++) lq[k] = tolower((unsigned char)lq[k]);
        
        for (int i = 0; i < g_numUsers; i++) {
            char un[64]; StripColorCodes(g_users[i].name, un, sizeof(un));
            for (int k = 0; un[k]; k++) un[k] = tolower((unsigned char)un[k]);
            
            if (strstr(un, lq) != NULL) {
                snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                    "^2%s^7 (NID: %d | PID: %llu | SID: %llu)\n", 
                    g_users[i].name, g_users[i].nektumID, 
                    (unsigned long long)g_users[i].playerid, (unsigned long long)g_users[i].steamid);
                foundCount++;
                if (foundCount >= 10) break;
            }
        }
    }
    
    if (foundCount == 0) SendResponse(invokerSlot, "^1[Nektum Shield] ^7No users found!");
    else SendResponse(invokerSlot, "%s", response);
}

/* ========================= ADMIN COMMANDS ========================= */
void Cmd_NS_Ban(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 80) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); 
        return; 
    }
    
    int argStart = 1;
    int adminSlot = invoker;
    if (invoker < 0 && Plugin_Cmd_Argc() >= 2) { 
        adminSlot = atoi(Plugin_Cmd_Argv(1)); 
        argStart = 2; 
    }
    
    if (Plugin_Cmd_Argc() < argStart + 2) { 
        SendResponse(adminSlot, "^1[Nektum Shield] ^7Usage: bb <query> <reason>"); 
        return; 
    }
    
    const char* query = Plugin_Cmd_Argv(argStart);
    char reason[256] = "";
    int len = 0;
    for (int i = argStart + 1; i < Plugin_Cmd_Argc(); i++) {
        len += snprintf(reason + len, sizeof(reason) - len, "%s%s", (i > argStart + 1) ? " " : "", Plugin_Cmd_Argv(i));
    }
    
    ProcessBan(adminSlot, query, reason);
}

void Cmd_NS_Unban(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 80) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); 
        return; 
    }
    
    if (Plugin_Cmd_Argc() < 2) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7Usage: ub <query>"); 
        return; 
    }
    
    ProcessUnban(invoker, Plugin_Cmd_Argv(1));
}

void Cmd_NS_FindUser(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 60) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); 
        return; 
    }
    
    if (Plugin_Cmd_Argc() < 2) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7Usage: fu <query>"); 
        return; 
    }
    
    ProcessFindUser(invoker, Plugin_Cmd_Argv(1));
}

void Cmd_NS_Mute(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 60) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); 
        return; 
    }
    
    if (Plugin_Cmd_Argc() < 3) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7Usage: mute <player> <reason>"); 
        return; 
    }
    
    char adminName[64] = "Console";
    if (invoker >= 0) {
        client_t* cl = Plugin_GetClientForClientNum(invoker);
        if (cl) snprintf(adminName, sizeof(adminName), "%s", cl->name);
    }
    
    const char* query = Plugin_Cmd_Argv(1);
    level_locals_t *level = Plugin_GetLevelBase();
    if (!level) return;
    
    for (int i = 0; i < level->maxclients; i++) {
        client_t* cl = Plugin_GetClientForClientNum(i);
        if (!cl || cl->state != CS_ACTIVE || IsBot(cl)) continue;
        
        char cleanName[64]; StripColorCodes(cl->name, cleanName, sizeof(cleanName));
        char lcn[64], lqn[64];
        snprintf(lcn, sizeof(lcn), "%s", cleanName);
        snprintf(lqn, sizeof(lqn), "%s", query);
        for (int k = 0; lcn[k]; k++) lcn[k] = tolower((unsigned char)lcn[k]);
        for (int k = 0; lqn[k]; k++) lqn[k] = tolower((unsigned char)lqn[k]);
        
        if (strstr(lcn, lqn) != NULL) {
            if (NS_IsPlayerMuted(cl->playerid)) {
                SendResponse(invoker, "^1[Nektum Shield] ^7%s is already muted!", cl->name);
                return;
            }
            
            for (int k = 0; k < g_numMutes; k++) {
                if (g_mutedPlayers[k].playerid == 0 && g_mutedPlayers[k].steamid == 0) {
                    g_mutedPlayers[k].playerid = cl->playerid;
                    g_mutedPlayers[k].steamid = cl->steamid;
                    snprintf(g_mutedPlayers[k].name, sizeof(g_mutedPlayers[k].name), "%s", cl->name);
                    snprintf(g_mutedPlayers[k].admin, sizeof(g_mutedPlayers[k].admin), "%s", adminName);
                    
                    char reason[256] = "";
                    int len = 0;
                    for (int j = 2; j < Plugin_Cmd_Argc(); j++) {
                        len += snprintf(reason + len, sizeof(reason) - len, "%s%s", (j > 2) ? " " : "", Plugin_Cmd_Argv(j));
                    }
                    snprintf(g_mutedPlayers[k].reason, sizeof(g_mutedPlayers[k].reason), "%s", reason);
                    g_mutedPlayers[k].muteTime = Plugin_GetRealtime();
                    g_numMutes++;
                    NS_SaveMutes();
                    
                    AC_Log(AC_va("[ADMIN] %s muted %s - Reason: %s", adminName, cl->name, reason));
                    SendResponse(-1, "^2[Nektum Shield] ^7%s ^7muted ^2%s", adminName, cl->name);
                    return;
                }
            }
            SendResponse(invoker, "^1[Nektum Shield] ^7Mute list is full!");
            return;
        }
    }
    SendResponse(invoker, "^1[Nektum Shield] ^7Player not found!");
}

void Cmd_NS_Unmute(void) {
    int invoker = Plugin_Cmd_GetInvokerSlot();
    if (Plugin_Cmd_GetInvokerPower() < 60) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7No permission."); 
        return; 
    }
    
    if (Plugin_Cmd_Argc() < 2) { 
        SendResponse(invoker, "^1[Nektum Shield] ^7Usage: unmute <player>"); 
        return; 
    }
    
    char adminName[64] = "Console";
    if (invoker >= 0) {
        client_t* cl = Plugin_GetClientForClientNum(invoker);
        if (cl) snprintf(adminName, sizeof(adminName), "%s", cl->name);
    }
    
    const char* query = Plugin_Cmd_Argv(1);
    int removed = -1;
    char removedName[64] = "";
    
    if (IsNumeric(query)) {
        uint64_t id = strtoull(query, NULL, 10);
        for (int i = 0; i < g_numMutes; i++) {
            if (g_mutedPlayers[i].playerid == id || g_mutedPlayers[i].steamid == id) {
                removed = i;
                snprintf(removedName, sizeof(removedName), "%s", g_mutedPlayers[i].name);
                break;
            }
        }
    }
    
    if (removed == -1) {
        char lq[64]; snprintf(lq, sizeof(lq), "%s", query);
        for (int k = 0; lq[k]; k++) lq[k] = tolower((unsigned char)lq[k]);
        
        for (int i = 0; i < g_numMutes; i++) {
            char mn[64]; StripColorCodes(g_mutedPlayers[i].name, mn, sizeof(mn));
            for (int k = 0; mn[k]; k++) mn[k] = tolower((unsigned char)mn[k]);
            if (strstr(mn, lq) != NULL) {
                removed = i;
                snprintf(removedName, sizeof(removedName), "%s", g_mutedPlayers[i].name);
                break;
            }
        }
    }
    
    if (removed == -1) {
        SendResponse(invoker, "^1[Nektum Shield] ^7No muted player found!");
        return;
    }
    
    for (int i = removed; i < g_numMutes - 1; i++) {
        g_mutedPlayers[i] = g_mutedPlayers[i + 1];
    }
    g_numMutes--;
    NS_SaveMutes();
    
    SendResponse(invoker, "^2[Nektum Shield] ^7Unmuted %s", removedName);
    AC_Log(AC_va("[ADMIN] %s unmuted %s", adminName, removedName));
}

/* ========================= PERSISTENT BAN SYSTEM ========================= */
void Nektum_PersistentBan(int clientNum, const char* reason) {
    client_t* cl = Plugin_GetClientForClientNum(clientNum);
    if (!cl || !cl->gentity) return;
    
    char ipBuf[64]; Plugin_NET_AdrToStringMT(&cl->netchan.remoteAddress, ipBuf, sizeof(ipBuf));
    char subnet24[20]; GetSubnet24(ipBuf, subnet24, sizeof(subnet24));
    
    NS_AddBan(cl->playerid, cl->steamid, cl->name, subnet24, reason, "Auto-Ban System");
    
    AC_Log(AC_va("[AUTO] Persistent ban applied! Player: %s (PID: %llu | SID: %llu | IP: %s) - Reason: %s", 
        cl->name, (unsigned long long)cl->playerid, (unsigned long long)cl->steamid, ipBuf, reason));
    
    Plugin_DropClient(clientNum, AC_va("English: You have been permanently banned from this server!\nReason: %s\n\nGerman: Sie wurden dauerhaft von diesem Server gebannt!\nGrund: %s", reason, reason));
}

/* ========================= NAME CHECK ========================= */
void PerformDelayedNameCheck(int id) {
    client_t* client = Plugin_GetClientForClientNum(id);
    if (!client || !client->gentity || IsBot(client)) return;
    
    char cleanName[64] = ""; 
    StripColorCodes(client->name, cleanName, sizeof(cleanName));
    char ipBuf[64]; 
    Plugin_NET_AdrToStringMT(&client->netchan.remoteAddress, ipBuf, sizeof(ipBuf));
    char currentSubnet[20]; 
    GetSubnet24(ipBuf, currentSubnet, sizeof(currentSubnet));
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
