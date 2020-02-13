#include"testprimer.h"

static char threadName[] = "Nit2";

DWORD WINAPI nit2run() {
	wait(sem12);	//ceka nit1
	signal(sem21); // signalizira nit1
	{
		File* src, * dst;
		char filepath[] = "/fajl1.dat";
		while ((src = FS::open(filepath, 'r')) == 0) {
			wait(mutex); cout << threadName << ":Neuspesno otvoren fajl '" << filepath << "'" << endl; signal(mutex);
			Sleep(1); // Ceka 1 milisekundu
		}
		wait(mutex); cout << threadName << ": Otvoren fajl '" << filepath << "'" << endl; signal(mutex);
		char filepath1[] = "/fajl2.dat";
		dst = FS::open(filepath1, 'w');
		wait(mutex); cout << threadName << ": Otvoren fajl '" << filepath1 << "'" << endl; signal(mutex);
		char c;
		while (!src->eof()) {
			src->read(1, &c);
			dst->write(1, &c);
		}
		wait(mutex); cout << threadName << ": Prepisan fajl '" << filepath << "' u '" << filepath1 << "'" << endl; signal(mutex);
		delete dst;
		wait(mutex); cout << threadName << ": Zatvoren fajl '" << filepath1 << "'" << endl; signal(mutex);
		delete src;
		wait(mutex); cout << threadName << ": Zatvoren fajl '" << filepath << "'" << endl; signal(mutex);

	}
	wait(mutex); cout << threadName << ": wait 1" << endl; signal(mutex);
	wait(sem12); // ceka nit 1	 
	{
		wait(mutex); cout << threadName << ": Broj fajlova na disku je " << FS::readRootDir() << endl; signal(mutex);
	}

	{
		char filepath[] = "/fajl2.dat";
		File* f = FS::open(filepath, 'r');
		wait(mutex); cout << threadName << ": Otvoren fajl " << filepath << "" << endl; signal(mutex);
		delete f;
		wait(mutex); cout << threadName << ": Zatvoren fajl " << filepath << "" << endl; signal(mutex);
	}

	{
		char filepath[] = "/fajl2.dat";
		File* f = FS::open(filepath, 'r');
		wait(mutex); cout << threadName << ": Otvoren fajl " << filepath << "" << endl; signal(mutex);
		ofstream fout("izlaz1.dat", ios::out | ios::binary);
		char* buff = new char[f->getFileSize()];
		f->read(f->getFileSize(), buff);
		fout.write(buff, f->getFileSize());
		wait(mutex); cout << threadName << ": Upisan '" << filepath << "' u fajl os domacina 'izlaz1.dat'" << endl; signal(mutex);
		delete[] buff;
		fout.close();
		delete f;
		wait(mutex); cout << threadName << ": Zatvoren fajl " << filepath << "" << endl; signal(mutex);
	}

	{
		char copied_filepath[] = "/fajll5.dat";
		File* copy = FS::open(copied_filepath, 'r');
		BytesCnt size = copy->getFileSize();
		wait(mutex); cout << threadName << ": Otvoren fajl '" << copied_filepath << "' i dohvacena velicina" << endl; signal(mutex);
		delete copy;
		wait(mutex); cout << threadName << ": Zatvoren fajl '" << copied_filepath << "'" << endl; signal(mutex);
		File* src, * dst;
		char filepath[] = "/fajl1.dat";
		src = FS::open(filepath, 'r');
		src->seek(0);//pozicionira se na pola fajla
		wait(mutex); cout << threadName << ": Otvoren fajl '" << filepath << "' i pozicionirani smo na polovini" << endl; signal(mutex);
		char filepath1[] = "/fajl25.dat";
		dst = FS::open(filepath1, 'w');
		wait(mutex); cout << threadName << ": Otvoren fajl '" << filepath1 << "'" << endl; signal(mutex);
		char c; BytesCnt cnt = src->getFileSize() - size;
		while (!src->eof() && cnt-- > 0) {
			src->read(1, &c);
			dst->write(1, &c);
		}
		wait(mutex); cout << threadName << ": Prepisana druga polovina '" << filepath << "' u '" << filepath1 << "'" << endl; signal(mutex);
		delete dst;
		wait(mutex); cout << threadName << ": Zatvoren fajl '" << filepath1 << "'" << endl; signal(mutex);
		delete src;
		wait(mutex); cout << threadName << ": Zatvoren fajl '" << filepath << "'" << endl; signal(mutex);
	}

	signal(sem21); // signalizira niti 1
	wait(mutex); cout << threadName << ": wait 1" << endl; signal(mutex);
	wait(sem12); // ceka nit 2	

	{
		char filepath[] = "/fajl25.dat";
		File* f = FS::open(filepath, 'r');
		wait(mutex); cout << threadName << ": Otvoren fajl " << filepath << "" << endl; signal(mutex);
		ofstream fout("izlaz2.dat", ios::out | ios::binary);
		char* buff = new char[f->getFileSize()];
		f->read(f->getFileSize(), buff);
		fout.write(buff, f->getFileSize());
		wait(mutex); cout << threadName << ": Upisan '" << filepath << "' u fajl os domacina 'izlaz1.dat'" << endl; signal(mutex);
		delete[] buff;
		fout.close();
		delete f;
		wait(mutex); cout << threadName << ": Zatvoren fajl " << filepath << "" << endl; signal(mutex);
	}

	{
		FS::unmount();
		wait(mutex); cout << threadName << ": Demontirana particija p1" << endl; signal(mutex);
	}


	wait(mutex); cout << threadName << ": Zavrsena!" << endl; signal(mutex);
	signal(semMain);
	return 0;
}