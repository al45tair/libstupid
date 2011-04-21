//
//  stupid.c
//  libstupid
//
//  Created by Alastair Houghton on 21/04/2011.
//  Copyright 2011 Alastair Houghton. All rights reserved.
//

#define _DARWIN_NO_64_BIT_INODE

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/attr.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

static bool
find_path (const char *restrict path,
           char *restrict new_path,
           bool ignore_last)
{
  const char *unmatched_path;
  char *ptr;
  char *pmax = new_path + PATH_MAX;
  char *matched_path = strdup (path);
  char *matched_end = matched_path + strlen (matched_path);
  struct stat st;
  
  // Find the part that actually exists
  while (matched_end > matched_path) {
    // Find the last '/', if any
    while (matched_end > matched_path && *--matched_end != '/');
  
    // convert to a NUL
    *matched_end = '\0';

    if (matched_end == matched_path)
      break;
    
    if (stat (matched_path, &st) == 0)
      break;
  }
  
  // This points at the first unmatched part
  unmatched_path = path + strlen (matched_path);
  
  if (matched_path[0] == '\0') {
    free (matched_path);
    matched_path = strdup (".");
  }
  
  if (strlen (matched_path) >= PATH_MAX) {
    free (matched_path);
    return false;
  }
  
  strcpy (new_path, matched_path);
  ptr = new_path + strlen (new_path);
  
  // Starting from matched_path, try to find successive pieces of unmatched
  // path
  while (*unmatched_path) {
    const char *pchunk = unmatched_path;
    const char *pend;
    
    // Skip leading slashes
    while (*pchunk == '/')
      ++pchunk;

    if (!*pchunk)
      break;
    
    // Find the end of the next piece
    pend = pchunk;
    while (*pend && *pend != '/')
      ++pend;

    unmatched_path = pend;
    
    size_t chunk_len = pend - pchunk;
    
    if (chunk_len == 1 && strcmp (".", pchunk) == 0) {
      if (pmax - ptr < 3) {
        free (matched_path);
        return false;
      }
      ptr = stpcpy (ptr, "/.");
    } else if (chunk_len == 2 && strcmp ("..", pchunk) == 0) {
      if (pmax - ptr < 4) {
        free (matched_path);
        return false;
      }
      ptr = stpcpy (ptr, "/..");
    } else {
      struct dirent de, *pde;
      bool found = false;
      
      // Try to locate a name that matches case-insensitively
      DIR *dir = opendir (new_path);
      while (readdir_r (dir, &de, &pde) == 0 && pde) {
        if (de.d_namlen == chunk_len
            && strncasecmp (de.d_name, pchunk, chunk_len) == 0) {
          found = true;
          if (pmax - ptr < chunk_len + 2) {
            free (matched_path);
            return false;
          }
          *ptr++ = '/';
          ptr = stpcpy (ptr, de.d_name);
          break;
        }
      }
      closedir (dir);
      
      if (!found) {
        free (matched_path);
        
        if (!*pend && ignore_last) {
          // In this case, the last part wasn't there, but we don't care
          fprintf (stderr, "wanted: %s\nmatched partial: %s\n", path, new_path);
          
          if (pmax - ptr < chunk_len + 2) {
            free (matched_path);
            return false;
          }
          *ptr++ = '/';
          strcpy (ptr, pchunk);
          return true;
        }
        
        return false;
      }
    }
  }
  
  fprintf (stderr, "wanted: %s\nmatched full: %s\n", path, new_path);
  
  free (matched_path);
  
  return true;
}

int
stupid_stat (const char *restrict path, struct stat *restrict buf) 
{
  int ret = stat (path, buf);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = stat (new_path, buf);
  }
  
  return ret;
}

int
stupid_lstat (const char *restrict path, struct stat *restrict buf) 
{
  int ret = lstat (path, buf);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = lstat (new_path, buf);
  }
  
  return ret;
}

extern int stat$INODE64(const char *restrict, struct stat64 *restrict);
extern int lstat$INODE64(const char *restrict, struct stat64 *restrict);

int
stupid_stat64 (const char *restrict path, struct stat64 *restrict buf)
{
  int ret = stat$INODE64 (path, buf);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = stat$INODE64 (new_path, buf);
  }
  
  return ret;
}

