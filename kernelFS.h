#ifndef _KFS_H_
#define _KFS_H_
#include "fs.h"

#define bufferInit(buffer) char buffer[2049]; buffer[2048]='\0';

class KernelFS
{
public:
	~KernelFS();
	static char mount(Partition* partition); 
	static char unmount(); 
	static char format(); 
	static FileCnt readRootDir();
	static char doesExist(char* fname); 
	static File* open(char* fname, char mode);
	static char deleteFile(char* fname);

protected:

	KernelFS();
	friend class FS;
	friend class FCB;
	friend class KernelFile;
	static Partition* partition;

	static char ERROR_CODE;
	static char SUCCESS_CODE;

	static int numOfOpenedFiles();
	static void initBitVector();
	static char initRootDir();

	static std::map<std::string,FCB*> openFileTable;

	static ClusterNo numOfBitVectorClusters;
	static ClusterNo nextFreeCluster;
	static char *bitVector;

	static ClusterNo getNextCluster(); //return 0 if all clusters are allocated;
	static void freeClust(ClusterNo);
	static ClusterNo getNumOfFreeClust();
	static ClusterNo numOfFreeClusters;

	static ClusterNo numOfFiles;

	static char findOnDisc(char*);
	static bool makeFCB(char* fname, ClusterNo clustNum);
	static bool writeEntry(char* fname, ClusterNo clustNum,char*);
	static bool deleteFromDisc(char*);
	static int myStrCmp(char*,char*);
	static int deleteData(ClusterNo);
	static char getDataFromFileFCB(char* fname, ClusterNo*,BytesCnt*);

	static ClusterNo positionInBuffer; //sluze samo kao pomocne promenjive koje se iz metoda makeFCB getDataFromFile prosledjuju u FCB(..)
	static ClusterNo indeks2;


	static CONDITION_VARIABLE partNull;
	static CONDITION_VARIABLE openedFiles;
	static CONDITION_VARIABLE readerWriter;
	static CRITICAL_SECTION mutex;

	static long thread_counter;
	static bool flagOpener;
	
};
#endif