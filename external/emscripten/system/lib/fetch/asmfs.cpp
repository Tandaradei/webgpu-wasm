// Copyright 2016 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#define __NEED_struct_iovec
#include "syscall_arch.h"
#include <ctype.h>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <emscripten/threading.h>
#include <libc/fcntl.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <wasi/api.h>

// Uncomment the following and clear the cache with emcc --clear-cache to rebuild this file to
// enable internal debugging. #define ASMFS_DEBUG

extern "C" {

// http://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers
#define MAX_PATHNAME_LENGTH 2000

#define INODE_TYPE uint32_t
#define INODE_FILE 1
#define INODE_DIR 2

struct inode {
  char name[NAME_MAX + 1]; // NAME_MAX actual bytes + one byte for null termination.
  inode* parent;           // ID of the parent node
  inode* sibling; // ID of a sibling node (these form a singular linked list that specifies the
                  // content under a directory)
  inode* child;   // ID of the first child node in a chain of children (the root of a linked list of
                  // inodes)
  uint32_t uid;   // User ID of the owner
  uint32_t gid;   // Group ID of the owning group
  uint32_t mode;  // r/w/x modes
  time_t ctime;   // Time when the inode was last modified
  time_t mtime;   // Time when the content was last modified
  time_t atime;   // Time when the content was last accessed
  size_t size;    // Size of the file in bytes
  size_t capacity; // Amount of bytes allocated to pointer data
  uint8_t* data;   // The actual file contents.

  INODE_TYPE type;

  emscripten_fetch_t* fetch;

  // Specifies a remote server address where this inode can be located.
  char* remoteurl;
};

#define EM_FILEDESCRIPTOR_MAGIC 0x64666d65U // 'emfd'
struct FileDescriptor {
  uint32_t magic;
  ssize_t file_pos;
  uint32_t mode;
  uint32_t flags;

  inode* node;
};

static inode* create_inode(INODE_TYPE type, int mode) {
  inode* i = (inode*)malloc(sizeof(inode));
  memset(i, 0, sizeof(inode));
  i->ctime = i->mtime = i->atime = time(0);
  i->type = type;
  i->mode = mode;
  return i;
}

// The current working directory of the application process.
static inode* cwd_inode = 0;

static inode* filesystem_root() {
  static inode* root_node = create_inode(INODE_DIR, 0777);
  return root_node;
}

static inode* get_cwd() {
  if (!cwd_inode)
    cwd_inode = filesystem_root();
  return cwd_inode;
}

static void set_cwd(inode* node) { cwd_inode = node; }

static void inode_abspath(inode* node, char* dst, int dstLen) {
  if (!node) {
    assert(dstLen >= (int)strlen("(null)") + 1);
    strcpy(dst, "(null)");
    return;
  }
  if (node == filesystem_root()) {
    assert(dstLen >= (int)strlen("/") + 1);
    strcpy(dst, "/");
    return;
  }
#define MAX_DIRECTORY_DEPTH 512
  inode* stack[MAX_DIRECTORY_DEPTH];
  int depth = 0;
  while (node->parent && depth < MAX_DIRECTORY_DEPTH) {
    stack[depth++] = node;
    node = node->parent;
  }
  char* dstEnd = dst + dstLen;
  *dstEnd-- = '\0';
  while (depth > 0 && dst < dstEnd) {
    if (dst < dstEnd)
      *dst++ = '/';
    --depth;
    int len = strlen(stack[depth]->name);
    if (len > dstEnd - dst)
      len = dstEnd - dst;
    strncpy(dst, stack[depth]->name, len);
    dst += len;
  }
}

// Deletes the given inode. Ignores (orphans) any children there might be
static void delete_inode(inode* node) {
  if (!node)
    return;
  if (node == filesystem_root())
    return; // As special case, do not allow deleting the filesystem root directory
#ifdef ASMFS_DEBUG
  EM_ASM(err('delete_inode: ' + UTF8ToString($0)), node->name);
#endif
  if (node->fetch)
    emscripten_fetch_close(node->fetch);
  free(node->remoteurl);
  free(node);
}

// Deletes the given inode and its subtree
static void delete_inode_tree(inode* node) {
  if (!node)
    return;
#ifdef ASMFS_DEBUG
  EM_ASM(err('delete_inode_tree: ' + UTF8ToString($0)), node->name);
#endif
  inode* child = node->child;
  while (child) {
    inode* sibling = child->sibling;
    delete_inode_tree(child->child);
    delete_inode(child);
    child = sibling;
  }
  if (node !=
      filesystem_root()) // As special case, do not allow deleting the filesystem root directory
  {
    delete_inode(node);
  } else {
    // For filesystem root, just make sure all children are gone.
    node->child = 0;
  }
}

// Makes node the child of parent.
static void link_inode(inode* node, inode* parent) {
  char parentName[PATH_MAX];
  inode_abspath(parent, parentName, PATH_MAX);
#ifdef ASMFS_DEBUG
  EM_ASM(err('link_inode: node "' + UTF8ToString($0) + '" to parent "' + UTF8ToString($1) + '".'),
    node->name, parentName);
#endif
  // When linking a node, it can't be part of the filesystem tree (but it can have children of its
  // own)
  assert(!node->parent);
  assert(!node->sibling);

  // The inode pointed by 'node' is not yet part of the filesystem, so it's not shared memory and
  // only this thread is accessing it. Therefore setting the node's parent here is not yet racy, do
  // that operation first.
  node->parent = parent;

  // This node is to become the first child of the parent, and the old first child of the parent
  // should become the sibling of this node, i.e.
  //  1) node->sibling = parent->child;
  //  2) parent->child = node;
  // However these two operations need to occur atomically in order to be coherent. To ensure that,
  // run the two operations in a CAS loop, which is possible because the first operation is not racy
  // until the node is 'published' to the filesystem tree by the compare_exchange operation.
  do {
    __atomic_load(
      &parent->child, &node->sibling, __ATOMIC_SEQ_CST); // node->sibling <- parent->child
  } while (
    !__atomic_compare_exchange(&parent->child, &node->sibling, &node, false, __ATOMIC_SEQ_CST,
      __ATOMIC_SEQ_CST)); // parent->child <- node if it had not raced to change value in between
}

// Traverse back in sibling linked list, or 0 if no such node exist.
static inode* find_predecessor_sibling(inode* node, inode* parent) {
  inode* child = parent->child;
  if (child == node)
    return 0;
  while (child && child->sibling != node)
    child = child->sibling;
  if (!child->sibling)
    return 0;
  return child;
}

static void unlink_inode(inode* node) {
#ifdef ASMFS_DEBUG
  EM_ASM(
    err('unlink_inode: node ' + UTF8ToString($0) + ' from its parent ' + UTF8ToString($1) + '.'),
    node->name, node->parent->name);
#endif
  inode* parent = node->parent;
  if (!parent)
    return;
  node->parent = 0;

  if (parent->child == node) {
    parent->child = node->sibling;
  } else {
    inode* predecessor = find_predecessor_sibling(node, parent);
    if (predecessor)
      predecessor->sibling = node->sibling;
  }
  node->parent = node->sibling = 0;
}

// Compares two strings for equality until a '\0' or a '/' is hit. Returns 0 if the strings differ,
// or a pointer to the beginning of the next directory component name of s1 if the strings are
// equal.
static const char* path_cmp(const char* s1, const char* s2, bool* is_directory) {
  *is_directory = true;
  while (*s1 == *s2) {
    if (*s1 == '/')
      return s1 + 1;
    if (*s1 == '\0') {
      *is_directory = false;
      return s1;
    }
    ++s1;
    ++s2;
  }
  if (*s1 == '/' && *s2 == '\0')
    return s1 + 1;
  if (*s1 == '\0' && *s2 == '/')
    return s1;
  return 0;
}

#define NIBBLE_TO_CHAR(x) ("0123456789abcdef"[(x)])
static void uriEncode(char* dst, int dstLengthBytes, const char* src) {
  char* end =
    dst + dstLengthBytes - 4; // Use last 4 bytes of dst as a guard area to avoid overflow below.
  while (*src && dst < end) {
    if (isalnum(*src) || *src == '-' || *src == '_' || *src == '.' || *src == '~')
      *dst++ = *src;
    else if (*src == '/')
      *dst++ = *src; // NB. forward slashes should generally be uriencoded, but for file path
                     // purposes, we want to keep them intact.
    else
      *dst++ = '%', *dst++ = NIBBLE_TO_CHAR(*src >> 4),
      *dst++ = NIBBLE_TO_CHAR(*src & 15); // This charater needs uriencoding.
    ++src;
  }
  *dst = '\0';
}

// Copies string 'path' to 'dst', but stops on the first forward slash '/' character.
// Returns number of bytes written, excluding null terminator
static int strcpy_inodename(char* dst, const char* path) {
  char* d = dst;
  while (*path && *path != '/')
    *dst++ = *path++;
  *dst = '\0';
  return dst - d;
}

// Copies src to dst, writes at most maxBytesToWrite out. Always null terminates dst. Returns the
// number of characters written, excluding null terminator.
static int strcpy_safe(char* dst, const char* src, int maxBytesToWrite) {
  char* dst_start = dst;
  char* dst_end = dst + maxBytesToWrite - 1;
  while (dst < dst_end && *src)
    *dst++ = *src++;
  *dst = '\0';
  return dst - dst_start;
}

// Returns a pointer to the basename part of the string, i.e. the string after the last occurrence
// of a forward slash character
static const char* basename_part(const char* path) {
  const char* s = path;
  while (*path) {
    if (*path == '/')
      s = path + 1;
    ++path;
  }
  return s;
}

static inode* create_directory_hierarchy_for_file(
  inode* root, const char* path_to_file, unsigned int mode) {
  assert(root);
  if (!root)
    return 0;

  // Traverse . and ..
  while (path_to_file[0] == '.') {
    if (path_to_file[1] == '/')
      path_to_file += 2; // Skip over redundant "./././././" blocks
    else if (path_to_file[1] == '\0')
      path_to_file += 1;
    else if (path_to_file[1] == '.' &&
             (path_to_file[2] == '/' ||
               path_to_file[2] == '\0')) // Go up to parent directories with ".."
    {
      root = root->parent;
      if (!root)
        return 0;
      assert(
        root->type == INODE_DIR); // Anything that is a parent should automatically be a directory.
      path_to_file += (path_to_file[2] == '/') ? 3 : 2;
    } else
      break;
  }
  if (path_to_file[0] == '\0')
    return 0;

  inode* node = root->child;
  while (node) {
    bool is_directory = false;
    const char* child_path = path_cmp(path_to_file, node->name, &is_directory);
#ifdef ASMFS_DEBUG
    EM_ASM_INT({err('path_cmp ' + UTF8ToString($0) + ', ' + UTF8ToString($1) + ', ' +
                    UTF8ToString($2) + ' .')},
      path_to_file, node->name, child_path);
#endif
    if (child_path) {
      if (is_directory && node->type != INODE_DIR)
        return 0; // "A component used as a directory in pathname is not, in fact, a directory"

      // The directory name matches.
      path_to_file = child_path;

      // Traverse . and ..
      while (path_to_file[0] == '.') {
        if (path_to_file[1] == '/')
          path_to_file += 2; // Skip over redundant "./././././" blocks
        else if (path_to_file[1] == '\0')
          path_to_file += 1;
        else if (path_to_file[1] == '.' &&
                 (path_to_file[2] == '/' ||
                   path_to_file[2] == '\0')) // Go up to parent directories with ".."
        {
          node = node->parent;
          if (!node)
            return 0;
          assert(node->type ==
                 INODE_DIR); // Anything that is a parent should automatically be a directory.
          path_to_file += (path_to_file[2] == '/') ? 3 : 2;
        } else
          break;
      }
      if (path_to_file[0] == '\0')
        return node;
      if (path_to_file[0] == '/' && path_to_file[1] == '\0' /* && node is a directory*/)
        return node;
      root = node;
      node = node->child;
    } else {
      node = node->sibling;
    }
  }
  const char* basename_pos = basename_part(path_to_file);
#ifdef ASMFS_DEBUG
  EM_ASM(err('path_to_file ' + UTF8ToString($0) + ' .'), path_to_file);
  EM_ASM(err('basename_pos ' + UTF8ToString($0) + ' .'), basename_pos);
#endif
  while (*path_to_file && path_to_file < basename_pos) {
    node = create_inode(INODE_DIR, mode);
    path_to_file += strcpy_inodename(node->name, path_to_file) + 1;
    link_inode(node, root);
#ifdef ASMFS_DEBUG
    EM_ASM(out('create_directory_hierarchy_for_file: created directory ' + UTF8ToString($0) +
               ' under parent ' + UTF8ToString($1) + '.'),
      node->name, node->parent->name);
#endif
    root = node;
  }
  return root;
}

#define RETURN_NODE_AND_ERRNO(node, errno)                                                         \
  do {                                                                                             \
    *out_errno = (errno);                                                                          \
    return (node);                                                                                 \
  } while (0)

// Given a pathname to a file/directory, finds the inode of the directory that would contain the
// file/directory, or 0 if the intermediate path doesn't exist. Note that the file/directory pointed
// to by path does not need to exist, only its parent does.
static inode* find_parent_inode(inode* root, const char* path, int* out_errno) {
  char rootName[PATH_MAX];
  inode_abspath(root, rootName, PATH_MAX);
#ifdef ASMFS_DEBUG
  EM_ASM(err('find_parent_inode(root="' + UTF8ToString($0) + '", path="' + UTF8ToString($1) + '")'),
    rootName, path);
#endif

  assert(out_errno); // Passing in error is mandatory.

  if (!root)
    RETURN_NODE_AND_ERRNO(0, ENOENT);
  if (!path)
    RETURN_NODE_AND_ERRNO(0, ENOENT);

  // Traverse . and ..
  while (path[0] == '.') {
    if (path[1] == '/')
      path += 2; // Skip over redundant "./././././" blocks
    else if (path[1] == '\0')
      path += 1;
    else if (path[1] == '.' &&
             (path[2] == '/' || path[2] == '\0')) // Go up to parent directories with ".."
    {
      root = root->parent;
      if (!root)
        RETURN_NODE_AND_ERRNO(0, ENOENT);
      assert(
        root->type == INODE_DIR); // Anything that is a parent should automatically be a directory.
      path += (path[2] == '/') ? 3 : 2;
    } else
      break;
  }
  if (path[0] == '\0')
    RETURN_NODE_AND_ERRNO(0, ENOENT);
  if (path[0] == '/' && path[1] == '\0')
    RETURN_NODE_AND_ERRNO(0, ENOENT);
  if (root->type != INODE_DIR)
    RETURN_NODE_AND_ERRNO(
      0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"

  // TODO: RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in translating pathname");
  // TODO: RETURN_ERRNO(EACCES, "one of the directories in the path prefix of pathname did not allow
  // search permission");

  const char* basename = basename_part(path);
  if (path == basename)
    RETURN_NODE_AND_ERRNO(root, 0);
  inode* node = root->child;
  while (node) {
    bool is_directory = false;
    const char* child_path = path_cmp(path, node->name, &is_directory);
    if (child_path) {
      if (is_directory && node->type != INODE_DIR)
        RETURN_NODE_AND_ERRNO(
          0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"

      // The directory name matches.
      path = child_path;

      // Traverse . and ..
      while (path[0] == '.') {
        if (path[1] == '/')
          path += 2; // Skip over redundant "./././././" blocks
        else if (path[1] == '\0')
          path += 1;
        else if (path[1] == '.' &&
                 (path[2] == '/' || path[2] == '\0')) // Go up to parent directories with ".."
        {
          node = node->parent;
          if (!node)
            RETURN_NODE_AND_ERRNO(0, ENOENT);
          assert(node->type ==
                 INODE_DIR); // Anything that is a parent should automatically be a directory.
          path += (path[2] == '/') ? 3 : 2;
        } else
          break;
      }

      if (path >= basename)
        RETURN_NODE_AND_ERRNO(node, 0);
      if (!*path)
        RETURN_NODE_AND_ERRNO(0, ENOENT);
      node = node->child;
      if (node->type != INODE_DIR)
        RETURN_NODE_AND_ERRNO(
          0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"
    } else {
      node = node->sibling;
    }
  }
  RETURN_NODE_AND_ERRNO(
    0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"
}

// Given a root inode of the filesystem and a path relative to it, e.g.
// "some/directory/dir_or_file", returns the inode that corresponds to "dir_or_file", or 0 if it
// doesn't exist. If the parameter out_closest_parent is specified, the closest (grand)parent node
// will be returned.
static inode* find_inode(inode* root, const char* path, int* out_errno) {
  char rootName[PATH_MAX];
  inode_abspath(root, rootName, PATH_MAX);
#ifdef ASMFS_DEBUG
  EM_ASM(err('find_inode(root="' + UTF8ToString($0) + '", path="' + UTF8ToString($1) + '")'),
    rootName, path);
#endif

  assert(out_errno); // Passing in error is mandatory.

  if (!root)
    RETURN_NODE_AND_ERRNO(0, ENOENT);

  // TODO: RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in translating pathname");
  // TODO: RETURN_ERRNO(EACCES, "one of the directories in the path prefix of pathname did not allow
  // search permission");

  // special-case finding empty string path "", "." or "/" returns the root searched in.
  if (root->type != INODE_DIR)
    RETURN_NODE_AND_ERRNO(
      0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"
  if (!path)
    RETURN_NODE_AND_ERRNO(root, 0);

  // Traverse . and ..
  while (path[0] == '.') {
    if (path[1] == '/')
      path += 2; // Skip over redundant "./././././" blocks
    else if (path[1] == '\0')
      path += 1;
    else if (path[1] == '.' &&
             (path[2] == '/' || path[2] == '\0')) // Go up to parent directories with ".."
    {
      root = root->parent;
      if (!root)
        RETURN_NODE_AND_ERRNO(0, ENOENT);
      assert(
        root->type == INODE_DIR); // Anything that is a parent should automatically be a directory.
      path += (path[2] == '/') ? 3 : 2;
    } else
      break;
  }
  if (path[0] == '\0')
    RETURN_NODE_AND_ERRNO(root, 0);

  inode* node = root->child;
  while (node) {
    bool is_directory = false;
    const char* child_path = path_cmp(path, node->name, &is_directory);
    if (child_path) {
      if (is_directory && node->type != INODE_DIR)
        RETURN_NODE_AND_ERRNO(
          0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"

      // The directory name matches.
      path = child_path;

      // Traverse . and ..
      while (path[0] == '.') {
        if (path[1] == '/')
          path += 2; // Skip over redundant "./././././" blocks
        else if (path[1] == '\0')
          path += 1;
        else if (path[1] == '.' &&
                 (path[2] == '/' || path[2] == '\0')) // Go up to parent directories with ".."
        {
          node = node->parent;
          if (!node)
            RETURN_NODE_AND_ERRNO(0, ENOENT);
          assert(node->type ==
                 INODE_DIR); // Anything that is a parent should automatically be a directory.
          path += (path[2] == '/') ? 3 : 2;
        } else
          break;
      }

      // If we arrived to the end of the search, this is the node we were looking for.
      if (path[0] == '\0')
        RETURN_NODE_AND_ERRNO(node, 0);
      if (path[0] == '/' && node->type != INODE_DIR)
        RETURN_NODE_AND_ERRNO(
          0, ENOTDIR); // "A component used as a directory in pathname is not, in fact, a directory"
      if (path[0] == '/' && path[1] == '\0')
        RETURN_NODE_AND_ERRNO(node, 0);
      node = node->child;
    } else {
      node = node->sibling;
    }
  }
  RETURN_NODE_AND_ERRNO(0, ENOENT);
}

// Same as above, but the root node is deduced from 'path'. (either absolute if path starts with
// "/", or relative)
static inode* find_inode(const char* path, int* out_errno) {
  inode* root;
  if (path[0] == '/')
    root = filesystem_root(), ++path;
  else
    root = get_cwd();
  return find_inode(root, path, out_errno);
}

void emscripten_asmfs_set_remote_url(const char* filename, const char* remoteUrl) {
  int err;
  inode* node = find_inode(filename, &err);
  if (!node)
    return;
  free(node->remoteurl);
  node->remoteurl = strdup(remoteUrl);
}

void emscripten_asmfs_set_file_data(const char* filename, char* data, size_t size) {
  int err;
  inode* node = find_inode(filename, &err);
  if (!node) {
    free(data);
    return;
  }
  free(node->data);
  node->data = (uint8_t*)data;
  node->size = node->capacity = size;
}

char* find_last_occurrence(char* str, char ch) {
  char* o = 0;
  while (*str) {
    if (*str == ch)
      o = str;
    ++str;
  }
  return o;
}

// Given a filename outputs the remote URL address that file can be located in.
void emscripten_asmfs_remote_url(const char* filename, char* outRemoteUrl, int maxBytesToWrite) {
  if (maxBytesToWrite <= 0 || !outRemoteUrl)
    return;
  *outRemoteUrl = '\0';
  if (maxBytesToWrite == 1)
    return;

  char trailing_path[PATH_MAX + 1] = {};
  char full_path[PATH_MAX + 1] = {};
  char full_path_temp[PATH_MAX + 1] = {};
  strcpy(full_path, filename);

  int err;
  inode* node = find_inode(full_path, &err);
  while (!node) {
    char* s = find_last_occurrence(full_path, '/');
    if (!s) {
      node = filesystem_root();
      strcpy(full_path_temp, trailing_path);
      strcpy(trailing_path, full_path);
      if (full_path_temp[0] != '\0') {
        strcat(trailing_path, "/");
        strcat(trailing_path, full_path_temp);
      }
      break;
    }
    *s = '\0';
    node = find_inode(full_path, &err);

    strcpy(full_path_temp, trailing_path);
    strcpy(trailing_path, filename + (s - full_path));
    if (full_path_temp[0] != '\0') {
      strcat(trailing_path, "/");
      strcat(trailing_path, full_path_temp);
    }
  }

  char uriEncodedPathName[3 * PATH_MAX + 4];
  full_path[0] = full_path[PATH_MAX] = full_path_temp[0] = full_path_temp[PATH_MAX] = '\0';

  while (node) {
    if (node->remoteurl && node->remoteurl[0] != '\0') {
      int nWritten = strcpy_safe(outRemoteUrl, node->remoteurl, maxBytesToWrite);
      if (maxBytesToWrite - nWritten > 1 && outRemoteUrl[nWritten - 1] != '/' &&
          full_path[0] != '/') {
        outRemoteUrl[nWritten++] = '/';
        outRemoteUrl[nWritten] = '\0';
      }
      strcat(full_path + strlen(full_path), trailing_path);
      uriEncode(uriEncodedPathName, 3 * PATH_MAX + 4, full_path);
      strcpy_safe(outRemoteUrl + nWritten,
        (outRemoteUrl[nWritten - 1] == '/' && uriEncodedPathName[0] == '/')
          ? (uriEncodedPathName + 1)
          : uriEncodedPathName,
        maxBytesToWrite - nWritten);
      return;
    }

    strcpy_safe(full_path_temp, full_path, PATH_MAX);
    int nWritten = strcpy_safe(full_path, node->name, PATH_MAX);
    if (full_path_temp[0] != '\0') {
      full_path[nWritten++] = '/';
      full_path[nWritten] = '\0';
      strcpy_safe(full_path + nWritten, full_path_temp, PATH_MAX - nWritten);
    }

    node = node->parent;
  }
  strcat(full_path + strlen(full_path), trailing_path);
  uriEncode(uriEncodedPathName, 3 * PATH_MAX + 4, full_path);
  strcpy_safe(outRemoteUrl, uriEncodedPathName, maxBytesToWrite);
}

// Debug function that dumps out the filesystem tree to console.
void emscripten_dump_fs_tree(inode* root, char* path) {
  char str[256];
  sprintf(str, "%s:", path);
  EM_ASM(out(UTF8ToString($0)), str);

  // Print out:
  // file mode | number of links | owner name | group name | file size in bytes | file last modified
  // time | path name which aligns with "ls -AFTRl" on console
  inode* child = root->child;
  uint64_t totalSize = 0;
  while (child) {
    sprintf(str, "%c%c%c%c%c%c%c%c%c%c  %d user%u group%u %lu Jan 1 1970 %s%c",
      child->type == INODE_DIR ? 'd' : '-', (child->mode & S_IRUSR) ? 'r' : '-',
      (child->mode & S_IWUSR) ? 'w' : '-', (child->mode & S_IXUSR) ? 'x' : '-',
      (child->mode & S_IRGRP) ? 'r' : '-', (child->mode & S_IWGRP) ? 'w' : '-',
      (child->mode & S_IXGRP) ? 'x' : '-', (child->mode & S_IROTH) ? 'r' : '-',
      (child->mode & S_IWOTH) ? 'w' : '-', (child->mode & S_IXOTH) ? 'x' : '-',
      1, // number of links to this file
      child->uid, child->gid,
      child->size ? child->size : (child->fetch ? (int)child->fetch->numBytes : 0), child->name,
      child->type == INODE_DIR ? '/' : ' ');
    EM_ASM(out(UTF8ToString($0)), str);

    totalSize += child->size;
    child = child->sibling;
  }

  sprintf(str, "total %llu bytes\n", totalSize);
  EM_ASM(out(UTF8ToString($0)), str);

  child = root->child;
  char* path_end = path + strlen(path);
  while (child) {
    if (child->type == INODE_DIR) {
      strcpy(path_end, child->name);
      strcat(path_end, "/");
      emscripten_dump_fs_tree(child, path);
    }
    child = child->sibling;
  }
}

void emscripten_asmfs_dump() {
  EM_ASM({err('emscripten_asmfs_dump()')});
  char path[PATH_MAX] = "/";
  emscripten_dump_fs_tree(filesystem_root(), path);
}

void emscripten_asmfs_discard_tree(const char* path) {
#ifdef ASMFS_DEBUG
  emscripten_asmfs_dump();
  EM_ASM(err('emscripten_asmfs_discard_tree: ' + UTF8ToString($0)), path);
#endif
  int err;
  inode* node = find_inode(path, &err);
  if (node && !err) {
    unlink_inode(node);
    delete_inode_tree(node);
  }
#ifdef ASMFS_DEBUG
  else
    EM_ASM(err('emscripten_asmfs_discard_tree failed, error ' + $0), err);
  emscripten_asmfs_dump();
#endif
}

#ifdef ASMFS_DEBUG
#define RETURN_ERRNO(errno, error_reason)                                                          \
  do {                                                                                             \
    EM_ASM(err(UTF8ToString($0) + '() returned errno ' + #errno + '(' + $1 + '): ' +               \
               error_reason + '!'),                                                                \
      __FUNCTION__, errno);                                                                        \
    return -errno;                                                                                 \
  } while (0)
#else
#define RETURN_ERRNO(errno, error_reason)                                                          \
  do {                                                                                             \
    return -(errno);                                                                               \
  } while (0)
#endif

static char stdout_buffer[4096] = {};
static int stdout_buffer_end = 0;
static char stderr_buffer[4096] = {};
static int stderr_buffer_end = 0;

static void print_stream(void* bytes, int numBytes, bool stdout) {
  char* buffer = stdout ? stdout_buffer : stderr_buffer;
  int& buffer_end = stdout ? stdout_buffer_end : stderr_buffer_end;

  memcpy(buffer + buffer_end, bytes, numBytes);
  buffer_end += numBytes;
  int new_buffer_start = 0;
  for (int i = 0; i < buffer_end; ++i) {
    if (buffer[i] == '\n') {
      buffer[i] = 0;
      EM_ASM_INT({out(UTF8ToString($0))}, buffer + new_buffer_start);
      new_buffer_start = i + 1;
    }
  }
  size_t new_buffer_size = buffer_end - new_buffer_start;
  memmove(buffer, buffer + new_buffer_start, new_buffer_size);
  buffer_end = new_buffer_size;
}

// TODO: Make thread-local storage.
static emscripten_asmfs_open_t __emscripten_asmfs_file_open_behavior_mode =
  EMSCRIPTEN_ASMFS_OPEN_REMOTE_DISCOVER;

void emscripten_asmfs_set_file_open_behavior(emscripten_asmfs_open_t behavior) {
  __emscripten_asmfs_file_open_behavior_mode = behavior;
}

emscripten_asmfs_open_t emscripten_asmfs_get_file_open_behavior() {
  return __emscripten_asmfs_file_open_behavior_mode;
}

// Returns true if the given file can be synchronously read by the main browser thread.
static bool emscripten_asmfs_file_is_synchronously_accessible(inode* node) {
  return node->data // If file was created from memory without XHR, e.g. via fopen("foo.txt", "w"),
                    // it will have node->data ptr backing.
         ||
         (node->fetch && node->fetch->data); // If the file was downloaded, it will be backed here.
}

static long open(const char* pathname, int flags, int mode) {
#ifdef ASMFS_DEBUG
  EM_ASM(err('open(pathname="' + UTF8ToString($0) + '", flags=0x' + ($1).toString(16) + ', mode=0' +
             ($2).toString(8) + ')'),
    pathname, flags, mode);
#endif

  int accessMode = (flags & O_ACCMODE);

  if ((flags & O_ASYNC))
    RETURN_ERRNO(ENOTSUP, "TODO: Opening files with O_ASYNC flag is not supported in ASMFS");
  if ((flags & O_DIRECT))
    RETURN_ERRNO(ENOTSUP, "TODO: O_DIRECT flag is not supported in ASMFS");
  if ((flags & O_DSYNC))
    RETURN_ERRNO(ENOTSUP, "TODO: O_DSYNC flag is not supported in ASMFS");

  // Spec says that the result of O_EXCL without O_CREAT is undefined.
  // We could enforce it as an error condition, as follows:
  //	if ((flags & O_EXCL) && !(flags & O_CREAT)) RETURN_ERRNO(EINVAL, "open() with O_EXCL flag
  //needs to always be paired with O_CREAT");
  // However existing earlier unit tests in Emscripten expect that O_EXCL is simply ignored when
  // O_CREAT was not passed. So do that for now.
  if ((flags & O_EXCL) && !(flags & O_CREAT)) {
#ifdef ASMFS_DEBUG
    EM_ASM(err('warning: open(pathname="' + UTF8ToString($0) + '", flags=0x' + ($1).toString(16) +
               ', mode=0' + ($2).toString(8) +
               ': flag O_EXCL should always be paired with O_CREAT. Ignoring O_EXCL)'),
      pathname, flags, mode);
#endif
    flags &= ~O_EXCL;
  }

  if ((flags & (O_NONBLOCK | O_NDELAY)))
    RETURN_ERRNO(
      ENOTSUP, "TODO: Opening files with O_NONBLOCK or O_NDELAY flags is not supported in ASMFS");
  if ((flags & O_PATH))
    RETURN_ERRNO(ENOTSUP, "TODO: Opening files with O_PATH flag is not supported in ASMFS");
  if ((flags & O_SYNC))
    RETURN_ERRNO(ENOTSUP, "TODO: Opening files with O_SYNC flag is not supported in ASMFS");

  // The flags:O_CLOEXEC flag is ignored, doesn't have meaning for Emscripten

  // TODO: the flags:O_DIRECT flag seems like a great way to let applications explicitly control
  // XHR/IndexedDB read/write buffering behavior?

  // The flags:O_LARGEFILE flag is ignored, we should always be largefile-compatible

  // TODO: The flags:O_NOATIME is ignored, file access times have not been implemented yet
  // The flags O_NOCTTY, O_NOFOLLOW

  if ((flags & O_TMPFILE)) {
    if (accessMode != O_WRONLY && accessMode != O_RDWR)
      RETURN_ERRNO(
        EINVAL, "O_TMPFILE was specified in flags, but neither O_WRONLY nor O_RDWR was specified");
    else
      RETURN_ERRNO(
        EOPNOTSUPP, "TODO: The filesystem containing pathname does not support O_TMPFILE");
  }

  // TODO: if (too_many_files_open) RETURN_ERRNO(EMFILE, "The per-process limit on the number of
  // open file descriptors has been reached, see getrlimit(RLIMIT_NOFILE)");

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  // Find if this file exists already in the filesystem?
  inode* root = (pathname[0] == '/') ? filesystem_root() : get_cwd();
  const char* relpath = (pathname[0] == '/') ? pathname + 1 : pathname;

  int err;
  inode* node = find_inode(root, relpath, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES,
      "Search permission is denied for one of the directories in the path prefix of pathname");
  if (err && err != ENOENT)
    RETURN_ERRNO(err, "find_inode() error");
  if (node) {
    if ((flags & O_DIRECTORY) && node->type != INODE_DIR)
      RETURN_ERRNO(ENOTDIR, "O_DIRECTORY was specified and pathname was not a directory");
    if (!(node->mode & 0444))
      RETURN_ERRNO(EACCES, "The requested access to the file is not allowed");
    if ((flags & O_CREAT) && (flags & O_EXCL))
      RETURN_ERRNO(EEXIST, "pathname already exists and O_CREAT and O_EXCL were used");
    if (node->type == INODE_DIR && accessMode != O_RDONLY)
      RETURN_ERRNO(EISDIR, "pathname refers to a directory and the access requested involved writing (that is, O_WRONLY or O_RDWR is set)");
    if (node->type == INODE_DIR && (flags & O_TRUNC))
      RETURN_ERRNO(EISDIR,
        "pathname refers to a directory and the access flags specified invalid flag O_TRUNC");

    // A current download exists to the file? Then wait for it to complete.
    if (node->fetch) {
      // On the main thread, the fetch must have already completed before we come here. If not, we
      // cannot stop to wait for it to finish, and must return a failure (file not found)
      if (emscripten_is_main_browser_thread()) {
        if (emscripten_fetch_wait(node->fetch, 0) != EMSCRIPTEN_RESULT_SUCCESS) {
          RETURN_ERRNO(ENOENT, "Attempted to open a file that is still downloading on the main browser thread. Could not block to wait! (try preloading the file to the filesystem before application start)");
        }
      } else {
        // On worker threads, we can pause to wait for the fetch.
        emscripten_fetch_wait(node->fetch, INFINITY);
      }
    }
  }

  if ((flags & O_CREAT) && ((flags & O_TRUNC) || (flags & O_EXCL))) {
    // Create a new empty file or truncate existing one.
    if (node) {
      if (node->fetch)
        emscripten_fetch_close(node->fetch);
      node->fetch = 0;
      node->size = 0;
    } else if ((flags & O_CREAT)) {
      inode* directory = create_directory_hierarchy_for_file(root, relpath, mode);
      node = create_inode((flags & O_DIRECTORY) ? INODE_DIR : INODE_FILE, mode);
      strcpy(node->name, basename_part(pathname));
      link_inode(node, directory);
    }
  } else if (!node || (node->type == INODE_FILE && !node->fetch && !node->data)) {
    emscripten_fetch_t* fetch = 0;
    if (!(flags & O_DIRECTORY) && accessMode != O_WRONLY) // Opening a file for reading?
    {
      // If there's no inode entry, check if we're not even interested in downloading the file?
      if (!node &&
          __emscripten_asmfs_file_open_behavior_mode != EMSCRIPTEN_ASMFS_OPEN_REMOTE_DISCOVER) {
        RETURN_ERRNO(
          ENOENT, "O_CREAT is not set, the named file does not exist in local filesystem and EMSCRIPTEN_ASMFS_OPEN_REMOTE_DISCOVER is not specified");
      }

      // Report an error if there is an inode entry, but file data is not synchronously available
      // and it should have been.
      if (node && !node->data &&
          __emscripten_asmfs_file_open_behavior_mode == EMSCRIPTEN_ASMFS_OPEN_MEMORY) {
        RETURN_ERRNO(
          ENOENT, "O_CREAT is not set, the named file exists, but file data is not synchronously available in memory (EMSCRIPTEN_ASMFS_OPEN_MEMORY specified)");
      }

      if (emscripten_is_main_browser_thread() &&
          (!node || !emscripten_asmfs_file_is_synchronously_accessible(node))) {
        RETURN_ERRNO(ENOENT,
          "O_CREAT is not set, the named file exists, but file data is not synchronously available in memory, and file open is attempted on the main thread which cannot synchronously open files! (try preloading the file to the filesystem before application start)");
      }

      // Kick off the file download, either from IndexedDB or via an XHR.
      emscripten_fetch_attr_t attr;
      emscripten_fetch_attr_init(&attr);
      strcpy(attr.requestMethod, "GET");
      attr.attributes = EMSCRIPTEN_FETCH_APPEND | EMSCRIPTEN_FETCH_LOAD_TO_MEMORY |
                        EMSCRIPTEN_FETCH_WAITABLE | EMSCRIPTEN_FETCH_PERSIST_FILE;
      // If asked to only do a read from IndexedDB, don't perform an XHR.
      if (__emscripten_asmfs_file_open_behavior_mode == EMSCRIPTEN_ASMFS_OPEN_INDEXEDDB) {
        attr.attributes |= EMSCRIPTEN_FETCH_NO_DOWNLOAD;
      }
      char
        path[3 * PATH_MAX + 4]; // times 3 because uri-encoding can expand the filename at most 3x.
      emscripten_asmfs_remote_url(pathname, path, 3 * PATH_MAX + 4);
      fetch = emscripten_fetch(&attr, path);

      // Synchronously wait for the fetch to complete.
      // NOTE: Theoretically could postpone blocking until the first read to the file, but the issue
      // there is that we wouldn't be able to return ENOENT below if the file did not exist on the
      // server, which could be harmful for some applications. Also fread()/fseek() very often
      // immediately follows fopen(), so the win would not be too great anyways.
      emscripten_fetch_wait(fetch, INFINITY);

      if (!(flags & O_CREAT) && (fetch->status != 200 || fetch->totalBytes == 0)) {
        emscripten_fetch_close(fetch);
        RETURN_ERRNO(ENOENT, "O_CREAT is not set and the named file does not exist (attempted emscripten_fetch() XHR to download)");
      }
    }

    if (node) {
      // If we had an existing inode entry, just associate the entry with the newly fetched data.
      if (node->type == INODE_FILE)
        node->fetch = fetch;
    } else if ((flags &
                 O_CREAT) // If the filesystem entry did not exist, but we have a create flag, ...
               || (!node && fetch)) // ... or if it did not exist in our fs, but it could be found
                                    // via fetch(), ...
    {
      // ... add it as a new entry to the fs.
      inode* directory = create_directory_hierarchy_for_file(root, relpath, mode);
      node = create_inode((flags & O_DIRECTORY) ? INODE_DIR : INODE_FILE, mode);
      strcpy(node->name, basename_part(pathname));
      node->fetch = fetch;
      link_inode(node, directory);
    } else {
      if (fetch)
        emscripten_fetch_close(fetch);
      RETURN_ERRNO(ENOENT, "O_CREAT is not set and the named file does not exist");
    }
    node->size = fetch ? node->fetch->totalBytes : 0;
  }

  FileDescriptor* desc = (FileDescriptor*)malloc(sizeof(FileDescriptor));
  desc->magic = EM_FILEDESCRIPTOR_MAGIC;
  desc->node = node;
  desc->file_pos = ((flags & O_APPEND) && node->fetch) ? node->fetch->totalBytes : 0;
  desc->mode = mode;
  desc->flags = flags;

  // TODO: The file descriptor needs to be a small number, man page:
  // "a small, nonnegative integer for use in subsequent system calls
  // (read(2), write(2), lseek(2), fcntl(2), etc.).  The file descriptor
  // returned by a successful call will be the lowest-numbered file
  // descriptor not currently open for the process."
  return (long)desc;
}

long __syscall5(long path, long flags, ...) // open
{
  va_list vl;
  va_start(vl, flags);
  int mode = va_arg(vl, int);
  va_end(vl);

  return open((const char *)path, flags, mode);
}

static long close(int fd) {
#ifdef ASMFS_DEBUG
  EM_ASM(err('close(fd=' + $0 + ')'), fd);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");

  if (desc->node && desc->node->fetch) {
    if (!emscripten_is_main_browser_thread()) {
      // TODO: This should not be necessary, but do it for now for consistency (test this out)
      emscripten_fetch_wait(desc->node->fetch, INFINITY);
    }

    // TODO: What to do to a XHRed/IndexedDB-backed unmodified file in memory when closing the file?
    //       free() or keep in memory?
    //       If user intends to reopen the file later (possibly often?), it is faster to keep the
    //       file in memory. (but it will consume more memory) If running on the main thread, the
    //       file cannot be loaded back synchronously if we let go of it, so things would break if
    //       the file is attempted to be loaded up again afterwards. Currently use a heuristic that
    //       if a file is closed on the main browser thread, do not free its backing storage. This
    //       can work for many
    //		 cases, but some kind of custom API might be best to add in the future? (e.g.
    //emscripten_fclose_and_retain() vs emscripten_fclose_and_free()?)
    if (!emscripten_is_main_browser_thread()) {
      emscripten_fetch_close(desc->node->fetch);
      desc->node->fetch = 0;
    }
  }
  desc->magic = 0;
  free(desc);
  return 0;
}

void emscripten_asmfs_populate(const char* pathname, int mode) {
  emscripten_asmfs_open_t prevBehavior = emscripten_asmfs_get_file_open_behavior();
  emscripten_asmfs_set_file_open_behavior(EMSCRIPTEN_ASMFS_OPEN_MEMORY);
  int fd = open(pathname, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, mode);
  if (fd > 0) {
    close(fd);
  }
  emscripten_asmfs_set_file_open_behavior(prevBehavior);
}

EMSCRIPTEN_RESULT emscripten_asmfs_preload_file(
  const char* url, const char* pathname, int mode, emscripten_fetch_attr_t* options) {
  if (!options) {
#ifdef ASMFS_DEBUG
    EM_ASM(err('emscripten_asmfs_preload_file: options not specified!'));
#endif
    return EMSCRIPTEN_RESULT_INVALID_PARAM;
  }

  if (!pathname) {
#ifdef ASMFS_DEBUG
    EM_ASM(err('emscripten_asmfs_preload_file: pathname not specified!'));
#endif
    return EMSCRIPTEN_RESULT_INVALID_PARAM;
  }

  // Find if this file exists already in the filesystem?
  inode* root = (pathname[0] == '/') ? filesystem_root() : get_cwd();
  const char* relpath = (pathname[0] == '/') ? pathname + 1 : pathname;

  int err;
  inode* node = find_inode(root, relpath, &err);
  // Filesystem traversal error?
  if (err && err != ENOENT) {
#ifdef ASMFS_DEBUG
    EM_ASM(err('emscripten_asmfs_preload_file: find_inode error ' + $0 + '!'), err);
#endif
    return EMSCRIPTEN_RESULT_INVALID_TARGET;
  }

  if (node && emscripten_asmfs_file_is_synchronously_accessible(node)) {
    // The file already exists, and its contents have already been preloaded - immediately fire the
    // success callback
    if (options->onsuccess)
      options->onsuccess(0);
    return EMSCRIPTEN_RESULT_SUCCESS;
  }

  // Kick off the file download, either from IndexedDB or via an XHR.
  emscripten_fetch_attr_t attr;
  memcpy(&attr, options, sizeof(emscripten_fetch_attr_t));
  if (strlen(attr.requestMethod) == 0)
    strcpy(attr.requestMethod, "GET");
  // In order for the file data to be synchronously accessible to the main browser thread, must load
  // it directly to memory.
  attr.attributes |= EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  // The following attributes cannot be present for preloading.
#ifdef ASMFS_DEBUG
  if ((attr.attributes & (EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_STREAM_DATA |
                           EMSCRIPTEN_FETCH_WAITABLE)) != 0)
    EM_ASM(err(
      'emscripten_asmfs_preload_file: cannot specify EMSCRIPTEN_FETCH_SYNCHRONOUS, EMSCRIPTEN_FETCH_STREAM_DATA or EMSCRIPTEN_FETCH_WAITABLE flags when preloading!'));
#endif
  attr.attributes &=
    ~(EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_STREAM_DATA | EMSCRIPTEN_FETCH_WAITABLE);
  // Default to EMSCRIPTEN_FETCH_APPEND if not specified.
  if (!(attr.attributes & (EMSCRIPTEN_FETCH_APPEND | EMSCRIPTEN_FETCH_REPLACE)))
    attr.attributes |= EMSCRIPTEN_FETCH_APPEND;

  emscripten_fetch_t* fetch;
  if (url)
    fetch = emscripten_fetch(&attr, url);
  else {
    char remoteUrl[3 * PATH_MAX +
                   4]; // times 3 because uri-encoding can expand the filename at most 3x.
    emscripten_asmfs_remote_url(pathname, remoteUrl, 3 * PATH_MAX + 4);
    fetch = emscripten_fetch(&attr, remoteUrl);
  }

  if (!node) {
    inode* directory = create_directory_hierarchy_for_file(root, relpath, mode);
    node = create_inode(INODE_FILE, mode);
    strcpy(node->name, basename_part(pathname));
    link_inode(node, directory);
  }
  node->fetch = fetch;

  return EMSCRIPTEN_RESULT_SUCCESS;
}

__wasi_errno_t __wasi_fd_close(__wasi_fd_t fd)
{
  return close(fd);
}

long __syscall9(long oldpath, long newpath) // link
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('link(oldpath="' + UTF8ToString($0) + '", newpath="' + UTF8ToString($1) + '")'),
    oldpath, newpath);
#endif
  ((void)oldpath);
  ((void)newpath);

  RETURN_ERRNO(ENOTSUP, "TODO: link() is a stub and not yet implemented in ASMFS");
}

long __syscall10(long path) // unlink
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('unlink(pathname="' + UTF8ToString($0) + '")'), pathname);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  int err;
  inode* node = find_inode(pathname, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in translating pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES,
      "One of the directories in the path prefix of pathname did not allow search permission");
  if (err == ENOENT)
    RETURN_ERRNO(ENOENT, "A component in pathname does not exist or is a dangling symbolic link");
  if (err)
    RETURN_ERRNO(err, "find_inode() error");

  if (!node)
    RETURN_ERRNO(ENOENT, "file does not exist");

  inode* parent = node->parent;

  if (parent && !(parent->mode & 0222))
    RETURN_ERRNO(EACCES, "Write access to the directory containing pathname is not allowed for the process's effective UID");

  // TODO: RETURN_ERRNO(EPERM, "The directory containing pathname has the sticky bit (S_ISVTX) set
  // and the process's effective user ID is neither the user ID of the file to be deleted nor that
  // of the directory containing it, and the process is not privileged");
  // TODO: RETURN_ERRNO(EROFS, "pathname refers to a file on a read-only filesystem");

  if (!(node->mode & 0222)) {
    if (node->type == INODE_DIR)
      RETURN_ERRNO(
        EISDIR, "directory deletion not permitted"); // Linux quirk: Return EISDIR error for not
                                                     // having permission to delete a directory.
    else
      RETURN_ERRNO(EPERM, "file deletion not permitted"); // but return EPERM error for no
                                                          // permission to delete a file.
  }

  if (node->child)
    RETURN_ERRNO(EISDIR, "directory is not empty"); // Linux quirk: Return EISDIR error if not being
                                                    // able to delete a nonempty directory.

  unlink_inode(node);      // Detach this from parent
  delete_inode_tree(node); // And delete the whole subtree

  return 0;
}

long __syscall12(long path) // chdir
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('chdir(pathname="' + UTF8ToString($0) + '")'), pathname);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  int err;
  inode* node = find_inode(pathname, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving path");
  if (err == EACCES)
    RETURN_ERRNO(EACCES, "Search permission is denied for one of the components of path");
  if (err == ENOENT)
    RETURN_ERRNO(
      ENOENT, "Directory component in pathname does not exist or is a dangling symbolic link");
  if (err)
    RETURN_ERRNO(err, "find_inode() error");
  if (!node)
    RETURN_ERRNO(ENOENT, "The directory specified in path does not exist");
  if (node->type != INODE_DIR)
    RETURN_ERRNO(ENOTDIR, "Path is not a directory");

  set_cwd(node);
  return 0;
}

long __syscall14(long path, long mode, long dev) // mknod
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('mknod(pathname="' + UTF8ToString($0) + '", mode=0' + ($1).toString(8) + ', dev=' +
             $2 + ')'),
    pathname, mode, dev);
#endif
  (void)pathname;
  (void)mode;
  (void)dev;

  RETURN_ERRNO(ENOTSUP, "TODO: mknod() is a stub and not yet implemented in ASMFS");
}

