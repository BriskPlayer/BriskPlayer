/*
 * CoolPlayer - Blazing fast audio player.
 * Copyright (C) 2000-2001 Niek Albers
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
#include "globals.h"
#include "CPI_CircleBuffer.h"

// C23 threading support with fallback
#if HAVE_C23_THREADING
    #include <threads.h>
    #include <stdatomic.h>
    #include <time.h>
    #include <stdlib.h>  // For aligned_alloc
#else
    // For Windows fallback, ensure we have the necessary types
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <stdlib.h>
#endif

#define CIC_WAITTIMEOUT  3000
#define CIC_TIMEOUT_NS   (CIC_WAITTIMEOUT * 1000000LL)  // Convert to nanoseconds for C23
void CircleBufferUninitialise(CPs_CircleBuffer* pCBuffer);
void CircleBufferWrite(CPs_CircleBuffer* pCBuffer, const void* pSourceBuffer, const unsigned int iNumBytes);
BOOL CircleBufferRead(CPs_CircleBuffer* pCBuffer, void* pDestBuffer, const size_t iBytesToRead, size_t* pbBytesRead);
void CircleFlush(CPs_CircleBuffer* pCBuffer);
unsigned int CircleGetFreeSpace(CPs_CircleBuffer* pCBuffer);
unsigned int CircleGetUsedSpace(CPs_CircleBuffer* pCBuffer);
void CircleSetComplete(CPs_CircleBuffer* pCBuffer);
BOOL CircleIsComplete(CPs_CircleBuffer* pCBuffer);
////////////////////////////////////////////////////////////////////////////////
//
// C23 Enhanced CircleBuffer Creation
//
CPs_CircleBuffer* CP_CreateCircleBuffer(const unsigned int iBufferSize)
{
#if HAVE_C23_THREADING
	// Create aligned buffer structure for better cache performance
	CPs_CircleBuffer* pNewBuffer = (CPs_CircleBuffer*)_aligned_malloc(sizeof(CPs_CircleBuffer), 64);
	if (!pNewBuffer) return NULL;
	
	// Initialize function pointers (same interface for compatibility)
	pNewBuffer->Uninitialise = CircleBufferUninitialise;
	pNewBuffer->Write = CircleBufferWrite;
	pNewBuffer->Read = CircleBufferRead;
	pNewBuffer->Flush = CircleFlush;
	pNewBuffer->GetUsedSize = CircleGetUsedSpace;
	pNewBuffer->GetFreeSize = CircleGetFreeSpace;
	pNewBuffer->SetComplete = CircleSetComplete;
	pNewBuffer->IsComplete = CircleIsComplete;
	
	// Enhanced buffer allocation with SIMD alignment
	pNewBuffer->m_iBufferSize = iBufferSize;
	pNewBuffer->m_pBuffer = (BYTE*)_aligned_malloc(iBufferSize, 16);  // 16-byte aligned for SIMD
	if (!pNewBuffer->m_pBuffer) {
		free(pNewBuffer);
		return NULL;
	}
	
	// Initialize atomic variables
	atomic_init(&pNewBuffer->m_iReadCursor, 0);
	atomic_init(&pNewBuffer->m_iWriteCursor, 0);
	atomic_init(&pNewBuffer->m_bComplete, false);
	
	// Initialize performance counters
	atomic_init(&pNewBuffer->m_bytes_written, 0);
	atomic_init(&pNewBuffer->m_bytes_read, 0);
	atomic_init(&pNewBuffer->m_lock_contentions, 0);
	atomic_init(&pNewBuffer->m_wait_timeouts, 0);
	
	// Initialize C23 threading primitives
	if (mtx_init(&pNewBuffer->m_access_mutex, mtx_plain) != thrd_success) {
		free(pNewBuffer->m_pBuffer);
		free(pNewBuffer);
		return NULL;
	}
	
	if (cnd_init(&pNewBuffer->m_data_available) != thrd_success) {
		mtx_destroy(&pNewBuffer->m_access_mutex);
		free(pNewBuffer->m_pBuffer);
		free(pNewBuffer);
		return NULL;
	}
	
	if (cnd_init(&pNewBuffer->m_space_available) != thrd_success) {
		cnd_destroy(&pNewBuffer->m_data_available);
		mtx_destroy(&pNewBuffer->m_access_mutex);
		free(pNewBuffer->m_pBuffer);
		free(pNewBuffer);
		return NULL;
	}
	
	return pNewBuffer;
	
#else
	// Legacy implementation for older compilers
	CPs_CircleBuffer* pNewBuffer = (CPs_CircleBuffer*)malloc(sizeof(CPs_CircleBuffer));
	if (!pNewBuffer) return NULL;
	
	pNewBuffer->Uninitialise = CircleBufferUninitialise;
	pNewBuffer->Write = CircleBufferWrite;
	pNewBuffer->Read = CircleBufferRead;
	pNewBuffer->Flush = CircleFlush;
	pNewBuffer->GetUsedSize = CircleGetUsedSpace;
	pNewBuffer->GetFreeSize = CircleGetFreeSpace;
	pNewBuffer->SetComplete = CircleSetComplete;
	pNewBuffer->IsComplete = CircleIsComplete;
	
	pNewBuffer->m_iBufferSize = iBufferSize;
	pNewBuffer->m_pBuffer = (BYTE*)malloc(iBufferSize);
	pNewBuffer->m_iReadCursor = 0;
	pNewBuffer->m_iWriteCursor = 0;
	pNewBuffer->m_bComplete = FALSE;
	pNewBuffer->m_evtDataAvailable = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&pNewBuffer->m_csCircleBuffer);
	
	return pNewBuffer;
#endif
}

//
// C23 Enhanced Buffer Cleanup
//
void CircleBufferUninitialise(CPs_CircleBuffer* pCBuffer)
{
	CP_CHECKOBJECT(pCBuffer);
	
#if HAVE_C23_THREADING
	// Clean up C23 threading primitives
	cnd_destroy(&pCBuffer->m_space_available);
	cnd_destroy(&pCBuffer->m_data_available);
	mtx_destroy(&pCBuffer->m_access_mutex);
	
	// Free aligned memory
	_aligned_free(pCBuffer->m_pBuffer);
	_aligned_free(pCBuffer);
#else
	// Legacy cleanup
	DeleteCriticalSection(&pCBuffer->m_csCircleBuffer);
	CloseHandle(pCBuffer->m_evtDataAvailable);
	free(pCBuffer->m_pBuffer);
	free(pCBuffer);
#endif
}

//
//
//
// C23 Enhanced Write Function with Atomic Operations
//
void CircleBufferWrite(CPs_CircleBuffer* pCBuffer, const void* _pSourceBuffer, const unsigned int _iNumBytes)
{
	if (!pCBuffer || !_pSourceBuffer || _iNumBytes == 0) return;
	
	size_t iBytesToWrite = _iNumBytes;
	const BYTE* pReadCursor = (const BYTE*)_pSourceBuffer;
	
	CP_ASSERT(iBytesToWrite <= pCBuffer->GetFreeSize(pCBuffer));
	
#if HAVE_C23_THREADING
	// Check completion status atomically
	if (atomic_load(&pCBuffer->m_bComplete)) {
		return;  // Buffer marked as complete, no more writes allowed
	}
	
	// Lock for thread-safe write operation
	if (mtx_lock(&pCBuffer->m_access_mutex) != thrd_success) {
		atomic_fetch_add(&pCBuffer->m_lock_contentions, 1);
		CP_TRACE0("CircleBufferWrite: Failed to acquire mutex");
		// Signal that we attempted to write data even though we failed
		// This prevents potential deadlocks if a reader is waiting
		cnd_signal(&pCBuffer->m_data_available);
		return;
	}
#else
	CP_ASSERT(pCBuffer->m_bComplete == FALSE);
	EnterCriticalSection(&pCBuffer->m_csCircleBuffer);
#endif
#if HAVE_C23_THREADING
	// Get current cursor positions atomically for C23 version
	size_t write_cursor = atomic_load(&pCBuffer->m_iWriteCursor);
	size_t read_cursor = atomic_load(&pCBuffer->m_iReadCursor);
	
	// Write data in chunks, handling buffer wraparound
	if (write_cursor >= read_cursor) {
		// Determine how much data we can fit into the end part of the buffer
		size_t chunk_size = pCBuffer->m_iBufferSize - write_cursor;
		
		if (chunk_size > iBytesToWrite)
			chunk_size = iBytesToWrite;
			
		// Copy the data
		memcpy(pCBuffer->m_pBuffer + write_cursor, pReadCursor, chunk_size);
		pReadCursor += chunk_size;
		iBytesToWrite -= chunk_size;
		
		// Update write cursor atomically (wrapping if needed)
		write_cursor += chunk_size;
		if (write_cursor >= pCBuffer->m_iBufferSize)
			write_cursor -= pCBuffer->m_iBufferSize;
	}
	
	// Fill the start part of the buffer with any data that may be left
	if (iBytesToWrite > 0) {
		memcpy(pCBuffer->m_pBuffer + write_cursor, pReadCursor, iBytesToWrite);
		write_cursor += iBytesToWrite;
		CP_ASSERT(write_cursor < pCBuffer->m_iBufferSize);
	}
	
	// Update cursor atomically and notify
	atomic_store(&pCBuffer->m_iWriteCursor, write_cursor);
	atomic_fetch_add(&pCBuffer->m_bytes_written, _iNumBytes);
	
	// Signal that data is available
	cnd_signal(&pCBuffer->m_data_available);
	mtx_unlock(&pCBuffer->m_access_mutex);
	
#else
	// Legacy implementation
	// We *know* there is enough space in the buffer for this entire stream
	if (pCBuffer->m_iWriteCursor >= pCBuffer->m_iReadCursor) {
		unsigned int iChunkSize = pCBuffer->m_iBufferSize - pCBuffer->m_iWriteCursor;
		
		if (iChunkSize > iBytesToWrite)
			iChunkSize = iBytesToWrite;
			
		memcpy(pCBuffer->m_pBuffer + pCBuffer->m_iWriteCursor, pReadCursor, iChunkSize);
		pReadCursor += iChunkSize;
		iBytesToWrite -= iChunkSize;
		
		pCBuffer->m_iWriteCursor += iChunkSize;
		if (pCBuffer->m_iWriteCursor >= pCBuffer->m_iBufferSize)
			pCBuffer->m_iWriteCursor -= pCBuffer->m_iBufferSize;
	}
	
	if (iBytesToWrite) {
		memcpy(pCBuffer->m_pBuffer + pCBuffer->m_iWriteCursor, pReadCursor, iBytesToWrite);
		pCBuffer->m_iWriteCursor += iBytesToWrite;
		CP_ASSERT(pCBuffer->m_iWriteCursor < pCBuffer->m_iBufferSize);
	}
	
	SetEvent(pCBuffer->m_evtDataAvailable);
	LeaveCriticalSection(&pCBuffer->m_csCircleBuffer);
#endif
}

//
// C23 Enhanced Read Function with Timeout Support
//
BOOL CircleBufferRead(CPs_CircleBuffer* pCBuffer, void* pDestBuffer, const size_t _iBytesToRead, size_t* pbBytesRead)
{
	if (!pCBuffer || !pDestBuffer || !pbBytesRead || _iBytesToRead == 0) {
		if (pbBytesRead) *pbBytesRead = 0;
		return FALSE;
	}
	
	size_t iBytesToRead = _iBytesToRead;
	size_t iBytesRead = 0;
	BOOL bComplete = FALSE;
	
	CP_CHECKOBJECT(pCBuffer);
	
#if HAVE_C23_THREADING
	while (iBytesToRead > 0 && !bComplete) {
		// Lock the buffer for read operations
		if (mtx_lock(&pCBuffer->m_access_mutex) != thrd_success) {
			atomic_fetch_add(&pCBuffer->m_lock_contentions, 1);
			*pbBytesRead = iBytesRead;
			return FALSE;
		}
		
		// Check if there's data available or if buffer is complete
		size_t used_space = CircleGetUsedSpace(pCBuffer);
		bComplete = atomic_load(&pCBuffer->m_bComplete);
		
		if (used_space == 0 && !bComplete) {
			// No data available, wait with timeout
			struct timespec timeout_time;
			clock_gettime(CLOCK_REALTIME, &timeout_time);
			timeout_time.tv_sec += CIC_WAITTIMEOUT / 1000;
			timeout_time.tv_nsec += (CIC_WAITTIMEOUT % 1000) * 1000000;
			
			int wait_result = cnd_timedwait(&pCBuffer->m_data_available, 
			                               &pCBuffer->m_access_mutex, 
			                               &timeout_time);
			
			if (wait_result == thrd_timedout) {
				atomic_fetch_add(&pCBuffer->m_wait_timeouts, 1);
				CP_TRACE0("C23 Circle buffer - did not fill in time!");
				mtx_unlock(&pCBuffer->m_access_mutex);
				*pbBytesRead = iBytesRead;
				return FALSE;
			}
			
			// Re-check after wait
			used_space = CircleGetUsedSpace(pCBuffer);
			bComplete = atomic_load(&pCBuffer->m_bComplete);
		
		// Perform actual read operation (C23 version)
		if (used_space > 0) {
			size_t read_cursor = atomic_load(&pCBuffer->m_iReadCursor);
			size_t write_cursor = atomic_load(&pCBuffer->m_iWriteCursor);
			
			// Read data from buffer
			if (read_cursor > write_cursor) {
				// Data wraps around - read from read_cursor to end of buffer
				size_t chunk_size = pCBuffer->m_iBufferSize - read_cursor;
				if (chunk_size > iBytesToRead)
					chunk_size = iBytesToRead;
					
				memcpy((BYTE*)pDestBuffer + iBytesRead,
				       pCBuffer->m_pBuffer + read_cursor, chunk_size);
				       
				iBytesRead += chunk_size;
				iBytesToRead -= chunk_size;
				read_cursor += chunk_size;
				
				if (read_cursor >= pCBuffer->m_iBufferSize)
					read_cursor = 0;
			}
			
			// Read remaining data from start of buffer
			if (iBytesToRead > 0 && read_cursor < write_cursor) {
				size_t chunk_size = write_cursor - read_cursor;
				if (chunk_size > iBytesToRead)
					chunk_size = iBytesToRead;
					
				memcpy((BYTE*)pDestBuffer + iBytesRead,
				       pCBuffer->m_pBuffer + read_cursor, chunk_size);
				       
				iBytesRead += chunk_size;
				iBytesToRead -= chunk_size;
				read_cursor += chunk_size;
			}
			
			// Update read cursor atomically
			atomic_store(&pCBuffer->m_iReadCursor, read_cursor);
			atomic_fetch_add(&pCBuffer->m_bytes_read, iBytesRead);
		}
		
		mtx_unlock(&pCBuffer->m_access_mutex);
	}
#else
	while (iBytesToRead > 0 && bComplete == FALSE) {
		DWORD dwWaitResult = WaitForSingleObject(pCBuffer->m_evtDataAvailable, CIC_WAITTIMEOUT);
		
		if (dwWaitResult == WAIT_TIMEOUT) {
			CP_TRACE0("Circle buffer - did not fill in time!");
			*pbBytesRead = iBytesRead;
			return FALSE;
		}
		
		EnterCriticalSection(&pCBuffer->m_csCircleBuffer);
		
		// Take what we can from the CBuffer
		
		if (pCBuffer->m_iReadCursor > pCBuffer->m_iWriteCursor)
		{
			unsigned int iChunkSize = pCBuffer->m_iBufferSize - pCBuffer->m_iReadCursor;
			
			if (iChunkSize > iBytesToRead)
				iChunkSize = (unsigned int)iBytesToRead;
				
			// Perform the read
			memcpy((BYTE*)pDestBuffer + iBytesRead,
				   pCBuffer->m_pBuffer + pCBuffer->m_iReadCursor,
				   iChunkSize);
			       
			iBytesRead += iChunkSize;
			iBytesToRead -= iChunkSize;
			
			pCBuffer->m_iReadCursor += iChunkSize;
			
			if (pCBuffer->m_iReadCursor >= pCBuffer->m_iBufferSize)
				pCBuffer->m_iReadCursor -= pCBuffer->m_iBufferSize;
		}
		
		if (iBytesToRead && pCBuffer->m_iReadCursor < pCBuffer->m_iWriteCursor)
		{
			unsigned int iChunkSize = pCBuffer->m_iWriteCursor - pCBuffer->m_iReadCursor;
			
			if (iChunkSize > iBytesToRead)
				iChunkSize = (unsigned int)iBytesToRead;
				
			// Perform the read
			memcpy((BYTE*)pDestBuffer + iBytesRead,
				   pCBuffer->m_pBuffer + pCBuffer->m_iReadCursor,
				   iChunkSize);
			       
			iBytesRead += iChunkSize;
			iBytesToRead -= iChunkSize;
			pCBuffer->m_iReadCursor += iChunkSize;
		}
		
		// Is there any more data to read
		if (pCBuffer->m_iReadCursor == pCBuffer->m_iWriteCursor) {
			if (pCBuffer->m_bComplete)
				bComplete = TRUE;
		} else {
			SetEvent(pCBuffer->m_evtDataAvailable);
		}
		
		LeaveCriticalSection(&pCBuffer->m_csCircleBuffer);
	}
#endif

	*pbBytesRead = iBytesRead;
	return bComplete ? FALSE : TRUE;
}

//
// C23 Enhanced Flush Function
//
void CircleFlush(CPs_CircleBuffer* pCBuffer)
{
	CP_CHECKOBJECT(pCBuffer);
	
#if HAVE_C23_THREADING
	if (mtx_lock(&pCBuffer->m_access_mutex) == thrd_success) {
		atomic_store(&pCBuffer->m_iReadCursor, 0);
		atomic_store(&pCBuffer->m_iWriteCursor, 0);
		atomic_store(&pCBuffer->m_bComplete, false);
		
		// Reset performance counters
		atomic_store(&pCBuffer->m_bytes_written, 0);
		atomic_store(&pCBuffer->m_bytes_read, 0);
		
		// Signal that space is available
		cnd_signal(&pCBuffer->m_space_available);
		mtx_unlock(&pCBuffer->m_access_mutex);
	}
#else
	EnterCriticalSection(&pCBuffer->m_csCircleBuffer);
	pCBuffer->m_iReadCursor = 0;
	pCBuffer->m_iWriteCursor = 0;
	LeaveCriticalSection(&pCBuffer->m_csCircleBuffer);
#endif
}

//
// C23 Enhanced Free Space Calculation (Lock-Free When Possible)
//
unsigned int CircleGetFreeSpace(CPs_CircleBuffer* pCBuffer)
{
	CP_CHECKOBJECT(pCBuffer);
	
#if HAVE_C23_THREADING
	// Use atomic loads for lock-free operation in most cases
	size_t write_cursor = atomic_load(&pCBuffer->m_iWriteCursor);
	size_t read_cursor = atomic_load(&pCBuffer->m_iReadCursor);
	
	size_t iNumBytesFree;
	
	if (write_cursor < read_cursor) {
		iNumBytesFree = (read_cursor - 1) - write_cursor;
	} else if (write_cursor == read_cursor) {
		iNumBytesFree = pCBuffer->m_iBufferSize - 1;  // Reserve one byte to distinguish full from empty
	} else {
		iNumBytesFree = (read_cursor - 1) + (pCBuffer->m_iBufferSize - write_cursor);
	}
	
	return (unsigned int)iNumBytesFree;
#else
	unsigned int iNumBytesFree;
	
	EnterCriticalSection(&pCBuffer->m_csCircleBuffer);
	
	if (pCBuffer->m_iWriteCursor < pCBuffer->m_iReadCursor)
		iNumBytesFree = (pCBuffer->m_iReadCursor - 1) - pCBuffer->m_iWriteCursor;
	else if (pCBuffer->m_iWriteCursor == pCBuffer->m_iReadCursor)
		iNumBytesFree = pCBuffer->m_iBufferSize - 1;
	else
		iNumBytesFree = (pCBuffer->m_iReadCursor - 1) + (pCBuffer->m_iBufferSize - pCBuffer->m_iWriteCursor);
		
	LeaveCriticalSection(&pCBuffer->m_csCircleBuffer);
	
	return iNumBytesFree;
#endif
}

//
// C23 Enhanced Used Space Calculation
//
unsigned int CircleGetUsedSpace(CPs_CircleBuffer* pCBuffer)
{
	CP_CHECKOBJECT(pCBuffer);
	return (unsigned int)(pCBuffer->m_iBufferSize - 1) - CircleGetFreeSpace(pCBuffer);
}

//
// C23 Enhanced Set Complete Function
//
void CircleSetComplete(CPs_CircleBuffer* pCBuffer)
{
	CP_CHECKOBJECT(pCBuffer);
	
#if HAVE_C23_THREADING
	// Atomically mark buffer as complete
	atomic_store(&pCBuffer->m_bComplete, true);
	
	// Wake up all waiting threads
	if (mtx_lock(&pCBuffer->m_access_mutex) == thrd_success) {
		cnd_broadcast(&pCBuffer->m_data_available);
		mtx_unlock(&pCBuffer->m_access_mutex);
	}
#else
	EnterCriticalSection(&pCBuffer->m_csCircleBuffer);
	pCBuffer->m_bComplete = TRUE;
	SetEvent(pCBuffer->m_evtDataAvailable);
	LeaveCriticalSection(&pCBuffer->m_csCircleBuffer);
#endif
}

//
// C23 Enhanced Is Complete Check (Lock-Free)
//
BOOL CircleIsComplete(CPs_CircleBuffer* pCBuffer)
{
	CP_CHECKOBJECT(pCBuffer);
	
#if HAVE_C23_THREADING
	return atomic_load(&pCBuffer->m_bComplete) ? TRUE : FALSE;
#else
	return pCBuffer->m_bComplete;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Performance Monitoring Functions

#if HAVE_C23_THREADING
// Get performance statistics for monitoring and debugging
void CP_GetCircleBufferStats(CPs_CircleBuffer* pCBuffer, 
                            uint64_t* bytes_written, 
                            uint64_t* bytes_read,
                            uint32_t* lock_contentions,
                            uint32_t* wait_timeouts)
{
	if (!pCBuffer) return;
	
	if (bytes_written) *bytes_written = atomic_load(&pCBuffer->m_bytes_written);
	if (bytes_read) *bytes_read = atomic_load(&pCBuffer->m_bytes_read);
	if (lock_contentions) *lock_contentions = atomic_load(&pCBuffer->m_lock_contentions);
	if (wait_timeouts) *wait_timeouts = atomic_load(&pCBuffer->m_wait_timeouts);
}

// Reset performance counters
void CP_ResetCircleBufferStats(CPs_CircleBuffer* pCBuffer)
{
	if (!pCBuffer) return;
	
	atomic_store(&pCBuffer->m_bytes_written, 0);
	atomic_store(&pCBuffer->m_bytes_read, 0);
	atomic_store(&pCBuffer->m_lock_contentions, 0);
	atomic_store(&pCBuffer->m_wait_timeouts, 0);
}

// Check buffer health (returns percentage filled)
unsigned int CP_GetCircleBufferFillPercentage(CPs_CircleBuffer* pCBuffer)
{
	if (!pCBuffer) return 0;
	
	unsigned int used = CircleGetUsedSpace(pCBuffer);
	return (used * 100) / (unsigned int)(pCBuffer->m_iBufferSize - 1);
}
#endif

//

