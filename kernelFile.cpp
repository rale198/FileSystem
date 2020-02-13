#include "kernelFile.h"
#include "fcb.h"
#define EOF_CONSTANT 0x1FFFFFFF
#define min(a,b) (a<=b)?a:b;

CRITICAL_SECTION KernelFile::destructorMutex;
KernelFile::~KernelFile() {

	EnterCriticalSection(&destructorMutex);
	fcb->decThread();
	char buff[2049];
	ClusterNo sz[1];
	sz[0] = fcb->size;
	if (!fcb->threads) {
		buff[2048] = '\0';

		KernelFS::partition->readCluster(fcb->indeks2, buff);
		memcpy(buff + fcb->position, sz, 4);
		KernelFS::partition->writeCluster(fcb->indeks2, buff);
		KernelFS::partition->writeCluster(fcb->index1[0], fcb->buffer);
		KernelFS::openFileTable.erase(name);

		delete fcb;
		WakeAllConditionVariable(&KernelFS::openedFiles);
		WakeAllConditionVariable(&KernelFS::readerWriter);
	}

	fcb = nullptr;
	LeaveCriticalSection(&destructorMutex);
} 

BytesCnt KernelFile::getIndexInIndex1(ClusterNo val)
{
	return val / (512 * 2048);
}

BytesCnt KernelFile::getIndexInIndex2(ClusterNo val)
{
	return (val%(512*2048)) / (2048);
}

BytesCnt KernelFile::getPositionInCluster(ClusterNo val)
{
	return (val % (512 * 2048)) % (2048);
}


char KernelFile::write(BytesCnt bytes, char* buffer) { 
	if (!bytes) //edge cases
		return KernelFS::SUCCESS_CODE;
	if (!buffer) 
		return KernelFS::ERROR_CODE;
	if (fcb->mode == 'r')
		return KernelFS::ERROR_CODE; //ne moze upis ako je otvoren za citanje
	if (bytes > KernelFS::getNumOfFreeClust() * 2048) //file to big!
		return KernelFS::ERROR_CODE;

	BytesCnt idx1= getIndexInIndex1(seeker);
	BytesCnt idx2 = getIndexInIndex2(this->seeker);

	ClusterNo entry[1];
	ClusterNo entry2[1];

	char buffer1[2049];
	buffer1[2048] = '\0';
	char dataCluster[2049];
	dataCluster[2048] = '\0';

	BytesCnt tmp = 0;

	for (int i = idx1<<2; i < 2048; i += 4) //obilazak po indeksu prvog nivoa
	{

		memcpy(entry, fcb->buffer + i, 4);

		if (!entry[0])
		{
			entry[0] = KernelFS::getNextCluster();
			if (!entry[0])
				return KernelFS::ERROR_CODE; //no memory left

			memcpy(fcb->buffer + i, entry, 4);
			memset(buffer1, 0x00, 2048);
		}
		else
			KernelFS::partition->readCluster(entry[0], buffer1);

		for (int j = getIndexInIndex2(seeker) << 2; j < 2048; j+=4)
		{
			memcpy(entry2, buffer1 + j, 4);

			if (!entry2[0])
			{
				entry2[0] = KernelFS::getNextCluster();
				if (!entry2[0])
					return KernelFS::ERROR_CODE; //no memory left

				memcpy(buffer1 + j, entry2, 4);
				memset(dataCluster, 0x00, 2048); //init of a data cluster
			}
			else
				KernelFS::partition->readCluster(entry2[0], dataCluster);

			ClusterNo pos = getPositionInCluster(seeker);

			if (ClusterSize - pos >= bytes)
			{
				memcpy(dataCluster + pos, buffer + tmp, bytes);
				seeker += bytes;
				if (fcb->size < seeker)
					fcb->size = seeker;

				KernelFS::partition->writeCluster(entry2[0], dataCluster);
				KernelFS::partition->writeCluster(entry[0], buffer1);
				return KernelFS::SUCCESS_CODE;
			}
			else
			{
				memcpy(dataCluster + pos, buffer + tmp, ClusterSize - pos);
				seeker += (ClusterSize - pos);
				bytes = bytes - (ClusterSize - pos);
				tmp += (ClusterSize - pos);
				KernelFS::partition->writeCluster(entry2[0], dataCluster);
			}
		}
		KernelFS::partition->writeCluster(entry2[0], dataCluster);
		KernelFS::partition->writeCluster(entry[0], buffer1);
	}
	return KernelFS::ERROR_CODE; 
}

