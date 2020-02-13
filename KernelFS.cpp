#include "kernelFS.h"
#include "File.h"
#include "fcb.h"
#include "kernelFile.h"
using namespace std;

#define par pair<char,FCB>
char KernelFS::ERROR_CODE = 0x00;
char KernelFS::SUCCESS_CODE = 0x01;
map<string,FCB*> KernelFS::openFileTable;
ClusterNo KernelFS::numOfFiles = 0;
long KernelFS::thread_counter = 0;
bool KernelFS::flagOpener = true;
ClusterNo KernelFS::numOfFreeClusters = 0;

Partition* KernelFS::partition = nullptr;
char* KernelFS::bitVector = nullptr;

ClusterNo KernelFS::numOfBitVectorClusters = 0;
ClusterNo KernelFS::nextFreeCluster = 0;

ClusterNo KernelFS::positionInBuffer = 0;
ClusterNo KernelFS::indeks2 = 0;

CONDITION_VARIABLE KernelFS::partNull;
CONDITION_VARIABLE KernelFS::openedFiles;
CONDITION_VARIABLE KernelFS::readerWriter;
CRITICAL_SECTION KernelFS::mutex;

KernelFS::KernelFS()
{
	InitializeCriticalSection(&KernelFS::mutex);
	InitializeConditionVariable(&partNull);
	InitializeConditionVariable(&openedFiles);
	InitializeConditionVariable(&readerWriter);
	KernelFile::initDestructorMutex();
}
KernelFS::~KernelFS()
{
	DeleteCriticalSection(&KernelFS::mutex);
	DeleteCriticalSection(&KernelFile::destructorMutex);
}

char KernelFS::mount(Partition* p) { 
	if (p == nullptr)
		return ERROR_CODE;

	EnterCriticalSection(&mutex);

	while (partition != nullptr)
	{
		SleepConditionVariableCS(&partNull, &mutex, INFINITE);
	}

	partition = p;

	ClusterNo num = p->getNumOfClusters();
	numOfBitVectorClusters = num / (2048 * 8) + (num % (2048 * 8) != 0);
	bitVector = new char[KernelFS::numOfBitVectorClusters*ClusterSize+1];
	bitVector[numOfBitVectorClusters * ClusterSize] = '\0';

	char buffer[2049];
	buffer[2048] = '\0';
	BytesCnt pos = 0;
	int retVal = 1;
	for (ClusterNo i = 0; i < numOfBitVectorClusters; i++) {
		retVal = p->readCluster(i, buffer);
		if (!retVal)
			return ERROR_CODE;
		memcpy(bitVector + pos, buffer, ClusterSize);
		pos += ClusterSize;
	}
	LeaveCriticalSection(&mutex);
	
	return SUCCESS_CODE;
};
										
char KernelFS::unmount()
{
	if (partition == nullptr)
		return ERROR_CODE;

	EnterCriticalSection(&mutex);

	while (openFileTable.size() > 0)
	{
		SleepConditionVariableCS(&openedFiles,&mutex,INFINITE);
	}


	char buffer[2049];
	buffer[2048] = '\0';
	BytesCnt pos = 0;
	int retVal = SUCCESS_CODE;
	for (ClusterNo i = 0; i < numOfBitVectorClusters; i++) {
		memcpy(buffer, bitVector + pos, ClusterSize);
		retVal = partition->writeCluster(i, buffer);
		if (!retVal)
			return ERROR_CODE;
		pos += ClusterSize;
	}

	delete[] bitVector;
	partition = nullptr;

	LeaveCriticalSection(&mutex);
	WakeAllConditionVariable(&partNull); // ovde mora ovakav redosled zbog potencijalnih nailazecih novih mountova


	return SUCCESS_CODE;
}

int KernelFS::numOfOpenedFiles()
{
	return openFileTable.size();
}

