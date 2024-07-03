#pragma once
#include "Point.h"
#include <vector>
#include <iostream>
#include <unordered_map>

class Drone{
    public:

        Drone (int _id, int _currentTimestamp) : id(_id), deadBatteryTimestamp(_currentTimestamp + 18000) {}

        int getX() {
            return x;
        }


        int getY() {
            return y;
        }

        void visualizePath() {
            std::cout << "[" ;
            for (Point p : path) {
                std::cout << "(" << p.x << "," << p.y << "),";
            }
            std::cout << "]\n";
        }

        int getId() {
            return id;
        }

        void addDestination(Point p) {
            path.push_back(p);
        }
        
        void verify(std::unordered_map<int, std::unordered_map<int, bool>>* pointIsVerified) {
            if (!((*pointIsVerified)[y][x])) {
                    (*pointIsVerified)[y][x] = true;
            }
            if (x > 0) {
                if (!((*pointIsVerified)[y][x-1])) {
                    (*pointIsVerified)[y][x-1] = true;
                }
            }
            if (x < 600) {
                if (!((*pointIsVerified)[y][x+1])) {
                    (*pointIsVerified)[y][x+1] = true;
                }
            }
            if (y > 0) {
                if (!((*pointIsVerified)[y-1][x])) {
                    (*pointIsVerified)[y-1][x] = true;
                }
            }
            if (y < 600) {
                if (!((*pointIsVerified)[y+1][x])) {
                    (*pointIsVerified)[y+1][x] = true;
                }
            }
        } 

        std::vector<Point> getPath() {
            return path;
        }

        int moveTowards(Point p, int currentTimestamp) {
            bool a = false;
            bool b = false;

            if (p.x > x) {
                x += 1;
                a = true;
            }
            
            else if (p.x < x) {
                x -= 1;
                a = true;
            }
            
            if (p.y > y) {
                y += 1;
                b = true;
            }
            else if (p.y < y) {
                y -=1;
                b = true;
            }
            if (x == p.x && y == p.y) {
                if (nextDestination < path.size() - 1) {
                    nextDestination++;
                }
                else {
                    path = {};
                    active = false;
                    nextDestination = 0;
                    deadBatteryTimestamp = -1;
                    chargingTimestamp = currentTimestamp;
                    charging = true;
                }
            }
            if (a && b) {
                return 17;
            }
            else {
                return 12;
            }
        }

        int move(int currentTimestamp) {
            Point p = getNextdestination();
            return moveTowards(p, currentTimestamp);
        }

        Point getNextdestination() {
            return path[nextDestination];
        }
        
        int getNextDestinationIndex() {
            return nextDestination;
        }

        int getDeadBatteryTimestamp() {
            return deadBatteryTimestamp;
        }

        bool isActive() {
            return active;
        }

        void activate(int currentTimestamp) {
            active = true;
            deadBatteryTimestamp = currentTimestamp + 18000;
            charging = false;
            chargingTimestamp = -1;
        }

        void shutDown() {
            active = false;
        }

        bool isCharging() {
            return charging;
        }

        void ready() {
            charging = false;
        }

        int getChargingTimestamp() {
            return chargingTimestamp;
        }

    private:
        std::vector<Point> path;
        int nextDestination = 0;
        int id;
        int x = 300;
        int y = 300;
        bool active = false;
        int deadBatteryTimestamp;
        bool charging = false;
        int chargingTimestamp = -1;
};