int
stupid_lstat64 (const char *restrict path, struct stat64 *restrict buf)
{
  int ret = lstat$INODE64 (path, buf);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = lstat$INODE64 (new_path, buf);
  }
  
  return ret;
}

int
stupid_open (const char *path, int oflag, mode_t mode) 
{
  int ret = open (path, oflag, mode);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, (oflag & O_CREAT) ? true : false))
      ret = open (new_path, oflag, mode);
  }
  
  return ret;
}

int
stupid_creat (const char *path, mode_t mode)
{
  int ret = creat (path, mode);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, true))
      ret = creat (new_path, mode);
  }
  
  return ret;
}

DIR *
stupid_opendir (const char *dirname) 
{
  DIR *ret = opendir (dirname);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (dirname, new_path, false))
      ret = opendir (new_path);
  }
  
  return ret;
}

char *
stupid_realpath (const char *restrict file_name,
                 char *restrict resolved_name) 
{
  char *ret = realpath (file_name, resolved_name);
  
  if (!ret && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (file_name, new_path, false))
      ret = realpath (new_path, resolved_name);
  }
  
  return ret;
}

int
stupid_chmod (const char *path, mode_t mode)
{
  int ret = chmod (path, mode);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = chmod (new_path, mode);
  }
  
  return ret;
}

int
stupid_chown (const char *path, uid_t owner, gid_t group)
{
  int ret = chown (path, owner, group);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = chown (new_path, owner, group);
  }
  
  return ret;
}

int
stupid_mkdir (const char *path, mode_t mode)
{
  int ret = mkdir (path, mode);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, true))
      ret = mkdir (new_path, mode);
  }
  
  return ret;
}

int
stupid_mknod (const char *path, mode_t mode, dev_t dev)
{
  int ret = mknod (path, mode, dev);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, true))
      ret = mknod (new_path, mode, dev);
  }
  
  return ret;
}

int
stupid_unlink (const char *path)
{
  int ret = unlink (path);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = unlink (new_path);
  }
  
  return ret;
}

int
stupid_rmdir (const char *path)
{
  int ret = rmdir (path);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = rmdir (new_path);
  }
  
  return ret;
}

int
stupid_link (const char *path1, const char *path2)
{
  int ret = link (path1, path2);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path1[PATH_MAX];
    
    if (find_path (path1, new_path1, false)) {
      ret = link (new_path1, path2);
      
      if (ret < 0 && errno == ENOENT) {
        char new_path2[PATH_MAX];
        
        if (find_path (path2, new_path2, true))
          ret = link (new_path1, new_path2);
      }
    }
  }
  
  return ret;
}

int
stupid_symlink (const char *path1, const char *path2)
{
  int ret = symlink (path1, path2);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path2[PATH_MAX];
    
    if (find_path (path2, new_path2, true))
      ret = symlink (path1, new_path2);
  }
  
  return ret;
}

int
stupid_mkfifo (const char *path, mode_t mode)
{
  int ret = mkfifo (path, mode);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, true))
      ret = mkfifo (new_path, mode);
  }
  
  return ret;
}

int
stupid_statfs (const char *path, struct statfs *buf)
{
  int ret = statfs (path, buf);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = statfs (new_path, buf);
  }
  
  return ret;
}

