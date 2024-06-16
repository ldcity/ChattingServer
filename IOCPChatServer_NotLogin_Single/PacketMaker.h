#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <algorithm>

using namespace std;

class PacketMaker
{
public:
	struct PacketField
	{
		string name;
		string type;
	};

	struct PacketSpec
	{
		string packetType;
		vector<PacketField> fields;
	};

public:
	PacketMaker()
	{

	}

	~PacketMaker()
	{

	}

	// 패킷 함수의 이름을 생성해주는 함수
	string generateMethodName(const string& packetType);

	void generateCode(const string& csvFile);

private:

};
