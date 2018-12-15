#include "glla.h"
#include "space_scene.h"
#include "scene.h"
#include "models/models.h"
#include "input_event.h"
#include "buffer_group.h"
//#include "experiments/deferred_framebuffer.h"
#include "lights.h"
#include "macros.h"
#include "shader_utils.h"
#include "space/stars.h"
#include "space/star_box.h"
#include "draw.h"
#include "drawf.h"
#include "entity/entity_components.h"
#include "space/procedural_planet.h"
#include "space/triangular_terrain_tile.h"
#include "math/utility.h"
#include "math/bpos.h"
#include "debug_graphics.h"
#include "space/solar_system.h"
#include "glsw_shaders.h"
#include "configuration/lua_configuration.h"
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <string.h>

extern int open_simplex_noise_seed;

SCENE_VTABLE(space);
float FOV = M_PI/2;// M_PI/2.7;
float far_distance = 10000000;
float near_distance = 1;
float log_depth_intermediate_factor = NAN; //Needs to be set in init.
int PRIMITIVE_RESTART_INDEX = 0xFFFFFFFF;

float screen_width, screen_height;

GLuint gVAO = 0;
amat4 inv_eye_frame;
static amat4 eye_frame = {.a = MAT3_IDENT,  .t = {6, 0, 0}};
bpos_origin eye_sector = {0, 0, 0};
//Object frames. Need a better system for this.
amat4 room_frame = {.a = MAT3_IDENT, .t = {  0,  -4,   -8}};
amat4 grid_frame = {.a = MAT3_IDENT, .t = {-50, -90, -550}};
amat4 tri_frame  = {.a = MAT3_IDENT, .t = {  0,   0,    0}};
bpos_origin room_sector = {0, 0, 0};
amat4 big_asteroid_frame = {.a = MAT3_IDENT, .t = {0, -4, -20}};
amat4 skybox_frame; //Set in init_render() and update()
float skybox_scale;
vec3 ambient_color = {0.01, 0.01, 0.01};
vec3 sun_direction = {0.1, 0.8, 0.1};
vec3 sun_color     = {0.1, 0.8, 0.1};
vec3 sun_position; // = bpos_remap((bpos){{0,0,0}, ssystem.origin}, eye_sector);
struct point_light_attributes point_lights = {.num_lights = 0};


const float planet_radius = 6000000;

solar_system ssystem = {0};
qvec3 solar_system_origin;
bool gen_solar_systems = false;
int64_t solar_system_star = 0;

Entity *ship_entity     = NULL;
Entity *camera_entity   = NULL;
Entity *sun_entity      = NULL;
Entity *star_box_entity = NULL;

GLfloat proj_mat[16];
GLfloat proj_view_mat[16];

Drawable d_ship, d_newship, d_teardropship, d_room, d_skybox;
Entity *entities;
int16_t nentities;

/* Lua Config */
extern lua_State *L;

void entities_init()
{
	ship_entity = entity_new();
	entity_make_physical(ship_entity, (Physical){
		.position.t = (vec3){0, 6000 + planet_radius, 0},
		.position.a = {{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}}}, /* Flip the ship around, because of where the planet seems to be spawning. */
		.origin = {0, 400, 0},
	});

	entity_make_controllable(ship_entity, (Controllable){
		.control = ship_control,
	});

	camera_entity = entity_new();
	entity_make_physical(camera_entity, (Physical){
		.position.t = {0, 4, 8},
	});

	entity_make_controllable(camera_entity, (Controllable){
		.control = camera_control,
		.context = ship_entity,
	});

	entity_make_scriptable(camera_entity, (Scriptable){
		.script = camera_script,
		.context = ship_entity,
	});


	star_box_entity = entity_new();
	entity_make_scriptable(star_box_entity, (Scriptable){
		.script = star_box_script,
		.context = camera_entity,
	});

	sun_entity = entity_new();
	entity_make_scriptable(sun_entity, (Scriptable){.script = sun_script});
}

void entities_deinit()
{
	entity_reset();
}

