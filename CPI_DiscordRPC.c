/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CPI_DiscordRPC.h"
#include "CPI_PlaylistItem.h"
#include <discord_rpc.h>
#include <time.h>

////////////////////////////////////////////////////////////////////////////////
//
// Discord Rich Presence integration for BriskPlayer
//
// Shows "Now Playing" information in the user's Discord profile:
//   Details: "Artist - Title"  (or just title / filename)
//   State:   "Album"           (if available)
//   Timestamps: elapsed / remaining
//
////////////////////////////////////////////////////////////////////////////////

#define DISCORD_APP_ID "1492632255573528869"

////////////////////////////////////////////////////////////////////////////////
// Helper: build a "Artist - Title" details string (max 128 bytes for Discord)
static void drpc_build_details(char* buf, size_t buflen, CP_HPLAYLISTITEM hItem)
{
    const char* artist = hItem ? CPLI_GetArtist(hItem) : NULL;
    const char* title  = hItem ? CPLI_GetTrackName(hItem) : NULL;

    if (!title || !title[0])
        title = hItem ? CPLI_GetFilename(hItem) : "Unknown";

    if (artist && artist[0])
        _snprintf_s(buf, buflen, _TRUNCATE, "%s - %s", artist, title);
    else
        _snprintf_s(buf, buflen, _TRUNCATE, "%s", title);
}

////////////////////////////////////////////////////////////////////////////////
// Discord event handlers
static void drpc_ready(const DiscordUser* user)        { (void)user; }
static void drpc_disconnected(int ec, const char* msg) { (void)ec; (void)msg; }
static void drpc_errored(int ec, const char* msg)      { (void)ec; (void)msg; }

////////////////////////////////////////////////////////////////////////////////
void CPI_DiscordRPC_Init(void)
{
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready        = drpc_ready;
    handlers.disconnected = drpc_disconnected;
    handlers.errored      = drpc_errored;

    Discord_Initialize(DISCORD_APP_ID, &handlers, 1, NULL);
}

////////////////////////////////////////////////////////////////////////////////
void CPI_DiscordRPC_Shutdown(void)
{
    Discord_ClearPresence();
    Discord_Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
void CPI_DiscordRPC_SetPlaying(CP_HPLAYLISTITEM hItem, int iDurationSecs)
{
    DiscordRichPresence presence;
    char details[128];
    char state[128];

    memset(&presence, 0, sizeof(presence));

    drpc_build_details(details, sizeof(details), hItem);
    presence.details = details;

    // Album as state line
    {
        const char* album = hItem ? CPLI_GetAlbum(hItem) : NULL;
        if (album && album[0])
        {
            _snprintf_s(state, sizeof(state), _TRUNCATE, "%s", album);
            presence.state = state;
        }
    }

    // Timestamps — show elapsed time counting up
    {
        int64_t now = (int64_t)time(NULL);
        presence.startTimestamp = now;

        if (iDurationSecs > 0)
            presence.endTimestamp = now + (int64_t)iDurationSecs;
    }

    presence.largeImageKey  = "briskplayer";
    presence.largeImageText = "BriskPlayer";

    Discord_UpdatePresence(&presence);
}

////////////////////////////////////////////////////////////////////////////////
void CPI_DiscordRPC_SetPaused(CP_HPLAYLISTITEM hItem)
{
    DiscordRichPresence presence;
    char details[128];
    char state[128];

    memset(&presence, 0, sizeof(presence));

    drpc_build_details(details, sizeof(details), hItem);
    presence.details = details;

    {
        const char* album = hItem ? CPLI_GetAlbum(hItem) : NULL;
        if (album && album[0])
        {
            _snprintf_s(state, sizeof(state), _TRUNCATE, "%s", album);
            presence.state = state;
        }
    }

    // No timestamps when paused — Discord will just show the text
    presence.largeImageKey  = "briskplayer";
    presence.largeImageText = "BriskPlayer";
    presence.smallImageKey  = "paused";
    presence.smallImageText = "Paused";

    Discord_UpdatePresence(&presence);
}

////////////////////////////////////////////////////////////////////////////////
void CPI_DiscordRPC_Clear(void)
{
    Discord_ClearPresence();
}

////////////////////////////////////////////////////////////////////////////////
void CPI_DiscordRPC_Update(void)
{
    Discord_RunCallbacks();
}
