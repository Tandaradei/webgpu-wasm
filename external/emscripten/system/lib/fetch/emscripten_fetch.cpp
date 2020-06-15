// Copyright 2016 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <bits/errno.h>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <emscripten/html5.h>
#include <emscripten/threading.h>
#include <math.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

// Uncomment the following and clear the cache with emcc --clear-cache to rebuild this file to
// enable internal debugging. #define FETCH_DEBUG

struct __emscripten_fetch_queue {
  emscripten_fetch_t** queuedOperations;
  int numQueuedItems;
  int queueSize;
};

static void fetch_free(emscripten_fetch_t* fetch);

extern "C" {
void emscripten_start_fetch(emscripten_fetch_t* fetch);
__emscripten_fetch_queue* _emscripten_get_fetch_work_queue();

__emscripten_fetch_queue* _emscripten_get_fetch_queue() {
  __emscripten_fetch_queue* queue = _emscripten_get_fetch_work_queue();
  if (!queue->queuedOperations) {
    queue->queueSize = 64;
    queue->numQueuedItems = 0;
    queue->queuedOperations =
      (emscripten_fetch_t**)malloc(sizeof(emscripten_fetch_t*) * queue->queueSize);
  }
  return queue;
}

int32_t _emscripten_fetch_get_response_headers_length(int32_t fetchID);
int32_t _emscripten_fetch_get_response_headers(int32_t fetchID, int32_t dst, int32_t dstSizeBytes);
void _emscripten_fetch_free(unsigned int);
}

void emscripten_proxy_fetch(emscripten_fetch_t* fetch) {
  // TODO: mutex lock
  __emscripten_fetch_queue* queue = _emscripten_get_fetch_queue();
  //	TODO handle case when queue->numQueuedItems >= queue->queueSize
  queue->queuedOperations[queue->numQueuedItems++] = fetch;
#ifdef FETCH_DEBUG
  EM_ASM(console.log('Queued fetch to fetch-worker to process. There are now ' + $0 +
                     ' operations in the queue.'),
    queue->numQueuedItems);
#endif
  // TODO: mutex unlock
}

void emscripten_fetch_attr_init(emscripten_fetch_attr_t* fetch_attr) {
  memset(fetch_attr, 0, sizeof(emscripten_fetch_attr_t));
}

