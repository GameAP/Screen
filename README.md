# Screen
Analogue screen Linux for Windows

Parameters
======
-t <type>	(start|get_console|send_command|kill)
-S <screen_name> name screen
-c <command>  command (example 'hlds.exe -game valve +ip 127.0.0.1 +port 27015 +map crossfire')

Examples
======
screen.exe -t start -S hldm -c "hlds.exe -game valve +ip 127.0.0.1 +port 27015 +map crossfire"
screen.exe -t send_command -S hldm -c "changelevel stalkyard"
screen.exe -t get_console -S hldm 
screen.exe -t kill -S hldm -c 
