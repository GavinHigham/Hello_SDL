#ifndef DYNAMIC_TERRAIN_H
#define DYNAMIC_TERRAIN_H
#include <stdlib.h>
#include <stdbool.h>
#include <glalgebra.h>
#include "buflist.h"
#include "procedural_terrain.h"

enum {
	NUM_CHILDREN = NUM_TRI_DIVS,
	TRI_BASE_LEN = 10000,
	PIXELS_PER_TRI = 1,
	MAX_SUBDIVISIONS = 10
};

typedef struct dynamic_terrain_node *PDTNODE;
struct dynamic_terrain_node {
	float dist; //Distance to camera
	PDTNODE children[NUM_CHILDREN];
	struct terrain t;
};

typedef struct drawlist_node *DRAWLIST;
struct drawlist_node {
	struct drawlist_node *next;
	struct terrain *t;
};

DRAWLIST drawlist_prepend(DRAWLIST list, struct terrain *t);
void drawlist_free(DRAWLIST list);

int dt_depth_per_distance(float distance);
int dt_node_distance_compare(const void *n1, const void *n2);
int dt_node_closeness_compare(const void *n1, const void *n2);
// int dt_add_children(PDTNODE root, STACK *pool, HEAP *drawlist, vec3 cam_pos, int depth);

PDTNODE new_tree(struct terrain t);
void subdivide_tree(PDTNODE tree, vec3 cam_pos, int depth);
void create_drawlist(PDTNODE tree, DRAWLIST *drawlist, int depth);
void prune_tree(PDTNODE tree, int depth);
void free_tree(PDTNODE tree);

#endif

/*
Algorithm details:

Traverse the tree, subdividing nodes that need more resolution, and buffering nodes that are already subdivided, but not on the GPU (and should be).

Traverse the tree again, gathering a list of nodes that are to be drawn (in frustrum, and correct subdiv level)

Traverse the tree a third time, gathering a list of nodes that could be deleted
	these nodes should be ordered by distance from the camera, with all child nodes coming before their parents in the ordering
	Can I take advantage of the fact that the distance of child nodes will be strongly correlated with the distance of their parents?
	Delete GPU buffers until GPU memory constraints are satisfied. Delete vertex data until system memory constraints are satisfied.

Later optimizations:
	Pool of GPU buffers and nodes, "free" to pool, and "new" from it
	Keep a list of potential nodes to be deleted, delete or recycle on-demand as needed
*/