#pragma once
#include "Point.h"
#include <vector>
#include <iostream>

class Drone{
    public:

        //default constructor
        //Drone () : id(-1) {}

        //implemented constructor
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
        
        void verify(bool (&space)[601][601]) {
            if (not(space[y][x])) {
                    space[y][x] = true;
            }
            if (x > 0) {
                if (not(space[y][x-1])) {
                    space[y][x-1] = true;
                }
            }
            if (x < 600) {
                if (not(space[y][x+1])) {
                    space[y][x+1] = true;
                }
            }
            if (y > 0) {
                if (not(space[y-1][x])) {
                    space[y-1][x] = true;
                }
            }
            if (y < 600) {
                if (not(space[y+1][x])) {
                    space[y+1][x] = true;
                }
            }
        } 

        std::vector<Point> getPath() {
            return path;
        }

        int moveTowards(Point p) {
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
                    //std::cout << "drone " << getId() << " back to base" << std::endl;
                }
            }
            if (a && b) {
                return 17;
            }
            else {
                return 12;
            }
        }

        int move() {
            Point p = getNextdestination();
            return moveTowards(p);
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
        }

        void shutDown() {
            active = false;
        }

    private:
        std::vector<Point> path;
        int nextDestination = 0;
        int id;
        int x = 300;
        int y = 300;
        bool active = false;
        int deadBatteryTimestamp;
};