long __syscall15(long path, long mode) // chmod
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('chmod(pathname="' + UTF8ToString($0) + '", mode=0' + ($1).toString(8) + ')'),
    pathname, mode);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  int err;
  inode* node = find_inode(pathname, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES, "Search permission is denied on a component of the path prefix");
  if (err == ENOENT)
    RETURN_ERRNO(
      ENOENT, "Directory component in pathname does not exist or is a dangling symbolic link");
  if (err)
    RETURN_ERRNO(err, "find_inode() error");
  if (!node)
    RETURN_ERRNO(ENOENT, "The file does not exist");

  // TODO: if (not allowed) RETURN_ERRNO(EPERM, "The effective UID does not match the owner of the
  // file");
  // TODO: read-only filesystems: if (fs is read-only) RETURN_ERRNO(EROFS, "The named file resides
  // on a read-only filesystem");

  node->mode = mode;
  return 0;
}

long __syscall33(long path, long mode) // access
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('access(pathname="' + UTF8ToString($0) + '", mode=0' + ($1).toString(8) + ')'),
    pathname, mode);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  if ((mode & F_OK) && (mode & (R_OK | W_OK | X_OK)))
    RETURN_ERRNO(EINVAL, "mode was incorrectly specified");

  int err;
  inode* node = find_inode(pathname, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES,
      "Search permission is denied for one of the directories in the path prefix of pathname");
  if (err == ENOENT)
    RETURN_ERRNO(ENOENT, "A component of pathname does not exist or is a dangling symbolic link");
  if (err)
    RETURN_ERRNO(err, "find_inode() error");
  if (!node)
    RETURN_ERRNO(ENOENT, "Pathname does not exist");

  // Just testing if a file exists?
  if ((mode & F_OK))
    return 0;

  // TODO: RETURN_ERRNO(EROFS, "Write permission was requested for a file on a read-only
  // filesystem");

  if ((mode & R_OK) && !(node->mode & 0444))
    RETURN_ERRNO(EACCES, "Read access would be denied to the file");
  if ((mode & W_OK) && !(node->mode & 0222))
    RETURN_ERRNO(EACCES, "Write access would be denied to the file");
  if ((mode & X_OK) && !(node->mode & 0111))
    RETURN_ERRNO(EACCES, "Execute access would be denied to the file");

  return 0;
}