static int globalFetchIdCounter = 1;
emscripten_fetch_t* emscripten_fetch(emscripten_fetch_attr_t* fetch_attr, const char* url) {
  if (!fetch_attr)
    return 0;
  if (!url)
    return 0;

  const bool synchronous = (fetch_attr->attributes & EMSCRIPTEN_FETCH_SYNCHRONOUS) != 0;
  const bool readFromIndexedDB =
    (fetch_attr->attributes & (EMSCRIPTEN_FETCH_APPEND | EMSCRIPTEN_FETCH_NO_DOWNLOAD)) != 0 ||
    ((fetch_attr->attributes & EMSCRIPTEN_FETCH_REPLACE) == 0);
  const bool writeToIndexedDB = (fetch_attr->attributes & EMSCRIPTEN_FETCH_PERSIST_FILE) != 0 ||
                                !strncmp(fetch_attr->requestMethod, "EM_IDB_", strlen("EM_IDB_"));
  const bool performXhr = (fetch_attr->attributes & EMSCRIPTEN_FETCH_NO_DOWNLOAD) == 0;
  const bool isMainBrowserThread = emscripten_is_main_browser_thread() != 0;
  if (isMainBrowserThread && synchronous && (performXhr || readFromIndexedDB || writeToIndexedDB)) {
#ifdef FETCH_DEBUG
    EM_ASM(
      err(
        'emscripten_fetch("' + UTF8ToString($0) +
        '") failed! Synchronous blocking XHRs and IndexedDB operations are not supported on the main browser thread. Try dropping the EMSCRIPTEN_FETCH_SYNCHRONOUS flag, or run with the linker flag --proxy-to-worker to decouple main C runtime thread from the main browser thread.'),
      url);
#endif
    return 0;
  }

  emscripten_fetch_t* fetch = (emscripten_fetch_t*)malloc(sizeof(emscripten_fetch_t));
  if (!fetch)
    return 0;
  memset(fetch, 0, sizeof(emscripten_fetch_t));
  fetch->id = globalFetchIdCounter++; // TODO: make this thread-safe!
  fetch->userData = fetch_attr->userData;
  fetch->__attributes.timeoutMSecs = fetch_attr->timeoutMSecs;
  fetch->__attributes.attributes = fetch_attr->attributes;
  fetch->__attributes.withCredentials = fetch_attr->withCredentials;
  fetch->__attributes.requestData = fetch_attr->requestData;
  fetch->__attributes.requestDataSize = fetch_attr->requestDataSize;
  strcpy(fetch->__attributes.requestMethod, fetch_attr->requestMethod);
  fetch->__attributes.onerror = fetch_attr->onerror;
  fetch->__attributes.onsuccess = fetch_attr->onsuccess;
  fetch->__attributes.onprogress = fetch_attr->onprogress;
  fetch->__attributes.onreadystatechange = fetch_attr->onreadystatechange;
#define STRDUP_OR_ABORT(s, str_to_dup)                                                             \
  if (str_to_dup) {                                                                                \
    s = strdup(str_to_dup);                                                                        \
    if (!s) {                                                                                      \
      fetch_free(fetch);                                                                           \
      return 0;                                                                                    \
    }                                                                                              \
  }
  STRDUP_OR_ABORT(fetch->url, url);
  STRDUP_OR_ABORT(fetch->__attributes.destinationPath, fetch_attr->destinationPath);
  STRDUP_OR_ABORT(fetch->__attributes.userName, fetch_attr->userName);
  STRDUP_OR_ABORT(fetch->__attributes.password, fetch_attr->password);
  STRDUP_OR_ABORT(fetch->__attributes.overriddenMimeType, fetch_attr->overriddenMimeType);
  if (fetch_attr->requestHeaders) {
    size_t headersCount = 0;
    while (fetch_attr->requestHeaders[headersCount])
      ++headersCount;
    const char** headers = (const char**)malloc((headersCount + 1) * sizeof(const char*));
    if (!headers) {
      fetch_free(fetch);
      return 0;
    }
    memset((void*)headers, 0, (headersCount + 1) * sizeof(const char*));

    for (size_t i = 0; i < headersCount; ++i) {
      headers[i] = strdup(fetch_attr->requestHeaders[i]);
      if (!headers[i])

      {
        for (size_t j = 0; j < i; ++j) {
          free((void*)headers[j]);
        }
        free((void*)headers);
        fetch_free(fetch);
        return 0;
      }
    }
    headers[headersCount] = 0;
    fetch->__attributes.requestHeaders = headers;
  }

#undef STRDUP_OR_ABORT

// In asm.js we can use a fetch worker, which is created from the main asm.js
// code. That lets us do sync operations by blocking on the worker etc.
// In the wasm backend we don't have a fetch worker implemented yet, however,
// we can still do basic synchronous fetches in the same places: if we can
// block on another thread then we aren't the main thread, and if we aren't
// the main thread then synchronous xhrs are legitimate.
#if __EMSCRIPTEN_PTHREADS__ && !defined(__wasm__)
  const bool waitable = (fetch_attr->attributes & EMSCRIPTEN_FETCH_WAITABLE) != 0;
  // Depending on the type of fetch, we can either perform it in the same Worker/thread than the
  // caller, or we might need to run it in a separate Worker. There is a dedicated fetch worker that
  // is available for the fetch, but in some scenarios it might be desirable to run in the same
  // Worker as the caller, so deduce here whether to run the fetch in this thread, or if we need to
  // use the fetch-worker instead.
  if (waitable // Waitable fetches can be synchronously waited on, so must always be proxied
      || (synchronous &&
           (readFromIndexedDB || writeToIndexedDB))) // Synchronous IndexedDB access needs proxying
  {
    emscripten_atomic_store_u32(&fetch->__proxyState, 1); // sent to proxy worker.
    emscripten_proxy_fetch(fetch);

    if (synchronous)
      emscripten_fetch_wait(fetch, INFINITY);
  } else
#endif
    emscripten_start_fetch(fetch);
  return fetch;
}

EMSCRIPTEN_RESULT emscripten_fetch_wait(emscripten_fetch_t* fetch, double timeoutMsecs) {
#if __EMSCRIPTEN_PTHREADS__
  if (!fetch)
    return EMSCRIPTEN_RESULT_INVALID_PARAM;
  uint32_t proxyState = emscripten_atomic_load_u32(&fetch->__proxyState);
  if (proxyState == 2)
    return EMSCRIPTEN_RESULT_SUCCESS; // already finished.
  if (proxyState != 1)
    return EMSCRIPTEN_RESULT_INVALID_PARAM; // the fetch should be ongoing?
#ifdef FETCH_DEBUG
  EM_ASM(console.log('fetch: emscripten_fetch_wait..'));
#endif
  if (timeoutMsecs <= 0)
    return EMSCRIPTEN_RESULT_TIMED_OUT;
  while (proxyState == 1 /*sent to proxy worker*/) {
    if (!emscripten_is_main_browser_thread()) {
      int ret = emscripten_futex_wait(&fetch->__proxyState, proxyState, timeoutMsecs);
      if (ret == -ETIMEDOUT)
        return EMSCRIPTEN_RESULT_TIMED_OUT;
      proxyState = emscripten_atomic_load_u32(&fetch->__proxyState);
    } else {
      EM_ASM({console.error(
        'fetch: emscripten_fetch_wait failed: main thread cannot block to wait for long periods of time! Migrate the application to run in a worker to perform synchronous file IO, or switch to using asynchronous IO.')});
      return EMSCRIPTEN_RESULT_FAILED;
    }
  }
#ifdef FETCH_DEBUG
  EM_ASM(console.log('fetch: emscripten_fetch_wait done..'));
#endif

  if (proxyState == 2)
    return EMSCRIPTEN_RESULT_SUCCESS;
  else
    return EMSCRIPTEN_RESULT_FAILED;
#else
  if (fetch->readyState >= 4 /*XMLHttpRequest.readyState.DONE*/)
    return EMSCRIPTEN_RESULT_SUCCESS; // already finished.
  if (timeoutMsecs == 0)
    return EMSCRIPTEN_RESULT_TIMED_OUT /*Main thread testing completion with sleep=0msecs*/;
  else {
#ifdef FETCH_DEBUG
    EM_ASM(console.error(
      'fetch: emscripten_fetch_wait() cannot stop to wait when building without pthreads!'));
#endif
    return EMSCRIPTEN_RESULT_FAILED /*Main thread cannot block to wait*/;
  }
#endif
}

