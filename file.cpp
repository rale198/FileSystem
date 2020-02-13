#include "kernelFile.h"
File::~File() {
	delete myImpl;
}

char File::write(BytesCnt num, char* buffer)
{
	return myImpl->write(num, buffer);
}
BytesCnt File::read(BytesCnt num, char* buffer)
{
	return myImpl->read(num, buffer);
}
char File::seek(BytesCnt pos)
{

	return myImpl->seek(pos);
}
BytesCnt File::filePos()
{
	return myImpl->filePos();

}
char File::eof()
{
	return myImpl->eof();
}
BytesCnt File::getFileSize()
{
	return myImpl->getFileSize();
}
char File::truncate()
{
	return myImpl->truncate();
}

File::File()
{
	this->myImpl = new KernelFile();

}

void File::setFCB(FCB* fcb)
{
	myImpl->setFCB(fcb);
}

void File::setSeekEof() { myImpl->setSeekEof(); }

void File::setName(std::string name)
{
	myImpl->setName(name);
}