#include "kernelFS.h"

KernelFS* FS::myImpl = new KernelFS();

FS::FS(){}

FS::~FS()
{
	
	myImpl = nullptr;
}

char FS::mount(Partition* partition)
{
	return myImpl->mount(partition);
}

char FS::unmount()
{
	return myImpl->unmount();
}

FileCnt FS::readRootDir()
{
	return myImpl->readRootDir();
}

char FS::doesExist(char* fname)
{
	return myImpl->doesExist(fname);
}

File* FS::open(char* fname, char mode)
{
	return myImpl->open(fname, mode);
}

char FS::deleteFile(char* fname)
{
	return myImpl->deleteFile(fname);
}

char FS::format()
{
	return myImpl->format();
}