static void init_lights()
{
	//                             Position,            color,                atten_c, atten_l, atten_e, intensity
	new_point_light(&point_lights, (vec3){0, 2, 0},   (vec3){1.0, 0.2, 0.2}, 0.0,     0.0,    1,    200);
	new_point_light(&point_lights, (vec3){0, 2, 0},   (vec3){1.0, 1.0, 1.0}, 0.0,     0.0,    1,    5);
	new_point_light(&point_lights, (vec3){0, 2, 0},   (vec3){0.8, 0.8, 1.0}, 0.0,     0.3,    0.04, 0.4);
	new_point_light(&point_lights, (vec3){10, 4, -7}, (vec3){0.4, 1.0, 0.4}, 0.1,     0.14,   0.07, 0.75);
	point_lights.shadowing[0] = true;
	point_lights.shadowing[1] = true;
	//point_lights.shadowing[2] = true;
	//point_lights.shadowing[3] = true;
}

void space_scene_resize(float width, float height)
{
	glViewport(0, 0, width, height);
	screen_width = width;
	screen_height = height;
	make_projection_matrix(FOV, screen_width/screen_height, -near_distance, -far_distance, proj_mat);	
}

//Set up everything needed to start rendering frames.
int space_scene_init()
{
	srand(open_simplex_noise_seed);
	checkErrors("Function enter");

	glsw_shaders_init();
	checkErrors("glsw_shaders_init");

	load_effects(
		effects.all,       LENGTH(effects.all),
		shader_file_paths, LENGTH(shader_file_paths),
		attribute_strings, LENGTH(attribute_strings),
		uniform_strings,   LENGTH(uniform_strings));
	checkErrors("load_effects");

	glGenVertexArrays(1, &gVAO);

	glUseProgram(0);
	glClearDepth(1);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glClearColor(0.01f, 0.02f, 0.03f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	checkErrors("glClear");
	log_depth_intermediate_factor = 2.0/log2(far_distance + 1.0);

	entities_init();
	checkErrors("Entities");
	init_lights();
	checkErrors("Lights");
	//stars_init();
	checkErrors("Init stars");
	star_box_init(eye_sector);
	checkErrors("Init star_box");
	debug_graphics_init();
	checkErrors("Init debug_graphics");
	proc_planet_init();
	ssystem = solar_system_new(eye_sector);
	solar_system_star = 0;


	glUseProgram(effects.forward.handle);
	glUniform1f(effects.forward.log_depth_intermediate_factor, log_depth_intermediate_factor);

	checkErrors("forward log_depth_intermediate_factor");

	glUseProgram(effects.skybox.handle);
	glUniform1f(effects.skybox.log_depth_intermediate_factor, log_depth_intermediate_factor);

	checkErrors("skybox log_depth_intermediate_factor");

	skybox_scale = 2*far_distance / sqrt(3);
	skybox_frame.a = mat3_scalemat(skybox_scale, skybox_scale, skybox_scale);
	skybox_frame.t = eye_frame.t;

	glPointSize(5);
	glEnable(GL_PROGRAM_POINT_SIZE);
	checkErrors("glPointSize");

	entity_update_components();

	glUseProgram(0);
	checkErrors("After init_render");

	gen_solar_systems = getglobbool(L, "gen_solar_systems", false);

	return 0;
}

void space_scene_deinit()
{
	solar_system_free(ssystem);
	solar_system_star = 0;
	//stars_deinit();
	proc_planet_deinit();
	star_box_deinit();
	debug_graphics_deinit();
	point_lights.num_lights = 0;
	entities_deinit();
}

static void forward_update_point_light(EFFECT *effect, struct point_light_attributes *lights, int i)
{
	vec3 position = lights->position[i];
	glUniform3f(effect->uLight_pos, position.x, position.y, position.z);
	glUniform3f(effect->uLight_col, position.x, position.y, position.z);
	glUniform4f(effect->uLight_attr, lights->atten_c[i], lights->atten_l[i], lights->atten_e[i], lights->intensity[i]);
}

//Eventually I'll have an algorithm to calculate potential visible set, etc.
//static Drawable *pvs[] = {&d_newship, &d_room};
static Drawable *pvs[] = {};//{&d_ship};

void space_scene_render()
{
	// //Create a list of planet tiles to draw.
	// int drawlist_max = 3000;
	// tri_tile *drawlist[ssystem.num_planets][drawlist_max];
	// int drawlist_count[ssystem.num_planets];
	// for (int i = 0; i < ssystem.num_planets; i++) {
	// 	bpos pos = {eye_frame.t - ssystem.planet_positions[i].offset, eye_sector - ssystem.planet_positions[i].origin};
	// 	drawlist_count[i] = proc_planet_drawlist(ssystem.planets[i], drawlist[i], drawlist_max, pos);
	// }

	//Future Gavin Reminder: You put this here so it can set the intersecting tile's override color before the draw.
	//bpos ray_start = {ship_entity->Physical->position.t - test_planets[0].pos.offset, ship_entity->Physical->origin - test_planets[0].pos.origin};
	//bpos intersection = {{0}};
	//float ship_altitude = proc_planet_altitude(test_planets[0].planet, ray_start, &intersection);
	//printf("Current ship altitude to planet 0: %f\n", ship_altitude);

	//float h = vec3_dist(eye_frame.t, test_planet->pos) - test_planet->radius; //If negative, we're below sea level.
	//printf("Height: %f\n", h);
	static float hella_time = 0.0;
	hella_time += 1.0/60.0;
	glUseProgram(effects.stars.handle);
	glUniform1f(effects.stars.hella_time, hella_time);
	glUseProgram(effects.forward.handle);
	glUniform1f(effects.forward.hella_time, hella_time);

	bool wireframe = false;

	//Compute inverse eye frame.
	{
		inv_eye_frame = amat4_inverse(eye_frame);
		float tmp[16];
		amat4_to_array(inv_eye_frame, tmp);
		amat4_buf_mult(proj_mat, tmp, proj_view_mat);
	}

	glDepthMask(GL_TRUE);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);

	//If we're outside the shadow volume, we can use z-pass instead of z-fail.
	//z-pass is faster, and not patent-encumbered.
	int zpass = key_state[SDL_SCANCODE_7];
	int apass = !key_state[SDL_SCANCODE_5]; //Ambient pass

	//Draw in wireframe if 'z' is held down.
	wireframe = key_state[SDL_SCANCODE_Z];
	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//Depth prepass, can also be used as an ambient pass.
	{
		glUseProgram(effects.forward.handle);
		if (apass) {
			glUniform1i(effects.forward.ambient_pass, 1);
			glUniform3f(effects.forward.uLight_col, VEC3_COORDS(sun_color));
			vec3 sun = bpos_remap((bpos){{0,0,0}, ssystem.origin}, eye_sector);
			glUniform3f(effects.forward.uLight_pos, VEC3_COORDS(sun)); //was sun_direction
		} else {
			glDrawBuffer(GL_NONE); //Disable drawing to the color buffer if no ambient pass, save on fillrate.
		}
		glUniform3f(effects.forward.camera_position, VEC3_COORDS(eye_frame.t));

		//Draw entities
		for (int i = 0; i < LENGTH(pvs); i++)
			draw_drawable(pvs[i]);

		//glDisable(GL_CULL_FACE);

		checkErrors("Before planets draw");
		proc_planet_draw(eye_frame, proj_view_mat, ssystem.planets, ssystem.planet_positions, ssystem.num_planets);
		glUseProgram(effects.forward.handle);

		glUniform3f(effects.forward.override_col, 1.0, 1.0, 1.0);

		checkErrors("After drawing into depth");
		glUniform1i(effects.forward.ambient_pass, 0);
	}

	//For each light, draw potential occluders to set the stencil buffer.
	//Then draw the scene, accumulating non-occluded light onto the models additively.
	for (int i = 0; i < point_lights.num_lights; i++) {
		//Render shadow volumes into the stencil buffer.
		if (point_lights.shadowing[i]) {
			glEnable(GL_DEPTH_CLAMP);
			glDepthFunc(GL_LESS);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 0, 0xff);

			if (zpass) {
				glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
				glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP); 
			} else {
				glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
				glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
			}

			glUseProgram(effects.shadow.handle);
			glUniform3f(effects.shadow.gLightPos, VEC3_COORDS(point_lights.position[i]));
			glUniform1i(effects.shadow.zpass, zpass);

			for (int i = 0; i < LENGTH(pvs); i++)
				draw_forward_adjacent(&effects.shadow, *pvs[i]->bg, *pvs[i]->frame);

			glDisable(GL_DEPTH_CLAMP);
			glEnable(GL_CULL_FACE);
			checkErrors("After rendering shadow volumes");
		}
		//Draw the shadowed scene.
		if (point_lights.enabled_for_draw[i]) {
			glDrawBuffer(GL_BACK);
			glStencilFunc(GL_EQUAL, 0x0, 0xFF);
			glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
			glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
			glUseProgram(effects.forward.handle);
			forward_update_point_light(&effects.forward, &point_lights, i);
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
			glDepthFunc(GL_EQUAL);

			checkErrors("Before forward draw shadowed");
			for (int i = 0; i < LENGTH(pvs); i++) {
				draw_drawable(pvs[i]);
				checkErrors("After forward draw shadowed");
			}

			// //Draw procedural planet
			// for (DRAWLIST l = terrain_list; l; l = l->next) {
			// 	draw_forward(&effects.forward, l->t->bg, tri_frame);
			// 	checkErrors("After drawing a tri_tile");
			// }

			glDisable(GL_BLEND);
			checkErrors("After drawing shadowed");
		}
		glClear(GL_STENCIL_BUFFER_BIT);
	}
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);

	glUseProgram(effects.skybox.handle);
	glUniform3f(effects.skybox.sun_direction, VEC3_COORDS(sun_direction));
	glUniform3f(effects.skybox.sun_color, VEC3_COORDS(sun_color));
	skybox_frame.t = eye_frame.t;
	//draw_drawable(&d_skybox);
	//stars_draw(eye_frame, proj_view_mat);
	//star_box_draw(eye_sector, proj_view_mat);
	star_box_draw((bpos_origin){0,0,0}, proj_view_mat);

	debug_graphics.lines.ship_to_planet.start = bpos_remap((bpos){ship_entity->physical->position.t, ship_entity->physical->origin}, eye_sector);
	debug_graphics.lines.ship_to_planet.end   = bpos_remap(ssystem.planet_positions[0], eye_sector);
	debug_graphics.lines.ship_to_planet.enabled = true;
	//debug_graphics.points.ship_to_planet_intersection.pos = bpos_remap(intersection, eye_sector);
	//debug_graphics.points.ship_to_planet_intersection.enabled = true;
	//debug_graphics_draw(eye_frame, proj_view_mat);

	checkErrors("After forward junk");
}

