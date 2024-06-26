#include "Drone.h"
#include <iostream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <random>
#include <chrono>
#include <hiredis/hiredis.h>


//The following three functions are defined as Redis Streams interaction functions

// Function to connect to Redis
redisContext* connectRedis(const char* hostname, int port) {
    redisContext* context = redisConnect(hostname, port);
    if (context == NULL || context->err) {
        if (context) {
            std::cerr << "Connection error: " << context->errstr << std::endl;
            redisFree(context);
        } else {
            std::cerr << "Connection error: cannot allocate redis context" << std::endl;
        }
        exit(1);
    }
    return context;
}

// Function to add an entry to a stream
void addEntry(redisContext* context, const char* stream, char* key, char* value) {
    redisReply* reply = (redisReply*) redisCommand(context, "XADD %s * %s %s", stream, key, value);
    if (reply == NULL) {
        std::cerr << "Failed to add entry to stream" << std::endl;
        return;
    }
    //std::cout << "Added entry with ID: " << reply->str << std::endl;
    freeReplyObject(reply);
}

// Function to read entries from a stream
void readEntries(redisContext* context, const char* stream) {
    redisReply* reply = (redisReply*) redisCommand(context, "XRANGE %s - +", stream);
    if (reply == NULL) {
        std::cerr << "Failed to read entries from stream" << std::endl;
        return;
    }
    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply* entry = reply->element[i];
        std::cout << "Entry ID: " << entry->element[0]->str << std::endl;
        for (size_t j = 1; j < entry->element[1]->elements; j += 2) {
            std::cout << " " << entry->element[1]->element[j]->str << ": " << entry->element[1]->element[j + 1]->str << std::endl;
        }
    }
    freeReplyObject(reply);
}

void readEntriesTest(redisContext* context, const char* stream) {
    redisReply* reply = (redisReply*) redisCommand(context, "XRANGE %s - +", stream);
    if (reply == NULL) {
        std::cerr << "Failed to read entries from stream" << std::endl;
        return;
    }
    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply* entry = reply->element[i];
        bool hasValueOne = false;

        // Check if any field has the value "1"
        for (size_t j = 1; j < entry->element[1]->elements; j += 2) {
            if (strcmp(entry->element[1]->element[j + 1]->str, "1") == 0) {
                hasValueOne = true;
                break;
            }
        }

        // If the entry has a field with value "1", print the entry
        if (hasValueOne) {
            std::cout << "Entry ID: " << entry->element[0]->str << std::endl;
            for (size_t j = 1; j < entry->element[1]->elements; j += 2) {
                std::cout << " " << entry->element[1]->element[j]->str << ": " << entry->element[1]->element[j + 1]->str << std::endl;
            }
        }
    }
    freeReplyObject(reply);
}


//string manipulation function
char* intToCharPtr(int num) {
    std::string temp = std::to_string(num);
    char* str = (char*)malloc((temp.length() + 1) * sizeof(char));
    if (str != nullptr) {
        strcpy(str, temp.c_str());
    }
    return str;
}

int randomRechargingTime() {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> distribution(72000, 108000);    

    int rechargingTime = distribution(gen);
    return rechargingTime;
}

int timeToTravel(int x, int y, int x1, int y1) {

    int tenthsOfSecond = 0;
    int diagonalMoves = 0;

    if (x != x1 and y != y1) {
        diagonalMoves = (abs(x-x1) > abs(y-y1)) ? (abs(y-y1)) : (abs(x-x1));
        int diagonalTravel = 17 * diagonalMoves;
        tenthsOfSecond += diagonalTravel;
    }

    if (not(abs(x-x1) == abs(y-y1))) {
        int linearTravel = 12 * ((abs(x-x1) > abs(y-y1)) ? (abs(x-x1)) - diagonalMoves : (abs(y-y1)) - diagonalMoves);
        tenthsOfSecond += linearTravel;
    }

    return tenthsOfSecond;
}

int createDrone(std::vector<Drone>* ptrToDrones, int currentTimestamp) {
    int id = ptrToDrones->size();
    ptrToDrones->push_back(Drone(id, currentTimestamp));
    return id;
}

