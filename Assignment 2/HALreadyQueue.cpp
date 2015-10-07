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

    // Init of ready queues based on type
    for (i = 0; i < NO_OF_READY_QUEUES; i++)
    {
        // Create circular queue if RR or FCFS
        if (cpuSchedulingPolicies[i].type == "RR" ||
            cpuSchedulingPolicies[i].type == "FCFS")
        {
            queues[i].queue = new processDescriptor[QUEUE_SIZE];
            queues[i].length = 0;
            queues[i].front = QUEUE_SIZE - 1;
            queues[i].back = queues[i].front;
        }
        // Create heap based priority queue if P
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
    // If no hierarical queues, do nothing.
    if (NO_OF_READY_QUEUES == 1) return process;
    else if (NO_OF_READY_QUEUES > 1)
    {
        process.contextSwitches--;
        // Determine if process has exceeded its context switches alotted
        // and check which direction it is travelling in queue
        if (process.contextSwitches == 0 && process.direction == "DOWN")
        {
            int i = ++process.queueNo;
            cout << "Process " << process.pid << " moved " << process.direction << " to queue " << process.queueNo << endl;
            // Check if process is now in the last queue in hierarchy
            // If it is, change its direction so it doesnt overflow downwards
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
            // Check if process is now in the top queue
            // If it is, change the direction
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
    // Check process to see what queue process should be placed in
    process = SetQueueNo(process);
    int i = process.queueNo;
cout << "Proccess to be enqueued in " << i << ": " << process.pid << endl;

    // Enqueue process in proper queue based on scheduling type
    if (cpuSchedulingPolicies[i].type == "RR" ||
        cpuSchedulingPolicies[i].type == "FCFS")
    {
        // Enqueue at back of circular queue for RR or FCFS
        queues[i].back = (queues[i].back + 1) % QUEUE_SIZE;
        queues[i].queue[queues[i].back] = process;
        queues[i].length ++;
    }
    else if (cpuSchedulingPolicies[i].type == "P")
    {
        // Enqueue on bottom of heap for P
        // Proagate up until its in the right place in queue
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
    // Loop through all ready queues, starting at the top
    for (int i = 0; i < NO_OF_READY_QUEUES; i++)
    {
        // If there is an element in the queue, this is where
        // we will dequeue from
        if (queues[i].length > 0)
        {
            // Remove from front of circular queue
            if (cpuSchedulingPolicies[i].type == "RR" ||
                cpuSchedulingPolicies[i].type == "FCFS")
            {
                queues[i].front = (queues[i].front + 1) % QUEUE_SIZE;
                queues[i].length--;
                toReturn = queues[i].queue[queues[i].front];
            }
            else if (cpuSchedulingPolicies[i].type == "P")
            {
                // Remove root, replace with last node in heap
                toReturn = queues[i].queue[1];
                queues[i].queue[1] = queues[i].queue[queues[i].length];
                queues[i].length--;
                processDescriptor temp = queues[i].queue[1];
                // Propagate new root down until it satisfies heap-order
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
            break; // Ensure no further queues are dequeued from
        }
    }
    return toReturn;
}
