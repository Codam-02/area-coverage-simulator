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
#include <libpq-fe.h>

//The following 5 functions are designed to interact with a locally hosted postgresql database

void do_exit(PGconn *conn) {
    PQfinish(conn);
    exit(1);
}

void create_table(const char* conninfo, const char* table_name, const char* col_names[], int col_count) {
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) == CONNECTION_BAD) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        do_exit(conn);
    }

    // Construct the CREATE TABLE query
    std::string create_table_query = "CREATE TABLE " + std::string(table_name) + " (";
    for (int i = 0; i < col_count; ++i) {
        if (i == 0) {
            create_table_query += std::string(col_names[i]) + " SERIAL PRIMARY KEY";
        } else {
            create_table_query += std::string(col_names[i]) + " VARCHAR(50)";
        }
        if (i < col_count - 1) create_table_query += ", ";
    }
    create_table_query += ")";

    // Execute the CREATE TABLE query
    PGresult *res = PQexec(conn, create_table_query.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Create table failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        do_exit(conn);
    }
    PQclear(res);

    PQfinish(conn);
}

void add_row(const char* conninfo, const char* table_name, const char* col_names[], const char* values[], int col_count) {
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) == CONNECTION_BAD) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        do_exit(conn);
    }

    // Construct the INSERT query
    std::string insert_query = "INSERT INTO " + std::string(table_name) + " (";
    for (int i = 0; i < col_count; ++i) {
        insert_query += col_names[i];
        if (i < col_count - 1) insert_query += ", ";
    }
    insert_query += ") VALUES (";
    for (int i = 0; i < col_count; ++i) {
        insert_query += "'" + std::string(values[i]) + "'";
        if (i < col_count - 1) insert_query += ", ";
    }
    insert_query += ")";

    // Execute the INSERT query
    PGresult *res = PQexec(conn, insert_query.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Insert failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        do_exit(conn);
    }
    PQclear(res);

    PQfinish(conn);
}


void read_values(const char* conninfo, const char* table_name) {
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) == CONNECTION_BAD) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        do_exit(conn);
    }

    std::string query = "SELECT * FROM " + std::string(table_name);
    PGresult *res = PQexec(conn, query.c_str());

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        do_exit(conn);
    }

    int nFields = PQnfields(res);

    for (int i = 0; i < PQntuples(res); i++) {
        for (int j = 0; j < nFields; j++) {
            std::cout << PQfname(res, j) << ": " << PQgetvalue(res, i, j) << "\n";
        }
        std::cout << std::endl;
    }

    PQclear(res);
    PQfinish(conn);
}

void erase_data(const char* conninfo) {
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) == CONNECTION_BAD) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        do_exit(conn);
    }

    // Query to get all table names in the public schema
    const char* query = "SELECT tablename FROM pg_tables WHERE schemaname = 'public'";
    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Fetch tables failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        do_exit(conn);
    }

    int nTables = PQntuples(res);

    for (int i = 0; i < nTables; i++) {
        std::string table_name = PQgetvalue(res, i, 0);
        std::string drop_query = "DROP TABLE IF EXISTS " + table_name + " CASCADE";
        PGresult *drop_res = PQexec(conn, drop_query.c_str());

        if (PQresultStatus(drop_res) != PGRES_COMMAND_OK) {
            std::cerr << "Drop table failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(drop_res);
            do_exit(conn);
        }

        PQclear(drop_res);
    }

    PQclear(res);
    PQfinish(conn);
}

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
    
    freeReplyObject(reply);
}

//Function to add a positive entry to deadDronesStream
void addDeadDroneEntry(redisContext* context, const char* stream, char* key) {
    char value[] = "1";
    addEntry(context, stream, key, value);
}

//This function converts a string (char*) to int
int charPtrToInt(const char* str) {
    if (str == nullptr) {
        std::cerr << "Invalid input: null pointer" << std::endl;
        return 0;
    }
    return atoi(str);
}

