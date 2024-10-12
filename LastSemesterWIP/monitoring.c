/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "monitoring.h"
#include "discovery.h"

sleepingPC* clientsList = NULL;
pthread_mutex_t mutexLock;

void AddNewClient(char* ip, char* mac, char* message)
{
    sleepingPC* newSleepingPC = malloc(sizeof(sleepingPC));

    newSleepingPC->ip = ip;
    newSleepingPC->mac = mac;
    newSleepingPC->lastMessage = message;

    pthread_mutex_lock(&mutexLock);
    if(clientsList == NULL)
    {
       clientsList = newSleepingPC;
    }
    else
    {
        sleepingPC* lastPC = GetLastMachine(clientsList);
        lastPC->next = newSleepingPC;
    }
    pthread_mutex_unlock(&mutexLock);
}

void RemoveClient(char* ip, char* mac)
{
    pthread_mutex_lock(&mutexLock);
    sleepingPC* parent = FindParentPC(ip, mac, *clientsList);
    if(parent->next->next != NULL)
    {
        sleepingPC* pcToRemove = parent->next;
        parent->next = parent->next->next;
        free(pcToRemove);
    }
    else
    {
        free(parent->next);
        parent->next = NULL;
    }
    pthread_mutex_unlock(&mutexLock);
}

sleepingPC* FindParentPC(char* ip, char* mac, sleepingPC rootSleepingPC)
{
    if(clientsList != NULL)
    {
        if(clientsList->next != NULL)
            if(strcmp(clientsList->next->ip, ip) == 0 && strcmp(clientsList->next->mac, mac))
                return clientsList;
    }

    return NULL;
}

sleepingPC* GetLastMachine(sleepingPC* root)
{
    if(root->next != NULL)
        return GetLastMachine(root->next);
    else
        return root;
}

void PrintAllClients()
{
    PrintClients(clientsList, 0);
}

void PrintClients(sleepingPC* rootSleepingPC, int counter)
{
    if(rootSleepingPC != NULL)
    {
        printf("\n%d IP: %s   STATUS: %s \n", counter, rootSleepingPC->ip, rootSleepingPC->lastMessage);
        if(rootSleepingPC->next != NULL)
            PrintClients(rootSleepingPC->next, counter++);
    }
}

void WakePC(sleepingPC* client)
{
    printf("Sending wakeup signal to %s", client->ip);
    char* magicPackage = "";

    for (size_t i = 0; i < 16; i++)
    {
        strcat(magicPackage, client->mac);
    }
    
    strcat(magicPackage, MAGIC_PACKAGE_SUFIX);

    SendMessage(magicPackage, client->ip, client->port);
}