BytesCnt KernelFile::read(BytesCnt bytes, char* buffer) {
	if (!bytes) //edge cases
		return 0;
	if (!buffer)
		return 0;
	if (seeker == fcb->size)
		return 0;
	if (KernelFS::openFileTable.find(this->name) == KernelFS::openFileTable.end())
		return 0;//kako moze da fajl bude zatvoren i da se poziva ova fja ???

	ClusterNo end = min((seeker + bytes), fcb->size); // do ovde se cita
	BytesCnt tmpPos = 0;
	BytesCnt idx1Seeker = getIndexInIndex1(seeker);
	BytesCnt idx2Seeker = getIndexInIndex2(seeker);
	
	BytesCnt idx1Eof = getIndexInIndex1(end);
	BytesCnt idx2Eof = getIndexInIndex2(end);

	ClusterNo entrySeeker[1];
	ClusterNo entry2Seeker[1];
	ClusterNo entryEof[1];
	ClusterNo entry2Eof[1];

	char buffer1[2049];
	buffer1[2048] = '\0';
	char dataCluster[2049];
	dataCluster[2048] = '\0';

	for (BytesCnt i = idx1Seeker << 2; i < (idx1Eof << 2); i += 4) //citanje svi clustera podataka, ciji se indeksi 1. i 2. nivoa razlikuju u odnosu na end
	{
		memcpy(entrySeeker, fcb->buffer + i, 4);

		if (!entrySeeker[0])
			return 0;//error;

		KernelFS::partition->readCluster(entrySeeker[0], buffer1);
		idx2Seeker = getIndexInIndex2(seeker);

		for (int j = idx2Seeker << 2; j < 2048; j += 4)
		{
			memcpy(entry2Seeker, buffer1 + j, 4);

			if (!entry2Seeker[0])
				return 0; //err

			KernelFS::partition->readCluster(entry2Seeker[0], dataCluster);
			BytesCnt skr = getPositionInCluster(seeker);
			memcpy(buffer + tmpPos, dataCluster+skr, ClusterSize-skr);
			tmpPos += (ClusterSize-skr);
			seeker += (ClusterSize - skr);
		}
	}

	memcpy(entryEof, fcb->buffer + (idx1Eof << 2), 4);
	KernelFS::partition->readCluster(entryEof[0], buffer1); // zajednici indeksni blok 2. nivoa

	idx2Seeker = getIndexInIndex2(seeker);

	for (BytesCnt i = idx2Seeker << 2; i < (idx2Eof << 2); i += 4) // za sve razlicite indeksne blokove 2. nivoau istom indeksnom bloku 1. nivoa
	{
		memcpy(entry2Seeker, buffer1 + i, 4);

		if (!entry2Seeker[0])
			return 0; //err

		KernelFS::partition->readCluster(entry2Seeker[0], dataCluster);
		BytesCnt skr = getPositionInCluster(seeker);
		memcpy(buffer + tmpPos, dataCluster + skr, ClusterSize - skr);
		tmpPos += (ClusterSize - skr);
		seeker += (ClusterSize - skr);
	}

	memcpy(entry2Eof,buffer1+(idx2Eof<<2) , 4);
	KernelFS::partition->readCluster(entry2Eof[0], dataCluster);

	BytesCnt posEof = getPositionInCluster(end);
	BytesCnt skr11 = getPositionInCluster(seeker);
	memcpy(buffer + tmpPos, dataCluster+skr11, posEof-skr11);
	tmpPos += (posEof - skr11);
	seeker += (posEof - skr11);

	return tmpPos;
}