void KernelFS::initBitVector() {

	BytesCnt tmpPos = 0;
	ClusterNo iter = (partition->getNumOfClusters() % 16384 != 0) ? (numOfBitVectorClusters - 1) : (numOfBitVectorClusters);

	for (ClusterNo i = 0; i < iter; i++)
	{
		memset(bitVector + tmpPos, 0xFF, 2048);
		tmpPos += ClusterSize;
	}

	if (partition->getNumOfClusters() % 16384 != 0)
	{
		ClusterNo num = partition->getNumOfClusters() % 16834;
		KernelFS::numOfFreeClusters = partition->getNumOfClusters();
		ClusterNo numOfBytes = num / 8;

		memset(bitVector+tmpPos, 0xFF, numOfBytes);
		tmpPos += numOfBytes;

		if (num % 8 != 0)
		{
			unsigned bitMask = 0x01;
			unsigned result = 0x00;
			for (ClusterNo i = 0; i < num % 8; i++)
			{
				result |= bitMask;
				bitMask <<= 1;
			}
			memset(bitVector + tmpPos, result, 1);
			//tmpPos+=numOfBytes;
		}
		memset(bitVector + tmpPos, 0x00, 2048 - numOfBytes);
	}

	for (int i = 0; i <= numOfBitVectorClusters; i++)
		KernelFS::getNextCluster();

};
char KernelFS::initRootDir() {

	char buffer[2049];
	char buffer2[2049];

	buffer[2048] = '\0'; //indeksni blok prvog nivoa


	buffer2[2048] = '\0'; // inicijalizacija indeksa 2. nivao
	memset(buffer2, 0x00, 2048);

	int retVal = 1;
	for (int i = 0; i < 2048; i += 4)
	{
		ClusterNo index1[1];
		index1[0] = getNextCluster();
		memcpy(buffer + i, index1, 4);

		retVal = partition->writeCluster(index1[0], buffer2);
		if (!retVal)
			return ERROR_CODE;

	}

	retVal = partition->writeCluster(numOfBitVectorClusters, buffer);

	if (!retVal)
		return ERROR_CODE;
	return SUCCESS_CODE;

};
char KernelFS::format() {

	if (partition == nullptr)
		return ERROR_CODE;

	EnterCriticalSection(&mutex);

	while (numOfOpenedFiles() > 0)
	{
		flagOpener = false;
		SleepConditionVariableCS(&openedFiles, &mutex, INFINITE);
	}

	flagOpener = true;
	initBitVector();
	char retVal = initRootDir();

	LeaveCriticalSection(&mutex);
	return retVal;
};

FileCnt KernelFS::readRootDir() { 

	if (KernelFS::partition == nullptr)
		return -1;
	return numOfFiles;
};

char KernelFS::findOnDisc(char* fname)
{
	if (partition == nullptr)
		return ERROR_CODE;

	char buffer[2049];
	char buffer1[2049];
	char buffer2[2049];

	int retVal = partition->readCluster(numOfBitVectorClusters, buffer);
	if (!retVal)
		return ERROR_CODE;
	buffer[2048] = '\0';

	for (int i = 0; i < 2048; i += 4)
	{
		ClusterNo idx1[1];

		memcpy(idx1, buffer + i, 4);

		retVal = partition->readCluster(idx1[0], buffer1);
		if (!retVal)
			return ERROR_CODE;
		buffer1[2048] = '\0';

		for (int j = 0; j < 2048; j += 4)
		{
			ClusterNo idx2[1];

			memcpy(idx2, buffer1 + j, 4);

			retVal = partition->readCluster(idx2[0], buffer2);
			if (!retVal)
				return ERROR_CODE;
			buffer2[2048] = '\0';

			for (int k = 0; k < 2048; k += 32)
			{
				char fcb[33];
				memcpy(fcb, buffer2 + k, 32);

				fcb[32] = '\0';

				if (!myStrCmp(fname,fcb))
					return SUCCESS_CODE;
			}
		}
	}
	return ERROR_CODE;
}
char KernelFS::doesExist(char* fname) { 

	if (fname == nullptr)
		return ERROR_CODE;

	EnterCriticalSection(&mutex);
	if (KernelFS::openFileTable.find((string)(fname+1)) != openFileTable.end())
	{
		LeaveCriticalSection(&mutex);
		return SUCCESS_CODE;
	}

	char ret = findOnDisc(fname+1);
	LeaveCriticalSection(&mutex);
	return ret;
};