long __syscall36() // sync
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('sync()'));
#endif

  // Spec mandates that "sync() is always successful".
  return 0;
}

// TODO: syscall38,  int rename(const char *oldpath, const char *newpath);

long emscripten_asmfs_mkdir(const char* pathname, mode_t mode) {
#ifdef ASMFS_DEBUG
  EM_ASM(err('mkdir(pathname="' + UTF8ToString($0) + '", mode=0' + ($1).toString(8) + ')'),
    pathname, mode);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  inode* root = (pathname[0] == '/') ? filesystem_root() : get_cwd();
  const char* relpath = (pathname[0] == '/') ? pathname + 1 : pathname;
  int err;
  inode* parent_dir = find_parent_inode(root, relpath, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES, "One of the directories in pathname did not allow search permission");
  if (err)
    RETURN_ERRNO(err, "find_inode() error");
  if (!parent_dir)
    RETURN_ERRNO(
      ENOENT, "A directory component in pathname does not exist or is a dangling symbolic link");

  // TODO: if (component of path wasn't actually a directory) RETURN_ERRNO(ENOTDIR, "A component
  // used as a directory in pathname is not, in fact, a directory");

  inode* existing = find_inode(parent_dir, basename_part(pathname), &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES, "One of the directories in pathname did not allow search permission");
  if (err && err != ENOENT)
    RETURN_ERRNO(err, "find_inode() error");
  if (existing)
    RETURN_ERRNO(EEXIST, "pathname already exists (not necessarily as a directory)");
  if (!(parent_dir->mode & 0222))
    RETURN_ERRNO(EACCES, "The parent directory does not allow write permission to the process");

  // TODO: read-only filesystems: if (fs is read-only) RETURN_ERRNO(EROFS, "Pathname refers to a
  // file on a read-only filesystem");

  inode* directory = create_inode(INODE_DIR, mode);
  strcpy(directory->name, basename_part(pathname));
  link_inode(directory, parent_dir);
  return 0;
}

