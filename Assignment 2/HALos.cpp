//
// HALos.cpp
//
// Copyright (c) 2015 Robert J. Hilderman.
// All rights reserved.
//

#include "HALos.h"

int main ()
{
    Initialize ();
    HALos ();

    return 0;
}

void HALos ()
{
    string type;
    string command;
    string arguments [MAX_COMMAND_LINE_ARGUMENTS];
    int i;
    bool okToScheduleNextProcess = true;

    do
    {
        BlockSignals ();
        if (commandToExecuteFromHALshell)
        {
            commandToExecuteFromHALshell = 0;
            command = GetMessageFromHALshell (arguments, type);
            HandleCommand (command, arguments, type);
        }
        else if (messageFromHAL9000)
        {
            messageFromHAL9000 = 0;
            GetMessageFromHAL9000 ();
            if (messageType == "PROCESS_DONE")
            {
                HandleFinishedProcess (okToScheduleNextProcess);
            }
            else if (messageType == "INTERRUPT")
            {
                HandleHAL9000Interrupt (okToScheduleNextProcess);
            }
            else if (messageType == "SYSTEM_CALL")
            {
                HandleSystemCall (okToScheduleNextProcess);
            }
        }
        else if (messageFromHALkeyboardDriver)
        {
            messageFromHALkeyboardDriver = 0;
            GetMessageFromHALkeyboardDriver ();
            HandleHALkeyboardDriverInterrupt ();
        }
        else if (messageFromHALdisplayDriver)
        {
            messageFromHALdisplayDriver = 0;
            GetMessageFromHALdisplayDriver ();
            HandleHALdisplayDriverInterrupt ();
        }
        else if (messageFromHALdiskDriver)
        {
            messageFromHALdiskDriver = 0;
            GetMessageFromHALdiskDriver ();
            HandleHALdiskDriverInterrupt ();
        }
        UnblockSignals ();
    } while (1);

    return;
}

int GetClockTicks()
{
    int lock;
    struct flock key;
    int clockTicks;

	ifstream clockFile;
	clockFile.open("HAL9000clock");
	if(!clockFile)
	{
		cout << "No clock ticks retrieved; unable to open HAL9000clock.";
		exit (1);
	}

    key.l_type = F_WRLCK;
    key.l_whence = SEEK_SET;
    key.l_start = 0;
    key.l_len = 0;
    key.l_pid = getpid ();
    lock = open ("HAL9000clockLock", O_RDONLY);
    fcntl (lock, F_SETLKW, &key);

	clockFile >> clockTicks;
	clockFile.close();

    key.l_type = F_UNLCK;
    fcntl (lock, F_SETLK, &key);
    close (lock);

    return clockTicks;
}

string GetMessageFromHALshell (string arguments [], string& type)
{
    ifstream commandLineFile;
    string command;
    int i;

    for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
    {
        arguments [i] = "";
    }

    commandLineFile.open ("HALshellToHALos");
    if (!commandLineFile)
    {
        cout << "HALos: connection to HALshell failed" << endl;
        exit (1);
    }
    if (getline (commandLineFile, command))
    {
        i = 0;
        while (getline (commandLineFile, arguments [i]))
        {
            i ++;
        }
    }
    else
    {
        cout << "HALos: command received from HALshell corrupted" << endl;
        exit (1);
    }
    commandLineFile.close ();

    type = "FOREGROUND_PROCESS";
    if (i > 0)
    {
        i --;
        if (arguments [i] == "-")
        {
            type = "BACKGROUND_PROCESS";
            arguments [i] = "";
        }
    }

    return command;
}

void HandleCommand (string command, string arguments [], string type)
{
    int i;
    processDescriptor process;
    string result;

    if (command == "shutdown" ||
        command == "restart")
    {
        ShutdownAndRestart (command, arguments);
        // never returns!
    }
    else if (command == "cull")
    {
        Cull (command, arguments);
        return;
    }

    result = CreateProcessImage (nextPid, command);
    if (result == "ok")
    {
        if (type == "BACKGROUND_PROCESS")
        {
            SendReturnStatusToHALshell (itos (nextPid), "", result, "");
        }
        if (cpuProcess.pid.length () == 0)
        {
            cpuProcess.pid = itos (nextPid);
            cpuProcess.type = type;
            cpuProcess.status = "NEW_PROCESS";
            cpuProcess.command = command;
            cpuProcess.queueNo = 0;
            cpuProcess.contextSwitches =
                cpuSchedulingPolicies[0].contextSwitchesUntilMoveDown;
            cpuProcess.direction = "DOWN";
            for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
            {
                cpuProcess.arguments [i] = arguments [i];
                if (arguments[i].substr(0, 9) == "priority=")
                {
                    cpuProcess.priority = atoi(arguments[i].substr(10).c_str());
                }
            }
            cpuProcess.returnValue = "";
            SendMessageToHAL9000 (cpuProcess);
        }
        else
        {
            process.pid = itos (nextPid);
            process.type = type;
            process.status = "NEW_PROCESS";
            process.command = command;
            process.queueNo = 0;
            process.contextSwitches =
                cpuSchedulingPolicies[0].contextSwitchesUntilMoveDown;
            process.direction = "DOWN";
            for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
            {
                process.arguments [0] = arguments [i];
                if (arguments[i].substr(0, 9) == "priority=")
                {
                    cpuProcess.priority = atoi(arguments[i].substr(10).c_str());
                }
            }
            cout << process.pid << " in queue " << process.queueNo << endl;
            process.returnValue = "";
            readyQueue.Enqueue (process);
        }
    }
    else
    {
        SendReturnStatusToHALshell (itos (nextPid), "", result, "");
    }
    nextPid ++;

    return;
}

void HandleFinishedProcess (bool okToScheduleNextProcess)
{
    if (cpuProcess.type == "FOREGROUND_PROCESS")
    {
        remove ((cpuProcess.pid + "_backingstore").c_str ());
	SendReturnStatusToHALshell (parameter1, parameter2, parameter3, parameter4);
    }
    if (readyQueue.IsEmpty ())
    {
        cpuProcess = nullProcess;
    }
    else
    {
        if (okToScheduleNextProcess)
        {
            cpuProcess = readyQueue.Dequeue ();
            SendMessageToHAL9000 (cpuProcess);
        }
        else
        {
            cpuProcess = nullProcess;
        }
    }

    return;
}

void HandleHAL9000Interrupt (bool okToScheduleNextProcess)
{
cout << parameter1 << endl;
    if (parameter1 == "QUANTUM_EXPIRED")
    {
        cpuProcess.systemCallFileIndex = -1;
        cpuProcess.systemCall = "";
        cpuProcess.systemCallParameter1 = "";
        cpuProcess.systemCallParameter2 = "";
        cpuProcess.systemCallParameter3 = "";
        cpuProcess.systemCallBuffer = "";
        cpuProcess.systemCallResult = "";
        cpuProcess.status = "CONTINUE_EXECUTING_PROCESS";
        readyQueue.Enqueue (cpuProcess);
        if (okToScheduleNextProcess)
        {
            cpuProcess = readyQueue.Dequeue ();
            SendMessageToHAL9000 (cpuProcess);
        }
        else
        {
            cpuProcess = nullProcess;
        }
    }

    return;
}

void HandleHALkeyboardDriverInterrupt ()
{
    keyboardProcess.systemCallFileIndex = -1;
    keyboardProcess.systemCall = "";
    keyboardProcess.systemCallParameter1 = "";
    keyboardProcess.systemCallParameter2 = "";
    keyboardProcess.systemCallParameter3 = "";
    keyboardProcess.systemCallBuffer = parameter3;
    keyboardProcess.systemCallResult = parameter4;
    if (cpuProcess.pid.length () == 0)
    {
        cpuProcess = keyboardProcess;
        cpuProcess = readyQueue.SetQueueNo(cpuProcess);
        SendMessageToHAL9000 (cpuProcess);
    }
    else
    {
        readyQueue.Enqueue (keyboardProcess);
    }

    if (keyboardQueue.IsEmpty ())
    {
        keyboardProcess = nullProcess;
    }
    else
    {
        keyboardProcess = keyboardQueue.Dequeue ();
        SendMessageToHALkeyboardDriver (keyboardProcess.pid, keyboardProcess.systemCall);
    }

    return;
}

