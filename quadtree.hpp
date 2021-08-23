#include <algorithm>
#include <memory>
#include <vector>

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
        std::unique_ptr<Rect> bounds;

        std::vector<std::shared_ptr<Quadtree>> nodes;
        std::vector<std::shared_ptr<Rect>> objects;

        Quadtree(std::unique_ptr<Rect> bounds, int max_objects = 10, int max_levels = 4, int level = 0) {
            this->bounds = std::move(bounds);
            this->max_objects = max_objects;
            this->max_levels = max_levels;
            this->level = level;
        }

        void split() __attribute__((hot)) {
            float nextLevel = this->level + 1;
            float subWidth = this->bounds->width / 2;
            float subHeight = this->bounds->height / 2;
            float x = this->bounds->x;
            float y = this->bounds->y;

            this->nodes.push_back(std::make_shared<Quadtree>(
                std::make_unique<Rect>(Rect {
                    x: x + subWidth,
                    y: y,
                    width: subWidth,
                    height: subHeight,
                    id: 0,
                    radius: 0
                }),
                this->max_objects,
                this->max_levels,
                nextLevel));

            this->nodes.push_back(std::make_shared<Quadtree>(
                std::make_unique<Rect>(Rect {
                    x: x,
                    y: y,
                    width: subWidth,
                    height: subHeight,
                    id: 0,
                    radius: 0
                }),
                this->max_objects,
                this->max_levels,
                nextLevel));

            this->nodes.push_back(std::make_shared<Quadtree>(
                std::make_unique<Rect>(Rect {
                    x: x,
                    y: y + subHeight,
                    width: subWidth,
                    height: subHeight,
                    id: 0,
                    radius: 0
                }),
                this->max_objects,
                this->max_levels,
                nextLevel));

            this->nodes.push_back(std::make_shared<Quadtree>(
                std::make_unique<Rect>(Rect {
                    x: x + subWidth,
                    y: y + subHeight,
                    width: subWidth,
                    height: subHeight
                }),
                this->max_objects,
                this->max_levels,
                nextLevel));
        }

        void getIndex(std::shared_ptr<Rect> pRect, std::vector<float>& indexes) {
            indexes.clear();
            float verticalMidpoint = this->bounds->x + (this->bounds->width / 2);
            float horizontalMidpoint = this->bounds->y + (this->bounds->height / 2);

            bool startIsNorth = pRect->y<horizontalMidpoint,
                startIsWest = pRect->x<verticalMidpoint,
                    endIsEast = pRect->x + pRect->width> verticalMidpoint,
                endIsSouth = pRect->y + pRect->height>
                                    horizontalMidpoint;

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
        }

        void insert(std::shared_ptr<Rect> pRect) __attribute__((hot)) {
            float i = 0;
            std::vector<float> indexes;

            if (this->nodes.size()) {
                this->getIndex(pRect, indexes);

                for (i = 0; i < indexes.size(); i++) {
                    this->nodes[indexes[i]]->insert(pRect);
                }
            }

            this->objects.push_back(pRect);

            if (this->objects.size() > static_cast<long unsigned int>(this->max_objects) && this->level < this->max_levels) {
                if (!this->nodes.size()) {
                    this->split();
                }

                // add all objects to their corresponding subnode
                for (i = 0; i < this->objects.size(); i++) {
                    this->getIndex(this->objects[i], indexes);
#pragma omp simd
                    for (long unsigned int k = 0; k < indexes.size(); k++) {
                        this->nodes[indexes[k]]->insert(this->objects[i]);
                    }
                }

                // clean up this node
                this->objects.clear();
            }
        }

        void retrieve(std::shared_ptr<Rect> pRect, std::vector<std::shared_ptr<Rect>>& returnObjects) __attribute__((hot)) {
            returnObjects.clear();
            std::vector<float> indexes;
            this->getIndex(pRect, indexes);
            returnObjects.resize(this->objects.size());
            std::copy(this->objects.begin(), this->objects.end(), returnObjects.begin());

            // if we have subnodes, retrieve their objects
            if (this->nodes.size()) {
#pragma omp simd
                for (long unsigned int i = 0; i < indexes.size(); i++) {
                    std::vector<std::shared_ptr<Rect>> concatCandidate;
                    this->nodes[indexes[i]]->retrieve(pRect, concatCandidate);
                    returnObjects.insert(returnObjects.end(), concatCandidate.begin(), concatCandidate.end());
                }
            }

            returnObjects.erase(remove_duplicates(returnObjects.begin(), returnObjects.end()), returnObjects.end());
        }

        void clear() {
            this->objects.clear();

#pragma omp simd
            for (long unsigned int i = 0; i < this->nodes.size(); i++) {
                if (this->nodes.size()) {
                    this->nodes[i]->clear();
                }
            }

            this->nodes.clear();
        }
    };
}; // namespace qt