File* KernelFS::open(char* fname, char mode) {

	if (fname == nullptr)
		return nullptr;
	
	if (!flagOpener)
		return nullptr;

	EnterCriticalSection(&mutex);

	char flag = 'b';
	ClusterNo* clustNum = new ClusterNo[1];
	char* buffer = nullptr;
	FCB* fcb = nullptr;

	if (mode == 'w')
	{
		while (openFileTable.find((string)(fname+1)) != openFileTable.end())
		{
			SleepConditionVariableCS(&readerWriter, &mutex, INFINITE);
		}

		flag = doesExist(fname);
		if (flag == SUCCESS_CODE)
		{
			if (deleteFile(fname) == ERROR_CODE) {
				LeaveCriticalSection(&mutex);
				return nullptr;
			}
		}

		clustNum[0] = KernelFS::getNextCluster(); //indeks prvog nivoa za fajl koji se pravi

		if (!clustNum[0]) {
			LeaveCriticalSection(&mutex);
			return nullptr;
		}

		if (makeFCB(fname+1, clustNum[0]) == false) {
			LeaveCriticalSection(&mutex);
			return nullptr;
		}
		buffer = new char[2049];
		memset(buffer, 0x00, 2049);
		buffer[2048] = '\0';

		fcb = new FCB(clustNum[0], buffer, mode, KernelFS::positionInBuffer, KernelFS::indeks2,0);
		KernelFS::positionInBuffer = KernelFS::indeks2 = 0;
		openFileTable[(string)(fname+1)] = fcb;
		numOfFiles++;
	}
	else if (mode == 'a') {

		while (openFileTable.find((string)(fname+1)) != openFileTable.end())
		{
			SleepConditionVariableCS(&readerWriter, &mutex, INFINITE);
		}

		flag = doesExist(fname);

		if (flag == ERROR_CODE) {
			LeaveCriticalSection(&mutex);
			return nullptr;
		}

		buffer = new char[2049];
		buffer[2048] = '\0';

		BytesCnt* retSize = new BytesCnt[1];
		char rt = getDataFromFileFCB(fname+1, clustNum,retSize);
		if (rt == ERROR_CODE)
			return nullptr;

		int retVal = partition->readCluster(clustNum[0], buffer);
		if (!retVal)
			return nullptr;
		fcb = new FCB(clustNum[0], buffer, mode, KernelFS::positionInBuffer, KernelFS::indeks2,retSize[0]);
		KernelFS::positionInBuffer = KernelFS::indeks2 = 0;
		openFileTable[(string)(fname+1)] = fcb;
		//numOfFiles++;
		delete[] retSize;
	}
	else if (mode == 'r')
	{

		char tmpMode = 'x';
		if(openFileTable.find((string)(fname+1))!=openFileTable.end())
			(openFileTable[(string)(fname+1)])->mode;

		if (tmpMode == 'r')
			fcb = (openFileTable[(string)(fname+1)]);
		else
		{
			while (openFileTable.find((string)(fname+1)) != openFileTable.end())
			{
				SleepConditionVariableCS(&readerWriter, &mutex, INFINITE);
			}

			flag = doesExist(fname);

			if (flag == ERROR_CODE) {
				LeaveCriticalSection(&mutex);
				return nullptr;
			}

			buffer = new char[2049];
			buffer[2048] = '\0';
			BytesCnt *sizeRet = new BytesCnt[1];
			char ch = getDataFromFileFCB(fname+1, clustNum, sizeRet);
			if (ch == ERROR_CODE)
				return nullptr;
			int retVal = partition->readCluster(clustNum[0], buffer);
			if (!retVal)
				return nullptr;
			fcb = new FCB(clustNum[0], buffer, mode, KernelFS::positionInBuffer, KernelFS::indeks2,sizeRet[0]);
			KernelFS::positionInBuffer = KernelFS::indeks2 = 0;
			openFileTable[(string)(fname+1)] = fcb;
			//numOfFiles++;
			delete[] sizeRet;
		}
	}
	else
	{
		LeaveCriticalSection(&mutex);
		return nullptr; //error mode
	}

	File* novi = new File();
	novi->setFCB(fcb);
	novi->setName(fname+1);
	if (mode == 'a')
		novi->setSeekEof();

	delete[] clustNum;
	LeaveCriticalSection(&mutex);
	return novi;
}
char KernelFS::getDataFromFileFCB(char* fname, ClusterNo* retVal,BytesCnt* sizeRet)
{
	
	char buffer[2049];
	char buffer1[2049];
	char buffer2[2049];

	int retVV = partition->readCluster(numOfBitVectorClusters, buffer);
	if (!retVV)
		return ERROR_CODE;
	buffer[2048] = '\0';

	for (int i = 0; i < 2048; i += 4)
	{
		ClusterNo idx1[1];

		memcpy(idx1, buffer + i, 4);

		retVV = partition->readCluster(idx1[0], buffer1);
		if (!retVV)
			return ERROR_CODE;
		buffer1[2048] = '\0';

		for (int j = 0; j < 2048; j += 4)
		{
			ClusterNo idx2[1];

			memcpy(idx2, buffer1 + j, 4);

			retVV = partition->readCluster(idx2[0], buffer2);
			if (!retVV)
				return ERROR_CODE;
			buffer2[2048] = '\0';

			for (int k = 0; k < 2048; k += 32)
			{
				char fcb[33];
				memcpy(fcb, buffer2 + k, 32);
				
				fcb[32] = '\0';

				if (!myStrCmp(fname, fcb))
				{
					KernelFS::indeks2 = idx2[0];
					KernelFS::positionInBuffer = k + 0x10;
					memcpy(retVal, buffer2 + k + 0x0c, 4);
					memcpy(sizeRet, buffer2 + k + 0x10, 4);
					return SUCCESS_CODE;
				}
			}
		}
	}
}
ClusterNo KernelFS::getNextCluster()
{
	EnterCriticalSection(&mutex);
	ClusterNo clustNum = 0;
	for (int i = 0; i < numOfBitVectorClusters*ClusterSize; i++)
	{
		unsigned short bitMask = 0x01;

		for (int j = 0; j < 8; j++, bitMask <<= 1)
		{
			clustNum++;
			if (bitVector[i] & bitMask)
			{
				bitVector[i] &= (~bitMask);
				KernelFS::numOfFreeClusters--;
				LeaveCriticalSection(&mutex);
				return clustNum;
			}
		}
	}
	LeaveCriticalSection(&mutex);
	return 0;
}