void emscripten_asmfs_unload_data(const char* pathname) {
  int err;
  inode* node = find_inode(pathname, &err);
  if (!node)
    return;

  free(node->data);
  node->data = 0;
  node->size = node->capacity = 0;
}

uint64_t emscripten_asmfs_compute_memory_usage_at_node(inode* node) {
  if (!node)
    return 0;
  uint64_t sz = sizeof(inode);
  if (node->data)
    sz += node->capacity > node->size ? node->capacity : node->size;
  if (node->fetch && node->fetch->data)
    sz += node->fetch->numBytes;
  return sz + emscripten_asmfs_compute_memory_usage_at_node(node->child) +
         emscripten_asmfs_compute_memory_usage_at_node(node->sibling);
}

uint64_t emscripten_asmfs_compute_memory_usage() {
  return emscripten_asmfs_compute_memory_usage_at_node(filesystem_root());
}

long __syscall39(long path, long mode) // mkdir
{
  return emscripten_asmfs_mkdir((const char *)path, mode);
}

long __syscall40(long path) // rmdir
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('rmdir(pathname="' + UTF8ToString($0) + '")'), pathname);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  if (!strcmp(pathname, ".") || (len >= 2 && !strcmp(pathname + len - 2, "/.")))
    RETURN_ERRNO(EINVAL, "pathname has . as last component");
  if (!strcmp(pathname, "..") || (len >= 3 && !strcmp(pathname + len - 3, "/..")))
    RETURN_ERRNO(ENOTEMPTY, "pathname has .. as its final component");

  int err;
  inode* node = find_inode(pathname, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(
      ENOTDIR, "A component used as a directory in pathname is not, in fact, a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links were encountered in resolving pathname");
  if (err == EACCES)
    RETURN_ERRNO(EACCES,
      "one of the directories in the path prefix of pathname did not allow search permission");
  if (err == ENOENT)
    RETURN_ERRNO(
      ENOENT, "A directory component in pathname does not exist or is a dangling symbolic link");
  if (err)
    RETURN_ERRNO(err, "find_inode() error");
  if (!node)
    RETURN_ERRNO(ENOENT, "directory does not exist");
  if (node == filesystem_root() || node == get_cwd())
    RETURN_ERRNO(EBUSY, "pathname is currently in use by the system or some process that prevents its removal (pathname is currently used as a mount point or is the root directory of the calling process)");
  if (node->parent && !(node->parent->mode & 0222))
    RETURN_ERRNO(EACCES, "Write access to the directory containing pathname was not allowed");
  if (node->type != INODE_DIR)
    RETURN_ERRNO(ENOTDIR, "pathname is not a directory");
  if (node->child)
    RETURN_ERRNO(ENOTEMPTY, "pathname contains entries other than . and ..");

  // TODO: RETURN_ERRNO(EPERM, "The directory containing pathname has the sticky bit (S_ISVTX) set
  // and the process's effective user ID is neither the user ID of the file to be deleted nor that
  // of the directory containing it, and the process is not privileged");
  // TODO: RETURN_ERRNO(EROFS, "pathname refers to a directory on a read-only filesystem");

  unlink_inode(node);

  return 0;
}

