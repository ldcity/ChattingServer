#include "PCH.h"
#include "PacketMaker.h"


// 메소드 이름 생성 함수
string PacketMaker::generateMethodName(const string& packetType)
{
	string methodName;
	size_t pos = packetType.find("RES");

	if (pos != string::npos) {
		methodName = packetType.substr(pos - 1); // 'Res' 직전 _ 부터
	}
	else {
		methodName = packetType;
	}

	// '_' 문자를 제거하고, 이후 첫 글자를 대문자로 변환
	transform(methodName.begin(), methodName.end(), methodName.begin(), ::tolower);
	pos = methodName.find("_");

	while (pos != string::npos) {
		methodName.erase(pos, 1);
		if (pos < methodName.size()) {
			methodName[pos] = toupper(methodName[pos]);
		}
		pos = methodName.find("_");
	}

	// 함수 이름 앞에 'mp'를 추가
	methodName = "mp" + methodName;

	return methodName;
}

void PacketMaker::generateCode(const string& csvFile)
{
	queue<PacketSpec> packetSpecs;

	ifstream file(csvFile);
	if (!file.is_open())
	{
		cout << "Failed to open CSV file : " << csvFile << endl;
		return;
	}

	string line;
	while (getline(file, line))
	{
		PacketSpec spec;
		istringstream iss(line);
		string token;

		getline(iss, token, ',');
		spec.packetType = token;

		while (getline(iss, token, ','))
		{
			if (token == "") continue;

			PacketField field;
			field.name = token;
			getline(iss, token, ',');
			field.type = token;
			spec.fields.push_back(field);
		}
		packetSpecs.push(spec);
	}

	file.close();

	packetSpecs.pop();

	// 하드코딩
	ofstream headerFile("Packet.h");
	headerFile << "#ifndef __PACKET_CLASS__" << endl;
	headerFile << "#define __PACKET_CLASS__" << endl << endl;

	ofstream sourceFile("Packet.cpp");
	sourceFile << "#include \"PCH.h\"" << endl;
	sourceFile << "#include \"Packet.h\"" << endl << endl;

	while (!packetSpecs.empty())
	{
		PacketSpec spec = packetSpecs.front();
		packetSpecs.pop();

		// 패킷 함수의 이름을 생성해주는 함수
		string methodName = generateMethodName(spec.packetType);

		headerFile << "void " << methodName << "(CPacket* packet";

		for (const auto& field : spec.fields) {
			if (field.type == "WCHAR" && field.name.find('[') != string::npos) {
				headerFile << ", WCHAR* " << field.name.substr(0, field.name.find('['));
			}
			else {
				headerFile << ", " << field.type << " " << field.name;
			}
		}

		headerFile << ");" << endl;

		sourceFile << "void " << methodName << "(CPacket* packet";

		for (const auto& field : spec.fields) {
			if (field.type == "WCHAR" && field.name.find('[') != string::npos) {
				string arrayName = field.name.substr(0, field.name.find('['));
				sourceFile << ", WCHAR* " << arrayName;
			}
			else {
				sourceFile << ", " << field.type << " " << field.name;
			}
		}

		sourceFile << ")" << endl;
		sourceFile << "{" << endl;
		sourceFile << "\tWORD type = " << spec.packetType << ";" << endl;
		bool flag = true;

		for (const auto& field : spec.fields) {
			if (field.type == "WCHAR" && field.name.find('[') != string::npos) {
				flag = true;
				string arrayName = field.name.substr(0, field.name.find('['));
				sourceFile << ";" << endl;
				sourceFile << "\tpacket->PutData((char*)" << arrayName << ", wcslen(" << arrayName << ") * sizeof(wchar_t));" << endl;
			}
			else {
				if (flag)
				{
					sourceFile << "\t*packet";
					flag = false;
				}
				if (field.name == "Type")
					sourceFile << " << " << "type";
				else
					sourceFile << " << " << field.name;
			}
		}
		sourceFile << ";" << endl;
		sourceFile << "}" << endl << endl;
	}

	headerFile << endl << "#endif" << endl;

	headerFile.close();
	sourceFile.close();
}
