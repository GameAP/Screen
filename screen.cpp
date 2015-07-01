#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <stdio.h>
#endif

#include <process.h>
#include <sstream>
#include <iostream>

#include <fstream>

#include <conio.h>
#include <string.h>
#include <tchar.h>
#pragma hdrstop

#include <map>

#include <jsoncpp/json/json.h>

#include "functions.h"

#define bzero(a) memset(a,0,sizeof(a)) //для сокращения писанины

int console_max_size = 4096;							// Размер консоли
std::map<std::string, std::string> console;

void show_help()
{
	std::cout << "**************************************\n";
	std::cout << "*     Welcome to GameAP Screen       *\n";
	std::cout << "**************************************\n\n";

	std::cout << "Program created by ET-NiK \n";
	std::cout << "Site: http://www.gameap.ru \n\n";

	std::cout << "Parameters\n";
	std::cout << "-t <type>	(start|get_console|send_command|kill)\n";
	std::cout << "-S <screen_name> name screen\n";
	std::cout << "-c <command>  command (example 'hlds.exe -game valve +ip 127.0.0.1 +port 27015 +map crossfire')\n\n";

	std::cout << "Examples:\n";
	std::cout << "screen.exe -t start -S hldm -c \"hlds.exe -game valve +ip 127.0.0.1 +port 27015 +map crossfire\"\n";
	std::cout << "screen.exe -t send_command -S hldm -c \"changelevel stalkyard\"\n";
	std::cout << "screen.exe -t get_console -S hldm \n";
	std::cout << "screen.exe -t kill -S hldm -c \n";
}

bool dirExists(const std::string& dirName_in)
{
  DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
  if (ftyp == INVALID_FILE_ATTRIBUTES)
    return false;  //something is wrong with your path!

  if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
    return true;   // this is a directory!

  return false;    // this is not a directory!
}

bool IsWinNT()  //проверка запуска под NT
{
	OSVERSIONINFO osv;
	osv.dwOSVersionInfoSize = sizeof(osv);
	GetVersionEx(&osv);
	return (osv.dwPlatformId == VER_PLATFORM_WIN32_NT);
}

BOOL IsProcessRunning(DWORD pid)
{
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    DWORD ret = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return ret == WAIT_TIMEOUT;
}

void ErrorMessage(char *str)  //вывод подробной информации об ошибке
{
	LPVOID msg;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // язык по умолчанию
		(LPTSTR) &msg,
		0,
		NULL
	);

	printf("%s: %s\n",str,msg);
	LocalFree(msg);
}

void console_append(std::string str_in, std::string name, int pid)
{
	int console_free = console_max_size-console[name].size();

	if (str_in.size() < console_free) {
		// Еще есть свободное место
		console[name].append(str_in);
	}
	else if (str_in.size() > console_free) {
		// Обрезаем console[name], чтобы было свободное место
		int size_requred = str_in.size() - console_free;
		console[name] = console[name].substr(size_requred, console_max_size);
		console[name].append(str_in);
	}
	else if (str_in.size() > console_max_size) {
		// Размер входящей строки больше console_max_size
		// Обрезаем входящие данные
		str_in = str_in.substr(str_in.size() - console_max_size, str_in.size());
		std::cout << "SIZE:" << str_in.size() << std::endl;
		console[name].append(str_in);
	}

	std::ofstream fout(".db/" + name + ".json", std::ios_base::out);

	Json::Value jwrite;
	jwrite["pid"]		= pid;
	jwrite["console"]	= console[name];
	jwrite["command"]	= "";

	fout << jwrite;
	fout.close();
}

void WriteStringToConsoleInput(HANDLE hInput, char *stringToWrie)
{
	char *pos, c;
	char keyCode;
	INPUT_RECORD ir;
	DWORD numberOfEventsWritten;

	pos = stringToWrie;
	c = *pos;
	while (c != 0)
	{
		if (c == '\n')
			c = '\r';
		keyCode = toupper(c);

		ir.EventType = KEY_EVENT;
		ir.Event.KeyEvent.bKeyDown = TRUE;
		ir.Event.KeyEvent.wRepeatCount = 1;
		ir.Event.KeyEvent.wVirtualKeyCode = keyCode;
		ir.Event.KeyEvent.wVirtualScanCode = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
		ir.Event.KeyEvent.uChar.UnicodeChar = (WCHAR)c;
		ir.Event.KeyEvent.dwControlKeyState = isupper(c) != 0 ? 0x80 : 0;

		if (!WriteConsoleInput(hInput, &ir, 1u, (LPDWORD)&numberOfEventsWritten))
			ErrorMessage("WriteConsoleInput");

		ir.Event.KeyEvent.bKeyDown = FALSE;

		if (!WriteConsoleInput(hInput, &ir, 1u, (LPDWORD)&numberOfEventsWritten))
			ErrorMessage("WriteConsoleInput");

		pos++;
		c = *pos;
	}
}