long __syscall41(long fd) // dup
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('dup(fd=' + $0 + ')'), fd);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");

  inode* node = desc->node;
  if (!node)
    RETURN_ERRNO(-1, "ASMFS internal error: file descriptor points to a nonexisting file");

  // TODO: RETURN_ERRNO(EMFILE, "The per-process limit on the number of open file descriptors has
  // been reached (see RLIMIT_NOFILE)");

  RETURN_ERRNO(ENOTSUP, "TODO: dup() is a stub and not yet implemented in ASMFS");
}

// TODO: syscall42: int pipe(int pipefd[2]);

long __syscall54(long fd, long request, ...) // ioctl/sysctl
{
  va_list vl;
  va_start(vl, request);
  char* argp = va_arg(vl, char*);
  va_end(vl);
#ifdef ASMFS_DEBUG
  EM_ASM(err('ioctl(fd=' + $0 + ', request=' + $1 + ', argp=0x' + $2 + ')'), fd, request, argp);
#endif
  (void)fd;
  (void)request;
  (void)argp;

  RETURN_ERRNO(ENOTSUP, "TODO: ioctl() is a stub and not yet implemented in ASMFS");
}

// TODO: syscall60: mode_t umask(mode_t mask);
// TODO: syscall63: dup2
// TODO: syscall83: symlink
// TODO: syscall85: readlink
// TODO: syscall91: munmap
// TODO: syscall94: fchmod
// TODO: syscall102: socketcall

