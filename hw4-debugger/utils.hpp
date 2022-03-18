#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

#define lambda_wrapper(func) [this](vector<string> args) { func(args); };

vector<string> tokenizer(string s) {
    stringstream ss(s);

    vector<string> tokens;
    string token;
    while (ss >> token) tokens.push_back(token);
    return tokens;
}

unsigned long long to_uul(string s) {
    int base = 10;
    if (!s.compare(0, 2, "0x")) base = 16;
    return stoull(s, NULL, base);
}

void die(string message) {
    cerr << "** ERROR: " << message << endl;
    exit(-1);
}