void HandleHALdisplayDriverInterrupt ()
{
    displayProcess.systemCallFileIndex = -1;
    displayProcess.systemCall = "";
    displayProcess.systemCallParameter1 = "";
    displayProcess.systemCallParameter2 = "";
    displayProcess.systemCallParameter3 = "";
    displayProcess.systemCallBuffer = "";
    displayProcess.systemCallResult = "";
    if (cpuProcess.pid.length () == 0)
    {
        cpuProcess = displayProcess;
        cpuProcess = readyQueue.SetQueueNo(cpuProcess);
        SendMessageToHAL9000 (cpuProcess);
    }
    else
    {
        readyQueue.Enqueue (displayProcess);
    }

    if (displayQueue.IsEmpty ())
    {
        displayProcess = nullProcess;
    }
    else
    {
        displayProcess = displayQueue.Dequeue ();
        SendMessageToHALdisplayDriver (displayProcess.pid, displayProcess.systemCall,
                                       displayProcess.systemCallParameter1);
    }

    return;
}

void HandleHALdiskDriverInterrupt ()
{
    int pid;
    fileDescriptor file;

    if (diskProcess.systemCall == "OPEN")
    {
        if (parameter5 == "FILE_OPEN_OK")
        {
            if (!diskProcess.fileTable.Find (diskProcess.systemCallParameter1,
                                             diskProcess.systemCallParameter2))
            {  
                diskProcess.fileTable.Insert (diskProcess.systemCallParameter1,
                                              diskProcess.systemCallParameter2,
                                              diskProcess.systemCallParameter3);
            }
            else 
            {
                parameter5 = "FILE_ALREADY_OPEN (disk driver interrupt)";
            }
        }
        diskProcess.systemCallFileIndex = -1;
        diskProcess.systemCall = "";
        diskProcess.systemCallParameter1 = "";
        diskProcess.systemCallParameter2 = "";
        diskProcess.systemCallParameter3 = "";
        diskProcess.systemCallBuffer = "";
        diskProcess.systemCallResult = parameter5;
    }
    else if (diskProcess.systemCall == "READ")
    {
        diskProcess.fileTable.SetP (diskProcess.systemCallFileIndex);
        diskProcess.fileTable.Write (parameter3);
        diskProcess.systemCallFileIndex = -1;
        diskProcess.systemCall = "";
        diskProcess.systemCallParameter1 = "";
        diskProcess.systemCallParameter2 = "";
        diskProcess.systemCallParameter3 = "";
        diskProcess.systemCallBuffer = parameter4;
        diskProcess.systemCallResult = parameter5;
    }
    else if (diskProcess.systemCall == "WRITE" || diskProcess.systemCall == "NEWLINE")
    {
        diskProcess.systemCallFileIndex = -1;
        diskProcess.systemCall = "";
        diskProcess.systemCallParameter1 = "";
        diskProcess.systemCallParameter2 = "";
        diskProcess.systemCallParameter3 = "";
        diskProcess.systemCallBuffer = "";
        diskProcess.systemCallResult = parameter5;
    }
    if (cpuProcess.pid.length () == 0)
    {
        cpuProcess = diskProcess;
        cpuProcess = readyQueue.SetQueueNo(cpuProcess);
        SendMessageToHAL9000 (cpuProcess);
    }
    else
    {
        readyQueue.Enqueue (diskProcess);
    }

    if (diskQueue.IsEmpty ())
    {
        diskProcess = nullProcess;
    }
    else
    {
        diskProcess = diskQueue.Dequeue ();
        if (diskProcess.systemCallFileIndex == -1)
        {
            SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                        diskProcess.systemCallParameter1,
                                        diskProcess.systemCallParameter3, "", "");
        }
        else
        {
            diskProcess.fileTable.SetP (diskProcess.systemCallFileIndex);
            file = diskProcess.fileTable.Read ();
            if (file.mode == "input")
            {
                SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                            file.name, file.mode, file.markerPosition, "");
            }
            else
            {
                SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                            file.name, file.mode, "", diskProcess.systemCallParameter1);
            }
        }
    }

    return;
}

void HandleSystemCall (bool okToScheduleNextProcess)
{
    processDescriptor process;
    fileDescriptor file;

    cpuProcess.status = "CONTINUE_EXECUTING_PROCESS";
    process = cpuProcess;

    if (readyQueue.IsEmpty ())
    {
        cpuProcess = nullProcess;
    }
    else
    {
        if (okToScheduleNextProcess)
        {
            cpuProcess = readyQueue.Dequeue ();
            SendMessageToHAL9000 (cpuProcess);
        }
        else
        {
            cpuProcess = nullProcess;
        }
    }

    if (parameter1 == "OPEN")
    {
        process.systemCallFileIndex = -1;
        process.systemCall = parameter1;
        process.systemCallParameter1 = parameter3;
        process.systemCallParameter2 = "";
        process.systemCallParameter3 = "";
        process.systemCallBuffer = "";
        process.systemCallResult = "";
        if (parameter3 == "keyboard" || parameter3 == "display")
        {
            process.systemCallParameter2 = parameter5;
            if (okToScheduleNextProcess)
            {
                if (cpuProcess.pid.length () == 0)
                {
                    cpuProcess = process;
                    cpuProcess = readyQueue.SetQueueNo(cpuProcess);
                    SendMessageToHAL9000 (cpuProcess);
                }
                else
                {
                    readyQueue.Enqueue (process);
                }
            }
            else
            {
                readyQueue.Enqueue (process);
            }
        }
        else // (parameter3 == some other file name)
        {
            process.systemCallParameter2 = parameter4;
            process.systemCallParameter3 = parameter5;
            if (diskProcess.pid.length () == 0)
            {
                diskProcess = process;
                SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                            diskProcess.systemCallParameter1,
                                            diskProcess.systemCallParameter3, "", "");
            }
            else
            {
                diskQueue.Enqueue (process);
            }
        }
    }
    else if (parameter1 == "READ")
    {
        if (parameter3 == "keyboard")
        {
            if (keyboardProcess.pid.length () == 0)
            {
                process.systemCall = parameter1;
                keyboardProcess = process;
                SendMessageToHALkeyboardDriver (keyboardProcess.pid, parameter1);
            }
            else
            {
                process.fileTable.Find ("keyboard", "");
                process.systemCallFileIndex = process.fileTable.GetP ();
                process.systemCall = parameter1;
                process.systemCallParameter1 = "";
                process.systemCallParameter2 = "";
                process.systemCallParameter3 = "";
                process.systemCallBuffer = "";
                process.systemCallResult = "";
                keyboardQueue.Enqueue (process);
            }
        }
        else // (parameter3 == some other file name)
        {
            process.fileTable.Find (parameter3, parameter4);
            file = process.fileTable.Read ();
            if (diskProcess.pid.length () == 0)
            {
                process.systemCallFileIndex = process.fileTable.GetP ();
                process.systemCall = parameter1;
                diskProcess = process;
                SendMessageToHALdiskDriver (diskProcess.pid, parameter1, parameter3, "input",
                                            file.markerPosition, "");
            }
            else
            {
                process.systemCallFileIndex = process.fileTable.GetP ();
                process.systemCall = parameter1;
                process.systemCallParameter1 = "";
                process.systemCallParameter2 = "";
                process.systemCallParameter3 = "";
                process.systemCallBuffer = "";
                process.systemCallResult = "";
                diskQueue.Enqueue (process);
            }
        }
    }
    else if (parameter1 == "WRITE" || parameter1 == "NEWLINE")
    {
        if (parameter3 == "display")
        {
            if (displayProcess.pid.length () == 0)
            {
                process.systemCall = parameter1;
                displayProcess = process;
                SendMessageToHALdisplayDriver (displayProcess.pid, parameter1, parameter5);
            }
            else
            {
                process.fileTable.Find ("display", "");
                process.systemCallFileIndex = process.fileTable.GetP ();
                process.systemCall = parameter1;
                process.systemCallParameter1 = parameter5;
                process.systemCallParameter2 = "";
                process.systemCallParameter3 = "";
                process.systemCallBuffer = "";
                process.systemCallResult = "";
                displayQueue.Enqueue (process);
            }
        }
        else // (parameter3 == some file name)
        {
            process.fileTable.Find (parameter3, parameter4);
            file = process.fileTable.Read ();
            if (diskProcess.pid.length () == 0)
            {
                process.systemCallFileIndex = process.fileTable.GetP ();
                process.systemCall = parameter1;
                diskProcess = process;
                SendMessageToHALdiskDriver (diskProcess.pid, parameter1, parameter3, "output", "", parameter5);
            }
            else
            {
                process.systemCallFileIndex = process.fileTable.GetP ();
                process.systemCall = parameter1;
                process.systemCallParameter1 = parameter5;
                process.systemCallParameter2 = "";
                process.systemCallParameter3 = "";
                process.systemCallBuffer = "";
                process.systemCallResult = "";
                diskQueue.Enqueue (process);
            }
        }
    }
    else if (parameter1 == "CLOSE")
    {
        process.fileTable.Find (parameter3, parameter4);
        process.fileTable.Delete ();
        process.systemCallFileIndex = -1;
        process.systemCall = "";
        process.systemCallParameter1 = "";
        process.systemCallParameter2 = "";
        process.systemCallParameter3 = "";
        process.systemCallBuffer = "";
        process.systemCallResult = "";
        if (okToScheduleNextProcess)
        {
            if (cpuProcess.pid.length () == 0)
            {
                cpuProcess = process;
                cpuProcess = readyQueue.SetQueueNo(cpuProcess);
                SendMessageToHAL9000 (cpuProcess);
            }
            else
            {
                readyQueue.Enqueue (process);
            }
        }
        else
        {
            readyQueue.Enqueue (process);
        }
    }

    return;
}

