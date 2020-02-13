#include"testprimer.h"

using namespace std;

HANDLE nit1, nit2;
DWORD ThreadID;

HANDLE semMain = CreateSemaphore(NULL, 0, 32, NULL);
HANDLE sem12 = CreateSemaphore(NULL, 0, 32, NULL);
HANDLE sem21 = CreateSemaphore(NULL, 0, 32, NULL);
HANDLE mutex = CreateSemaphore(NULL, 1, 32, NULL);

Partition* partition;

char* ulazBuffer;
int ulazSize;

int main() {
	clock_t startTime, endTime;
	cout << "Pocetak testa!" << endl;
	startTime = clock();//pocni merenje vremena

	{//ucitavamo ulazni fajl u bafer, da bi nit 1 i 2 mogle paralelno da citaju
		FILE* f = fopen("ulaz.dat", "rb");
		if (f == 0) {
			cout << "GRESKA: Nije nadjen ulazni fajl 'ulaz.dat' u os domacinu!" << endl;
			system("PAUSE");
			return 0;//exit program
		}
		ulazBuffer = new char[32 * 1024 * 1024];//32MB
		ulazSize = fread(ulazBuffer, 1, 32 * 1024 * 1024, f);
		fclose(f);
	}

	nit1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)nit1run, NULL, 0, &ThreadID); //kreira i startuje niti
	nit2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)nit2run, NULL, 0, &ThreadID);

	for (int i = 0; i < 2; i++) wait(semMain);//cekamo da se niti zavrse
	delete[] ulazBuffer;
	endTime = clock();
	cout << "Kraj test primera!" << endl;
	cout << "Vreme izvrsavanja: " << ((double)(endTime - startTime) / ((double)CLOCKS_PER_SEC / 1000.0)) << "ms!" << endl;
	CloseHandle(mutex);
	CloseHandle(semMain);
	CloseHandle(sem12);
	CloseHandle(sem21);
	CloseHandle(nit1);
	CloseHandle(nit2);
	return 0;
}