EMSCRIPTEN_RESULT emscripten_fetch_close(emscripten_fetch_t* fetch) {
  if (!fetch)
    return EMSCRIPTEN_RESULT_SUCCESS; // Closing null pointer is ok, same as with free().

#if __EMSCRIPTEN_PTHREADS__
  emscripten_atomic_store_u32(&fetch->__proxyState, 0);
#endif
  // This function frees the fetch pointer so that it is invalid to access it anymore.
  // Use a few key fields as an integrity check that we are being passed a good pointer to a valid
  // fetch structure, which has not been yet closed. (double close is an error)
  if (fetch->id == 0 || fetch->readyState > 4)
    return EMSCRIPTEN_RESULT_INVALID_PARAM;

  // This fetch is aborted. Call the error handler if the fetch was still in progress and was
  // canceled in flight.
  if (fetch->readyState != 4 /*DONE*/ && fetch->__attributes.onerror) {
    fetch->status = (unsigned short)-1;
    strcpy(fetch->statusText, "aborted with emscripten_fetch_close()");
    fetch->__attributes.onerror(fetch);
  }

  fetch_free(fetch);
  return EMSCRIPTEN_RESULT_SUCCESS;
}

size_t emscripten_fetch_get_response_headers_length(emscripten_fetch_t *fetch) {
  if (!fetch || fetch->readyState < 2) return 0;

  return (size_t)_emscripten_fetch_get_response_headers_length((int32_t)fetch->id);
}

size_t emscripten_fetch_get_response_headers(emscripten_fetch_t *fetch, char *dst, size_t dstSizeBytes) {
  if (!fetch || fetch->readyState < 2) return 0;

  return (size_t)_emscripten_fetch_get_response_headers((int32_t)fetch->id, (int32_t)dst, (int32_t)dstSizeBytes);
}

char **emscripten_fetch_unpack_response_headers(const char *headersString) {
  // Get size of output array and allocate.
  size_t numHeaders = 0;
  for(const char *pos = strchr(headersString, '\n'); pos; pos = strchr(pos + 1, '\n'))
  {
    numHeaders++;
  }
  char **unpackedHeaders = (char**)malloc(sizeof(char*) * ((numHeaders * 2) + 1));
  unpackedHeaders[numHeaders * 2] = NULL;

  // Allocate each header.
  const char *rowStart = headersString;
  const char *rowEnd = strchr(rowStart, '\n');
  for(size_t headerNum = 0; rowEnd; headerNum += 2)
  {
    const char *split = strchr(rowStart, ':');
    size_t headerSize = (size_t)split - (size_t)rowStart;
    char* header = (char*)malloc(headerSize + 1);
    strncpy(header, rowStart, headerSize);
    header[headerSize] = '\0';

    size_t valueSize = (size_t)rowEnd - (size_t)split;
    char* value = (char*)malloc(valueSize + 1);
    strncpy(value, split + 1, valueSize);
    value[valueSize] = '\0';

    unpackedHeaders[headerNum] = header;
    unpackedHeaders[headerNum+1] = value;

    rowStart = rowEnd + 1;
    rowEnd = strchr(rowStart, '\n');
  }

  return unpackedHeaders;
}

void emscripten_fetch_free_unpacked_response_headers(char **unpackedHeaders) {
  if(unpackedHeaders)
  {
    for(size_t i = 0; unpackedHeaders[i]; ++i)
      free((void*)unpackedHeaders[i]);
    free((void*)unpackedHeaders);
  }
}

void emscripten_fetch_free(unsigned int id) {
  return _emscripten_fetch_free(id);
}

static void fetch_free(emscripten_fetch_t* fetch) {
  emscripten_fetch_free(fetch->id);
  fetch->id = 0;
  free((void*)fetch->data);
  free((void*)fetch->url);
  free((void*)fetch->__attributes.destinationPath);
  free((void*)fetch->__attributes.userName);
  free((void*)fetch->__attributes.password);
  if (fetch->__attributes.requestHeaders) {
    for (size_t i = 0; fetch->__attributes.requestHeaders[i]; ++i)
      free((void*)fetch->__attributes.requestHeaders[i]);
    free((void*)fetch->__attributes.requestHeaders);
  }
  free((void*)fetch->__attributes.overriddenMimeType);
  free(fetch);
}

} // extern "C"