void setDronesPathings(std::vector<Drone>* ptrToDrones) {
    int d = 0;
    Point center;
    center.x = 300;
    center.y = 300;
    
    for (int i = 1; i < 601; i+=2) {
        Point northern;
        Point southern;
        Point dest;
        northern.x = i;
        southern.x = i;
        northern.y = 0;
        southern.y = 600;
        dest.x = i;

        createDrone(ptrToDrones, 0);
        dest.y = 250;
        for (int count = 0; count < 2; count++) {
        (*ptrToDrones)[d].addDestination(northern);
        (*ptrToDrones)[d].addDestination(dest);
        }
        (*ptrToDrones)[d].addDestination(center);
        d++;
        
        createDrone(ptrToDrones, 0);
        dest.y = 350;
        for (int count = 0; count < 2; count++) {
        (*ptrToDrones)[d].addDestination(southern);
        (*ptrToDrones)[d].addDestination(dest);
        }
        (*ptrToDrones)[d].addDestination(center);
        d++;
    }

    for (int i = 251; i < 351; i+= 2) {
        Point western;
        Point eastern;
        Point dest;
        western.y = i;
        eastern.y = i;
        western.x = 250;
        eastern.x = 350;
        dest.y = i;

        createDrone(ptrToDrones, 0);
        dest.x = 0;
        (*ptrToDrones)[d].addDestination(western);
        for (int count = 0; count < 2; count++) {
        (*ptrToDrones)[d].addDestination(dest);
        (*ptrToDrones)[d].addDestination(western);
        }
        (*ptrToDrones)[d].addDestination(center);
        d++;

        createDrone(ptrToDrones, 0);
        dest.x = 600;
        (*ptrToDrones)[d].addDestination(eastern);
        for (int count = 0; count < 2; count++) {
        (*ptrToDrones)[d].addDestination(dest);
        (*ptrToDrones)[d].addDestination(eastern);
        }
        (*ptrToDrones)[d].addDestination(center);
        d++;
    }
    for (int i = 251; i < 351; i+=4) {
        Point topLeft;
        topLeft.x = i;
        topLeft.y = 250;
        Point topRight;
        topRight.x = i + 2;
        topRight.y = 250;
        Point bottomLeft;
        bottomLeft.x = i;
        bottomLeft.y = 350;
        Point bottomRight;
        bottomRight.x = i + 2;
        bottomRight.y = 350;

        createDrone(ptrToDrones, 0);
        (*ptrToDrones)[d].addDestination(topLeft);
        for (int count = 0; count < 4; count++) {
        (*ptrToDrones)[d].addDestination(topRight);
        (*ptrToDrones)[d].addDestination(bottomRight);
        (*ptrToDrones)[d].addDestination(bottomLeft);
        (*ptrToDrones)[d].addDestination(topLeft);
        }
        (*ptrToDrones)[d].addDestination(center);
        d++;
    }
}

void setTimestamps(std::vector<int>* ptrToCheckTimestamps, int seconds) {
    const int timeLimit = seconds; //total amount of simulation time expressed in tenths of second
    int i = 1;
    std::unordered_set<int> addedNumbers;  // Set to keep track of added numbers

    while (true) {
        int product_1_2 = i * 12;
        int product_1_7 = i * 17;
        if (product_1_2 > timeLimit) {
            break;
        }
        if (addedNumbers.find(product_1_2) == addedNumbers.end()) {
            (*ptrToCheckTimestamps).push_back(product_1_2);
            addedNumbers.insert(product_1_2);
        }
        if (product_1_7 <= timeLimit) {
            if (addedNumbers.find(product_1_7) == addedNumbers.end()) {
                (*ptrToCheckTimestamps).push_back(product_1_7);
                addedNumbers.insert(product_1_7);
            }
        }
        i++;
    }
    std::sort((*ptrToCheckTimestamps).begin(), (*ptrToCheckTimestamps).end());
}

void moveToStartingPositions(std::vector<Drone>* ptrToDrones) {
    while (true) {
        bool a = true;
        for (Drone& drone : (*ptrToDrones)) {
            if (!drone.isActive()) {
                drone.activate(0);
            }
            if ((drone).getX() != (drone).getPath()[0].x || (drone).getY() != (drone).getPath()[0].y) {
                (drone).move();
                a = false;
            }
        }
        if (a) {
            break;
        }
    }
}

/*
void visualizeIntVector(std::vector<int>* ptrToVector) {
    if ((*ptrToVector).size() == 0) {
        std::cout << "vector is empty";
    }
    else {
        for (int i : (*ptrToVector)) {
            std::cout << i << ", ";
        }
    }
    std::cout << std::endl;
}
*/