//Collect these in a camera module?
controllable_callback(camera_control)
{

	// puts("1");
	Physical *camera = entity->physical;
	// puts("2");
	//printf("%p\n", entity->Controllable);
	//printf("%p\n", entity->Controllable->context);
	Physical *ship = ((Entity *)entity->controllable->context)->physical;
	//Translate the camera using WASD.
	float camera_speed = 0.5;
	// puts("3");
	camera->position.t = camera->position.t + //Honestly I just tried things at random until it worked, but here's my guess:
		mat3_multvec(mat3_transp(ship->position.a), // 2) Convert those coordinates from world-space to ship-space.
			mat3_multvec(camera->position.a, (vec3){ // 1) Move relative to the frame pointed at the ship.
			(key_state[SDL_SCANCODE_D] - key_state[SDL_SCANCODE_A]) * camera_speed,
			(key_state[SDL_SCANCODE_Q] - key_state[SDL_SCANCODE_E]) * camera_speed,
			(key_state[SDL_SCANCODE_S] - key_state[SDL_SCANCODE_W]) * camera_speed}));
	// puts("4");

	if (key_state[SDL_SCANCODE_8]) {
		double dist = 0;
		int nearest_star_idx = star_box_find_nearest_star_idx(camera->origin, &dist);
		printf("Camera at "); qvec3_print(camera->origin); printf(", nearest star is %i, distance %f.\n", nearest_star_idx, dist);
	}

	//Hacking this together, should put in the right spot later.
	if (gen_solar_systems) {
		double dist = INFINITY;
		int64_t nearest_star_idx = star_box_find_nearest_star_idx(camera->origin, &dist);
		if (solar_system_star != nearest_star_idx) {
			solar_system_free(ssystem);
			ssystem = solar_system_new(eye_sector);
			solar_system_star = nearest_star_idx;
			solar_system_origin = star_box_get_star_origin(nearest_star_idx);
		}
	}
}