void run(std::string command, std::string sname, std::string directory)
{
	if (!dirExists(".db")) {
		exec("mkdir .db");
	}
	
	char buf[1024];           //буфер ввода/вывода

	STARTUPINFO si;
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;        //структура security для пайпов
	PROCESS_INFORMATION pi;

	HANDLE hOutput, hInput;
	HANDLE newstdin, newstdout, read_stdout, write_stdin;  //дескрипторы пайпов

	if (directory == "") {
		directory = "C:\\Windows\\System32\\";
	}

	if (IsWinNT()) {
		//инициализация security для Windows NT
		InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(&sd, true, NULL, false);
		sa.lpSecurityDescriptor = &sd;
	}
	else {
		sa.lpSecurityDescriptor = NULL;
	}

	hInput = GetStdHandle(STD_INPUT_HANDLE);
	hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;       //разрешаем наследование дескрипторов


	if (!CreatePipe(&newstdin, &write_stdin, &sa, 0)) {
		// Создаем пайп для stdin
		ErrorMessage("CreatePipe");
		_getch();
		return;
	}

	if (!CreatePipe(&read_stdout, &newstdout, &sa, 0)) {
		// Создаем пайп для stdout
		ErrorMessage("CreatePipe");
		_getch();
		CloseHandle(newstdin);
		CloseHandle(write_stdin);
		return;
	}

	GetStartupInfo(&si);	//создаем startupinfo для дочернего процесса

	/*

	Параметр dwFlags сообщает функции CreateProcess
	как именно надо создать процесс.

	STARTF_USESTDHANDLES управляет полями hStd*.
	STARTF_USESHOWWINDOW управляет полем wShowWindow.

	*/

	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	//si.dwFlags = STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = newstdout;
	si.hStdError = newstdout;   //подменяем дескрипторы для
	si.hStdInput = hInput;    // дочернего процесса

	//const size_t cSize = strlen(command.c_str()) + 1;
	wchar_t* szCmdline = new wchar_t[strlen(command.c_str()) + 1];
	mbstowcs(szCmdline, command.c_str(), strlen(command.c_str()) + 1);

	//directory = "C:\\servers\\demonand2007\\rust_ba5f26_28019\\";

	wchar_t* CurDir = new wchar_t[strlen(directory.c_str()) + 1];
	mbstowcs(CurDir, directory.c_str(), strlen(directory.c_str()) + 1);

	//создаем дочерний процесс

	//if (!CreateProcess(szCmdline, NULL, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
	if (!CreateProcess(NULL, szCmdline, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, CurDir, &si, &pi)) {
		ErrorMessage("CreateProcess");
		_getch();
		CloseHandle(newstdin);
		CloseHandle(newstdout);
		CloseHandle(read_stdout);
		CloseHandle(write_stdin);
		exit(0);
	}

	// std::cout << "Process started with pid=" << pi.dwProcessId << std::endl;

	unsigned long exit = 0;  //код завершения процесса
	unsigned long bread;   //кол-во прочитанных байт
	unsigned long avail;   //кол-во доступных байт

	bzero(buf);

	Json::Value jroot;	// Read
	Json::Reader jreader(Json::Features::strictMode());

	std::fstream fin;
	std::string json_in;

	FreeConsole();
	Sleep(100);
	AttachConsole(pi.dwProcessId);

	ShowWindow(GetConsoleWindow(), SW_HIDE);

	for (;;)      //основной цикл программы
	{
		Sleep(1000);

		GetExitCodeProcess(pi.hProcess, &exit); //пока дочерний процесс не закрыт
		
		if (exit != STILL_ACTIVE && exit != 4294967295) {
			break;
		}
		
		fin.open(".db/" + sname + ".json");

		json_in = "";

		if (fin) {
			for (std::string line; std::getline(fin, line); json_in = json_in + line);
			jreader.parse(json_in, jroot, false);
			fin.close();
			//std::cout << "FILE: " << json_in << std::endl;
		}

		PeekNamedPipe(read_stdout, buf, 1023, &bread, &avail, NULL);

		//Проверяем, есть ли данные для чтения в stdout
		if (bread != 0) {
			bzero(buf);

			if (avail > 1023) {
				while (bread >= 1023)
				{
					ReadFile(read_stdout, buf, 1023, &bread, NULL);  //читаем из пайпа stdout
					console_append((const char*)buf, sname, pi.dwProcessId);
					bzero(buf);
				}
			}
			else {
				ReadFile(read_stdout, buf, 1023, &bread, NULL);
				console_append((const char*)buf, sname, pi.dwProcessId);;
			}
		}

		if (jroot["command"].asString() != "") {
			char *send_command = new char[jroot["command"].asString().length() + 1];
			strcpy(send_command, jroot["command"].asCString());

			WriteStringToConsoleInput(hInput, send_command);
			WriteStringToConsoleInput(hInput, "\n");

			console_append("", sname, pi.dwProcessId);
		}
	}

	std::string file_name = ".db/" + sname + ".json";
	remove(&file_name[0]);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(newstdin);
	CloseHandle(newstdout);
	CloseHandle(read_stdout);
	CloseHandle(write_stdin);
}

void run(std::string command, std::string sname)
{
	run(command, sname, "C:\\Windows\\System32\\");
}

