#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "procedural_planet.h"
#include "math/geometry.h"
#include "math/utility.h"
#include "macros.h"
#include "input_event.h" //For controller hotkeys
#include "open-simplex-noise-in-c/open-simplex-noise.h"
#include "element.h"

//Suppress prints
#define printf(...) while(0) {}


extern bpos_origin eye_sector;
extern bpos_origin tri_sector;
 
//Adapted from http://www.glprogramming.com/red/chapter02.html
static const float x = 0.525731112119133606;
static const float z = 0.850650808352039932;
static const vec3 ico_v[] = {    
	{-x, 0, z}, { x, 0,  z}, {-x,  0,-z}, { x,  0, -z},
	{ 0, z, x}, { 0, z, -x}, { 0, -z, x}, { 0, -z, -x},
	{ z, x, 0}, {-z, x,  0}, { z, -x, 0}, {-z, -x,  0}
};

static const GLuint ico_i[] = { 
	0,4,1,   0,9,4,   9,5,4,   4,5,8,   4,8,1,    
	8,10,1,  8,3,10,  5,3,8,   5,2,3,   2,7,3,    
	7,10,3,  7,6,10,  7,11,6,  11,0,6,  0,1,6, 
	6,1,10,  9,0,11,  9,11,2,  9,2,5,   7,2,11
};

//In my debug view, these colors are applied to tiles based on the number of times they've been split.
vec3 primary_color_by_depth[] = {
	{1.0, 0.0, 0.0}, //Red
	{0.5, 0.5, 0.0}, //Yellow
	{0.0, 1.0, 0.0}, //Green
	{0.0, 0.5, 0.5}, //Cyan
	{0.0, 0.0, 1.0}, //Blue
	{0.5, 0.0, 0.5}, //Purple
};

const vec3 proc_planet_up = (vec3){z/3, (z+z+x)/3, 0}; //Centroid of ico_i[3]
//Originally made to get rid of the black dot at the poles. Weirdly, they disappeared when I tilted the axis.
//Keep around and use on the tiles (and descendent tiles) of the poles to get rid of the black dots, should they reappear.
//const vec3 proc_planet_not_up = (vec3){-(z+z+x)/3, z/3, 0};
extern float screen_width;
extern struct osn_context *osnctx;

// Static Functions //

static tri_tile * tree_tile(quadtree_node *tree)
{
	return (tri_tile *)tree->data;
}

static int splits_per_distance(float distance, float scale)
{
	return fmax(fmin(log2(scale/distance), PROC_PLANET_TILE_MAX_SUBDIVISIONS), 0);
}

static float split_tile_radius(int depth, float base_length)
{
	return base_length / pow(2, depth);
}

static void tri_tile_split(tri_tile *t, tri_tile *out[DEFAULT_NUM_TRI_TILE_DIVS]);

//TODO: See if I can factor out depth somehow.
static int proc_planet_subdiv_depth(proc_planet *planet, tri_tile *tile, int depth, bpos cam_pos)
{
	//Convert camera and tile position to planet-coordinates.
	//These calculations might hit the limits of floating-point precision if the planet is really large.
	cam_pos.offset = bpos_remap(cam_pos, (bpos_origin){0});
	vec3 tile_pos = bpos_remap((bpos){tile->centroid, tile->sector}, (bpos_origin){0});
	vec3 surface_pos = cam_pos.offset * planet->radius/vec3_mag(cam_pos.offset);

	float altitude = vec3_mag(cam_pos.offset) - planet->radius;
	float tile_radius = split_tile_radius(depth, planet->edge_len);
	float tile_dist = vec3_dist(surface_pos, tile_pos) - tile_radius;
	float subdiv_dist = fmax(altitude, tile_dist);
	float scale_factor = (screen_width * planet->edge_len) / (2 * PROC_PLANET_TILE_PIXELS_PER_TRI * PROC_PLANET_NUM_TILE_ROWS);

	// //Maybe I can handle this when preparing the drawlist, instead?
	if (distance_to_horizon(planet->radius, fmax(altitude, 0)) < (vec3_dist(cam_pos.offset, tile_pos) - tile_radius))
		return 0; //Tiles beyond the horizon should not be split.

	return splits_per_distance(subdiv_dist, scale_factor);
}

