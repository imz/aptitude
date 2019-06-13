// pkg_acqfile.h
//
//  File acquirers that Don't Suck.

#include <apt-pkg/acquire-item.h>

class pkgAcqFileSane:public pkgAcquire::Item
// This is frustrating: pkgAcqFile is **almost** good enough, but has some
// hardcoded stuff that makes it not quite work.
//
//  Based heavily on that class, though.
{
  pkgAcquire::ItemDesc Desc;
  string Md5Hash;
  unsigned int Retries;

public:
  pkgAcqFileSane(pkgAcquire *Owner, string URI,
		 string Description, string ShortDesc, string filename);

  virtual void Failed(const string &Message, pkgAcquire::MethodConfig *Cnf) override;
  virtual string MD5Sum() override {return Md5Hash;}
  virtual string DescURI() override {return Desc.URI;}
  virtual ~pkgAcqFileSane() {}
};

// Hack around the broken pkgAcqArchive.
bool get_archive(pkgAcquire *Owner, pkgSourceList *Sources,
		 pkgRecords *Recs, pkgCache::VerIterator const &Version,
		 const std::string &directory, std::string &StoreFilename);
