/*
 * CPI_RIFFUtil.c — shared RIFF parsing helpers.
 *
 * SkipToChunk is used by both CPI_Player_CoDec_WAV.c (when the C WAV codec
 * is compiled) and CPI_PlaylistItem.c (for WAV duration probing in the
 * playlist).  When ENABLE_RUST_CODECS=ON the C WAV codec is excluded, so
 * this file provides the definition instead.
 *
 * Compiled only when ENABLE_RUST_CODECS=ON (see CMakeLists.txt); when the
 * C WAV codec IS compiled, it supplies its own definition of SkipToChunk.
 */

#include "stdafx.h"
#include "CP_RIFFStructs.h"

BOOL SkipToChunk(HANDLE hFile, CPs_RIFFChunk* pChunk, const char cChunkID[4])
{
	DWORD dwBytesRead;
	int iMaxChunks = 4096;

	while (--iMaxChunks > 0
			&& ReadFile(hFile, pChunk, sizeof(*pChunk), &dwBytesRead, NULL)
			&& dwBytesRead == sizeof(*pChunk))
	{
		if (memcmp(pChunk->m_cID, cChunkID, 4) == 0)
			return TRUE;

		if (pChunk->m_dwLength == 0 || pChunk->m_dwLength > 0x7FFFFFFF)
			return FALSE;

		SetFilePointer(hFile, (LONG)pChunk->m_dwLength, NULL, FILE_CURRENT);
	}

	return FALSE;
}