//This function reads data from the deadDronesStream and visualizes a console log
void deadDronesMonitor(redisContext* context) {
    char stream[] = "dds";
    redisReply* reply = (redisReply*) redisCommand(context, "XRANGE %s - +", stream);
    if (reply == NULL) {
        std::cerr << "Failed to read entries from stream" << std::endl;
        return;
    }

    if (reply->elements == 0) {
        std::cout << "OK! No drones have died in this epoch" << std::endl;
    }
    else {
        std::cout << "ERROR! " << reply->elements << " drones have died during this epoch" << std::endl;
    }

    (redisReply*) redisCommand(context, "DEL %s", stream);
    freeReplyObject(reply);
}

//This function reads data from the rechargingDronesStream and visualizes a console log
void chargingDronesMonitor(redisContext* context) {
    char stream[] = "rds";
    redisReply* reply = (redisReply*) redisCommand(context, "XRANGE %s - +", stream);
    if (reply == NULL) {
        std::cerr << "Failed to read entries from stream" << std::endl;
        return;
    }

    if (reply->elements == 0) {
        std::cout << "No drones are currently charging" << std::endl;
    }
    else {
        std::cout << reply->elements << " drones are currently charging" << std::endl;
    }

    (redisReply*) redisCommand(context, "DEL %s", stream);
    freeReplyObject(reply);
}

//This function reads data from the pointCoverageStream and visualizes a console log
void pointCoverageMonitor(redisContext* context, int epoch) {
    char stream[] = "pcs";
    redisReply* reply = (redisReply*) redisCommand(context, "XRANGE %s - +", stream);
    if (reply == NULL) {
        std::cerr << "Failed to read entries from stream" << std::endl;
        return;
    }

    size_t i = epoch - 1;
    redisReply* entry = reply->element[i];

    const char* value = entry->element[1]->element[1]->str;
    if (strcmp(value,"0") == 0) {
        std::cout << "OK! All points were successfully covered by drones" << std::endl;
    }
    else {
        std::cout << "ERROR! " << value << " points were not covered by drones" << std::endl;
    }
    
    freeReplyObject(reply);
}

//This function reads data from the rechargingTimersStream and visualizes a console log
void chargingTimersMonitor(redisContext* context, bool dronesEnteredCharging) {
    char stream[] = "rts";

    if (!dronesEnteredCharging) {
        std::cout << "OK! No drones entered charging phase during this epoch" << std::endl;
        (redisReply*) redisCommand(context, "DEL %s", stream); 
        return;
    }

    redisReply* reply = (redisReply*) redisCommand(context, "XRANGE %s - +", stream);
    if (reply == NULL) {
        std::cerr << "Failed to read entries from stream" << std::endl;
        return;
    }

    bool pass = true;

    for (size_t i = 0; i < reply->elements; i++) {
        redisReply* entry = reply->element[i];
        int value = charPtrToInt(entry->element[1]->element[1]->str);
        if (value < 72000 || value > 108000) {
            pass = false;
            break;
        }
    }

    if (pass) {
        std::cout << "OK! All recharging timers are between 2 and 3 hours" << std::endl;
    }
    else {
        std::cout << "ERROR! Recharging timers are not between 2 and 3 hours" << std::endl;
    }

    (redisReply*) redisCommand(context, "DEL %s", stream); 
    freeReplyObject(reply);
}

//This function converts a int to string (char*) and returns its pointer
char* intToCharPtr(int num) {
    std::string temp = std::to_string(num);
    char* str = (char*)malloc((temp.length() + 1) * sizeof(char));
    if (str != nullptr) {
        strcpy(str, temp.c_str());
    }
    return str;
}

//This function generates a random value between 2 and 3 hours (in tenths of second)
int randomRechargingTime() {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> distribution(72000, 108000);    

    int rechargingTime = distribution(gen);
    return rechargingTime;
}

//This function takes two point-coordinates as arguments and returns the time to travel from one to another at 30Km/h (in tenths of second)
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

//Creates a new drone, assigns it an id and adds it to the drone vector
int createDrone(std::vector<Drone>* ptrToDrones, int currentTimestamp) {
    int id = ptrToDrones->size();
    ptrToDrones->push_back(Drone(id, currentTimestamp));
    return id;
}

