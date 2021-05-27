#include <algorithm>
#include <vector>

#pragma once
#pragma clang diagnostic ignored "-Wgnu-designator"

namespace qt {
    template <typename ForwardIterator>
    ForwardIterator remove_duplicates(ForwardIterator first, ForwardIterator last) {
        auto new_last = first;

        for (auto current = first; current != last; ++current) {
            if (std::find(first, new_last, *current) == new_last) {
                if (new_last != current) *new_last = *current;
                ++new_last;
            }
        }

        return new_last;
    }

    struct Rect {
        float x;
        float y;
        float width;
        float height;
        unsigned id;
        unsigned radius;

        bool operator==(Rect pRect) {
            return x == pRect.x && y == pRect.y && width == pRect.width && pRect.height == height;
        }
    };

    class Quadtree {
        public:
            int max_objects = 10;
            int max_levels = 4;

            int level = 0;
            Rect bounds;

            std::vector<Quadtree> nodes;
            std::vector<Rect> objects;

            Quadtree(Rect bounds, int max_objects=10, int max_levels=4, int level=0) {
                this->bounds = bounds;
                this->max_objects = max_objects;
                this->max_levels = max_levels;
                this->level = level;
            }

            void split() __attribute__((hot)) {
                float nextLevel = this->level + 1;
                float subWidth = this->bounds.width/2;
                float subHeight = this->bounds.height/2;
                float x = this->bounds.x;
                float y = this->bounds.y;

                this->nodes.push_back(Quadtree(
                    Rect {
                        x: x + subWidth,
                        y: y,
                        width: subWidth,
                        height: subHeight,
                        id: 0,
                        radius: 0
                    }, this->max_objects, this->max_levels, nextLevel)
                );
                
                this->nodes.push_back(Quadtree(
                    Rect {
                        x: x,
                        y: y,
                        width: subWidth,
                        height: subHeight,
                        id: 0,
                        radius: 0
                    }, this->max_objects, this->max_levels, nextLevel)
                );
                
                this->nodes.push_back(Quadtree(
                    Rect {
                        x: x,
                        y: y + subHeight,
                        width: subWidth,
                        height: subHeight,
                        id: 0,
                        radius: 0
                    }, this->max_objects, this->max_levels, nextLevel)
                );
                
                this->nodes.push_back(Quadtree(
                    Rect {
                        x: x + subWidth,
                        y: y + subHeight,
                        width: subWidth,
                        height: subHeight
                    }, this->max_objects, this->max_levels, nextLevel)
                );
            }

            std::vector<float> getIndex(Rect pRect) {
                std::vector<float> indexes;
                float verticalMidpoint = this->bounds.x + (this->bounds.width/2);
                float horizontalMidpoint = this->bounds.y + (this->bounds.height/2);

                bool startIsNorth = pRect.y < horizontalMidpoint,
                    startIsWest  = pRect.x < verticalMidpoint,
                    endIsEast    = pRect.x + pRect.width > verticalMidpoint,
                    endIsSouth   = pRect.y + pRect.height > horizontalMidpoint;    
                
                // top-right quad
                if (startIsNorth && endIsEast) {
                    indexes.push_back(0);
                }
                
                // top-left quad
                if (startIsWest && startIsNorth) {
                    indexes.push_back(1);
                }

                // bottom-left quad
                if (startIsWest && endIsSouth) {
                    indexes.push_back(2);
                }

                // bottom-right quad
                if (endIsEast && endIsSouth) {
                    indexes.push_back(3);
                }
            
                return indexes;
            }

            void insert(Rect pRect) __attribute__((hot)) {
                float i = 0;
                std::vector<float> indexes;

                if (this->nodes.size()) {
                    indexes = this->getIndex(pRect);

                    for (i=0; i<indexes.size(); i++) {
                        this->nodes[indexes[i]].insert(pRect);
                    }
                }

                this->objects.push_back(pRect);

                if(this->objects.size() > static_cast<long unsigned int>(this->max_objects) && this->level < this->max_levels) {
                    if(!this->nodes.size()) {
                        this->split();
                    }

                    // add all objects to their corresponding subnode
                    for(i=0; i<this->objects.size(); i++) {
                        indexes = this->getIndex(this->objects[i]);
                        for(long unsigned int k=0; k<indexes.size(); k++) {
                            this->nodes[indexes[k]].insert(this->objects[i]);
                        }
                    }

                    // clean up this node
                    this->objects.clear();
                }
            }

            std::vector<Rect> retrieve(Rect pRect) __attribute__((hot)) {
                std::vector<float> indexes = this->getIndex(pRect);
                std::vector<Rect> returnObjects = this->objects;

                // if we have subnodes, retrieve their objects
                if(this->nodes.size()) {
                    for(long unsigned int i=0; i<indexes.size(); i++) {
                        std::vector<Rect> concatCanidate = this->nodes[indexes[i]].retrieve(pRect);
                        returnObjects.insert(returnObjects.end(), concatCanidate.begin(), concatCanidate.end());
                    }
                }

                returnObjects.erase(remove_duplicates(returnObjects.begin(), returnObjects.end()), returnObjects.end());

                return returnObjects;
            }

            void clear() {
                this->objects.clear();

                for(long unsigned int i=0; i < this->nodes.size(); i++) {
                    if(this->nodes.size()) {
                        this->nodes[i].clear();
                    }
                }

                this->nodes.clear();
            }
    };
};
