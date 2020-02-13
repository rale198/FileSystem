#ifndef _FCB_H_
#define _FCB_H_

#include "kernelFS.h"
class FCB
{
private:

	ClusterNo index1[1];
	char *buffer;
	int threads;
	ClusterNo size;
	char mode;
	ClusterNo position;
	ClusterNo indeks2;

	CRITICAL_SECTION mutExcl;

	FCB(ClusterNo cl, char* buffer1, char m,ClusterNo pos,ClusterNo idx2,BytesCnt sz) :buffer(buffer1),mode(m),size(sz),threads(0),position(pos),indeks2(idx2){
		index1[0] = cl;
		buffer[2048] = '\0';
		InitializeCriticalSection(&mutExcl);
	}

	~FCB()
	{
		delete[] buffer;
		DeleteCriticalSection(&mutExcl);
	}

	void incThread()
	{
		EnterCriticalSection(&mutExcl);
		this->threads++;
		LeaveCriticalSection(&mutExcl);
	}

	void decThread()
	{
		EnterCriticalSection(&mutExcl);
		this->threads--;
		LeaveCriticalSection(&mutExcl);
	}

	friend class KernelFS;
	friend class FS;
	friend class KernelFile;

};
#endif