void SendReturnStatusToHALshell (string pid, string returnValue, string message, string returnOther)
{
    ofstream returnStatusFile;
    union sigval dummyValue;
   
    returnStatusFile.open ("HALosToHALshell");
    if (!returnStatusFile)
    {
        cout << "HALos: unable to initialize return status buffer" << endl;
        exit (1);
    }

    returnStatusFile << pid << endl;
    returnStatusFile << returnValue << endl;
    returnStatusFile << message << endl;
    returnStatusFile << returnOther << endl;
    returnStatusFile.close ();

    if (sigqueue (HALshellPid, SIGRTMIN, dummyValue) == -1)
    {
        cout << "HALos: return code signal not sent to HALshell" << endl;
        exit (1);
    }

    return;
}

void GetMessageFromHAL9000 ()
{
    static int seqNo = 0;
    static string fileNamePrefix = "HAL9000ToHALos_";
    static string fileName;
    ifstream hal9000MessageFile;

    if (seqNo > 0)
    {
        remove (fileName.c_str ());
    }

    seqNo ++;
    fileName = fileNamePrefix + itos (seqNo);
    hal9000MessageFile.open (fileName.c_str ());
    if (!hal9000MessageFile)
    {
        cout << "HALos: connection to HAL9000 failed" << endl;
        exit (1);
    }

    getline (hal9000MessageFile, messageType);
    getline (hal9000MessageFile, parameter1);
    getline (hal9000MessageFile, parameter2);
    getline (hal9000MessageFile, parameter3);
    getline (hal9000MessageFile, parameter4);
    getline (hal9000MessageFile, parameter5);

    if (!hal9000MessageFile)
    {
        cout << "HALos: message not received from HAL9000" << endl;
        exit (1);
    }

    hal9000MessageFile.close ();

    if (seqNo == INT_MAX)
    {
        seqNo = 0;
    }

    return;
}

void GetMessageFromHALkeyboardDriver ()
{
    ifstream halKeyboardDriverMessageFile;

    halKeyboardDriverMessageFile.open ("HALkeyboardDriverToHALos");
    if (!halKeyboardDriverMessageFile)
    {
        cout << "HALos: connection to HAL keyboard driver failed" << endl;
        exit (1);
    }

    getline (halKeyboardDriverMessageFile, messageType);
    getline (halKeyboardDriverMessageFile, parameter1);
    getline (halKeyboardDriverMessageFile, parameter2);
    getline (halKeyboardDriverMessageFile, parameter3);
    getline (halKeyboardDriverMessageFile, parameter4);

    if (!halKeyboardDriverMessageFile)
    {
        cout << "HALos: message not received from HAL keyboard driver" << endl;
        exit (1);
    }

    halKeyboardDriverMessageFile.close ();

    return;
}

void GetMessageFromHALdisplayDriver ()
{
    ifstream halDisplayDriverMessageFile;

    halDisplayDriverMessageFile.open ("HALdisplayDriverToHALos");
    if (!halDisplayDriverMessageFile)
    {
        cout << "HALos: connection to HAL display driver failed" << endl;
        exit (1);
    }

    getline (halDisplayDriverMessageFile, messageType);
    getline (halDisplayDriverMessageFile, parameter1);
    getline (halDisplayDriverMessageFile, parameter2);

    if (!halDisplayDriverMessageFile)
    {
        cout << "HALos: message not received from HAL display driver" << endl;
        exit (1);
    }

    halDisplayDriverMessageFile.close ();

    return;
}

void GetMessageFromHALdiskDriver ()
{
    static int seqNo = 0;
    static string fileNamePrefix = "HALdiskDriverToHALos_";
    static string fileName;
    ifstream halDiskDriverMessageFile;

    if (seqNo > 0)
    {
        remove (fileName.c_str ());
    }

    seqNo ++;
    fileName = fileNamePrefix + itos (seqNo);
    halDiskDriverMessageFile.open (fileName.c_str ());
    if (!halDiskDriverMessageFile)
    {
        cout << "HALos: connection to HAL disk driver failed" << endl;
        exit (1);
    }

    getline (halDiskDriverMessageFile, messageType);
    getline (halDiskDriverMessageFile, parameter1);
    getline (halDiskDriverMessageFile, parameter2);
    getline (halDiskDriverMessageFile, parameter3);
    getline (halDiskDriverMessageFile, parameter4);
    getline (halDiskDriverMessageFile, parameter5);

    if (!halDiskDriverMessageFile)
    {
        cout << "HALos: message not received from HAL disk driver" << endl;
        exit (1);
    }

    halDiskDriverMessageFile.close ();

    if (seqNo == INT_MAX)
    {
        seqNo = 0;
    }

    return;
}

string CreateProcessImage (int pid, string command)
{
    ifstream programSourceCodeFile;
    int programTextStartAddress;
    int currentProgramTextAddress;
    int segmentSize;
    string symbol;
    string value;
    memoryCell contents;
    bool mainSeen = false;
    bool functionSeen = false;

    GetProcessImageTemplate ();

    programSourceCodeFile.open (command.c_str ());
    if (!programSourceCodeFile)
    {
        command = command + ".hal";
        programSourceCodeFile.open (command.c_str ());
        if (!programSourceCodeFile)
        {
            return ("command not found");
        }
    }

    SetKernelVariableValue ("PID", itos (pid));
    SetKernelVariableValue ("PROGRAM_NAME", command);
    programTextStartAddress = GetMemorySegmentBoundary ("PROGRAM_TEXT_START_ADDRESS", segmentSize);
    SetKernelVariableValue ("START_ADDRESS", itos (programTextStartAddress));
    ram.Clear (programTextStartAddress);
    ram.SetP (programTextStartAddress);

    programSourceCodeFile >> symbol;
    while (programSourceCodeFile)
    {
        if (ram.GetP () == programTextStartAddress + segmentSize)
        {
            return ("size of program source code exceeds " + itos (segmentSize) + " lines");
        }
        programSourceCodeFile.ignore (256, ':');
        getline (programSourceCodeFile, value);
        if (symbol != "comment")
        {
            if (symbol == "function" ||
                symbol == "label")
            {
                if (symbol == "function")
                {
                    functionSeen = true;
                }
                if (value == "main")
                {
                    mainSeen = true;
                }
                currentProgramTextAddress = ram.GetP ();
                contents.symbol = symbol;
                contents.value = value;
                AllocateGlobalSymbol (contents, currentProgramTextAddress);
                ram.SetP (currentProgramTextAddress);
            }
            else if (symbol == "variable" ||
                     symbol == "constant" ||
                     symbol == "file")
            {
                if (!functionSeen)
                {
                    currentProgramTextAddress = ram.GetP ();
                    contents.symbol = symbol;
                    contents.value = value;
                    AllocateGlobalSymbol (contents, currentProgramTextAddress);
                    ram.SetP (currentProgramTextAddress);
                }
            }
            else
            {
                if (!functionSeen)
                {
                    if (value == "integer" ||
                        value == "float" ||
                        value == "string" ||
                        value == "input" ||
                        value == "output")
                    {
                        currentProgramTextAddress = ram.GetP ();
                        contents.symbol = symbol;
                        contents.value = value;
                        AssignTypeToGlobalSymbol (contents);
                        ram.SetP (currentProgramTextAddress);
                    }
                    else if (IsInteger (value))
                    {
                        currentProgramTextAddress = ram.GetP ();
                        contents.symbol = symbol;
                        contents.value = value;
                        AllocateGlobalArray (contents);
                        ram.SetP (currentProgramTextAddress);
                    }
                    else
                    {
                        return ("unrecognized data type" + value);
                    }
                }
            }
            if (functionSeen)
            {
                ram.Write (symbol, value);
                ram.IterateUp ();
            }
        }
        programSourceCodeFile >> symbol;
    }
    currentProgramTextAddress = ram.GetP ();
    SetKernelVariableValue ("END_ADDRESS", itos (currentProgramTextAddress - 1));
    ram.SetP (currentProgramTextAddress);

    programSourceCodeFile.close ();

	SetKernelVariableValue("CREATION_TIME", itos (GetClockTicks()));

    ProcessImageToFile (pid, "backingstore");


    return ("ok");
}