char KernelFile::seek(BytesCnt bytes) {

	if (bytes<0 || bytes>=EOF_CONSTANT)
		return KernelFS::ERROR_CODE;
	seeker = bytes;
	return KernelFS::SUCCESS_CODE;
}

BytesCnt KernelFile::filePos() { return seeker; }

char KernelFile::eof() {
	return (seeker == fcb->size) ? 2 : ((seeker < fcb->size) ? KernelFS::ERROR_CODE : KernelFS::SUCCESS_CODE); //kako moze da nastane greska ovde
}

BytesCnt KernelFile::getFileSize() { return fcb->size; }

char KernelFile::truncate() { 

	if (fcb->mode == 'r')
		return KernelFS::ERROR_CODE;
	if (seeker == fcb->size)
		return KernelFS::SUCCESS_CODE;

	BytesCnt idx1Seeker = getIndexInIndex1(seeker);
	BytesCnt idx2Seeker = getIndexInIndex2(seeker);

	BytesCnt idx1Eof = getIndexInIndex1(fcb->size);
	BytesCnt idx2Eof = getIndexInIndex2(fcb->size);

	ClusterNo entrySeeker[1];
	// ClusterNo entry2Seeker[1]; never used for truncate
	ClusterNo entryEof[1];
	ClusterNo entry2Eof[1];

	char buffer1[2049];
	buffer1[2048] = '\0';

	for (BytesCnt i = (idx1Eof << 2); i > (idx1Seeker << 2); i -= 4) 
	{
		memcpy(entryEof, fcb->buffer + i, 4);
		memset(fcb->buffer + i, 0x00, 4);

		if (!entryEof[0])
			return KernelFS::ERROR_CODE;//error;

		KernelFS::partition->readCluster(entryEof[0], buffer1);
		idx2Eof = getIndexInIndex2(fcb->size);

		KernelFS::freeClust(entryEof[0]);

		for (BytesCnt j = 0; j <= (idx2Eof << 2); j += 4)
		{
			memcpy(entry2Eof, buffer1 + j, 4);

			if (!entry2Eof[0])
				return KernelFS::ERROR_CODE; //err

			KernelFS::freeClust(entry2Eof[0]);
			BytesCnt position = getPositionInCluster(fcb->size);
			fcb->size -= position;
		}
	}

	memcpy(entrySeeker, fcb->buffer + (idx1Seeker << 2), 4);
	KernelFS::partition->readCluster(entrySeeker[0], buffer1); // zajednici indeksni blok 2. nivoa

	idx2Seeker = getIndexInIndex2(seeker);

	bool flag = false;
	for (BytesCnt i = (getIndexInIndex2(fcb->size)<<2); i > (idx2Seeker << 2); i -= 4) // za sve razlicite indeksne blokove 2. nivoau istom indeksnom bloku 1. nivoa
	{
		flag = true;
		
		memcpy(entry2Eof, buffer1 + i, 4);
		memset(buffer1 + i, 0x00, 4);

		if (!entry2Eof[0])
			return KernelFS::ERROR_CODE; //err

		KernelFS::freeClust(entry2Eof[0]);
		BytesCnt posit = getPositionInCluster(fcb->size);
		fcb->size -= posit;
	}

	if(flag==true) //ne upisujem ako je dataCluster isti za seeker i eof
		KernelFS::partition->writeCluster(entrySeeker[0], buffer1);
	fcb->size = seeker;

	return KernelFS::SUCCESS_CODE;

}

void KernelFile::setFCB(FCB* fcb)
{
	this->fcb = fcb;
	fcb->incThread();
}

void KernelFile::setName(std::string name)
{
	this->name = name;
}
KernelFile::KernelFile() {
	seeker = 0;
}
void KernelFile::setSeekEof()
{
	seeker = fcb->size;
}

void KernelFile::initDestructorMutex()
{
	InitializeCriticalSection(&KernelFile::destructorMutex);
}