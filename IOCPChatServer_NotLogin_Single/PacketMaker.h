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

	// ��Ŷ �Լ��� �̸��� �������ִ� �Լ�
	string generateMethodName(const string& packetType);

	void generateCode(const string& csvFile);

private:

};