int
stupid_getattrlist (const char *path,
                    struct attrlist *attrList,
                    void *attrBuf,
                    size_t attrBufSize,
                    unsigned int options)
{
  int ret = getattrlist (path, attrList, attrBuf, attrBufSize, options);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = getattrlist (new_path, attrList, attrBuf, attrBufSize, options);
  }
  
  if (ret >= 0 
      && (attrList->volattr & ATTR_VOL_INFO)
      && (attrList->volattr & ATTR_VOL_CAPABILITIES)) {
    // We need to go hunting for volume attributes
    uint8_t *ptr = (uint8_t *)attrBuf;
    uint8_t *pend = ptr + attrBufSize;
    attrgroup_t cmnAttrs = attrList->commonattr;
    attrgroup_t volAttrs = attrList->volattr;

    if (cmnAttrs & ATTR_CMN_RETURNED_ATTRS) {
      attribute_set_t *pset = (attribute_set_t *)ptr;
      
      if ((uint8_t *)(pset + 1) > pend)
        return ret;
      
      cmnAttrs = pset->commonattr;
      volAttrs = pset->volattr;
    
      ptr += sizeof (attribute_set_t);
    }
    
    if (cmnAttrs & ATTR_CMN_NAME)
      ptr += sizeof (attrreference_t);
    
    if (cmnAttrs & ATTR_CMN_DEVID)
      ptr += sizeof (dev_t);
    
    if (cmnAttrs & ATTR_CMN_FSID)
      ptr += sizeof (fsid_t);
    
    if (cmnAttrs & ATTR_CMN_OBJTYPE)
      ptr += sizeof (fsobj_type_t);
    
    if (cmnAttrs & ATTR_CMN_OBJTAG)
      ptr += sizeof (fsobj_tag_t);
    
    if (cmnAttrs & ATTR_CMN_OBJID)
      ptr += sizeof (fsobj_id_t);
    
    if (cmnAttrs & ATTR_CMN_OBJPERMANENTID)
      ptr += sizeof (fsobj_id_t);
    
    if (cmnAttrs & ATTR_CMN_PAROBJID)
      ptr += sizeof (fsobj_id_t);
    
    if (cmnAttrs & ATTR_CMN_SCRIPT)
      ptr += sizeof (text_encoding_t);
    
    if (cmnAttrs & ATTR_CMN_CRTIME)
      ptr += sizeof (struct timespec);
    
    if (cmnAttrs & ATTR_CMN_MODTIME)
      ptr += sizeof (struct timespec);
    
    if (cmnAttrs & ATTR_CMN_CHGTIME)
      ptr += sizeof (struct timespec);
    
    if (cmnAttrs & ATTR_CMN_ACCTIME)
      ptr += sizeof (struct timespec);
    
    if (cmnAttrs & ATTR_CMN_BKUPTIME)
      ptr += sizeof (struct timespec);
    
    if (cmnAttrs & ATTR_CMN_FNDRINFO)
      ptr += 32;
    
    if (cmnAttrs & ATTR_CMN_OWNERID)
      ptr += sizeof (uid_t);
    
    if (cmnAttrs & ATTR_CMN_GRPID)
      ptr += sizeof (gid_t);
    
    if (cmnAttrs & ATTR_CMN_ACCESSMASK)
      ptr += sizeof (u_int32_t);
    
    if (cmnAttrs & ATTR_CMN_NAMEDATTRCOUNT)
      ptr += sizeof (u_int32_t);
    
    if (cmnAttrs & ATTR_CMN_NAMEDATTRLIST)
      ptr += sizeof (attrreference_t);
    
    if (cmnAttrs & ATTR_CMN_FLAGS)
      ptr += sizeof (u_int32_t);
    
    if (cmnAttrs & ATTR_CMN_USERACCESS)
      ptr += sizeof (u_int32_t);
    
    if (cmnAttrs & ATTR_CMN_EXTENDED_SECURITY)
      ptr += sizeof (attrreference_t);
    
    if (cmnAttrs & ATTR_CMN_UUID)
      ptr += sizeof (guid_t);
    
    if (cmnAttrs & ATTR_CMN_GRPUUID)
      ptr += sizeof (guid_t);
    
    if (cmnAttrs & ATTR_CMN_FILEID)
      ptr += sizeof (u_int64_t);
    
    if (cmnAttrs & ATTR_CMN_PARENTID)
      ptr += sizeof (u_int64_t);
    
    if (cmnAttrs & ATTR_CMN_FULLPATH)
      ptr += sizeof (attrreference_t);
    
    if (volAttrs & ATTR_VOL_FSTYPE)
      ptr += sizeof (uint32_t);
    
    if (volAttrs & ATTR_VOL_SIGNATURE)
      ptr += sizeof (uint32_t);
    
    if (volAttrs & ATTR_VOL_SIZE)
      ptr += sizeof (off_t);
    
    if (volAttrs & ATTR_VOL_SPACEFREE)
      ptr += sizeof (off_t);
    
    if (volAttrs & ATTR_VOL_SPACEAVAIL)
      ptr += sizeof (off_t);
    
    if (volAttrs & ATTR_VOL_MINALLOCATION)
      ptr += sizeof (off_t);
    
    if (volAttrs & ATTR_VOL_ALLOCATIONCLUMP)
      ptr += sizeof (off_t);
    
    if (volAttrs & ATTR_VOL_IOBLOCKSIZE)
      ptr += sizeof (u_int32_t);
    
    if (volAttrs & ATTR_VOL_OBJCOUNT)
      ptr += sizeof (u_int32_t);
    
    if (volAttrs & ATTR_VOL_FILECOUNT)
      ptr += sizeof (u_int32_t);
    
    if (volAttrs & ATTR_VOL_DIRCOUNT)
      ptr += sizeof (u_int32_t);
    
    if (volAttrs & ATTR_VOL_MAXOBJCOUNT)
      ptr += sizeof (u_int32_t);
    
    if (volAttrs & ATTR_VOL_MOUNTPOINT)
      ptr += sizeof (attrreference_t);
    
    if (volAttrs & ATTR_VOL_NAME)
      ptr += sizeof (attrreference_t);
    
    if (volAttrs & ATTR_VOL_MOUNTFLAGS)
      ptr += sizeof (u_int32_t);
    
    if (volAttrs & ATTR_VOL_MOUNTEDDEVICE)
      ptr += sizeof (attrreference_t);
    
    if (volAttrs & ATTR_VOL_ENCODINGSUSED)
      ptr += sizeof (unsigned long long);
    
    if (volAttrs & ATTR_VOL_CAPABILITIES) {
      vol_capabilities_attr_t *caps = (vol_capabilities_attr_t *)ptr;
      
      if ((uint8_t *)(caps + 1) > pend)
        return ret;
      
      // Turn off the case-sensitive flag
      caps->capabilities[VOL_CAPABILITIES_FORMAT] \
        &= ~VOL_CAP_FMT_CASE_SENSITIVE;
    }
  }
  
  return ret;
}

