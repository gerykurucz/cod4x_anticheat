/*
 * ============================================================
 *  NEKTUM SHIELD - Helper Functions
 *  Version: 3.0 Refactored
 *  Developer: Nobody
 * ============================================================
 */

#ifndef NEKTUMSHIELD_HELPERS_H
#define NEKTUMSHIELD_HELPERS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include "./pinc.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ========================= STRING HELPERS ========================= */

/**
 * Thread-safe string formatting with rotating buffers
 * @param format Printf-style format string
 * @return Formatted string (static buffer, overwritten on next call)
 */
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

/**
 * Strip color codes from a string (^0-^9)
 * @param in Input string with color codes
 * @param out Output buffer for cleaned string
 * @param outSize Size of output buffer
 */
void StripColorCodes(const char* in, char* out, int outSize) {
    if (!in || !out || outSize <= 0) return;
    
    int i = 0, j = 0;
    while (in[i] && j < outSize - 1) {
        if (in[i] == '^' && in[i+1] != '\0') { 
            i += 2; 
        } else { 
            out[j++] = in[i++]; 
        }
    }
    out[j] = '\0';
}

/**
 * Extract /24 subnet from IP address
 * @param ip Input IP address
 * @param out Output buffer for subnet
 * @param outSize Size of output buffer
 */
void GetSubnet24(const char* ip, char* out, int outSize) {
    if (!ip || !out || outSize <= 0) return;
    
    strncpy(out, ip, outSize - 1);
    out[outSize - 1] = '\0';
    char* lastDot = strrchr(out, '.');
    if (lastDot) *lastDot = '\0';
    else out[0] = '\0';
}

/**
 * Extract subnet from query string (handles both full IP and subnet)
 * @param query Input query string
 * @param subnet Output buffer for subnet
 * @param outSize Size of output buffer
 */
static inline void ExtractSubnet(const char* query, char* subnet, int outSize) {
    int dots = 0;
    for (int i = 0; query[i]; i++) {
        if (query[i] == '.') dots++;
    }
    if (dots == 3) GetSubnet24(query, subnet, outSize);
    else snprintf(subnet, outSize, "%s", query);
}

/* ========================= VALIDATION HELPERS ========================= */

/**
 * Check if string is a valid IP address
 * @param str String to check
 * @return qtrue if valid IP, qfalse otherwise
 */
static inline qboolean IsIPAddress(const char* str) {
    if (!str) return qfalse;
    
    int dots = 0, nums = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '.') dots++;
        else if (str[i] >= '0' && str[i] <= '9') nums++;
        else return qfalse;
    }
    return ((dots == 2 || dots == 3) && nums >= 4);
}

/**
 * Check if string is purely numeric (minimum 5 digits)
 * @param str String to check
 * @return qtrue if numeric, qfalse otherwise
 */
static inline qboolean IsNumeric(const char* str) {
    if (!str || !str[0]) return qfalse;
    
    int len = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9') return qfalse;
        len++;
    }
    return (len >= 5);
}

/**
 * Check if IP belongs to CGNAT range
 * @param ip IP address to check
 * @return qtrue if CGNAT, qfalse otherwise
 */
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

/**
 * Check if client is a bot
 * @param cl Client pointer
 * @return qtrue if bot, qfalse otherwise
 */
static inline qboolean IsBot(client_t *cl) {
    if (!cl) return qtrue;
    if (cl->netchan.remoteAddress.type == NA_BOT) return qtrue;
    if (cl->steamid == 0 && cl->playerid == 0) return qtrue;
    return qfalse;
}

/**
 * Check if two identities match (for ban/user lookup)
 * @param user User structure to compare
 * @param pid Player ID
 * @param sid Steam ID
 * @param subnet /24 subnet
 * @param clientStat Client stat value (Nektum ID)
 * @return qtrue if match found, qfalse otherwise
 */
static inline qboolean IsSameIdentity(const nsUser_t* user, uint64_t pid, uint64_t sid, const char* subnet, int clientStat) {
    if (!user || !subnet) return qfalse;
    
    if (clientStat >= 10000 && user->nektumID == clientStat) return qtrue;
    if (user->playerid != 0 && user->playerid == pid) return qtrue;
    if (user->steamid != 0 && user->steamid == sid) return qtrue;
    if (user->subnet24[0] != '\0' && subnet[0] != '\0' && 
        !IsCGNAT(subnet) && strcmp(user->subnet24, subnet) == 0) return qtrue;
    return qfalse;
}

/* ========================= MATH HELPERS ========================= */

/**
 * Calculate yaw angle difference (handles wraparound)
 * @param yaw1 First yaw angle
 * @param yaw2 Second yaw angle
 * @return Absolute difference in degrees (0-180)
 */
float GetYawDist(float yaw1, float yaw2) {
    float diff = fabsf(yaw1 - yaw2);
    if (diff > 180.0f) diff = 360.0f - diff;
    return diff;
}

/**
 * Convert short integer to angle (0-360 degrees)
 * @param s Short integer angle value
 * @return Angle in degrees
 */
float ShortToAngle(int s) {
    return (float)(s & 65535) * (360.0f / 65536.0f);
}

/**
 * Calculate dot product of two vectors
 * @param v1 First vector
 * @param v2 Second vector
 * @return Dot product result
 */
static inline float DotProduct(const vec3_t v1, const vec3_t v2) {
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

/**
 * Subtract vector b from vector a
 * @param a First vector
 * @param b Second vector
 * @param out Output vector (a - b)
 */
static inline void VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out) {
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

/**
 * Calculate vector length (magnitude)
 * @param v Input vector
 * @return Length of vector
 */
static inline float VectorLength(const vec3_t v) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

/**
 * Normalize vector to unit length
 * @param v Input vector
 * @param out Output normalized vector
 */
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

/**
 * Convert angles to forward direction vector
 * @param angles Input angles (pitch, yaw, roll)
 * @param forward Output forward vector
 */
static inline void AngleVectors(const vec3_t angles, vec3_t forward) {
    float angle;
    float sp, sy, cp, cy;
    
    angle = angles[0] * (M_PI / 180.0f);
    sp = sinf(angle);
    cp = cosf(angle);
    
    angle = angles[1] * (M_PI / 180.0f);
    sy = sinf(angle);
    cy = cosf(angle);
    
    if (forward) {
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;
    }
}

/* ========================= COMMUNICATION HELPERS ========================= */

/**
 * Send chat response to client or broadcast
 * @param slot Client slot (-1 for broadcast)
 * @param format Printf-style format string
 */
static inline void SendResponse(int slot, const char* format, ...) {
    static char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    
    if (slot >= 0 && slot < MAXP) {
        Plugin_ChatPrintf(slot, "%s", buffer);
    } else {
        Plugin_ChatPrintf(-1, "%s", buffer);
    }
}

#endif /* NEKTUMSHIELD_HELPERS_H */