static bool proc_planet_split_visit(quadtree_node *node, void *context)
{
	struct planet_terrain_context *ctx = (struct planet_terrain_context *)context;
	tri_tile *tile = tree_tile(node);
	ctx->visited++;

	//Cap the number of subdivisions per whole-tree traversal (per frame essentially)
	if (node->depth == 0)
		ctx->splits_left = ctx->splits_max;

	int depth = proc_planet_subdiv_depth(ctx->planet, tile, node->depth, ctx->cam_pos);

	if (depth > node->depth && !quadtree_node_has_children(node) && ctx->splits_left > 0) {
		tri_tile *new_tiles[DEFAULT_NUM_TRI_TILE_DIVS];
		tri_tile_split(tile, new_tiles);
		quadtree_node_add_children(node, (void **)new_tiles);
		ctx->splits_left--;
	}

	if (nes30_buttons[INPUT_BUTTON_START])
		tile->override_col = primary_color_by_depth[depth % LENGTH(primary_color_by_depth)];
	else
		tile->override_col = (vec3){1, 1, 1};

	return depth > node->depth;
}

static bool proc_planet_drawlist_visit(quadtree_node *node, void *context)
{
	struct planet_terrain_context *ctx = (struct planet_terrain_context *)context;
	tri_tile *tile = tree_tile(node);
	int depth = proc_planet_subdiv_depth(ctx->planet, tile, node->depth, ctx->cam_pos);

	if (depth == node->depth || !quadtree_node_has_children(node))
		if (ctx->num_tiles < ctx->max_tiles)
			ctx->tiles[ctx->num_tiles++] = tile;

	return depth > node->depth;
}

static float noise3(vec3 pos)
{
	return (1+open_simplex_noise3(osnctx, pos.x, pos.y, pos.z))/2;
}

static tri_tile * proc_planet_vertices_and_normals(struct element_properties *elements, int num_elements, tri_tile *t, height_map_func height, vec3 planet_pos, float noise_radius, float amplitude)
{
	//vec3 brownish = {0.30, .27, 0.21};
	//vec3 whiteish = {0.96, .94, 0.96};
	//vec3 orangeish = (vec3){255, 181, 112} / 255;

	float epsilon = noise_radius / 100000; //TODO: Check this value or make it empirical somehow.
	for (int i = 0; i < t->num_vertices; i++) {
		//Points towards vertex from planet origin
		vec3 pos = t->positions[i] - planet_pos;
		float m = vec3_mag(pos);
		//Point at the surface of our simulated smaller planet.
		vec3 noise_surface = pos * (noise_radius/m);

		//Basis vectors
		//TODO: Use a different "up" vector on the poles.
		vec3 x = vec3_normalize(vec3_cross(proc_planet_up, pos));
		vec3 z = vec3_normalize(vec3_cross(pos, x));
		vec3 y = vec3_normalize(pos);

		//Calculate position and normal on a sphere of noise_radius radius.
		//position_normal_color(height, x, y, z, epsilon, &noise_surface, &t->normals[i], &t->colors[i]);

		{
			//Create two points, scootched out along the basis vectors.
			vec3 pos1 = x * epsilon + noise_surface;
			vec3 pos2 = z * epsilon + noise_surface;
			vec3 p0 = noise_surface * 0.00005, p1 = pos1 * 0.00005, p2 = pos2 * 0.00005;

			vec3 cmy, sum_cmy = {0,0,0}; float k, sum_k = 0, sum_h = 0;
			double ys[3] = {0};
			for (int j = 0; j < num_elements; j++) {
				rgb_to_cmyk(elements[j].color, &cmy, &k);
				float offset = 4529 * j;//random number
				float scale = pow(2, j*2);
				float mag = amplitude / scale;
				vec3 turb = {ys[0], ys[1], ys[2]};
				double h = noise3(turb + p0 * scale + offset);
				sum_h += h;
				sum_cmy += cmy*h;
				sum_k += k*h;
				ys[0] += mag * h;
				ys[1] += mag * noise3(turb + p1 * scale + offset);
				ys[2] += mag * noise3(turb + p2 * scale + offset);
			}
			cmy = sum_cmy / sum_h; k = sum_k / sum_h;
			cmyk_to_rgb(cmy, k, &t->colors[i]);
			t->colors[i] /= 255;

			//Find procedural heights, and add them.
			// pos1      = y * height(pos1, &color1) + pos1;
			// pos2      = y * height(pos2, &color2) + pos2;
			// noise_surface = y * height(noise_surface, &c) + noise_surface;
			pos1          = y * amplitude * ys[1] + pos1;
			pos2          = y * amplitude * ys[2] + pos2;
			noise_surface = y * amplitude * ys[0] + noise_surface;

			//Compute the normal.
			t->normals[i] = vec3_normalize(vec3_cross(pos1 - noise_surface, pos2 - noise_surface));
			//Compute the new surface position.
			//Scale back up to planet size.
			t->positions[i] = noise_surface * (m/noise_radius) + planet_pos;
		}
	}
	return t;
}

