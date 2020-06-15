/**
 * @license
 * Copyright 2015 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */

var LibraryIDBStore = {
  // A simple IDB-backed storage mechanism. Suitable for saving and loading large files asynchronously. This does
  // *NOT* use the emscripten filesystem, intentionally, to avoid overhead. It lets you application define whatever
  // filesystem-like layer you want, with the overhead 100% controlled by you. At the extremes, you could either
  // just store large files, with almost no extra code; or you could implement a file b-tree using posix-compliant
  // filesystem on top.
  $IDBStore:
#include IDBStore.js
  ,

  emscripten_idb_async_load: function(db, id, arg, onload, onerror) {
    IDBStore.getFile(UTF8ToString(db), UTF8ToString(id), function(error, byteArray) {
      if (error) {
        if (onerror) {{{ makeDynCall('vi') }}}(onerror, arg);
        return;
      }
      var buffer = _malloc(byteArray.length);
      HEAPU8.set(byteArray, buffer);
      {{{ makeDynCall('viii') }}}(onload, arg, buffer, byteArray.length);
      _free(buffer);
    });
  },
  emscripten_idb_async_store: function(db, id, ptr, num, arg, onstore, onerror) {
    // note that we copy the data here, as these are async operatins - changes to HEAPU8 meanwhile should not affect us!
    IDBStore.setFile(UTF8ToString(db), UTF8ToString(id), new Uint8Array(HEAPU8.subarray(ptr, ptr+num)), function(error) {
      if (error) {
        if (onerror) {{{ makeDynCall('vi') }}}(onerror, arg);
        return;
      }
      if (onstore) {{{ makeDynCall('vi') }}}(onstore, arg);
    });
  },
  emscripten_idb_async_delete: function(db, id, arg, ondelete, onerror) {
    IDBStore.deleteFile(UTF8ToString(db), UTF8ToString(id), function(error) {
      if (error) {
        if (onerror) {{{ makeDynCall('vi') }}}(onerror, arg);
        return;
      }
      if (ondelete) {{{ makeDynCall('vi') }}}(ondelete, arg);
    });
  },
  emscripten_idb_async_exists: function(db, id, arg, oncheck, onerror) {
    IDBStore.existsFile(UTF8ToString(db), UTF8ToString(id), function(error, exists) {
      if (error) {
        if (onerror) {{{ makeDynCall('vi') }}}(onerror, arg);
        return;
      }
      if (oncheck) {{{ makeDynCall('vii') }}}(oncheck, arg, exists);
    });
  },

#if EMTERPRETIFY_ASYNC
  emscripten_idb_load__deps: ['$EmterpreterAsync'],
  emscripten_idb_load: function(db, id, pbuffer, pnum, perror) {
    EmterpreterAsync.handle(function(resume) {
      IDBStore.getFile(UTF8ToString(db), UTF8ToString(id), function(error, byteArray) {
        if (error) {
          {{{ makeSetValueAsm('perror', 0, '1', 'i32') }}};
          resume();
          return;
        }
        var buffer = _malloc(byteArray.length); // must be freed by the caller!
        HEAPU8.set(byteArray, buffer);
        {{{ makeSetValueAsm('pbuffer', 0, 'buffer', 'i32') }}};
        {{{ makeSetValueAsm('pnum',    0, 'byteArray.length', 'i32') }}};
        {{{ makeSetValueAsm('perror',  0, '0', 'i32') }}};
        resume();
      });
    });
  },
  emscripten_idb_store__deps: ['$EmterpreterAsync'],
  emscripten_idb_store: function(db, id, ptr, num, perror) {
    EmterpreterAsync.handle(function(resume) {
      IDBStore.setFile(UTF8ToString(db), UTF8ToString(id), new Uint8Array(HEAPU8.subarray(ptr, ptr+num)), function(error) {
        {{{ makeSetValueAsm('perror', 0, '!!error', 'i32') }}};
        resume();
      });
    });
  },
  emscripten_idb_delete__deps: ['$EmterpreterAsync'],
  emscripten_idb_delete: function(db, id, perror) {
    EmterpreterAsync.handle(function(resume) {
      IDBStore.deleteFile(UTF8ToString(db), UTF8ToString(id), function(error) {
        {{{ makeSetValueAsm('perror', 0, '!!error', 'i32') }}};
        resume();
      });
    });
  },
  emscripten_idb_exists__deps: ['$EmterpreterAsync'],
  emscripten_idb_exists: function(db, id, pexists, perror) {
    EmterpreterAsync.handle(function(resume) {
      IDBStore.existsFile(UTF8ToString(db), UTF8ToString(id), function(error, exists) {
        {{{ makeSetValueAsm('pexists', 0, '!!exists', 'i32') }}};
        {{{ makeSetValueAsm('perror',  0, '!!error', 'i32') }}};
        resume();
      });
    });
  },
  // extra worker methods - proxied
  emscripten_idb_load_blob__deps: ['$EmterpreterAsync'],
  emscripten_idb_load_blob: function(db, id, pblob, perror) {
    EmterpreterAsync.handle(function(resume) {
      assert(!IDBStore.pending);
      IDBStore.pending = function(msg) {
        IDBStore.pending = null;
        var blob = msg.blob;
        if (!blob) {
          {{{ makeSetValueAsm('perror', 0, '1', 'i32') }}};
          resume();
          return;
        }
        assert(blob instanceof Blob);
        var blobId = IDBStore.blobs.length;
        IDBStore.blobs.push(blob);
        {{{ makeSetValueAsm('pblob', 0, 'blobId', 'i32') }}};
        resume();
      };
      postMessage({
        target: 'IDBStore',
        method: 'loadBlob',
        db: UTF8ToString(db),
        id: UTF8ToString(id)
      });
    });
    /*
    EmterpreterAsync.handle(function(resume) {
      IDBStore.getFile(UTF8ToString(db), UTF8ToString(id), function(error, blob) {
        if (error) {
          {{{ makeSetValueAsm('perror', 0, '1', 'i32') }}};
          resume();
          return;
        }
        assert(blob instanceof Blob);
        var blobId = IDBStore.blobs.length;
        IDBStore.blobs.push(blob);
        {{{ makeSetValueAsm('pblob', 0, 'blobId', 'i32') }}};
        resume();
      });
    });
    */
  },
  emscripten_idb_store_blob__deps: ['$EmterpreterAsync'],
  emscripten_idb_store_blob: function(db, id, ptr, num, perror) {
    EmterpreterAsync.handle(function(resume) {
      assert(!IDBStore.pending);
      IDBStore.pending = function(msg) {
        IDBStore.pending = null;
        {{{ makeSetValueAsm('perror', 0, '!!msg.error', 'i32') }}};
        resume();
      };
      postMessage({
        target: 'IDBStore',
        method: 'storeBlob',
        db: UTF8ToString(db),
        id: UTF8ToString(id),
        blob: new Blob([new Uint8Array(HEAPU8.subarray(ptr, ptr+num))])
      });
    });
    /*
    EmterpreterAsync.handle(function(resume) {
      IDBStore.setFile(UTF8ToString(db), UTF8ToString(id), new Blob([new Uint8Array(HEAPU8.subarray(ptr, ptr+num))]), function(error) {
        {{{ makeSetValueAsm('perror', 0, '!!error', 'i32') }}};
        resume();
      });
    });
    */
  },
  emscripten_idb_read_from_blob__deps: ['$EmterpreterAsync'],
  emscripten_idb_read_from_blob: function(blobId, start, num, buffer) {
    var blob = IDBStore.blobs[blobId];
    if (!blob) return 1;
    if (start+num > blob.size) return 2;
    var byteArray = (new FileReaderSync()).readAsArrayBuffer(blob.slice(start, start+num));
    HEAPU8.set(new Uint8Array(byteArray), buffer);
    return 0;
  },
  emscripten_idb_free_blob__deps: ['$EmterpreterAsync'],
  emscripten_idb_free_blob: function(blobId) {
    assert(IDBStore.blobs[blobId]);
    IDBStore.blobs[blobId] = null;
  },
#else
#if WASM_BACKEND && ASYNCIFY
  emscripten_idb_load: function(db, id, pbuffer, pnum, perror) {
    Asyncify.handleSleep(function(wakeUp) {
      IDBStore.getFile(UTF8ToString(db), UTF8ToString(id), function(error, byteArray) {
        if (error) {
          {{{ makeSetValueAsm('perror', 0, '1', 'i32') }}};
          wakeUp();
          return;
        }
        var buffer = _malloc(byteArray.length); // must be freed by the caller!
        HEAPU8.set(byteArray, buffer);
        {{{ makeSetValueAsm('pbuffer', 0, 'buffer', 'i32') }}};
        {{{ makeSetValueAsm('pnum',    0, 'byteArray.length', 'i32') }}};
        {{{ makeSetValueAsm('perror',  0, '0', 'i32') }}};
        wakeUp();
      });
    });
  },
  emscripten_idb_store: function(db, id, ptr, num, perror) {
    Asyncify.handleSleep(function(wakeUp) {
      IDBStore.setFile(UTF8ToString(db), UTF8ToString(id), new Uint8Array(HEAPU8.subarray(ptr, ptr+num)), function(error) {
        {{{ makeSetValueAsm('perror', 0, '!!error', 'i32') }}};
        wakeUp();
      });
    });
  },
  emscripten_idb_delete: function(db, id, perror) {
    Asyncify.handleSleep(function(wakeUp) {
      IDBStore.deleteFile(UTF8ToString(db), UTF8ToString(id), function(error) {
        {{{ makeSetValueAsm('perror', 0, '!!error', 'i32') }}};
        wakeUp();
      });
    });
  },
  emscripten_idb_exists: function(db, id, pexists, perror) {
    Asyncify.handleSleep(function(wakeUp) {
      IDBStore.existsFile(UTF8ToString(db), UTF8ToString(id), function(error, exists) {
        {{{ makeSetValueAsm('pexists', 0, '!!exists', 'i32') }}};
        {{{ makeSetValueAsm('perror',  0, '!!error', 'i32') }}};
        wakeUp();
      });
    });
  },
  // extra worker methods - proxied
  emscripten_idb_load_blob: function(db, id, pblob, perror) {
    Asyncify.handleSleep(function(wakeUp) {
      assert(!IDBStore.pending);
      IDBStore.pending = function(msg) {
        IDBStore.pending = null;
        var blob = msg.blob;
        if (!blob) {
          {{{ makeSetValueAsm('perror', 0, '1', 'i32') }}};
          wakeUp();
          return;
        }
        assert(blob instanceof Blob);
        var blobId = IDBStore.blobs.length;
        IDBStore.blobs.push(blob);
        {{{ makeSetValueAsm('pblob', 0, 'blobId', 'i32') }}};
        wakeUp();
      };
      postMessage({
        target: 'IDBStore',
        method: 'loadBlob',
        db: UTF8ToString(db),
        id: UTF8ToString(id)
      });
    });
  },
  emscripten_idb_store_blob: function(db, id, ptr, num, perror) {
    Asyncify.handleSleep(function(wakeUp) {
      assert(!IDBStore.pending);
      IDBStore.pending = function(msg) {
        IDBStore.pending = null;
        {{{ makeSetValueAsm('perror', 0, '!!msg.error', 'i32') }}};
        wakeUp();
      };
      postMessage({
        target: 'IDBStore',
        method: 'storeBlob',
        db: UTF8ToString(db),
        id: UTF8ToString(id),
        blob: new Blob([new Uint8Array(HEAPU8.subarray(ptr, ptr+num))])
      });
    });
  },
  emscripten_idb_read_from_blob: function(blobId, start, num, buffer) {
    var blob = IDBStore.blobs[blobId];
    if (!blob) return 1;
    if (start+num > blob.size) return 2;
    var byteArray = (new FileReaderSync()).readAsArrayBuffer(blob.slice(start, start+num));
    HEAPU8.set(new Uint8Array(byteArray), buffer);
    return 0;
  },
  emscripten_idb_free_blob: function(blobId) {
    assert(IDBStore.blobs[blobId]);
    IDBStore.blobs[blobId] = null;
  },
#else
  emscripten_idb_load: function() {
    throw 'Please compile your program with async support in order to use synchronous operations like emscripten_idb_load, etc.';
  },
  emscripten_idb_store: function() {
    throw 'Please compile your program with async support in order to use synchronous operations like emscripten_idb_store, etc.';
  },
  emscripten_idb_delete: function() {
    throw 'Please compile your program with async support in order to use synchronous operations like emscripten_idb_delete, etc.';
  },
  emscripten_idb_exists: function() {
    throw 'Please compile your program with async support in order to use synchronous operations like emscripten_idb_exists, etc.';
  },
#endif // WASM_BACKEND && ASYNCIFY
#endif // EMTERPRETIFY_ASYNC
};

autoAddDeps(LibraryIDBStore, '$IDBStore');
mergeInto(LibraryManager.library, LibraryIDBStore);