__wasi_errno_t __wasi_fd_sync(__wasi_fd_t fd)
{
  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");

  inode* node = desc->node;
  if (!node)
    RETURN_ERRNO(-1, "ASMFS internal error: file descriptor points to a non-file");

  return 0;
}

// TODO: syscall133: fchdir

__wasi_errno_t __wasi_fd_seek(__wasi_fd_t fd, __wasi_filedelta_t offset,
    __wasi_whence_t whence, __wasi_filesize_t *newoffset)
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('llseek(fd=' + $0 + ', offset=' + $1 + ', newoffset=0x' +
             ($2).toString(16) + ', whence=' + $3 + ')'),
    fd, offset, newoffset, whence);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");

  if (desc->node->fetch) {
    if (emscripten_is_main_browser_thread()) {
      if (emscripten_fetch_wait(desc->node->fetch, 0) != EMSCRIPTEN_RESULT_SUCCESS) {
        RETURN_ERRNO(ENOENT, "Attempted to seek a file that is still downloading on the main browser thread. Could not block to wait! (try preloading the file to the filesystem before application start)");
      }
    } else
      emscripten_fetch_wait(desc->node->fetch, INFINITY);
  }

  int64_t newPos;
  switch (whence) {
    case SEEK_SET:
      newPos = offset;
      break;
    case SEEK_CUR:
      newPos = desc->file_pos + offset;
      break;
    case SEEK_END:
      newPos = (desc->node->fetch ? desc->node->fetch->numBytes : desc->node->size) + offset;
      break;
    case 3 /*SEEK_DATA*/:
      RETURN_ERRNO(EINVAL, "whence is invalid (sparse files, whence=SEEK_DATA, is not supported");
    case 4 /*SEEK_HOLE*/:
      RETURN_ERRNO(EINVAL, "whence is invalid (sparse files, whence=SEEK_HOLE, is not supported");
    default:
      RETURN_ERRNO(EINVAL, "whence is invalid");
  }
  if (newPos < 0)
    RETURN_ERRNO(EINVAL, "The resulting file offset would be negative");
  if (newPos > 0x7FFFFFFFLL)
    RETURN_ERRNO(EOVERFLOW, "The resulting file offset cannot be represented in an off_t");

  desc->file_pos = newPos;

  if (newoffset)
    *newoffset = desc->file_pos;
  return 0;
}

