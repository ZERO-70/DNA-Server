#include<iostream>
#include<fstream>
#include<string>
using namespace std;
void main() {
	ifstream file("Human adenovirus 2, complete genome.fasta");
	if (!file.is_open()) {
		cout << "[unable to open file]" << endl;
		return;
	}
	string line;
	int nol = 5;
	while (nol && getline(file, line)) {
		cout << line << endl;
		nol--;
	}
}