static void reproject_vertices_to_spherical(vec3 vertices[], int num_vertices, vec3 pos, float radius)
{
	for (int i = 0; i < num_vertices; i++) {
		vec3 d = vertices[i] - pos;
		vertices[i] = d * radius/vec3_mag(d) + pos;
	}
}

static void proc_planet_finishing_touches(tri_tile *t, void *finishing_touches_context)
{
	//Retrieve tile's planet from finishing_touches_context.
	proc_planet *p = (proc_planet *)finishing_touches_context;
	vec3 planet_pos = bpos_remap((bpos){0}, t->sector);
	//Curve the tile around planet by normalizing each vertex's distance to the planet and scaling by planet radius.
	reproject_vertices_to_spherical(t->positions, t->num_vertices, planet_pos, p->radius);
	//Apply perturbations to the surface and calculate normals.
	//Since noise doesn't compute well on huge planets, noise is calculated on a simulated smaller planet and scaled up.
	struct element_properties props[p->num_elements];
	for (int i = 0; i < p->num_elements; i++)
		props[i] = element_get_properties(p->elements[i]);
	proc_planet_vertices_and_normals(props, p->num_elements, t, p->height, planet_pos, p->noise_radius, p->amplitude);
}

static void tri_tile_split(tri_tile *t, tri_tile *out[DEFAULT_NUM_TRI_TILE_DIVS])
{
	for (int i = 0; i < DEFAULT_NUM_TRI_TILE_DIVS; i++) {
		out[i] = new_tri_tile();
		out[i]->depth = t->depth + 1;
	}

	vec3 new_tile_vertices[] = {
		vec3_lerp(t->tile_vertices[0], t->tile_vertices[1], 0.5),
		vec3_lerp(t->tile_vertices[0], t->tile_vertices[2], 0.5),
		vec3_lerp(t->tile_vertices[1], t->tile_vertices[2], 0.5)
	};

	proc_planet *planet = (proc_planet *)t->finishing_touches_context;
	vec3 planet_pos = bpos_remap((bpos){0}, t->sector);
	reproject_vertices_to_spherical(new_tile_vertices, 3, planet_pos, planet->radius);

	init_tri_tile(out[0], (vec3[3]){t->tile_vertices[0],  new_tile_vertices[0], new_tile_vertices[1]}, t->sector, PROC_PLANET_NUM_TILE_ROWS, t->finishing_touches, t->finishing_touches_context);
	init_tri_tile(out[1], (vec3[3]){new_tile_vertices[0], t->tile_vertices[1],  new_tile_vertices[2]}, t->sector, PROC_PLANET_NUM_TILE_ROWS, t->finishing_touches, t->finishing_touches_context);
	init_tri_tile(out[2], (vec3[3]){new_tile_vertices[0], new_tile_vertices[2], new_tile_vertices[1]}, t->sector, PROC_PLANET_NUM_TILE_ROWS, t->finishing_touches, t->finishing_touches_context);
	init_tri_tile(out[3], (vec3[3]){new_tile_vertices[1], new_tile_vertices[2], t->tile_vertices[2]},  t->sector, PROC_PLANET_NUM_TILE_ROWS, t->finishing_touches, t->finishing_touches_context);
	printf("Dividing %p, depth %i into %d new tiles.\n", t, t->depth, DEFAULT_NUM_TRI_TILE_DIVS);
}

static float fbm(vec3 p)
{
	return open_simplex_noise3(osnctx, p.x, p.y, p.z);
}

static float distorted_height(vec3 pos, vec3 *variety)
{
	vec3 a = {1.4, 1.08, 1.3};
	vec3 b = {3.8, 7.9, 2.1};

	vec3 h1 = {
		fbm(pos),
		fbm(pos + a),
		fbm(pos + b)
	};
	vec3 h2 = {
		fbm(pos + 2.4*h1),
		fbm(pos + 2*h1 + a),
		fbm(pos + 1*h1 + b)
	};

	*variety = h1 + h2 / 2;
	return fbm(pos + 0.65*h2);
}

float proc_planet_height(vec3 pos, vec3 *variety)
{
	pos = pos * 0.0015;
	vec3 v1, v2;
	float height = (
		distorted_height(pos, &v1) +
		distorted_height(pos * 2, &v2)
		) / 2;
	*variety = (v1 + v2) / 2;
	return TERRAIN_AMPLITUDE * height;
}

// Public Functions //