// TODO: syscall144 msync

static long readv(int fd, const iovec *iov, int iovcnt) // syscall145
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('readv(fd=' + $0 + ', iov=0x' + ($1).toString(16) + ', iovcnt=' + $2 + ')'), fd, iov,
    iovcnt);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");

  inode* node = desc->node;
  if (!node)
    RETURN_ERRNO(-1, "ASMFS internal error: file descriptor points to a non-file");
  if (node->type == INODE_DIR)
    RETURN_ERRNO(EISDIR, "fd refers to a directory");
  if (node->type != INODE_FILE /* TODO: && node->type != socket */)
    RETURN_ERRNO(EINVAL, "fd is attached to an object which is unsuitable for reading");

  // TODO: if (node->type == INODE_FILE && desc has O_NONBLOCK && read would block)
  // RETURN_ERRNO(EAGAIN, "The file descriptor fd refers to a file other than a socket and has been
  // marked nonblocking (O_NONBLOCK), and the read would block");
  // TODO: if (node->type == socket && desc has O_NONBLOCK && read would block)
  // RETURN_ERRNO(EWOULDBLOCK, "The file descriptor fd refers to a socket and has been marked
  // nonblocking (O_NONBLOCK), and the read would block");

  if (node->fetch) {
    if (emscripten_is_main_browser_thread()) {
      if (emscripten_fetch_wait(node->fetch, 0) != EMSCRIPTEN_RESULT_SUCCESS) {
        RETURN_ERRNO(ENOENT, "Attempted to read a file that is still downloading on the main browser thread. Could not block to wait! (try preloading the file to the filesystem before application start)");
      }
    } else
      emscripten_fetch_wait(node->fetch, INFINITY);
  }

  if (node->size > 0 && !node->data && (!node->fetch || !node->fetch->data))
    RETURN_ERRNO(-1, "ASMFS internal error: no file data available");
  if (iovcnt < 0)
    RETURN_ERRNO(EINVAL, "The vector count, iovcnt, is less than zero");

  ssize_t total_read_amount = 0;
  for (int i = 0; i < iovcnt; ++i) {
    ssize_t n = total_read_amount + iov[i].iov_len;
    if (n < total_read_amount)
      RETURN_ERRNO(EINVAL, "The sum of the iov_len values overflows an ssize_t value");
    if (!iov[i].iov_base && iov[i].iov_len > 0)
      RETURN_ERRNO(
        EINVAL, "iov_len specifies a positive length buffer but iov_base is a null pointer");
    total_read_amount = n;
  }

  size_t offset = desc->file_pos;
  uint8_t* data = node->data ? node->data : (node->fetch ? (uint8_t*)node->fetch->data : 0);
  size_t size = node->data ? node->size : (node->fetch ? node->fetch->numBytes : 0);
  for (int i = 0; i < iovcnt; ++i) {
    ssize_t dataLeft = size - offset;
    if (dataLeft <= 0)
      break;
    size_t bytesToCopy = (size_t)dataLeft < iov[i].iov_len ? dataLeft : iov[i].iov_len;
    memcpy(iov[i].iov_base, &data[offset], bytesToCopy);
#ifdef ASMFS_DEBUG
    EM_ASM(err('readv requested to read ' + $0 + ', read  ' + $1 + ' bytes from offset ' + $2 +
               ', new offset: ' + $3 + ' (file size: ' + $4 + ')'),
      (int)iov[i].iov_len, (int)bytesToCopy, (int)offset, (int)(offset + bytesToCopy), (int)size);
#endif
    offset += bytesToCopy;
  }
  ssize_t numRead = offset - desc->file_pos;
  desc->file_pos = offset;
  return numRead;
}

long __syscall3(long fd, long buf, long count) // read
{
#ifdef ASMFS_DEBUG
  EM_ASM(
    err('read(fd=' + $0 + ', buf=0x' + ($1).toString(16) + ', count=' + $2 + ')'), fd, buf, count);
#endif

  iovec io = {(void*)buf, (size_t)count};
  return readv(fd, &io, 1);
}

static long writev(int fd, const iovec *iov, int iovcnt) // syscall146
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('writev(fd=' + $0 + ', iov=0x' + ($1).toString(16) + ', iovcnt=' + $2 + ')'), fd, iov,
    iovcnt);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (fd != 1 /*stdout*/ &&
      fd != 2 /*stderr*/) // TODO: Resolve the hardcoding of stdin,stdout & stderr
  {
    if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
      RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");
  }

  if (iovcnt < 0)
    RETURN_ERRNO(EINVAL, "The vector count, iovcnt, is less than zero");

  ssize_t total_write_amount = 0;
  for (int i = 0; i < iovcnt; ++i) {
    ssize_t n = total_write_amount + iov[i].iov_len;
    if (n < total_write_amount)
      RETURN_ERRNO(EINVAL, "The sum of the iov_len values overflows an ssize_t value");
    if (!iov[i].iov_base && iov[i].iov_len > 0)
      RETURN_ERRNO(
        EINVAL, "iov_len specifies a positive length buffer but iov_base is a null pointer");
    total_write_amount = n;
  }

  if (fd == 1 /*stdout*/ || fd == 2 /*stderr*/) {
    ssize_t bytesWritten = 0;
    for (int i = 0; i < iovcnt; ++i) {
      print_stream(iov[i].iov_base, iov[i].iov_len, fd == 1);
      bytesWritten += iov[i].iov_len;
    }
    return bytesWritten;
  } else {
    // Enlarge the file in memory to fit space for the new data
    size_t newSize = desc->file_pos + total_write_amount;
    inode* node = desc->node;
    if (node->capacity < newSize) {
      size_t newCapacity =
        (newSize > (size_t)(node->capacity * 1.25)
            ? newSize
            : (size_t)(
                node->capacity * 1.25)); // Geometric increases in size for amortized O(1) behavior
      uint8_t* newData = (uint8_t*)realloc(node->data, newCapacity);
      if (!newData) {
        newData = (uint8_t*)malloc(newCapacity);
        memcpy(newData, node->data, node->size);
        // TODO: init gaps with zeroes.
        free(node->data);
      }
      node->data = newData;
      node->size = newSize;
      node->capacity = newCapacity;
    }

    for (int i = 0; i < iovcnt; ++i) {
      memcpy((uint8_t*)node->data + desc->file_pos, iov[i].iov_base, iov[i].iov_len);
      desc->file_pos += iov[i].iov_len;
    }
  }
  return total_write_amount;
}

long __syscall4(long fd, long buf, long count) // write
{
#ifdef ASMFS_DEBUG
  EM_ASM(
    err('write(fd=' + $0 + ', buf=0x' + ($1).toString(16) + ', count=' + $2 + ')'), fd, buf, count);
#endif

  iovec io = {(void*)buf, (size_t)count};
  return writev(fd, &io, 1);
}

// WASI support: provide a shim between the wasi fd_write syscall and the
// syscall146 that is implemented here in ASMFS.
// TODO: Refactor ASMFS's syscall146 into a direct handler for fd_write.

__wasi_errno_t __wasi_fd_write(
    __wasi_fd_t fd,
    const __wasi_ciovec_t *iovs,
    size_t iovs_len,
    size_t *nwritten
)
{
  long result;
  result = writev(fd, (const iovec*)iovs, iovs_len);
  if (result < 0) {
    *nwritten = 0;
    return result;
  }
  *nwritten = result;
  return 0;
}

// TODO: syscall148: fdatasync
// TODO: syscall168: poll

// TODO: syscall180: pread64
// TODO: syscall181: pwrite64

