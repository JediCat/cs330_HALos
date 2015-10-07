//
// HALreadyQueue.cpp
//
// Copyright (c) 2015 Robert J. Hilderman.
// All rights reserved.
//
#include <iostream>
using namespace std;

#include "HALreadyQueue.h"

ReadyQueueType::ReadyQueueType (int queueSizeIn, int noOfReadyQueuesIn, cpuSchedulingPolicyCriteria* cpuSchedulingPoliciesIn)
{
    int i;

    QUEUE_SIZE = queueSizeIn;
    NO_OF_READY_QUEUES = noOfReadyQueuesIn;
    queues = new readyQueueDescriptor [NO_OF_READY_QUEUES];

    cpuSchedulingPolicies = cpuSchedulingPoliciesIn;

    for (i = 0; i < NO_OF_READY_QUEUES; i++)
    {
        if (cpuSchedulingPolicies[i].type == "RR" ||
            cpuSchedulingPolicies[i].type == "FCFS")
        {
            queues[i].queue = new processDescriptor[QUEUE_SIZE];
            queues[i].length = 0;
            queues[i].front = QUEUE_SIZE - 1;
            queues[i].back = queues[i].front;
        }
        else if (cpuSchedulingPolicies[i].type == "P")
        {
            queues[i].queue = new processDescriptor[QUEUE_SIZE];
            queues[i].length = 0;
            queues[i].front = 1;
            queues[i].back = -1;
        }
    }
}

ReadyQueueType::~ReadyQueueType ()
{
}

int ReadyQueueType::Length (int queueNo)
{
    return queues[queueNo].length;
}

bool ReadyQueueType::IsEmpty ()
{
    int i = 0;
    for (i = 0; i < NO_OF_READY_QUEUES; i++)
    {
        if (queues[i].length == 0)
        {
            return true;
        }
    }

    return false;
}

bool ReadyQueueType::IsFull (int queueNo)
{
    if (queues[queueNo].length == QUEUE_SIZE)
    {
        return true;
    }

    return false;
}

processDescriptor ReadyQueueType::SetQueueNo(processDescriptor process)
{
    if (NO_OF_READY_QUEUES == 1) return process;
    else if (NO_OF_READY_QUEUES > 1)
    {
        process.contextSwitches--;
        if (process.contextSwitches == 0 && process.direction == "DOWN")
        {
            int i = ++process.queueNo;
            cout << "Process " << process.pid << " moved " << process.direction << " to queue " << process.queueNo << endl;
            if (i == NO_OF_READY_QUEUES - 1)
            {
                process.direction = "UP";
                process.contextSwitches = 
                    cpuSchedulingPolicies[i].contextSwitchesUntilMoveUp;
            }
            else
            {
                process.contextSwitches =
                    cpuSchedulingPolicies[i].contextSwitchesUntilMoveDown;
            }
        }
        else if (process.contextSwitches == 0 && process.direction == "UP")
        {
            int i = --process.queueNo;
            cout << "Process " << process.pid << " moved " << process.direction << " to queue " << process.queueNo << endl;
            if (i == 0)
            {
                process.direction = "DOWN";
                process.contextSwitches =
                    cpuSchedulingPolicies[i].contextSwitchesUntilMoveDown;
            }
            else
            {
                process.contextSwitches =
                    cpuSchedulingPolicies[i].contextSwitchesUntilMoveUp;
            }
        }
    }
    return process;
}

void ReadyQueueType::Enqueue (processDescriptor process)
{
    process = SetQueueNo(process);
    int i = process.queueNo;
cout << "Proccess to be enqueued in " << i << ": " << process.pid << endl;

    if (cpuSchedulingPolicies[i].type == "RR" ||
        cpuSchedulingPolicies[i].type == "FCFS")
    {
        queues[i].back = (queues[i].back + 1) % QUEUE_SIZE;
        queues[i].queue[queues[i].back] = process;
        queues[i].length ++;
    }
    else if (cpuSchedulingPolicies[i].type == "P")
    {
        queues[i].length++;
        int j = queues[i].length;
        while (j > 1 && process.priority < queues[i].queue[j / 2].priority)
        {
            queues[i].queue[j] = queues[i].queue[j / 2];
            j = j / 2;
        }
        queues[i].queue[j] = process;
    }
    return;
}

processDescriptor ReadyQueueType::Dequeue ()
{
    processDescriptor toReturn;
    for (int i = 0; i < NO_OF_READY_QUEUES; i++)
    {
cout << i << ": "  << queues[i].length << endl;
        if (queues[i].length > 0)
        {
            if (cpuSchedulingPolicies[i].type == "RR" ||
                cpuSchedulingPolicies[i].type == "FCFS")
            {
                queues[i].front = (queues[i].front + 1) % QUEUE_SIZE;
                queues[i].length--;
                toReturn = queues[i].queue[queues[i].front];
            }
            else if (cpuSchedulingPolicies[i].type == "P")
            {
cout << "Is this where it's fucking up?\n";
                toReturn = queues[i].queue[1];
                queues[i].queue[1] = queues[i].queue[queues[i].length];
                queues[i].length--;
                processDescriptor temp = queues[i].queue[1];
                int j = 1;
                while (j * 2 <= queues[i].length)
                {
                    int child = j * 2;
                    if (child != queues[i].length && queues[i].queue[child + 1].priority < queues[i].queue[child].priority)
                        child++;
                    if (queues[i].queue[child].priority < temp.priority)
                        queues[i].queue[j] = queues[i].queue[child];
                    else break;
                    j = child;
                }
            }
        }
    }
    return toReturn;
}