bool KernelFS::makeFCB(char* fname, ClusterNo clustNum)
{
	char buffer[2049];
	char buffer1[2049];
	char buffer2[2049];


	int retVal = partition->readCluster(numOfBitVectorClusters, buffer);
	if (!retVal)
		return false;
	buffer[2048] = '\0'; //indeksni blok prvog nivoa


	for (int i = 0; i < 2048; i += 4)
	{
		ClusterNo index1[1]; //indeks prvog nivoa
		memcpy(index1, buffer + i, 4);

		ClusterNo index2[1];

		retVal = partition->readCluster(index1[0], buffer1);
		if (!retVal)
			return false;
		buffer1[2048] = '\0'; //indeksni blok 2. nivoa

		for (int j = 0; j < 2048; j += 4)
		{
			memcpy(index2, buffer1 + j, 4);

			if (!index2[0])
			{
				index2[0] = getNextCluster();
				if (!index2[0])
					return false;

				memset(buffer2, 0x00, 2048);
				buffer2[2048] = '\0';
				
				if (writeEntry(fname,clustNum,buffer2) == false)
					return false;

				memcpy(buffer1 + j, index2, 4);
				retVal = partition->writeCluster(index2[0], buffer2);
				if (!retVal)
					return false;
				retVal = partition->writeCluster(index1[0], buffer1);
				if (!retVal)
					return false;

				KernelFS::indeks2 = index2[0];
				return true;
			}

			retVal = partition->readCluster(index2[0], buffer2);
			if (!retVal)
				return false;

			if (writeEntry(fname, clustNum, buffer2) == true)
			{
				retVal = partition->writeCluster(index2[0], buffer2);
				if (!retVal)
					return false;
				KernelFS::indeks2 = index2[0];
				return true;
			}
		}
	}
	return false;
}

bool KernelFS::writeEntry(char* fname, ClusterNo clustNum,char* cluster)
{
	
	for (int k = 0; k < 2048; k += 32)
	{
		char fcb[33];

		memcpy(fcb, cluster + k, 32);
		fcb[32] = '\0';

		char tester[33];
		memset(tester, 0x00, 32);
		tester[32] = '\0';

		if (!strcmp(tester, fcb))
		{
			char name[9];
			char extension[4];
			name[8] = '\0';
			extension[3] = '\0';

			unsigned int i = 0;

			for (; i < strlen(fname) && fname[i] != '.'; i++)
				name[i] = fname[i];

			for (unsigned int j = i; j < 8; j++)
				name[j] = ' ';

			int iter = 0;
			for (unsigned int j = i + 1; j < strlen(fname); j++)
			{
				extension[iter++] = fname[j];
			}

			for (; iter < 3; iter++)
				extension[iter] = ' ';

			ClusterNo clNum[1];
			clNum[0] = clustNum;

			memcpy(fcb, name, 8);
			memcpy(fcb + 8, extension, 3);
			memcpy(fcb + 0x0c, clNum, 4);
			memcpy(cluster + k, fcb, 32);

			KernelFS::positionInBuffer = k + 0x10;
			return true;
		}
	}

	return false;
}