int
stupid_setattrlist (const char *path,
                    struct attrlist *attrList,
                    void *attrBuf,
                    size_t attrBufSize,
                    unsigned int options)
{
  int ret = setattrlist (path, attrList, attrBuf, attrBufSize, options);
  
  if (ret < 0 && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (path, new_path, false))
      ret = setattrlist (new_path, attrList, attrBuf, attrBufSize, options);
  }
  
  return ret;
}

FILE *
stupid_fopen (const char *restrict filename, const char *restrict mode)
{
  FILE *ret = fopen (filename, mode);
  
  if (!ret && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (filename, new_path, strncmp (mode, "r", 1) != 0))
      ret = fopen (new_path, mode);
  }
  
  return ret;
}

FILE *
stupid_freopen (const char *restrict filename, const char *restrict mode,
                FILE *restrict stream)
{
  FILE *ret = freopen (filename, mode, stream);
  
  if (!ret && errno == ENOENT) {
    char new_path[PATH_MAX];
    
    if (find_path (filename, new_path, strncmp (mode, "r", 1) != 0))
      ret = freopen (new_path, mode, stream);
  }
  
  return ret;
}

static const struct { void *n; void *o; } interposers[]
__attribute__((section("__DATA, __interpose"))) = {
  { stupid_stat, stat },
  { stupid_lstat, lstat },
  { stupid_stat64, stat$INODE64 },
  { stupid_lstat64, lstat$INODE64 },
  { stupid_open, open },
  { stupid_creat, creat },
  { stupid_opendir, opendir },
  { stupid_realpath, realpath },
  { stupid_chmod, chmod },
  { stupid_chown, chown },
  { stupid_mkdir, mkdir },
  { stupid_mknod, mknod },
  { stupid_mkfifo, mkfifo },
  { stupid_unlink, unlink },
  { stupid_rmdir, rmdir },
  { stupid_link, link },
  { stupid_symlink, symlink },
  { stupid_statfs, statfs },
  { stupid_getattrlist, getattrlist },
  { stupid_setattrlist, setattrlist },
  { stupid_fopen, fopen },
  { stupid_freopen, freopen },
};