void GetProcessImageTemplate ()
{
    ifstream processImageTemplateFile;
    char uselessCharacter;
    int address;
    char fieldSeparator;
    string symbol;
    string value;
    size_t foundLowerCaseAddress;
    size_t foundUpperCaseAddress;
    size_t foundInstructionPointer;

    processImageTemplateFile.open ("0_template");
    if (!processImageTemplateFile)
    {
        cout << "HALos: no process image template" << endl;
        exit (1);
    }

    ram.Clear (0);

    processImageTemplateFile >> uselessCharacter;
    while (processImageTemplateFile)
    {
        processImageTemplateFile >> uselessCharacter;
        processImageTemplateFile >> uselessCharacter;
        processImageTemplateFile >> address;
        ram.SetP (address);
        processImageTemplateFile >> fieldSeparator;
        processImageTemplateFile >> symbol;
        processImageTemplateFile.ignore (256, ':');
        foundLowerCaseAddress = symbol.find ("address");
        foundUpperCaseAddress = symbol.find ("ADDRESS");
        foundInstructionPointer = symbol.find ("INSTRUCTION_POINTER");
        if (foundLowerCaseAddress != string::npos ||
            foundUpperCaseAddress != string::npos ||
            foundInstructionPointer != string::npos)
        {
            processImageTemplateFile >> uselessCharacter;
            processImageTemplateFile >> uselessCharacter;
            processImageTemplateFile >> uselessCharacter;
        }
        getline (processImageTemplateFile, value);
        ram.Write (symbol, value);
        processImageTemplateFile >> uselessCharacter;
    }

    processImageTemplateFile.close ();

    return;
}

void ProcessImageToFile (int pid, string location)
{
    ofstream processImageFile;
    string processPid;
    memoryCell contents;
    int i;
    int globalSymbolsTableStartAddress;
    int globalSymbolsTableEndAddress;
    int functionCallStackEndAddress;
    int functionCallStackStartAddress;
    int segmentSize;

    processPid = itos (pid) + "_" + location;
    processImageFile.open (processPid.c_str ());
    if (!processImageFile)
    {
        cout << "HALos: unable to create image file for process " << processPid << endl;
        exit (1);
    }

    globalSymbolsTableStartAddress = GetMemorySegmentBoundary ("GLOBAL_SYMBOLS_TABLE_START_ADDRESS", segmentSize);
    globalSymbolsTableEndAddress = globalSymbolsTableStartAddress + segmentSize;
    functionCallStackEndAddress = GetMemorySegmentBoundary ("FUNCTION_CALL_STACK_START_ADDRESS", segmentSize) + 1;
    functionCallStackStartAddress = functionCallStackEndAddress - segmentSize;

    for (i = 0; i < MEMORY_SIZE; i ++)
    {
        ram.SetP (i);
        contents = ram.Read ();
        if (contents.symbol.length () > 0)
        {
            processImageFile << "0d_";
            processImageFile << i << ": ";
            processImageFile << contents.symbol << " :";
            if (AddressField (contents.symbol, i,
                              globalSymbolsTableStartAddress,
                              globalSymbolsTableEndAddress,
                              functionCallStackStartAddress,
                              functionCallStackEndAddress))
            {
                processImageFile << "0d_";
            }
            processImageFile << contents.value << endl;
        }
    }
    processImageFile.close ();

    return;
}

bool AddressField (string symbol, int i, int globalSymbolsTableStartAddress, int globalSymbolsTableEndAddress,
                   int functionCallStackStartAddress, int functionCallStackEndAddress)
{
    size_t foundLowerCaseAddress;
    size_t foundUpperCaseAddress;
    size_t foundInstructionPointer;

    foundLowerCaseAddress = symbol.find ("address");
    foundUpperCaseAddress = symbol.find ("ADDRESS");
    foundInstructionPointer = symbol.find ("INSTRUCTION_POINTER");
    if (foundLowerCaseAddress != string::npos ||
        foundUpperCaseAddress != string::npos ||
        foundInstructionPointer != string::npos)
    {
        return true;
    }
    else
    {
        if (symbol.length () > 0)
        {
            if (i >= globalSymbolsTableStartAddress && i < globalSymbolsTableEndAddress)
            {
                return true;
            }
            else if (i >= functionCallStackStartAddress && i < functionCallStackEndAddress)
            {
                return true;
            }
        }
    }

    return false;
}

void SetKernelVariableValue (string kernelVariableDescription, string value)
{
    int startAddress;
    int segmentSize;
    memoryCell contents;

    startAddress = GetMemorySegmentBoundary ("KERNEL_SPACE_START_ADDRESS", segmentSize);
    startAddress = startAddress + segmentSize;
    ram.SetP (startAddress);
    ram.IterateDown ();
    while (1)
    {
        contents = ram.Read ();
        if (contents.symbol == kernelVariableDescription)
        {
            break;
        }
        ram.IterateDown ();
    }
    ram.ReWrite ("", value);

    return;
}

int GetMemorySegmentBoundary (string segmentStartAddressDescription, int& segmentSize)
{
    int startAddress;
    memoryCell contents;

    ram.ResetP ();
    ram.IterateUp ();
    while (1)
    {
        contents = ram.Read ();
        if (contents.symbol == segmentStartAddressDescription)
        {
            break;
        }
        ram.IterateUp ();
    }
    startAddress = atoi (contents.value.c_str ());
    ram.IterateUp ();
    contents = ram.Read ();
    segmentSize = atoi (contents.value.c_str ());

    return startAddress;
}

