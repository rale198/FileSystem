#include <iostream>
#include <thread>
using namespace std;

int main() {
	
	thread vuki([]() {std::cout << "Hi vuki, thank you for you help!"; }); 
	vuki.join();
	return 0;
}