//----------------------------------------------------------------------

void main(int argc, char *argv[])
{

	std::string type		= "";
	std::string command		= "";
	std::string sname		= "";
	std::string directory	= "";

	for (int i = 0; i < argc - 1; i++) {
		if (std::string(argv[i]) == "-t") {
			// Тип
			type = argv[i + 1];
			i++;
		}
		else if (std::string(argv[i]) == "-S") {
			// Имя
			sname = argv[i + 1];
			i++;
		}
		else if (std::string(argv[i]) == "-c") {
			// Команда
			command = argv[i + 1];
			i++;
		}
		else if (std::string(argv[i]) == "-d") {
			// Рабочая директория
			directory = argv[i + 1];
			i++;
		}
		else {
			// ...
		}
	}

	if (type == "start") {
		if (command == "") {
			std::cout << "Empty command" << std::endl;
			exit(0);
		}

		if (sname == "") {
			std::cout << "Empty screen name" << std::endl;
			exit(0);
		}

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		std::string fork_command = "screen -t run -S " + sname + " -d \"" + directory + "\" -c \"" + command + "\"";

		// Test
		//std::string fork_command = "C:\\Windows\\system32\\notepad.exe";

		// std::cout << "Fork: " << fork_command << std::endl;

		wchar_t* szCmdline = new wchar_t[strlen(fork_command.c_str()) + 1];
		mbstowcs(szCmdline, fork_command.c_str(), strlen(fork_command.c_str()) + 1);

		// Start the child process. 
		if (!CreateProcess(NULL,   // No module name (use command line)
			szCmdline,        // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			0,              // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi)           // Pointer to PROCESS_INFORMATION structure
			)
		{
			printf("CreateProcess failed (%d).\n", GetLastError());
			return;
		}

		// Wait until child process exits.
		//WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

	}
	else if (type == "run") {
		// Запуск программы

		if (command == "") {
			std::cout << "Empty command" << std::endl;
			exit(0);
		}

		if (sname == "") {
			std::cout << "Empty screen name" << std::endl;
			exit(0);
		}

		run(command, sname, directory);
	}
	else if (type == "get_console") {
		// Получение содержимого

		if (sname == "") {
			std::cout << "Empty screen name" << std::endl;
			exit(0);
		}

		std::fstream fin;
		fin.open(".db/" + sname + ".json");

		Json::Value jroot;	// Read
		Json::Reader jreader(Json::Features::strictMode());

		std::string json_in;

		if (fin) {
			for (std::string line; std::getline(fin, line); json_in = json_in + line);
			jreader.parse(json_in, jroot, false);
			fin.close();
		}

		if (!IsProcessRunning(jroot["pid"].asInt())) {
			std::string file_name = ".db/" + sname + ".json";
			remove(&file_name[0]);
			exit(0);
		}

		std::cout << jroot["console"].asString() << std::endl;

	}
	else if (type == "send_command") {
		// Отправка команды

		if (command == "") {
			std::cout << "Empty command" << std::endl;
			exit(0);
		}

		if (sname == "") {
			std::cout << "Empty screen name" << std::endl;
			exit(0);
		}

		// Открытие json
		std::fstream fin(".db/" + sname + ".json");
		//fin.open(".db/" + sname + ".json");

		Json::Value jroot;	// Read
		Json::Reader jreader(Json::Features::strictMode());

		std::string json_in;

		if (fin) {
			for (std::string line; std::getline(fin, line); json_in = json_in + line);
			jreader.parse(json_in, jroot, false);
			fin.close();
		}

		if (jroot["pid"].asString() == "") {
			std::cout << "Unknown screen name" << std::endl;
			exit(0);
		}

		// Запись json
		std::ofstream fout(".db/" + sname + ".json", std::ios_base::out);

		Json::Value jwrite;
		jroot["command"] = command;

		fout << jroot;
		fout.close();
	}
	else if (type == "kill") {
		// Завершение процесса

		if (sname == "") {
			std::cout << "Empty screen name" << std::endl;
			exit(0);
		}

		std::fstream fin;
		fin.open(".db/" + sname + ".json");

		Json::Value jroot;	// Read
		Json::Reader jreader(Json::Features::strictMode());

		std::string json_in;

		if (fin) {
			for (std::string line; std::getline(fin, line); json_in = json_in + line);
			jreader.parse(json_in, jroot, false);
			fin.close();
		}

		if (jroot["pid"].asString() != "") {

			std::string cmd_kill_str = "taskkill /PID " + jroot["pid"].asString();
			wchar_t* cmd_kill = new wchar_t[strlen(cmd_kill_str.c_str()) + 1];
			mbstowcs(cmd_kill, cmd_kill_str.c_str(), strlen(cmd_kill_str.c_str()) + 1);

			::_tsystem(cmd_kill);

			//std::string file_name = ".db/" + sname + ".json";
			//remove(&file_name[0]);
			
		}
		else {
			std::cout << "Unknown screen name" << std::endl;
		}

	}
	else {
		show_help();
	}

}

//----------------------------EOF-----------------------------------
//------------------------------------------------------------------