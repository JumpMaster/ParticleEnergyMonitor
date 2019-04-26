#include "presencemanager.h"
#include "application.h"
#include "ArduinoJson.h"

extern PublishQueue pq;

PresenceManager::PresenceManager() { }

void PresenceManager::subscribe() {
    bool result = Particle.subscribe("presence", &PresenceManager::handler, this, MY_DEVICES);

    /*
    if (result)
        pq.Publish("LOG", "subscribe succeeded");
    else
        pq.Publish("LOG", "subscribe failed");
    */
}

void PresenceManager::syncRequest() {
    pq.publish("presence", "{\"method\":\"user\",\"type\":\"publish\",\"reason\":\"sync\"}");
}

void PresenceManager::process() {
    if (isMaster() && millis() > lastMasterUpdate+masterCallbackTime) {
        // for (int i = 0; i < users.size(); i++)
	       // ExportUser(users[i].name, users[i].location);
	        
        //pq.publish("presence", "{\"method\":\"user\",\"type\":\"publish\",\"reason\":\"master\"}");
        pq.publish("presence", "{\"method\":\"cluster\",\"type\":\"masterping\"}");
        lastMasterUpdate = millis();
    } else {
        if (millis() > lastMasterUpdate+(masterCallbackTime*2)) {
            pq.publish("presence", "{\"method\":\"cluster\",\"type\":\"election\"}");
            lastMasterUpdate = millis() + random(10000);
        }
    }
}

void PresenceManager::PublishUser(User *user) {
    pq.publish("presence", String::format("{\"method\":\"user\",\"type\":\"update\",\"user\":\"%s\",\"location\":\"%s\",\"updated\":%d}", user->name, user->location, user->lastUpdated));
}

void PresenceManager::PublishUsers() {
    for (int i = 0; i < users.size(); i++)
        PublishUser(&users[i]);
}

void PresenceManager::ExportUser(const char *name, const char *location) {
    char data[256];
	snprintf(data, sizeof(data), "{\"user\":\"%s\",\"location\":\"%s\"}", name, location);
    pq.publish("locationData", data);
}

void PresenceManager::handler(const char *eventName, const char *data) {

    StaticJsonBuffer<300> jsonBuffer;
    JsonObject& jsonRoot = jsonBuffer.parseObject(data);

    // Test if parsing succeeds.
    if (!jsonRoot.success()) {
        pq.publish("LOG", "parseObject() failed");
        return;
    }
    
    const char* method = jsonRoot["method"];

    //### METHOD == USER ###
    if (strcmp(method, "user") == 0) {
        const char* requestType = jsonRoot["type"];
        
        //### TYPE == UPDATE ###
        if (strcmp(requestType, "update") == 0) {
            const char* name = jsonRoot["user"];
            const char* location = jsonRoot["location"];
            long updated = jsonRoot["updated"];
            
            bool new_user = true;
            
            if (updated == 0) {
                updated = Time.now();
                if (isMaster())
                    ExportUser(name, location);
            }

            for (int i = 0; i < users.size(); i++) {
                if (strcmp(users[i].name, name) == 0) {
                    if (updated > users[i].lastUpdated) {
                        users[i].lastUpdated = updated;
                        strcpy(users[i].location, location);
                    } else if (updated < users[i].lastUpdated)
                        PublishUser(&users[i]);
                    new_user = false;
                }
            }
            
            if (new_user) {
                User new_user;
                strcpy(new_user.name, name);
                strcpy(new_user.location, location);
                new_user.lastUpdated = updated;
                users.push_back(new_user);
                if (!hasSync && clusterUsers == users.size())
                    hasSync = true;
            }
            
            //pq->publish("LOG", String::format("User %s is at %s at %d", name, location, updated));
        
        //### TYPE == REMOVE ###
        } else if (strcmp(requestType, "remove") == 0) {
            const char* name = jsonRoot["user"];
        
            for (int i = 0; i < users.size(); i++) {
                if (strcmp(users[i].name, name) == 0) {
                    users.erase(users.begin() + i);
                }
            }
        
        //### TYPE == PUBLISH ###
        } else if (strcmp(requestType, "publish") == 0) {// && isMaster()) {
            // const char* requestReason = jsonRoot["reason"];
            // if (strcmp(requestReason, "master") == 0)
                // lastMasterUpdate = millis()  + random(10000);
            // if (strcmp(requestReason, "sync") == 0 && isMaster()) {
            if (isMaster()) {
                char data[256];
            	snprintf(data, sizeof(data), "{\"method\":\"user\",\"type\":\"count\",\"total\":%d}", users.size());
                pq.publish("presence", data);
            }
            // }
                
            if (users.size() > 0)
                PublishUsers();
        
        //### TYPE == COUNT ###
        } else if (strcmp(requestType, "count") == 0 && clusterUsers == 0) {
            clusterUsers = jsonRoot["total"];
            
            if (!hasSync && clusterUsers == users.size())
                    hasSync = true;
        }

    
    //### METHOD == CLUSTER ###
    } else if (strcmp(method, "cluster") == 0) {
        const char* requestType = jsonRoot["type"];
        
        //### TYPE == MASTER PING ###
        if (strcmp(requestType, "masterping") == 0) {
            lastMasterUpdate = millis() + random(10000);
        //### TYPE == ELECTION ###
        } else if (strcmp(requestType, "election") == 0) {
            lastMasterUpdate = millis() + random(10000);
            nodeID = random(100,999);
            masterID = nodeID;
            
            char data[256];
	        snprintf(data, sizeof(data), "{\"method\":\"cluster\",\"type\":\"vote\",\"nodeid\":%d,\"deviceID\":\"%s\"}", nodeID, System.deviceID().c_str());
            pq.publish("presence", data);
        
        //### TYPE == VOTE ###
        } else if (strcmp(requestType, "vote") == 0) {
            int ID = jsonRoot["nodeid"];
            const char* deviceID = jsonRoot["deviceID"];
            if (ID == nodeID && strcmp(deviceID, System.deviceID()) != 0) {
                pq.publish("presence", "{\"method\":\"cluster\",\"type\":\"election\"}");
            } else if (ID < masterID)
                masterID = ID;
                
        //### TYPE == STATUS ###
        } else if (strcmp(requestType, "status") == 0) {
            char data[256];
	        snprintf(data, sizeof(data), "{\"method\":\"response\",\"type\":\"status\",\"nodeID\":%d,\"isMaster\":%d}", nodeID, isMaster());
            pq.publish("presence", data);
        }
    }
}

bool PresenceManager::isAnyone(const char *location) {
    for (int i = 0; i < users.size(); i++) {
        if (strcmp(users[i].location, location) == 0)
            return true;
    }
    
    return false;
}

bool PresenceManager::isEveryone(const char *location) {
    for (int i = 0; i < users.size(); i++) {
        if (strcmp(users[i].location, location) != 0)
            return false;
    }
    
    return true;
}

bool PresenceManager::isUser(const char *user, const char *location) {
    for (int i = 0; i < users.size(); i++) {
        if (strcmp(users[i].name, user) == 0 && strcmp(users[i].location, location) == 0)
            return true;
    }
    
    return false;
}

char *PresenceManager::whereIs(const char *name) {
    for (int i = 0; i < users.size(); i++) {
        if (strcmp(users[i].name, name) == 0)
            return users[i].location;
    }
    
    return "";
}