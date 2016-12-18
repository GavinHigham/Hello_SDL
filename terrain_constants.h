#ifndef TERRAIN_CONSTANTS_H
#define TERRAIN_CONSTANTS_H

enum {
	//The more rows, the fewer draw calls, and the fewer primitive restart indices hurting memory locality.
	//The fewer rows, the fewer overall vertices, and the better overall culling efficiency.
	//If I keep it as a power-of-two, I can avoid using spherical linear interpolation, and it will be faster.
	DEFAULT_NUM_TRI_TILE_ROWS = 64,
	//A triangular tile is divided in "triforce" fashion, that is,
	//by dividing along the edges of a triangle whose vertices are the bisection of each original tile edge.
	DEFAULT_NUM_TRI_TILE_DIVS = 4,
	TRI_BASE_LEN = 10000,
	PIXELS_PER_TRI = 2,
	MAX_SUBDIVISIONS = 4,
	YIELD_AFTER_DIVS = 5,
	NUM_ICOSPHERE_FACES = 20
};

#endif