void AllocateGlobalSymbol (memoryCell contents, int currentProgramTextAddress)
{
    int globalValuesTableStartAddress;
    int lastGlobalValuesTableAddress;
    int globalSymbolsTableStartAddress;
    int lastGlobalSymbolsTableAddress;
    int segmentSize;

    globalValuesTableStartAddress = GetMemorySegmentBoundary ("GLOBAL_VALUES_TABLE_START_ADDRESS", segmentSize);
    lastGlobalValuesTableAddress = GetKernelVariableIntegerValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS");
    ram.SetP (lastGlobalValuesTableAddress);
    ram.IterateUp ();
    lastGlobalValuesTableAddress = ram.GetP ();
    if (lastGlobalValuesTableAddress == globalValuesTableStartAddress + segmentSize)
    {
        cout << "HALos: size of global values table exceeds segment size" << endl;
        return;
    }
    if (contents.symbol == "function" ||
        contents.symbol == "label")
    {
        ram.Write ("constant_address", itos (currentProgramTextAddress));
    }
    else if (contents.symbol == "constant")
    {
        ram.Write ("constant", "undefined_type");
    }
    else if (contents.symbol == "variable")
    {
        ram.Write ("variable", "undefined_type");
    }
    else if (contents.symbol == "file")
    {
        if (contents.value == "keyboard")
        {
            ram.Write ("input_" + contents.symbol, "closed");
        }
        else if (contents.value == "display")
        {
            ram.Write ("output_" + contents.symbol, "closed");
        }
        else
        {
            ram.Write (contents.symbol, "");
        }
    }
    SetKernelVariableValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS", itos (lastGlobalValuesTableAddress));
    globalSymbolsTableStartAddress = GetMemorySegmentBoundary ("GLOBAL_SYMBOLS_TABLE_START_ADDRESS", segmentSize);
    lastGlobalSymbolsTableAddress = GetKernelVariableIntegerValue ("LAST_GLOBAL_SYMBOLS_TABLE_ADDRESS");
    ram.SetP (lastGlobalSymbolsTableAddress);
    ram.IterateUp ();
    lastGlobalSymbolsTableAddress = ram.GetP ();
    if (lastGlobalSymbolsTableAddress == globalSymbolsTableStartAddress + segmentSize)
    {
        cout << "HALos: size of global symbols table exceeds segment size" << endl;
        exit (1);
    }
    ram.Write (contents.value, itos (lastGlobalValuesTableAddress));
    SetKernelVariableValue ("LAST_GLOBAL_SYMBOLS_TABLE_ADDRESS", itos (lastGlobalSymbolsTableAddress));

    if (contents.symbol == "file")
    {
        if (contents.value != "keyboard" && contents.value != "display")
        {
            globalValuesTableStartAddress = GetMemorySegmentBoundary ("GLOBAL_VALUES_TABLE_START_ADDRESS", segmentSize);
            lastGlobalValuesTableAddress = GetKernelVariableIntegerValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS");
            ram.SetP (lastGlobalValuesTableAddress);
            ram.IterateUp ();
            lastGlobalValuesTableAddress = ram.GetP ();
            if (lastGlobalValuesTableAddress == globalValuesTableStartAddress + segmentSize)
            {
                cout << "HALos: size of global values table exceeds segment size" << endl;
                return;
            }
            ram.Write ("file_name", "");
            SetKernelVariableValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS", itos (lastGlobalValuesTableAddress));
        }
    }

    return;
}

void AssignTypeToGlobalSymbol (memoryCell contents)
{
    int segmentSize;
    int address;
    string symbol;
    string type;

    symbol = contents.symbol;
    type = contents.value;

    address = GetGlobalSymbolAddress (symbol);
    if (address != -1)
    {
        ram.SetP (address);
        contents = ram.Read ();
        ram.SetP (atoi (contents.value.c_str ()));
        contents = ram.Read ();
        if (contents.symbol == "constant" ||
            contents.symbol == "variable")
        {
            if (type == "integer")
            {
                ram.ReWrite (contents.symbol + "_" + type, "0");
            }
            else if (type == "float")
            {
                ram.ReWrite (contents.symbol + "_" + type, "0.0");
            }
            else if (type == "string")
            {
                ram.ReWrite (contents.symbol + "_" + type, "");
            }
            else
            {
                cout << "HALos: unrecognized data type " << type << " for global symbol " << symbol << endl;
                exit (1);
            }
        }
        else if (contents.symbol == "file")
        {
            if (type == "input")
            {
                ram.ReWrite (type + "_" + contents.symbol, "closed");
            }
            else if (type == "output")
            {
                ram.ReWrite (type + "_" + contents.symbol, "closed");
            }
            else
            {
                cout << "HALos: unrecognized data type " << type << " for global symbol " << symbol << endl;
                exit (1);
            }
            ram.IterateUp ();
            contents = ram.Read ();
            ram.ReWrite (type + "_" + contents.symbol, "");
        }
    }
    else
    {
        cout << "HALos: symbol " << symbol << " not found in global symbol table" << endl;
        exit (1);
    }

    return;
}