scriptable_callback(camera_script)
{
	Physical *camera = entity->physical;
	//This won't work in situations where the ship entity is shuffled around.
	Physical *ship = ((Entity *)entity->scriptable->context)->physical;

	//float camera_ease = 0.5; //Using dt on this gives a jittery camera.
	//float target_ease = 0.5;
	//ship.eased_camera.t       = amat4_multvec(ship.velocity, ship.eased_camera.t);
	// ship.eased_camera.t      = vec3_lerp(ship.eased_camera.t,      ship.locked_camera.t,      camera_ease);
	// ship.eased_camera_target = vec3_lerp(ship.eased_camera_target, ship.locked_camera_target, target_ease);

	//The eye should look from itself to a point in front of the ship, and its "up" should be "up" from the ship's orientation.
	// ship.eased_camera.a = mat3_lookat(
	// 	mat3_multvec(ship.position.a, ship.eased_camera.t),
	// 	mat3_multvec(ship.position.a, ship.eased_camera_target),
	// 	mat3_multvec(ship.position.a, (vec3){0, 1, 0}));
	camera->origin = ship->origin;
	camera->position.a = mat3_lookat(
		mat3_multvec(ship->position.a, camera->position.t) + mat3_multvec(ship->position.a, ship->position.t), //Look source, relative to ship.
		mat3_multvec(ship->position.a, ship->position.t), //Look target (ship), relative to ship.
		mat3_multvec(ship->position.a, (vec3){0, 1, 0})); //Up vector, relative to ship.
}