proc_planet * proc_planet_new(float radius, height_map_func height, int *elements, int num_elements)
{
	proc_planet *p = malloc(sizeof(proc_planet));
	p->radius = radius;
	for (int i = 0; i < num_elements; i++)
		p->elements[i] = elements[i];
	p->num_elements = num_elements;
	//p->color_family = color_family; //TODO: Will eventually choose from a palette of good color combos.
	p->noise_radius = radius/1000; //TODO: Determine the largest reasonable noise radius, map input radius to a good range.
	p->amplitude = TERRAIN_AMPLITUDE * 1.4; //TODO: Choose a good number, pass this through the chain of calls.
	p->edge_len = radius / sin(2.0*M_PI/5.0);
	printf("Edge len: %f\n", p->edge_len);
	p->height = height;

	//Initialize the planet terrain
	for (int i = 0; i < NUM_ICOSPHERE_FACES; i++) {
		vec3 verts[] = {
			ico_v[ico_i[3*i]]   * radius,
			ico_v[ico_i[3*i+1]] * radius,
			ico_v[ico_i[3*i+2]] * radius
		};

		p->tiles[i] = quadtree_new(new_tri_tile(), 0);
		//Initialize tile with verts expressed relative to p->sector.
		init_tri_tile((tri_tile *)p->tiles[i]->data, verts, (bpos_origin){0, 0, 0}, PROC_PLANET_NUM_TILE_ROWS, &proc_planet_finishing_touches, p);
	}

	return p;
}

void proc_planet_free(proc_planet *p)
{
	for (int i = 0; i < NUM_ICOSPHERE_FACES; i++)
		quadtree_free(p->tiles[i], (void (*)(void *))free_tri_tile, false);
	free(p);
}

int proc_planet_drawlist(proc_planet *p, tri_tile **tiles, int max_tiles, bpos cam_pos)
{
	struct planet_terrain_context context = {
		.splits_max = 50, //Total number of splits for this call of drawlist.
		.cam_pos = cam_pos,
		.planet = p,
		.tiles = tiles,
		.max_tiles = max_tiles
	};
	//TODO: Check the distance here and draw an imposter instead of the whole planet if it's far enough.

	for (int i = 0; i < NUM_ICOSPHERE_FACES; i++) {
		quadtree_preorder_visit(p->tiles[i], proc_planet_split_visit, &context);
		quadtree_preorder_visit(p->tiles[i], proc_planet_drawlist_visit, &context);
		//TODO: Find a good way to implement prune.
		//terrain_tree_prune(p->tiles[i], proc_planet_split_depth, &context, (terrain_tree_free_fn)free_tri_tile);
	}

	printf("Num tiles drawn:%d\n", context.visited);
	return context.num_tiles;
}

struct proc_planet_tile_raycast_context {
	//Input
	bpos pos;
	//Output
	tri_tile *intersecting_tile;
	bpos intersection;
};

bool proc_planet_tile_raycast(quadtree_node *node, void *context)
{
	struct proc_planet_tile_raycast_context *ctx = (struct proc_planet_tile_raycast_context *)context;
	tri_tile *t = tree_tile(node);
	vec3 intersection;
	vec3 local_start = bpos_remap(ctx->pos, t->sector);
	vec3 local_end = bpos_remap((bpos){0}, t->sector);
	int result = ray_tri_intersect(local_start, local_end, tree_tile(node)->tile_vertices, &intersection);
	if (result == 1) {
		//t->override_col *= (vec3){0.1, 1.0, 1.0};
		//printf("Tile intersection found! Tile: %i\n", t->tile_index);
		ctx->intersecting_tile = t;
		ctx->intersection.offset = intersection;
		ctx->intersection.origin = t->sector;
	}
	return result == 1;
}

//Raycast towards the planet center and find the altitude on the deepest terrain tile. O(log(n)) complexity in the number of planet tiles.
float proc_planet_altitude(proc_planet *p, bpos start, bpos *intersection)
{
	//TODO: Don't loop through everything to find this.
	float smallest_dist = INFINITY;
	float smallest_tile_dist = INFINITY;
	struct proc_planet_tile_raycast_context context = {
		.pos = start,
		.intersecting_tile = NULL
	};
	for (int i = 0; i < NUM_ICOSPHERE_FACES; i++) {
		quadtree_preorder_visit(p->tiles[i], proc_planet_tile_raycast, &context);
		tri_tile *t = context.intersecting_tile;
		if (t) {
			vec3 local_start = bpos_remap(start, t->sector);
			vec3 local_end = bpos_remap((bpos){0}, t->sector);
			float dist = tri_tile_raycast_depth(t, local_start, local_end);

			//Because we'll also find an intersection on the back of the planet, choose smallest distance.
			if (dist < smallest_dist)
				smallest_dist = dist;

			float tile_dist = vec3_dist(local_start, t->centroid);
			if (tile_dist < smallest_tile_dist) {
				smallest_tile_dist = tile_dist;
				*intersection = context.intersection;
			}
		}
		context.intersecting_tile = NULL;
	}
	return smallest_dist;
}
