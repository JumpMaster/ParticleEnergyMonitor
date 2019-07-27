#ifndef __PRESENCEMANAGER_H__
#define __PRESENCEMANAGER_H__
#define isMaster() nodeID > 0 && nodeID == masterID

#include "application.h"
#include "publishqueue.h"

typedef struct User
{
  public:
    int id;
    char name[20];
	long lastUpdated = 1;
	char location[20];
} User;

class PresenceManager
{
  public:
    PresenceManager();
    void subscribe();
    void process();
    void handler(const char *eventName, const char *data);
    void syncRequest();
    bool isAnyone(const char *location);
    bool isEveryone(const char *location);
    bool isUser(const char *name, const char *location);
    char *whereIs(const char *name);
    bool hasSync;
  protected:
    const unsigned long masterCallbackTime = 60000; // 1 minutes
    void PublishUsers();
    void PublishUser(User *user);
    void ExportUser(const char *name, const char *location);
    std::vector<User> users;
    unsigned long lastMasterUpdate;
    int nodeID;
    int masterID;
    unsigned int clusterUsers;
};

#endif  // End of __PRESENCEMANAGER_H__ definition check