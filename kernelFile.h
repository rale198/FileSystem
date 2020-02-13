#ifndef _KFILE_H_
#define _KFILE_H_

#include <string>
#include "File.h"
class KernelFile
{
private:
	~KernelFile(); //zatvaranje fajla
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate();
	KernelFile();
	void setFCB(FCB*);

	FCB* fcb;
	friend class File;
	friend class FCB;
	friend class KernelFS;

	BytesCnt seeker;
	std::string name;

	void setName(std::string);
	void setSeekEof();

	BytesCnt getIndexInIndex1(ClusterNo);
	BytesCnt getIndexInIndex2(ClusterNo);
	BytesCnt getPositionInCluster(ClusterNo);

	static CRITICAL_SECTION destructorMutex;
	static void initDestructorMutex();
};
#endif