int replacingTime(Drone* ptrToDrone) {
    Drone drone = *ptrToDrone;
    if (drone.getPath().size() == 18) {
        return 12000 - timeToTravel(300,300,(drone.getPath()[0].x),(drone.getPath()[0].y));
    }
    else {
        int res = 0;
        for (int i = 0; i < drone.getPath().size() - 2; i++) {
            res += timeToTravel(drone.getPath()[i].x, drone.getPath()[i].y, drone.getPath()[i+1].x, drone.getPath()[i+1].y);
        }
        return res - timeToTravel(300,300,(drone.getPath()[0].x),(drone.getPath()[0].y));
    }
}

bool droneShouldStall(Drone* ptrToDrone) {
    Drone drone = *ptrToDrone;
    Point p = drone.getPath()[0];
    if (drone.getX() == p.x && drone.getY() == p.y) {
        if (drone.getNextDestinationIndex() != 1 && drone.getNextDestinationIndex() != 17) {
            return true;
        }
        return false;
    }
    return false;
}

void insertInSortedVector(std::vector<int>& vec, int num) {
    auto it = std::lower_bound(vec.begin(), vec.end(), num);
    vec.insert(it, num);
}

void runSimulation(int seconds) {

    const char* hostname = "127.0.0.1";  // Localhost IP address
    int port = 6379;                    // Default Redis port

    redisContext* ptrToRedisContext = connectRedis(hostname, port);
    const char* deadDronesStream = "dds";

    bool space[601][601];
    memset(space, false, sizeof(space));

    std::vector<Drone> drones;
    std::vector<Drone>* ptrToDrones = &drones;
    std::unordered_set<int> idleDrones = {};
    std::unordered_set<int> deadDrones = {};
    setDronesPathings(ptrToDrones);
    moveToStartingPositions(ptrToDrones);


    int startingPositionsTimestamp = (timeToTravel(300,300,1,0));
    int timeSinceStart = (timeToTravel(300,300,1,0));
    int timeSinceLastVerification = 0;
    int epoch = 1;

    std::vector<int> checkTimestamps;
    std::unordered_set<int> addedTimestamps = {};
    checkTimestamps.push_back(timeSinceStart);
    std::unordered_map<int, std::unordered_set<int>> dronesToMove;
    std::unordered_map<int, std::unordered_set<int>> dronesToReplace;
    std::unordered_map<int, std::unordered_set<int>> dronesDoneCharging;


    //update dronesToReplace hashmap with eventual new timestamps
    for (int i = 0; i < drones.size(); i++) {
        drones[i].verify(space);

        dronesToMove[timeSinceStart].insert(i);

        int replacingTimestamp = timeSinceStart + replacingTime(&(drones[i]));

        if (addedTimestamps.find(replacingTimestamp) == addedTimestamps.end()) {
            insertInSortedVector(checkTimestamps, replacingTimestamp);
            addedTimestamps.insert(replacingTimestamp);
        }

        dronesToReplace[replacingTimestamp].insert(i);
    }



    while (timeSinceStart < seconds * 10 + startingPositionsTimestamp) {

        //handle drones done charging
        std::unordered_set<int> chargedDrones = dronesDoneCharging[timeSinceStart];
        if (!(chargedDrones.empty())) {
            for (int chargedDroneIndex : chargedDrones) {
                idleDrones.insert(chargedDroneIndex);
            }
        }

        std::unordered_set<int> dyingDrones = dronesToReplace[timeSinceStart];
        if (!(dyingDrones.empty())) {
            for (int droneIndex : dyingDrones) {
                Drone& drone = drones[droneIndex];
                std::vector<Point> dronePath = drone.getPath();
                if (dronePath.empty()) {
                    std::cerr << "ERR: dronePath can never be empty" << std::endl;
                }
                int indexToReplacingDrone;
                if (idleDrones.empty()) {
                    indexToReplacingDrone = createDrone(&drones, timeSinceStart);
                }
                else {
                    auto it = idleDrones.begin();
                    indexToReplacingDrone = *it;
                    idleDrones.erase(it);
                }
                Drone& replacingDrone = drones[indexToReplacingDrone];
                replacingDrone.activate(timeSinceStart);
                for (Point p : dronePath) {
                    replacingDrone.addDestination(p);
                }
                dronesToMove[timeSinceStart].insert(indexToReplacingDrone);

                int nextReplacingTimestamp = timeSinceStart + replacingTime(&replacingDrone) + timeToTravel(300, 300, (replacingDrone.getPath()[0]).x, (replacingDrone.getPath()[0]).y);
                dronesToReplace[nextReplacingTimestamp].insert(indexToReplacingDrone);

                if (addedTimestamps.find(nextReplacingTimestamp) == addedTimestamps.end()) {
                    insertInSortedVector(checkTimestamps, nextReplacingTimestamp);
                    addedTimestamps.insert(nextReplacingTimestamp);
                }

            }
        }

        std::unordered_set<int> movingDrones = dronesToMove[timeSinceStart];
        for (int droneIndex : movingDrones) {
            Drone& drone = drones[droneIndex];
            if (drone.getDeadBatteryTimestamp() != -1 && drone.getDeadBatteryTimestamp() <= timeSinceStart && (deadDrones.find(droneIndex) == deadDrones.end())) {
                int id = drones[droneIndex].getId();
                char* key = intToCharPtr(id);
                char value[] = "1";
                addEntry(ptrToRedisContext, deadDronesStream, key, value);
                deadDrones.insert(droneIndex);
                drone.shutDown();
            }
            if (!drone.isActive()) {
                continue;
            }

            int movementTime = drone.move();
            drone.verify(space);

            if (!drone.isActive()) {
                int chargingTimestamp = timeSinceStart + randomRechargingTime();
                dronesDoneCharging[chargingTimestamp].insert(droneIndex);

                if (addedTimestamps.find(chargingTimestamp) == addedTimestamps.end()) {
                    insertInSortedVector(checkTimestamps, chargingTimestamp);
                    addedTimestamps.insert(chargingTimestamp);
                }

            }
            
            int newTimestamp;
            if (drone.getPath().size() == 18 && droneShouldStall(&drone)) {
                newTimestamp = timeSinceStart + movementTime + 552;
            }
            else {
                newTimestamp = timeSinceStart + movementTime;
            }

            if (addedTimestamps.find(newTimestamp) == addedTimestamps.end()) {
                    insertInSortedVector(checkTimestamps, newTimestamp);
                    addedTimestamps.insert(newTimestamp);
            }

            dronesToMove[newTimestamp].insert(droneIndex);
        }
        
        dronesToMove.erase(timeSinceStart);
        
        if (checkTimestamps.empty()) {
            break;
        }
        checkTimestamps.erase(checkTimestamps.begin());
        timeSinceStart = checkTimestamps[0];

        if ((timeSinceStart - startingPositionsTimestamp) % 3000 == 0 && timeSinceStart != startingPositionsTimestamp) {
            
            /*
                - Remove other dead drones checks
                - Fetch all dead drones data
                - Add all entries to Redis' "mystream"
                - Read entries and call montior funtion
            */

            for (int i = 0; i < drones.size(); i++) {
                if (deadDrones.find(i) != deadDrones.end()) {
                    continue;
                }
                int id = drones[i].getId();
                char* key = intToCharPtr(id);
                char value[] = "0";
                addEntry(ptrToRedisContext, deadDronesStream, key, value);
            }

            std::cout << "Checking epoch " << epoch << "...\t";
            int errors = 0;
            for (int i = 0; i < 601; i++) {
                for (int j = 0; j < 601; j++) {
                    if (!space[i][j]) {
                        errors++;
                    }
                }
            }
            if (errors == 0) {
                std::cout << "All points are verified at epoch " << epoch  << ", time elapsed:\t" << (timeSinceStart) / 36000 << " hours\t" << ((timeSinceStart) % 36000) / 600 << " minutes\t" << float((timeSinceStart) % 600) / 10 << " seconds" << std::endl;
            }
            else {
                std::cout << "ERROR: " << errors << " points were not verified at epoch " << epoch << std::endl;
            }
            std::cout << "The number of drones at the end of epoch " << epoch << " is " << drones.size() << std::endl;
            if (deadDrones.size() > 0) {
                std::cout << "ERROR: " << deadDrones.size() << " drones have died during coverage:" << std::endl;
                readEntriesTest(ptrToRedisContext, deadDronesStream);
            }
            else {
                std::cout << "No drones have died during coverage" << std::endl;
            }

            std::cout << "\n\n" << std::endl;
            memset(space, false, sizeof(space));
            epoch++;
            for (Drone& drone : drones) {
                if (drone.isActive()) {
                    drone.verify(space);
                }
            }
        }
    }
}


int main() {
    runSimulation(3000);
    return 0;
}