bool KernelFS::deleteFromDisc(char* fname)
{
	char buffer[2049];
	char buffer1[2049];
	char buffer2[2049];


	int retVal = partition->readCluster(numOfBitVectorClusters, buffer);
	if (!retVal)
		return false;
	buffer[2048] = '\0'; //indeksni blok prvog nivoa


	for (int i = 0; i < 2048; i += 4)
	{
		ClusterNo index1[1]; //indeks prvog nivoa
		memcpy(index1, buffer + i, 4);

		ClusterNo index2[1];

		retVal = partition->readCluster(index1[0], buffer1);
		if (!retVal)
			return false;
		buffer1[2048] = '\0'; //indeksni blok 2. nivoa

		for (int j = 0; j < 2048; j += 4)
		{
			memcpy(index2, buffer1 + j, 4);

			if (!index2[0]) //mislim da treba obrisati ovaj if!!!
			{
				return false; //no more used clusters 
			}

			retVal = partition->readCluster(index2[0], buffer2);
			if (!retVal)
				return false;
			buffer2[2048] = '\0';

			for (int k = 0; k < 2048; k += 32)
			{
				char fcb[33];
				memcpy(fcb, buffer2 + k, 32);

				fcb[32] = '\0';


				if (!myStrCmp(fname,fcb)) {

					ClusterNo idx1[1];
					ClusterNo size[1];
					memcpy(idx1, fcb + 0x0c, 4);
					memcpy(size, fcb + 0x10, 4);

					if (size[0] > 0)
					{
						retVal = deleteData(idx1[0]);
						if (!retVal)
							return false;
					}

					memset(fcb, 0x00, 32);
					memcpy(buffer2 + k, fcb, 32);
					freeClust(idx1[0]);

					char tst[2049];
					tst[2048] = '\0';
					memset(tst, 0x00, 2048);

					if (!strcmp(tst, buffer2))
					{
						freeClust(index2[0]);
						memset(buffer1+j,0x00,4);
						retVal = partition->writeCluster(index1[0], buffer1);
						if (!retVal)
							return false;
					}
					else {
						retVal = partition->writeCluster(index2[0], buffer2);
						if (!retVal)
							return false;
					}
					
					return true;
				}
			}
		}
	}
	return false;
}

int KernelFS::deleteData(ClusterNo index1)
{
	char buffer[2049];
	buffer[2048] = '\0';
	char buffer1[2049];
	buffer1[2048] = '\0';


	int retVal = partition->readCluster(index1, buffer);
	if (!retVal)
		return 0;
	ClusterNo entry1[1];
	ClusterNo entry2[1];

	for (int i = 0; i < 2048; i += 4)
	{
		memcpy(entry1, buffer + i, 4);

		if (!entry1[0])
			break;

		freeClust(entry1[0]);

		retVal = partition->readCluster(entry1[0], buffer1);
		if (!retVal)
			return 0;
		for (int j = 0; j < 2048; j += 4)
		{
			memcpy(entry2, buffer1 + j, 4);

			if (entry2[0] == 0)
				break;

			freeClust(entry2[0]);
		}

	}
}
int KernelFS::myStrCmp(char* fname, char* fcb)
{
	char comparator[13];
	comparator[12] = '\0';

	int padder = 0, iter = 0;

	while (fname[iter] != '.')
		comparator[iter] = fname[iter++];

	unsigned int oldIter = iter + 1;
	while (iter < 8)
		comparator[iter++] = ' ';
	comparator[8] = '.';
	iter++;

	while (oldIter < strlen(fname))
		comparator[iter++] = fname[oldIter++];

	while (iter < 12)
		comparator[iter++] = ' ';

	char fullName[13];
	memcpy(fullName, fcb, 8);
	fullName[8] = '.';
	memcpy(fullName + 9, fcb + 8, 3); //ili uneto ime da se dopuni sa space ili ovom da se obrise
	fullName[12] = '\0';

	return strcmp(fullName, comparator);
}
void KernelFS::freeClust(ClusterNo cluster)
{
	EnterCriticalSection(&mutex);
	int idx = cluster / 8;
	int position = cluster % 8;

	unsigned short bitMask = 0x01;

	for (int i = 0; i < position; i++)
		bitMask <<= 1;

	if (!(bitVector[idx] & bitMask))
	{
		KernelFS::numOfFreeClusters++;
	}
	bitVector[idx] |= bitMask;
	LeaveCriticalSection(&mutex);
}

ClusterNo KernelFS::getNumOfFreeClust()
{
	return KernelFS::numOfFreeClusters;
}

char KernelFS::deleteFile(char* fname) { 

	if (fname == nullptr)
		return ERROR_CODE;


	EnterCriticalSection(&mutex);
	if (openFileTable.find((string)(fname+1)) != openFileTable.end())
	{
		LeaveCriticalSection(&mutex);
		return ERROR_CODE;
	}

	bool deleted = deleteFromDisc(fname+1);

	if (deleted == false)
	{
		LeaveCriticalSection(&mutex);
		return ERROR_CODE;
	}

	numOfFiles--;
	LeaveCriticalSection(&mutex);
	return SUCCESS_CODE;

};