long __syscall183(long buf, long size) // getcwd
{
#ifdef ASMFS_DEBUG
  EM_ASM(err('getcwd(buf=0x' + $0 + ', size= ' + $1 + ')'), buf, size);
#endif

  if (!buf && size > 0)
    RETURN_ERRNO(EFAULT, "buf points to a bad address");
  if (buf && size == 0)
    RETURN_ERRNO(EINVAL, "The size argument is zero and buf is not a null pointer");

  inode* cwd = get_cwd();
  if (!cwd)
    RETURN_ERRNO(-1, "ASMFS internal error: no current working directory?!");
  // TODO: RETURN_ERRNO(ENOENT, "The current working directory has been unlinked");
  // TODO: RETURN_ERRNO(EACCES, "Permission to read or search a component of the filename was
  // denied");
  inode_abspath(cwd, (char *)buf, size);
  if (strlen((char *)buf) >= size - 1)
    RETURN_ERRNO(ERANGE, "The size argument is less than the length of the absolute pathname of the working directory, including the terminating null byte.  You need to allocate a bigger array and try again");

  return 0;
}

// TODO: syscall192: mmap2
// TODO: syscall193: truncate64
// TODO: syscall194: ftruncate64

static long __stat64(inode* node, struct stat* buf) {
  buf->st_dev =
    1; // ID of device containing file: Hardcode 1 for now, no meaning at the moment for Emscripten.
  buf->st_ino = (ino_t)node; // TODO: This needs to be an inode ID number proper.
  buf->st_mode = node->mode;
  switch (node->type) {
    case INODE_DIR:
      buf->st_mode |= S_IFDIR;
      break;
    case INODE_FILE:
      buf->st_mode |= S_IFREG;
      break; // Regular file
             /* TODO:
             case socket: buf->st_mode |= S_IFSOCK; break;
             case symlink: buf->st_mode |= S_IFLNK; break;
             case block device: buf->st_mode |= S_IFBLK; break;
             case character device: buf->st_mode |= S_IFCHR; break;
             case FIFO: buf->st_mode |= S_IFIFO; break;
             */
  }
  buf->st_nlink = 1; // The number of hard links. TODO: Use this for real when links are supported.
  buf->st_uid = node->uid;
  buf->st_gid = node->gid;
  buf->st_rdev = 1; // Device ID (if special file) No meaning right now for Emscripten.
  buf->st_size = node->fetch ? node->fetch->totalBytes : 0;
  if (node->size > (size_t)buf->st_size)
    buf->st_size = node->size;
  buf->st_blocks =
    (buf->st_size + 511) / 512; // The syscall docs state this is hardcoded to # of 512 byte blocks.
  buf->st_blksize = 1024 * 1024; // Specifies the preferred blocksize for efficient disk I/O.
  buf->st_atim.tv_sec = node->atime;
  buf->st_mtim.tv_sec = node->mtime;
  buf->st_ctim.tv_sec = node->ctime;
  return 0;
}

long __syscall195(long path, long buf) // SYS_stat64
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('SYS_stat64(pathname="' + UTF8ToString($0) + '", buf=0x' + ($1).toString(16) + ')'),
    pathname, buf);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  // Find if this file exists already in the filesystem?
  inode* root = (pathname[0] == '/') ? filesystem_root() : get_cwd();
  const char* relpath = (pathname[0] == '/') ? pathname + 1 : pathname;

  int err;
  inode* node = find_inode(root, relpath, &err);

  if (!node && (err == ENOENT || err == ENOTDIR)) {
    // Populate the file from the CDN to the filesystem if it didn't yet exist.
    long fd = open(pathname, O_RDONLY, 0777);
    if (fd)
      close(fd);
    node = find_inode(root, relpath, &err);
  }

  if (err == ENOTDIR)
    RETURN_ERRNO(ENOTDIR, "A component of the path prefix of pathname is not a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links encountered while traversing the path");
  if (err == EACCES)
    RETURN_ERRNO(EACCES,
      "Search permission is denied for one of the directories in the path prefix of pathname");
  if (err && err != ENOENT)
    RETURN_ERRNO(err, "find_inode() error");
  if (err == ENOENT || !node)
    RETURN_ERRNO(ENOENT, "A component of pathname does not exist");

  return __stat64(node, (struct stat *)buf);
}

long __syscall196(long path, long buf) // SYS_lstat64
{
  const char* pathname = (const char *)path;
#ifdef ASMFS_DEBUG
  EM_ASM(err('SYS_lstat64(pathname="' + UTF8ToString($0) + '", buf=0x' + ($1).toString(16) + ')'),
    pathname, buf);
#endif

  int len = strlen(pathname);
  if (len > MAX_PATHNAME_LENGTH)
    RETURN_ERRNO(ENAMETOOLONG, "pathname was too long");
  if (len == 0)
    RETURN_ERRNO(ENOENT, "pathname is empty");

  // Find if this file exists already in the filesystem?
  inode* root = (pathname[0] == '/') ? filesystem_root() : get_cwd();
  const char* relpath = (pathname[0] == '/') ? pathname + 1 : pathname;

  // TODO: When symbolic links are implemented, make this return info about the symlink itself and
  // not the file it points to.
  int err;
  inode* node = find_inode(root, relpath, &err);
  if (err == ENOTDIR)
    RETURN_ERRNO(ENOTDIR, "A component of the path prefix of pathname is not a directory");
  if (err == ELOOP)
    RETURN_ERRNO(ELOOP, "Too many symbolic links encountered while traversing the path");
  if (err == EACCES)
    RETURN_ERRNO(EACCES,
      "Search permission is denied for one of the directories in the path prefix of pathname");
  if (err && err != ENOENT)
    RETURN_ERRNO(err, "find_inode() error");
  if (err == ENOENT || !node)
    RETURN_ERRNO(ENOENT, "A component of pathname does not exist");

  return __stat64(node, (struct stat*)buf);
}

long __syscall197(long fd, long buf) // SYS_fstat64
{
#ifdef ASMFS_DEBUG
  EM_ASM(
    err('SYS_fstat64(fd="' + UTF8ToString($0) + '", buf=0x' + ($1).toString(16) + ')'), fd, buf);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "fd isn't a valid open file descriptor");

  inode* node = desc->node;
  if (!node)
    RETURN_ERRNO(ENOENT, "A component of pathname does not exist");

  return __stat64(node, (struct stat*)buf);
}

// TODO: syscall198: lchown
// TODO: syscall207: fchown32
// TODO: syscall212: chown32

long __syscall220(long fd, long dirp, long count) // getdents64 (get directory entries 64-bit)
{
  dirent* de = (dirent*)dirp;
  unsigned int dirents_size =
    count /
    sizeof(dirent); // The number of dirent structures that can fit into the provided buffer.
  dirent* de_end = de + dirents_size;
#ifdef ASMFS_DEBUG
  EM_ASM(err('getdents64(fd=' + $0 + ', de=0x' + ($1).toString(16) + ', count=' + $2 + ')'), fd, de,
    count);
#endif

  FileDescriptor* desc = (FileDescriptor*)fd;
  if (!desc || desc->magic != EM_FILEDESCRIPTOR_MAGIC)
    RETURN_ERRNO(EBADF, "Invalid file descriptor fd");

  inode* node = desc->node;
  if (!node)
    RETURN_ERRNO(ENOENT, "No such directory");
  if (dirents_size == 0)
    RETURN_ERRNO(EINVAL, "Result buffer is too small");
  if (node->type != INODE_DIR)
    RETURN_ERRNO(ENOTDIR, "File descriptor does not refer to a directory");

  inode* dotdot =
    node->parent ? node->parent : node; // In "/", the directory ".." refers to itself.

  ssize_t orig_file_pos = desc->file_pos;
  ssize_t file_pos = 0;
  // There are always two hardcoded directories "." and ".."
  if (de >= de_end)
    return desc->file_pos - orig_file_pos;
  if (desc->file_pos <= file_pos) {
    de->d_ino = (ino_t)node; // TODO: Create inode numbers instead of using pointers
    de->d_off = file_pos + sizeof(dirent);
    de->d_reclen = sizeof(dirent);
    de->d_type = DT_DIR;
    strcpy(de->d_name, ".");
    ++de;
    desc->file_pos += sizeof(dirent);
  }
  file_pos += sizeof(dirent);

  if (de >= de_end)
    return desc->file_pos - orig_file_pos;
  if (desc->file_pos <= file_pos) {
    de->d_ino = (ino_t)dotdot; // TODO: Create inode numbers instead of using pointers
    de->d_off = file_pos + sizeof(dirent);
    de->d_reclen = sizeof(dirent);
    de->d_type = DT_DIR;
    strcpy(de->d_name, "..");
    ++de;
    desc->file_pos += sizeof(dirent);
  }
  file_pos += sizeof(dirent);

  node = node->child;
  while (node && de < de_end) {
    if (desc->file_pos <= file_pos) {
      de->d_ino = (ino_t)node; // TODO: Create inode numbers instead of using pointers
      de->d_off = file_pos + sizeof(dirent);
      de->d_reclen = sizeof(dirent);
      de->d_type = (node->type == INODE_DIR) ? DT_DIR : DT_REG /*Regular file*/;
      de->d_name[255] = 0;
      strncpy(de->d_name, node->name, 255);
      ++de;
      desc->file_pos += sizeof(dirent);
    }
    node = node->sibling;
    file_pos += sizeof(dirent);
  }

  return desc->file_pos - orig_file_pos;
}

// TODO: syscall221: fcntl64
// TODO: syscall268: statfs64
// TODO: syscall269: fstatfs64
// TODO: syscall295: openat
// TODO: syscall296: mkdirat
// TODO: syscall297: mknodat
// TODO: syscall298: fchownat
// TODO: syscall300: fstatat64
// TODO: syscall301: unlinkat
// TODO: syscall302: renameat
// TODO: syscall303: linkat
// TODO: syscall304: symlinkat
// TODO: syscall305: readlinkat
// TODO: syscall306: fchmodat
// TODO: syscall307: faccessat
// TODO: syscall320: utimensat
// TODO: syscall324: fallocate
// TODO: syscall330: dup3
// TODO: syscall331: pipe2
// TODO: syscall333: preadv
// TODO: syscall334: pwritev

} // ~extern "C"
