
#include "FakeStore.h"
#include "include/types.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <iostream>
#include <cassert>
#include <errno.h>
#include <dirent.h>


#include "include/config.h"
#undef dout
#define  dout(l)    if (l<=g_conf.debug) cout << "osd" << whoami << ".fakestore "


#define HASH_DIRS       100LL
#define HASH_FUNC(x)    (((x)/13LL)%HASH_DIRS)




FakeStore::FakeStore(char *base, int whoami) 
{
  this->basedir = base;
  this->whoami = whoami;
}


int FakeStore::init() 
{
  string mydir;
  get_dir(mydir);

  dout(5) << "init with basedir " << mydir << endl;

  // make sure global base dir exists
  struct stat st;
  int r = ::stat(basedir.c_str(), &st);
  if (r != 0) {
	dout(1) << "unable to stat basedir " << basedir << ", r = " << r << endl;
	return r;
  }

  // all okay.
  return 0;
}

int FakeStore::finalize() 
{
  dout(5) << "finalize" << endl;
  // nothing
}




////

void FakeStore::get_dir(string& dir) {
  static char s[30];
  sprintf(s, "%d", whoami);
  dir = basedir + "/" + s;
}
void FakeStore::get_oname(object_t oid, string& fn) {
  static char s[100];
  sprintf(s, "%d/%02lld/%lld", whoami, HASH_FUNC(oid), oid);
  fn = basedir + "/" + s;
  //  dout(1) << "oname is " << fn << endl;
}


void FakeStore::wipe_dir(string mydir)
{
  DIR *dir = opendir(mydir.c_str());
  if (dir) {
	dout(1) << "wiping " << mydir << endl;
	struct dirent *ent = 0;
	
	while (ent = readdir(dir)) {
	  if (ent->d_name[0] == '.') continue;
	  dout(25) << "mkfs unlinking " << ent->d_name << endl;
	  string fn = mydir + "/" + ent->d_name;
	  unlink(fn.c_str());
	}	
	
	closedir(dir);
  } else {
	dout(1) << "mkfs couldn't read dir " << mydir << endl;
  }
}

int FakeStore::mkfs()
{
  int r;
  struct stat st;
  string mydir;
  get_dir(mydir);

  dout(1) << "mkfs in " << mydir << endl;

  // make sure my dir exists
  r = ::stat(mydir.c_str(), &st);
  if (r != 0) {
	dout(1) << "creating " << mydir << endl;
	mkdir(mydir.c_str(), 0755);
	r = ::stat(mydir.c_str(), &st);
	if (r != 0) {
	  dout(1) << "couldnt create dir, r = " << r << endl;
	  return r;
	}
  }
  else wipe_dir(mydir);

  // hashed bits too
  for (int i=0; i<HASH_DIRS; i++) {
	char s[4];
	sprintf(s, "%02d", i);
	string subdir = mydir + "/" + s;
	r = ::stat(subdir.c_str(), &st);
	if (r != 0) {
	  dout(2) << " creating " << subdir << endl;
	  mkdir(subdir.c_str(), 0755);
	  r = ::stat(subdir.c_str(), &st);
	  if (r != 0) {
		dout(1) << "couldnt create subdir, r = " << r << endl;
		return r;
	  }
	}
	else
	  wipe_dir( subdir );
  }
}



bool FakeStore::exists(object_t oid)
{
  struct stat st;
  if (stat(oid, &st) == 0) 
	return true;
  else
	return false;
}

int FakeStore::stat(object_t oid,
					struct stat *st)
{
  dout(20) << "stat " << oid << endl;
  string fn;
  get_oname(oid,fn);
  return ::stat(fn.c_str(), st);
}

int FakeStore::remove(object_t oid) 
{
  dout(20) << "remove " << oid << endl;
  string fn;
  get_oname(oid,fn);
  return ::unlink(fn.c_str());
}

int FakeStore::truncate(object_t oid, off_t size)
{
  dout(20) << "truncate " << oid << " size " << size << endl;
  string fn;
  get_oname(oid,fn);
  ::truncate(fn.c_str(), size);
}

int FakeStore::read(object_t oid, 
					size_t len, off_t offset,
					char *buffer) {
  dout(20) << "read " << oid << " len " << len << " off " << offset << endl;

  string fn;
  get_oname(oid,fn);
  
  int fd = open(fn.c_str(), O_RDONLY);
  if (fd < 0) return fd;
  flock(fd, LOCK_EX);    // lock for safety
  
  off_t actual = lseek(fd, offset, SEEK_SET);
  size_t got = 0;
  if (actual == offset) {
	got = ::read(fd, buffer, len);
  }
  flock(fd, LOCK_UN);
  close(fd);
  return got;
}

int FakeStore::write(object_t oid,
					 size_t len, off_t offset,
					 char *buffer) {
  dout(20) << "write " << oid << " len " << len << " off " << offset << endl;

  string fn;
  get_oname(oid,fn);
  
  int fd = open(fn.c_str(), O_WRONLY|O_CREAT);
  if (fd < 0) return fd;
  flock(fd, LOCK_EX);    // lock for safety
  fchmod(fd, 0664);
  
  off_t actual = lseek(fd, offset, SEEK_SET);
  size_t did = 0;
  if (actual == offset) {
	did = ::write(fd, buffer, len);
  }
  flock(fd, LOCK_UN);
  close(fd);
  
  return did;
}