//Called on a vector of 725 drones, this function sets optimal paths for each drone for them to cover a 6Km x 6Km space
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

//low-level function that handles timestamp implementation and distribution
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

//This function activates all 725 drones for them to move towards their initial-coverage position
void moveToStartingPositions(std::vector<Drone>* ptrToDrones) {
    while (true) {
        bool a = true;
        for (Drone& drone : (*ptrToDrones)) {
            if (!drone.isActive()) {
                drone.activate(0);
            }
            if ((drone).getX() != (drone).getPath()[0].x || (drone).getY() != (drone).getPath()[0].y) {
                (drone).move(-1);
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

//returns timestamp (in tenths of second) in which a new drone should leave the control center to replace the drone that ptrToDrone points to
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

//returns true if the drone that ptrToDrone points to should stall to sync with other drones
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

//vector manipulation functioj
void insertInSortedVector(std::vector<int>& vec, int num) {
    auto it = std::lower_bound(vec.begin(), vec.end(), num);
    vec.insert(it, num);
}

//main function, handles all sections of the simulation
void runSimulation(int seconds) {

    //postgresql database connection info
    const char* conninfo = "dbname=drones_database user=codam password=1234 hostaddr=127.0.0.1 port=5432";
    erase_data(conninfo);

    const char* pointCoverageTable = "pct";
    const char* pctCols[] = {"epoch", "output"};
    create_table(conninfo, pointCoverageTable, pctCols, 2);

    const char* hostname = "127.0.0.1";  // Localhost IP address
    int port = 6379;                    // Default Redis port

    redisContext* ptrToRedisContext = connectRedis(hostname, port);

    const char* deadDronesStream = "dds";
    const char* rechargingDroneStream = "rds";
    const char* pointCoverageStream = "pcs";
    const char* rechargingTimersStream = "rts";

    (redisReply*) redisCommand(ptrToRedisContext, "DEL %s", deadDronesStream);          //
    (redisReply*) redisCommand(ptrToRedisContext, "DEL %s", rechargingDroneStream);     // Flush all Redis streams
    (redisReply*) redisCommand(ptrToRedisContext, "DEL %s", pointCoverageStream);       // objects to avoid data overwriting
    (redisReply*) redisCommand(ptrToRedisContext, "DEL %s", rechargingTimersStream);    //

    std::vector<Drone> drones;
    std::vector<Drone>* ptrToDrones = &drones;

    std::unordered_set<int> idleDrones = {}; //contains indexes of drones that are done charging

    std::unordered_set<int> deadDrones = {}; //contains indexes of drones that have dead batteries

    setDronesPathings(ptrToDrones); //sets path for each drone
    moveToStartingPositions(ptrToDrones); //gets all drones to their initial coverage position


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
    std::unordered_map<int, int> chargeCompleteAt;
    std::unordered_map<int, std::unordered_map<int, bool>> pointIsVerified;

    bool dronesAreCharging = false;
    bool dronesEnteredCharging = false;


    //set a replacing time for each drone and save it in 'dronesToReplace'
    for (int i = 0; i < drones.size(); i++) {
        drones[i].verify(&pointIsVerified);

        dronesToMove[timeSinceStart].insert(i);

        int replacingTimestamp = timeSinceStart + replacingTime(&(drones[i]));

        if (addedTimestamps.find(replacingTimestamp) == addedTimestamps.end()) {
            insertInSortedVector(checkTimestamps, replacingTimestamp);
            addedTimestamps.insert(replacingTimestamp);
        }

        dronesToReplace[replacingTimestamp].insert(i);
    }


    //This loop iterates through all timestamps and performs all functions due in runtime
    while (timeSinceStart < seconds * 10 + startingPositionsTimestamp) {

        //handle drones done charging in current timestamp
        std::unordered_set<int> chargedDrones = dronesDoneCharging[timeSinceStart];
        if (!(chargedDrones.empty())) {
            for (int chargedDroneIndex : chargedDrones) {
                idleDrones.insert(chargedDroneIndex);
                drones[chargedDroneIndex].ready();
            }
        }

        //handles drones that need replacement in current timestamp
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
                    indexToReplacingDrone = createDrone(&drones, timeSinceStart); //if there's no idle drones, a new one is created
                }
                else {
                    auto it = idleDrones.begin();
                    indexToReplacingDrone = *it;                                  //otherwise one of the idle drones is selected
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

        //hanldes drone movement in current timestamp
        std::unordered_set<int> movingDrones = dronesToMove[timeSinceStart];
        for (int droneIndex : movingDrones) {
            Drone& drone = drones[droneIndex];
            if (drone.getDeadBatteryTimestamp() != -1 && drone.getDeadBatteryTimestamp() <= timeSinceStart && (deadDrones.find(droneIndex) == deadDrones.end())) {
                int id = drones[droneIndex].getId();
                char* key = intToCharPtr(id);
                addDeadDroneEntry(ptrToRedisContext, deadDronesStream, key);
                deadDrones.insert(droneIndex);
                drone.shutDown();
            }
            if (!drone.isActive()) {
                continue;
            }

            int movementTime = drone.move(timeSinceStart);
            drone.verify(&pointIsVerified);

            if (!drone.isActive()) {
                int chargingTimestamp = timeSinceStart + randomRechargingTime();
                addEntry(ptrToRedisContext, rechargingTimersStream, intToCharPtr(drone.getId()), intToCharPtr(chargingTimestamp - timeSinceStart));
                dronesDoneCharging[chargingTimestamp].insert(droneIndex);
                chargeCompleteAt[droneIndex] = chargingTimestamp;
                dronesEnteredCharging = true;

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
            
            for (int i = 0; i < drones.size(); i++) {
                Drone& drone = drones[i];
                int id = drone.getId();
                if (drone.isCharging()) {
                    char* key = intToCharPtr(id);
                    char* value = intToCharPtr((timeSinceStart-drone.getChargingTimestamp())*100/(chargeCompleteAt[i]-drone.getChargingTimestamp()));
                    addEntry(ptrToRedisContext, rechargingDroneStream, key, value);
                    dronesAreCharging = true;
                }
            }

            int nonCoveredPoints = 0;
            for (int i = 0; i < 601; i++) {
                for (int j = 0; j < 601; j++) {
                    if (!pointIsVerified[i][j]) {
                        nonCoveredPoints++;
                    }
                }
            }

            std::cout << "EPOCH " << epoch << "\n";

            char* key = intToCharPtr(epoch);
            char* value = intToCharPtr(nonCoveredPoints);
            addEntry(ptrToRedisContext, pointCoverageStream, key, value);
            const char* values[] = {(intToCharPtr(epoch)), (nonCoveredPoints == 0 ? "pass" : "fail")};
            add_row(conninfo, pointCoverageTable, pctCols, values, 2);
            
            std::cout << "Time since simulation start:\t" << (timeSinceStart) / 36000 << " hours\t" << ((timeSinceStart) % 36000) / 600 << " minutes\t" << std::endl;

            std::cout << "The number of drones at the end of epoch " << epoch << " is " << drones.size() << "\n" << std::endl;

            chargingDronesMonitor(ptrToRedisContext);
            chargingTimersMonitor(ptrToRedisContext, dronesEnteredCharging);
            pointCoverageMonitor(ptrToRedisContext, epoch);
            deadDronesMonitor(ptrToRedisContext);

            deadDrones = {};
            dronesAreCharging = false;
            dronesEnteredCharging = false;

            std::cout << "\n\n" << std::endl;
            pointIsVerified.clear();
            (redisReply*) redisCommand(ptrToRedisContext, "DEL %s", rechargingDroneStream);

            epoch++;
            for (Drone& drone : drones) {
                if (drone.isActive()) {
                    drone.verify(&pointIsVerified);
                }
            }
        }
    }
}


int main() {
    runSimulation(1500);
    return 0;
}