void AllocateGlobalArray (memoryCell contents)
{
    int globalValuesTableStartAddress;
    int lastGlobalValuesTableAddress;
    int segmentSize;
    int noOfArrayElements = atoi (contents.value.c_str ());
    string symbol;
    int i;

    globalValuesTableStartAddress = GetMemorySegmentBoundary ("GLOBAL_VALUES_TABLE_START_ADDRESS", segmentSize);
    lastGlobalValuesTableAddress = GetKernelVariableIntegerValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS");
    ram.SetP (lastGlobalValuesTableAddress);
    if (lastGlobalValuesTableAddress == globalValuesTableStartAddress + noOfArrayElements + segmentSize)
    {
        cout << "HALos: size of global values table exceeds segment size" << endl;
        exit (1);
    }
    contents = ram.Read ();
    symbol = contents.symbol;
    contents.symbol = contents.symbol + "_array_0";
    ram.ReWrite (contents.symbol, contents.value);

    for (i = 1; i < noOfArrayElements; i ++)
    {
        ram.SetP (GetKernelVariableIntegerValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS"));
        ram.IterateUp ();
        contents.symbol = symbol + "_array_" + itos (i);
        ram.Write (contents.symbol, contents.value);
        SetKernelVariableValue ("LAST_GLOBAL_VALUES_TABLE_ADDRESS", itos (ram.GetP ()));
    }

    return;
}

int GetKernelVariableIntegerValue (string kernelVariableDescription)
{
    int startAddress;
    int segmentSize;
    memoryCell contents;
    int kernelVariableValue;

    startAddress = GetMemorySegmentBoundary ("KERNEL_SPACE_START_ADDRESS", segmentSize);
    startAddress = startAddress + segmentSize;
    ram.SetP (startAddress);
    ram.IterateDown ();
    while (1)
    {
        contents = ram.Read ();
        if (contents.symbol == kernelVariableDescription)
        {
            break;
        }
        ram.IterateDown ();
    }
    kernelVariableValue = atoi (contents.value.c_str ());

    return (kernelVariableValue);
}

int GetGlobalSymbolAddress (string symbol)
{
    int globalSymbolsTableStartAddress;
    int lastGlobalSymbolsTableAddress;
    int segmentSize;
    memoryCell contents;

    globalSymbolsTableStartAddress = GetMemorySegmentBoundary ("GLOBAL_SYMBOLS_TABLE_START_ADDRESS", segmentSize);
    lastGlobalSymbolsTableAddress = GetKernelVariableIntegerValue ("LAST_GLOBAL_SYMBOLS_TABLE_ADDRESS");
    ram.SetP (globalSymbolsTableStartAddress);
    contents = ram.Read ();
    while (ram.GetP () <= lastGlobalSymbolsTableAddress)
    {
        if (contents.symbol == symbol)
        {
            return ram.GetP ();
        }
        ram.IterateUp ();
        contents = ram.Read ();
    }

    return -1;
}

bool IsInteger (string value)
{
    int i;

    for (i = 0; i < value.length (); i ++)
    {
        if (i == 0 && value [i] == '-')
        {
            continue;
        }
        else if (!isdigit (value [i]))
        {
            return false;
        }
    }

    return true;
}

void ShutdownAndRestart (string command, string arguments [])
{
    int i;

    cout << "HALos: terminating ..." << endl;
    usleep (SLEEP_DELAY);

    cpuProcess.pid = itos (nextPid);
    cpuProcess.status = "NEW_PROCESS";
    cpuProcess.command = command;
    for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
    {
        cpuProcess.arguments [i] = arguments [i];
    }
    SendMessageToHAL9000 (cpuProcess);
    SendMessageToHALkeyboardDriver ("0", "SHUTDOWN");
    SendMessageToHALdisplayDriver ("0", "SHUTDOWN", "");
    SendMessageToHALdiskDriver ("0", "SHUTDOWN", "", "", "", "");

    exit (0);
}

void Cull (string command, string arguments [])
{
    int i;
    processDescriptor process;
    fileDescriptor file;
    int queueLength;
    bool processFound;
    bool okToScheduleNextProcess = true;

    process.pid = itos (0);
    if (arguments [0].length () == 0)
    {
        process.type = "BACKGROUND_PROCESS";
    }
    else
    {
        process.type = "FOREGROUND_PROCESS";
    }
    process.status = "PAUSE_ANY_EXECUTING_PROCESS";
    process.command = command;
    for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
    {
        process.arguments [i] = "";
    }
    process.returnValue = "";
    SendMessageToHAL9000 (process);
    process = nullProcess;

    UnblockSignals ();
    // The "whole" system must be idle before culling a process.
    // So, the first step is to stop any executing process.
    do
    {
        BlockSignals ();
        if (messageFromHAL9000)
        {
            messageFromHAL9000 = 0;
            GetMessageFromHAL9000 ();
            if (messageType == "PROCESS_DONE")
            {
                HandleFinishedProcess (!okToScheduleNextProcess);
            }
            else if (messageType == "INTERRUPT")
            {
                HandleHAL9000Interrupt (!okToScheduleNextProcess);
            }
            else if (messageType == "SYSTEM_CALL")
            {
                HandleSystemCall (!okToScheduleNextProcess);
            }
            else if (messageType == "EXECUTING_PROCESS_PAUSED")
            {
                cpuProcess.status = "CONTINUE_EXECUTING_PROCESS";
                UnblockSignals ();
                break;
            }
            else if (messageType == "NO_EXECUTING_PROCESS")
            {
                UnblockSignals ();
                break;
            }
        }
        UnblockSignals ();
    } while (messageType != "EXECUTING_PROCESS_PAUSED" && messageType != "NO_EXECUTING_PROCESS");
    // Second, wait for any display output and disk input/output requests
    // to finish.
    while (displayProcess.pid.length () > 0 || diskProcess.pid.length () > 0)
    {
        BlockSignals ();
        if (messageFromHALdisplayDriver)
        {
            messageFromHALdisplayDriver = 0;
            GetMessageFromHALdisplayDriver ();
            displayProcess.systemCallFileIndex = -1;
            displayProcess.systemCall = "";
            displayProcess.systemCallParameter1 = "";
            displayProcess.systemCallParameter2 = "";
            displayProcess.systemCallParameter3 = "";
            displayProcess.systemCallBuffer = "";
            displayProcess.systemCallResult = "";
            readyQueue.Enqueue (displayProcess);
            displayProcess = nullProcess;
        }
        else if (messageFromHALdiskDriver)
        {
            messageFromHALdiskDriver = 0;
            GetMessageFromHALdiskDriver ();
            if (diskProcess.systemCall == "OPEN")
            {
                if (parameter5 == "FILE_OPEN_OK")
                {
                    if (!diskProcess.fileTable.Find (diskProcess.systemCallParameter1,
                                                     diskProcess.systemCallParameter2))
                    {  
                        diskProcess.fileTable.Insert (diskProcess.systemCallParameter1,
                                                      diskProcess.systemCallParameter2,
                                                      diskProcess.systemCallParameter3);
                    }
                    else 
                    {
                        parameter5 = "FILE_ALREADY_OPEN (cull)";
                    }
                }
                diskProcess.systemCallFileIndex = -1;
                diskProcess.systemCall = "";
                diskProcess.systemCallParameter1 = "";
                diskProcess.systemCallParameter2 = "";
                diskProcess.systemCallParameter3 = "";
                diskProcess.systemCallBuffer = "";
                diskProcess.systemCallResult = parameter5;
            }
            else if (diskProcess.systemCall == "READ")
            {
                diskProcess.fileTable.SetP (diskProcess.systemCallFileIndex);
                diskProcess.fileTable.Write (parameter3);
                diskProcess.systemCallFileIndex = -1;
                diskProcess.systemCall = "";
                diskProcess.systemCallParameter1 = "";
                diskProcess.systemCallParameter2 = "";
                diskProcess.systemCallParameter3 = "";
                diskProcess.systemCallBuffer = parameter4;
                diskProcess.systemCallResult = parameter5;
            }
            else if (diskProcess.systemCall == "WRITE" || diskProcess.systemCall == "NEWLINE")
            {
                diskProcess.systemCallFileIndex = -1;
                diskProcess.systemCall = "";
                diskProcess.systemCallParameter1 = "";
                diskProcess.systemCallParameter2 = "";
                diskProcess.systemCallParameter3 = "";
                diskProcess.systemCallBuffer = "";
                diskProcess.systemCallResult = parameter5;
            }
            readyQueue.Enqueue (diskProcess);
            diskProcess = nullProcess;
        }
        UnblockSignals ();
    }
    BlockSignals ();
    // If the cull command was entered on the command line in HALshell,
    // then arguments [0] will contain the pid of the process to cull.
    if (arguments [0].length () > 0)
    {
        // If the process being culled is the one that was running
        // when the cull command was received, then cull it.
        if (cpuProcess.pid.length () > 0 &&
            cpuProcess.pid == arguments [0])
        {
            remove ((cpuProcess.pid + "_backingstore").c_str ());
            SendReturnStatusToHALshell (cpuProcess.pid, "", "process culled", "");
            if (readyQueue.IsEmpty ())
            {
                cpuProcess = nullProcess;
            }
            else
            {
                cpuProcess = readyQueue.Dequeue ();
                SendMessageToHAL9000 (cpuProcess);
            }
            if (!displayQueue.IsEmpty ())
            {
                displayProcess = displayQueue.Dequeue ();
                SendMessageToHALdisplayDriver (displayProcess.pid, displayProcess.systemCall,
                                               displayProcess.systemCallParameter1);
            }
            if (!diskQueue.IsEmpty ())
            {
                diskProcess = diskQueue.Dequeue ();
                if (diskProcess.systemCallFileIndex == -1)
                {
                    SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                diskProcess.systemCallParameter1,
                                                diskProcess.systemCallParameter3, "", "");
                }
                else
                {
                    diskProcess.fileTable.SetP (diskProcess.systemCallFileIndex);
                    file = diskProcess.fileTable.Read ();
                    if (file.mode == "input")
                    {
                        SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                    file.name, file.mode, file.markerPosition, "");
                    }
                    else
                    {
                        SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                    file.name, file.mode, "", diskProcess.systemCallParameter1);
                    }
                }
            }
            if (!keyboardQueue.IsEmpty ())
            {
                keyboardProcess = keyboardQueue.Dequeue ();
                SendMessageToHALkeyboardDriver (keyboardProcess.pid, keyboardProcess.systemCall);
            }
        }
        else
        {
            // Otherwise, find and remove the process with pid == arguments [0].
            // It could be in the ready queue. If found there, cull it.
            queueLength = 0;
            for (int j = 0; j < NO_OF_READY_QUEUES; j++)
            {
                queueLength += readyQueue.Length(j);
            }
            processFound = false;
            for (i = 0; i < queueLength; i ++)
            {
                process = readyQueue.Dequeue ();
                if (process.pid == arguments [0])
                {
                    processFound = true;
                }
                else
                {
                    readyQueue.Enqueue (process);
                }
            }
            if (processFound)
            {
                remove ((arguments [0] + "_backingstore").c_str ());
                SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
            }
            else
            {
                // Or it could be in the display queue. If found there, cull it.
                queueLength = displayQueue.Length ();
                processFound = false;
                for (i = 0; i < queueLength; i ++)
                {
                    process = displayQueue.Dequeue ();
                    if (process.pid == arguments [0])
                    {
                        processFound = true;
                    }
                    else
                    {
                        displayQueue.Enqueue (process);
                    }
                }
                if (processFound)
                {
                    remove ((arguments [0] + "_backingstore").c_str ());
                    SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
                }
                else
                {
                    // Or it could be in the disk queue. If found there, cull it.
                    queueLength = diskQueue.Length ();
                    processFound = false;
                    for (i = 0; i < queueLength; i ++)
                    {
                        process = diskQueue.Dequeue ();
                        if (process.pid == arguments [0])
                        {
                            processFound = true;
                        }
                        else
                        {
                            diskQueue.Enqueue (process);
                        }
                    }
                    if (processFound)
                    {
                        remove ((arguments [0] + "_backingstore").c_str ());
                        SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
                    }
                    else
                    {
                        // Or it could be in the keyboard queue. If found there, cull it.
                        queueLength = keyboardQueue.Length ();
                        processFound = false;
                        for (i = 0; i < queueLength; i ++)
                        {
                            process = keyboardQueue.Dequeue ();
                            if (process.pid == arguments [0])
                            {
                                processFound = true;
                            }
                            else
                            {
                                keyboardQueue.Enqueue (process);
                            }
                        }
                        if (processFound)
                        {
                            remove ((arguments [0] + "_backingstore").c_str ());
                            SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
                        }
                        else
                        {
                            // Or it may not exist.
                            SendReturnStatusToHALshell (arguments [0], "", "process not found", "");
                        }
                    }
                }
            }
            // If a process was paused, restart it.
            if (cpuProcess.pid.length () > 0)
            {
                SendMessageToHAL9000 (cpuProcess);
            }
            // Or there is no process ready to run.
            else if (readyQueue.IsEmpty ())
            {
                cpuProcess = nullProcess;
            }
            // Or there is a ready process, so run it.
            else
            {
                cpuProcess = readyQueue.Dequeue ();
                SendMessageToHAL9000 (cpuProcess);
            }
            if (!displayQueue.IsEmpty ())
            {
                displayProcess = displayQueue.Dequeue ();
                SendMessageToHALdisplayDriver (displayProcess.pid, displayProcess.systemCall,
                                               displayProcess.systemCallParameter1);
            }
            if (!diskQueue.IsEmpty ())
            {
                diskProcess = diskQueue.Dequeue ();
                if (diskProcess.systemCallFileIndex == -1)
                {
                    SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                diskProcess.systemCallParameter1,
                                                diskProcess.systemCallParameter3, "", "");
                }
                else
                {
                    diskProcess.fileTable.SetP (diskProcess.systemCallFileIndex);
                    file = diskProcess.fileTable.Read ();
                    if (file.mode == "input")
                    {
                        SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                    file.name, file.mode, file.markerPosition, "");
                    }
                    else
                    {
                        SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                    file.name, file.mode, "", diskProcess.systemCallParameter1);
                    }
                }
            }
            if (!keyboardQueue.IsEmpty ())
            {
                keyboardProcess = keyboardQueue.Dequeue ();
                SendMessageToHALkeyboardDriver (keyboardProcess.pid, keyboardProcess.systemCall);
            }
        }
    }
    // If the cull command was generated by pressing ctrl/c in HALshell,
    // then arguments [0] will be blank. This means that the process to be
    // culled is a foreground process of some kind.
    else // (arguments [0].length () == 0)
    {
        // If it is the current keyboard process, cull it.
        if (keyboardProcess.pid.length () > 0)
        {
            remove ((keyboardProcess.pid + "_backingstore").c_str ());
            SendReturnStatusToHALshell (keyboardProcess.pid, "", "process culled", "");
            if (keyboardQueue.IsEmpty ())
            {
                keyboardProcess = nullProcess;
            }
            else
            {
                keyboardProcess = keyboardQueue.Dequeue ();
                SendMessageToHALkeyboardDriver (keyboardProcess.pid, keyboardProcess.systemCall);
            }
            if (cpuProcess.pid.length () > 0)
            {
                SendMessageToHAL9000 (cpuProcess);
            }
            // Or there is no process ready to run.
            else if (readyQueue.IsEmpty ())
            {
                cpuProcess = nullProcess;
            }
            // Or there is a process ready to run, so run it.
            else
            {
                cpuProcess = readyQueue.Dequeue ();
                SendMessageToHAL9000 (cpuProcess);
            }
        }    
        // Or, if the process being culled is the one that was running
        // when the cull command was received, then cull it.
        else if (cpuProcess.pid.length () > 0 &&
                 cpuProcess.type == "FOREGROUND_PROCESS")
        {
            remove ((cpuProcess.pid + "_backingstore").c_str ());
            SendReturnStatusToHALshell (cpuProcess.pid, "", "process culled", "");
            // There is no process ready to run.
            if (readyQueue.IsEmpty ())
            {
                cpuProcess = nullProcess;
            }
            // Or there is a process ready to run, so run it.
            else
            {
                cpuProcess = readyQueue.Dequeue ();
                SendMessageToHAL9000 (cpuProcess);
            }
        }
        else
        {
            // Otherwise, find and remove the foreground process.
            // It could be in the ready queue. If found there, cull it.
            queueLength = 0;
            for (int j = 0; j < NO_OF_READY_QUEUES; j++)
            {
                queueLength += readyQueue.Length (j);
            }
            processFound = false;
            for (i = 0; i < queueLength; i ++)
            {
                process = readyQueue.Dequeue ();
                if (process.type == "FOREGROUND_PROCESS")
                {
                    processFound = true;
                    arguments [0] = process.pid;
                }
                else
                {
                    readyQueue.Enqueue (process);
                }
            }
            if (processFound)
            {
                remove ((arguments [0] + "_backingstore").c_str ());
                SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
            }
            else
            {
                // Or it could be in the display queue. If found there, cull it.
                queueLength = displayQueue.Length ();
                processFound = false;
                for (i = 0; i < queueLength; i ++)
                {
                    process = displayQueue.Dequeue ();
                    if (process.type == "FOREGROUND_PROCESS")
                    {
                        processFound = true;
                        arguments [0] = process.pid;
                    }
                    else
                    {
                        displayQueue.Enqueue (process);
                    }
                }
                if (processFound)
                {
                    remove ((arguments [0] + "_backingstore").c_str ());
                    SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
                }
                else
                {
                    // Or it could be in the disk queue. If found there, cull it.
                    queueLength = diskQueue.Length ();
                    processFound = false;
                    for (i = 0; i < queueLength; i ++)
                    {
                        process = diskQueue.Dequeue ();
                        if (process.type == "FOREGROUND_PROCESS")
                        {
                            processFound = true;
                            arguments [0] = process.pid;
                        }
                        else
                        {
                            diskQueue.Enqueue (process);
                        }
                    }
                    if (processFound)
                    {
                        remove ((arguments [0] + "_backingstore").c_str ());
                        SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
                    }
                    else
                    {
                        // Or it could be in the keyboard queue. If found there, cull it.
                        queueLength = keyboardQueue.Length ();
                        processFound = false;
                        for (i = 0; i < queueLength; i ++)
                        {
                            process = keyboardQueue.Dequeue ();
                            if (process.type == "FOREGROUND_PROCESS")
                            {
                                processFound = true;
                                arguments [0] = process.pid;
                            }
                            else
                            {
                                keyboardQueue.Enqueue (process);
                            }
                        }
                        if (processFound)
                        {
                            remove ((arguments [0] + "_backingstore").c_str ());
                            SendReturnStatusToHALshell (arguments [0], "", "process culled", "");
                        }
                        else
                        {
                            // Or it may not exist.
                            SendReturnStatusToHALshell (arguments [0], "", "process not found", "");
                        }
                    }
                }
            }
            // If a process was paused, restart it.
            if (cpuProcess.pid.length () > 0)
            {
                SendMessageToHAL9000 (cpuProcess);
            }
            // Or there is no process ready to run.
            else if (readyQueue.IsEmpty ())
            {
                cpuProcess = nullProcess;
            }
            // Or there is a process ready to run, so run it.
            else
            {
                cpuProcess = readyQueue.Dequeue ();
                SendMessageToHAL9000 (cpuProcess);
            }
            if (!displayQueue.IsEmpty ())
            {
                displayProcess = displayQueue.Dequeue ();
                SendMessageToHALdisplayDriver (displayProcess.pid, displayProcess.systemCall,
                                               displayProcess.systemCallParameter1);
            }
            if (!diskQueue.IsEmpty ())
            {
                diskProcess = diskQueue.Dequeue ();
                if (diskProcess.systemCallFileIndex == -1)
                {
                    SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                diskProcess.systemCallParameter1,
                                                diskProcess.systemCallParameter3, "", "");
                }
                else
                {
                    diskProcess.fileTable.SetP (diskProcess.systemCallFileIndex);
                    file = diskProcess.fileTable.Read ();
                    if (file.mode == "input")
                    {
                        SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                    file.name, file.mode, file.markerPosition, "");
                    }
                    else
                    {
                        SendMessageToHALdiskDriver (diskProcess.pid, diskProcess.systemCall,
                                                    file.name, file.mode, "", diskProcess.systemCallParameter1);
                    }
                }
            }
            if (!keyboardQueue.IsEmpty ())
            {
                keyboardProcess = keyboardQueue.Dequeue ();
                SendMessageToHALkeyboardDriver (keyboardProcess.pid, keyboardProcess.systemCall);
            }
        }
    }

    return;
}