scriptable_callback(sun_script)
{
	static float sunscale = 50.0;
	if (key_state[SDL_SCANCODE_EQUALS])
		sunscale += 0.1;
	if (key_state[SDL_SCANCODE_MINUS])
		sunscale -= 0.1;
	sun_color = ambient_color * sunscale;
}

//TODO: Move the update function out of the renderer module, come up with a good interface for things that need to be accessed in both.
void space_scene_update(float dt)
{
	static float light_time = 0;
	light_time += dt * 0.2;
	point_lights.position[0] = (vec3){10*cos(light_time), 4, 10*sin(light_time)-8};
	sun_position = bpos_remap((bpos){{0,0,0}, ssystem.origin}, eye_sector);

	//Commented out because I kept hitting y and getting annoyed.
	// if (key_state[SDL_SCANCODE_Y]) {
	// 	printf("Skybox scale is %f, what would you like the scale to be?\n", skybox_scale);
	// 	scanf("%f", &skybox_scale);
	// 	skybox_frame.a = mat3_scalemat(skybox_scale, skybox_scale, skybox_scale);
	// }

	entity_update_components();

	//bpos ray_start = {ship_entity->Physical->position.t - test_planets[0].pos.offset, ship_entity->Physical->origin - test_planets[0].pos.origin};
	//bpos intersection = {0};
	//float ship_altitude = proc_planet_altitude(test_planets[0].planet, ray_start, &intersection);
	//printf("Current ship altitude to planet 0: %f\n", ship_altitude);

	eye_frame = (amat4){camera_entity->physical->position.a, amat4_multpoint(ship_entity->physical->position, camera_entity->physical->position.t)};
	eye_sector = ship_entity->physical->origin;
	bpos_split_fix(&eye_frame.t, &eye_sector);


	point_lights.enabled_for_draw[2] = key_state[SDL_SCANCODE_6];
}