void SendMessageToHAL9000 (processDescriptor process)
{
    ofstream halOsMessageFile;
    int i;
    union sigval dummyValue;

    halOsMessageFile.open ("HALosToHAL9000");
    if (!halOsMessageFile)
    {
        cout << "HALos: unable to initialize HAL9000 message buffer" << endl;
        exit (1);
    }
    else
    {
        halOsMessageFile << process.status << endl;
        halOsMessageFile << process.pid << endl;
        halOsMessageFile << process.command << endl;
        for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
        {
            halOsMessageFile << process.arguments [i] << endl;
        }
        halOsMessageFile << process.systemCallBuffer << endl;
        halOsMessageFile << process.systemCallResult << endl;
        halOsMessageFile.close ();
        if (sigqueue (HAL9000Pid, SIGRTMIN, dummyValue) == -1)
        {
            cout << "HALos: message not sent to HAL9000" << endl;
            exit (1);
        }
    }

    return;
}

void SendMessageToHALkeyboardDriver (string pid, string systemCall)
{
    ofstream halOsMessageFile;
    union sigval dummyValue;

    halOsMessageFile.open ("HALosToHALkeyboardDriver");
    if (!halOsMessageFile)
    {
        cout << "HALos: unable to initialize HAL keyboard driver message buffer" << endl;
        exit (1);
    }
    else
    {
        halOsMessageFile << pid << endl;
        halOsMessageFile << systemCall << endl;
        halOsMessageFile.close ();
        if (sigqueue (HALkeyboardDriverPid, SIGRTMIN, dummyValue) == -1)
        {
            cout << "HALos: message not sent to HAL keyboard driver" << endl;
            exit (1);
        }
    }

    return;
}

void SendMessageToHALdisplayDriver (string pid, string systemCall, string buffer)
{
    ofstream halOsMessageFile;
    union sigval dummyValue;

    halOsMessageFile.open ("HALosToHALdisplayDriver");
    if (!halOsMessageFile)
    {
        cout << "HALos: unable to initialize HAL display driver message buffer" << endl;
        exit (1);
    }
    else
    {
        halOsMessageFile << pid << endl;
        halOsMessageFile << systemCall << endl;
        halOsMessageFile << buffer << endl;
        halOsMessageFile.close ();
        if (sigqueue (HALdisplayDriverPid, SIGRTMIN, dummyValue) == -1)
        {
            cout << "HALos: message not sent to HAL display driver" << endl;
            exit (1);
        }
    }

    return;
}

void SendMessageToHALdiskDriver (string pid, string systemCall, string fileName, string mode,
                                 string markerPosition, string buffer)
{
    ofstream halOsMessageFile;
    union sigval dummyValue;

    halOsMessageFile.open ("HALosToHALdiskDriver");
    if (!halOsMessageFile)
    {
        cout << "HALos: unable to initialize HAL disk driver message buffer" << endl;
        exit (1);
    }
    else
    {
        halOsMessageFile << pid << endl;
        halOsMessageFile << systemCall << endl;
        halOsMessageFile << fileName << endl;
        halOsMessageFile << mode << endl;
        halOsMessageFile << markerPosition << endl;
        halOsMessageFile << buffer << endl;
        halOsMessageFile.close ();
        if (sigqueue (HALdiskDriverPid, SIGRTMIN, dummyValue) == -1)
        {
            cout << "HALos: message not sent to HAL display driver" << endl;
            exit (1);
        }
    }

    return;
}

processDescriptor NullProcess ()
{
    processDescriptor process;
    int i;

    process.pid = "";
    process.type = "";
    process.status = "";
    process.command = "";
    for (i = 0; i < MAX_COMMAND_LINE_ARGUMENTS; i ++)
    {
        process.arguments [i] = "";
    }
    process.returnValue = "";
    process.fileTable.Clear ();
    process.systemCallFileIndex = -1;
    process.systemCall = "";
    process.systemCallParameter1 = "";
    process.systemCallParameter2 = "";
    process.systemCallParameter3 = "";
    process.systemCallBuffer = "";
    process.systemCallResult = "";

    return process;
}

string itos (int i)
{
    stringstream s;

    s << i;

    return s.str ();
}

void Initialize ()
{
    int i;

    cout << "HALbios: HALos OK" << endl;
    usleep (SLEEP_DELAY);

    cout << "HALos: initializing ..." << endl;
    usleep (SLEEP_DELAY);

    cout << "HALos: loading HALshell ..." << endl;
    if ((sigemptyset (&interruptMask) == -1) ||
        (sigaddset (&interruptMask, SIGRTMIN) == -1))
    {
        cout << "HALos: unable to initialize signal mask" << endl;
        exit (1);
    }
    act.sa_sigaction = &SignalHandler;
    act.sa_mask = interruptMask;
    act.sa_flags = SA_SIGINFO;
    if ((sigaction (SIGRTMIN, &act, NULL) == -1))
    {
        cout << "HALos: unable to connect to HALshell" << endl;
        exit (1);
    }
    signal (SIGINT, SIG_IGN);
    usleep (SLEEP_DELAY);

    HAL9000Pid = getppid ();

    HALshellPid = fork ();
    if (HALshellPid < 0)
    {
        cout << "HALos: HALshell process creation failed" << endl;
        exit (1);
    }
    else if (HALshellPid == 0)
    {
        execle ("HALshell", "HALshell", (char *) NULL, environ);
        cout << "HALos: HALshell failed to load" << endl;
        exit (1);
    }

    HALkeyboardDriverPid = fork ();
    if (HALkeyboardDriverPid < 0)
    {
        cout << "HALos: HALkeyboardDriver process creation failed" << endl;
        exit (1);
    }
    else if (HALkeyboardDriverPid == 0)
    {
        execle ("HALkeyboardDriver", "HALkeyboardDriver", (char *) NULL, environ);
        cout << "HALos: HALkeyboardDriver failed to load" << endl;
        exit (1);
    }

    HALdisplayDriverPid = fork ();
    if (HALdisplayDriverPid < 0)
    {
        cout << "HALos: HALdisplayDriver process creation failed" << endl;
        exit (1);
    }
    else if (HALdisplayDriverPid == 0)
    {
        execle ("HALdisplayDriver", "HALdisplayDriver", (char *) NULL, environ);
        cout << "HALos: HALdisplayDriver failed to load" << endl;
        exit (1);
    }

    HALdiskDriverPid = fork ();
    if (HALdiskDriverPid < 0)
    {
        cout << "HALos: HALdiskDriver process creation failed" << endl;
        exit (1);
    }
    else if (HALdiskDriverPid == 0)
    {
        execle ("HALdiskDriver", "HALdiskDriver", (char *) NULL, environ);
        cout << "HALos: HALdiskDriver failed to load" << endl;
        exit (1);
    }

    nullProcess = NullProcess ();
    cpuProcess = nullProcess;
    keyboardProcess = nullProcess;
    displayProcess = nullProcess;
    diskProcess = nullProcess;

    return;
}

static void SignalHandler (int signalNo, siginfo_t* info, void* context)
{
    if (signalNo == SIGRTMIN)
    {
        if (info -> si_pid == HALshellPid)
        {
            commandToExecuteFromHALshell = 1;
        }
        else if (info -> si_pid == HAL9000Pid)
        {
            messageFromHAL9000 = 1;
        }
        else if (info -> si_pid == HALkeyboardDriverPid)
        {
            messageFromHALkeyboardDriver = 1;
        }
        else if (info -> si_pid == HALdisplayDriverPid)
        {
            messageFromHALdisplayDriver = 1;
        }
        else if (info -> si_pid == HALdiskDriverPid)
        {
            messageFromHALdiskDriver = 1;
        }
    }
}
