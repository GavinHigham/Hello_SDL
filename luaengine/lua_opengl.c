//Lua headers
#include <lua-5.4.4/src/lua.h>
#include <lua-5.4.4/src/lauxlib.h>
#include <lua-5.4.4/src/lualib.h>
//Graphics header includes OpenGL
#include "graphics.h"

/*
Lua wrappers for

glActiveTexture
glBindBuffer
glBindTexture
glBindVertexArray
glBufferData
glClear
glClearColor
glClearDepth
glDeleteBuffers
glDeleteProgram
glDeleteVertexArrays
glDepthFunc
glDisable
glDrawElementsInstanced
glEnable
glEnableVertexAttribArray
glGenBuffers
glGenVertexArrays
glGetAttribLocation
glGetUniformLocation
glPolygonMode
glPrimitiveRestartIndex
glUseProgram
glVertexAttribDivisor
glVertexAttribPointer
glViewport

TODO(Gavin): Make userdata types for all parameters so that calling OpenGL functions is "typesafe"
*/

struct tu_glVertexArrays
{
	size_t num;
	GLuint arrays[0];
};

struct tu_glVertexArray
{
	int ref; //From luaL_ref, points to the VertexArrays object this array is part of.
	GLuint array;
};

struct tu_glBuffers
{
	size_t num;
	GLuint buffers[0];
};

struct tu_glBuffer
{
	int ref; //From luaL_ref, points to the Buffers object this buffer is part of.
	GLuint buffer;
};

static int lua_glActiveTexture(lua_State *L)
{
	glActiveTexture((GLenum)luaL_checkinteger(L, 1)); //texture
	return 0;
}

static int lua_Buffers(lua_State *L)
{
	GLsizei n = (GLsizei)luaL_checkinteger(L, 1);
	size_t size = sizeof(struct tu_glBuffers) + sizeof(GLuint) * n;
	struct tu_glBuffers *buffers = lua_newuserdatauv(L, size, 0);
	memset(buffers, 0, size);
	buffers->num = n;
	luaL_setmetatable(L, "tu.Buffers");
	glGenBuffers(n, buffers->buffers);
	return 1;
}

static int lua_Buffers__gc(lua_State *L)
{
	struct tu_glBuffers *buffers = luaL_checkudata(L, 1, "tu.Buffers");
	// printf("Buffers being collected\n");
	glDeleteBuffers(buffers->num, buffers->buffers);
	return 0;
}

static int lua_Buffers__index(lua_State *L)
{
	struct tu_glBuffers *buffers = luaL_checkudata(L, 1, "tu.Buffers");
	lua_Integer i = luaL_checkinteger(L, 2);
	if (i > 0 && i <= buffers->num) {
		struct tu_glBuffer *buffer = lua_newuserdatauv(L, sizeof(struct tu_glBuffer), 0);
		//TODO: Carefully review the order of these statements.
		luaL_getmetatable(L, "tu.Buffer");
		lua_pushvalue(L, 1);
		buffer->ref = luaL_ref(L, -2); //buffers will now not be collected
		// printf("ref-ing Buffer %i\n", buffer->ref);
		buffer->buffer = buffers->buffers[i-1];
		lua_setmetatable(L, -2); //buffer will now unref on __gc
	}
	else {
		lua_pushnil(L);
	}
	return 1;
}

static int lua_Buffers__len(lua_State *L)
{
	struct tu_glBuffers *buffers = luaL_checkudata(L, 1, "tu.Buffers");
	lua_pushinteger(L, buffers->num);
	return 1;
}

static int lua_Buffer__gc(lua_State *L)
{
	struct tu_glBuffer *buffer = luaL_checkudata(L, 1, "tu.Buffer");
	lua_getmetatable(L, -1);
	// printf("unref-ing Buffer %i\n", buffer->ref);
	luaL_unref(L, -1, buffer->ref);
	return 0;
}

static int lua_glBindBuffer(lua_State *L)
{
	GLenum *buffertarget = luaL_checkudata(L, 1, "tu.BufferTarget");
	struct tu_glBuffer *buffer = luaL_checkudata(L, 2, "tu.Buffer");

	glBindBuffer(*buffertarget, buffer->buffer);
	return 0;
}

/*
Approaches for passing Lua tables in as buffers 2021/07/08

1. Have an "attribute pointer mapping" table, essentially what's built up by calls to glVertexAttribPointer.
This could be an actual table or an object built up through successive calls. Once constructed, it is used to
interpret a Lua table as a buffer, value-by-value, casting to the expected size.

2. Put expected vertex attributes into the keys of a table. Their "type" is set through various constructor functions.
{
	color = vec3(),
	time = float(),
}

Vertex buffers *could be* tables of these vertex attribute tables (slow with string keys)
{
	{color = vec3(1,0,0), time = 10},
	{color = vec3(0,1,0), time = 10},
}
Having just arrays of numbers is faster, the issue is figuring out what order the attributes should be in.
{1, 0, 0, 10, 0, 1, 0, 10}
Including the name as a member of the attribute pointer entry solves this problem but makes the code look a bit messier.
{
	{color = vec3()},
	{time = float()},
}
We need to specify how to interpret the vertex data *somewhere*.
An array of structs in C is nice because it allows named AND ordered members with a clean syntax and no overhead.

3. Create a C object that's indexable by Lua to hold a vertex buffer.
This is very fast and can be compatible with vertex buffers described in C.
Unfortunately you wouldn't be able to take advantage of Lua's table constructor syntax
(without a conversion and an attribute pointer mapping)

2022/10/01

Using constructor functions:

VertexFormat {
	gl.vec3 'color',
	gl.float 'time',
}

Could be something like:

local buffers = gl.Buffers(1)
buffers:bind:bufferData
*/

static int lua_glBufferData(lua_State *L)
{
	//TODO make it possible to directly buffer Lua tables (maybe with a simple wrapper function)
	//Alternatively, make userdata buffer types that can be filled from Lua
	void *data = lua_touserdata(L, 3);
	if (!data)
		luaL_error(L, "BufferData expects a non-null data buffer");

	glBufferData(
		(GLenum)luaL_checkinteger(L, 1), //target
		(GLsizeiptr)luaL_checkinteger(L, 2), //size
		(const void*)data, //data
		(GLenum)luaL_checkinteger(L, 4)); //usage
	return 0;
}

static int lua_glBindTexture(lua_State *L)
{
	glBindTexture((GLenum)luaL_checkinteger(L, 1), //target
		(GLuint)luaL_checkinteger(L, 2)); //texture
	return 0;
}

static int lua_VertexArrays__index(lua_State *L)
{
	struct tu_glVertexArrays *arrays = luaL_checkudata(L, 1, "tu.VertexArrays");
	lua_Integer i = luaL_checkinteger(L, 2);
	if (i > 0 && i <= arrays->num)
		lua_pushinteger(L, arrays->arrays[i-1]);
	else
		lua_pushnil(L);
	return 1;
}

static int lua_glBindVertexArray(lua_State *L)
{
	glBindVertexArray((GLuint)luaL_checkinteger(L, 1)); //array
	return 0;
}

static int lua_glClear(lua_State *L)
{
	glClear((GLbitfield)luaL_checkinteger(L, 1)); //mask
	return 0;
}

static int lua_glClearColor(lua_State *L)
{
	glClearColor(
		(GLclampf)luaL_checknumber(L, 1), //red
		(GLclampf)luaL_checknumber(L, 2), //green
		(GLclampf)luaL_checknumber(L, 3), //blue
		(GLclampf)luaL_checknumber(L, 4)); //alpha
	return 0;
}

static int lua_glClearDepth(lua_State *L)
{
	glClearDepth((GLclampd)luaL_checknumber(L, 1)); //depth
	return 0;
}

static int lua_glDeleteShader(lua_State *L)
{
	GLuint *shader = luaL_checkudata(L, 1, "tu.Shader");
	// printf("lua_glDeleteShader was called on shader %i!\n", *shader);
	glDeleteShader(*shader);
	return 0;
}

static int lua_glDeleteProgram(lua_State *L)
{
	GLuint *program = luaL_checkudata(L, 1, "tu.ShaderProgram");
	// printf("lua_glDeleteProgram was called on shader program %i!\n", *program);
	glDeleteProgram(*program);
	return 0;
}

//Takes a table of integers and frees the vertex arrays associated with those integers 
static int lua_glDeleteVertexArrays(lua_State *L)
{
	struct tu_glVertexArrays *arrays = luaL_checkudata(L, 1, "tu.VertexArrays");
	glDeleteVertexArrays(arrays->num, arrays->arrays);
	return 0;
}

static int lua_glDepthFunc(lua_State *L)
{
	glDepthFunc((GLenum)luaL_checkinteger(L, 1)); //func
	return 0;
}

static int lua_glDisable(lua_State *L)
{
	GLenum *enableval = luaL_checkudata(L, 1, "tu.EnableValue");
	glDisable(*enableval);
	return 0;
}

static int lua_glDrawElementsInstanced(lua_State *L)
{
	glDrawElementsInstanced(
		(GLenum)luaL_checkinteger(L, 1),//mode
		(GLsizei)luaL_checkinteger(L, 1),//count
		(GLenum)luaL_checkinteger(L, 1),//type
		(const void *)luaL_checkinteger(L, 1),//indices
		(GLsizei)luaL_checkinteger(L, 1)); //primcount
	return 0;
}

static int lua_glEnable(lua_State *L)
{
	GLenum *enableval = luaL_checkudata(L, 1, "tu.EnableValue");
	glEnable(*enableval);
	return 0;
}

static int lua_glEnableVertexAttribArray(lua_State *L)
{
	glEnableVertexAttribArray((GLuint)luaL_checkinteger(L, 1)); //index
	return 0;
}

//Returns a table of integers
static int lua_vertexArrays(lua_State *L)
{
	GLsizei n = (GLsizei)luaL_checkinteger(L, 1);
	size_t size = sizeof(struct tu_glVertexArrays) + sizeof(GLuint) * n;
	struct tu_glVertexArrays *arrays = lua_newuserdatauv(L, size, 0);
	memset(arrays, 0, size);
	arrays->num = n;
	luaL_setmetatable(L, "tu.VertexArrays");
	glGenVertexArrays(n, arrays->arrays);
	return 1;
}

static int lua_glGetAttribLocation(lua_State *L)
{
	GLuint *program = luaL_checkudata(L, 1, "tu.ShaderProgram");
	lua_pushinteger(L, glGetAttribLocation(*program,
		(const GLchar*)luaL_checkstring(L, 2)));
	return 1;
}

static int lua_glGetUniformLocation(lua_State *L)
{
	GLuint *program = luaL_checkudata(L, 1, "tu.ShaderProgram");
	lua_pushinteger(L, glGetUniformLocation(*program,
		(const GLchar*)luaL_checkstring(L, 2)));
	return 1;
}

static int lua_glPolygonMode(lua_State *L)
{
	glPolygonMode((GLenum) luaL_checkinteger(L, 1), //face
		(GLenum)luaL_checkinteger(L, 2)); //mode
	return 0;
}

static int lua_glPrimitiveRestartIndex(lua_State *L)
{
	glPrimitiveRestartIndex((GLuint)luaL_checkinteger(L, 1)); //index
	return 0;
}

static int lua_glUseProgram(lua_State *L)
{
	GLuint *program = luaL_checkudata(L, 1, "tu.ShaderProgram");
	glUseProgram(*program);
	return 0;
}

static int lua_glVertexAttribDivisor(lua_State *L)
{
	glVertexAttribDivisor((GLuint)luaL_checkinteger(L, 1), //index
		(GLuint)luaL_checkinteger(L, 2)); //divisor
	return 0;
}

static int lua_glVertexAttribPointer(lua_State *L)
{
	glVertexAttribPointer(
		(GLint)luaL_checkinteger(L, 1), //index
		(GLint)luaL_checkinteger(L, 2), //size
		(GLenum)luaL_checkinteger(L, 3), //type,
		(GLboolean)luaL_checkinteger(L, 4), //normalized,
		(GLsizei)luaL_checkinteger(L, 5), //stride
		(const void *) luaL_checkinteger(L, 6)); //pointer
	return 0;
}

static int lua_glViewport(lua_State *L)
{
	glViewport((GLint)luaL_checkinteger(L, 1), (GLint)luaL_checkinteger(L, 2),
		(GLsizei)luaL_checkinteger(L, 3), (GLsizei)luaL_checkinteger(L, 4));
	return 0;
}

/* Opinionated Lua convenience wrappers */

extern char *shader_enum_to_string(GLenum shader_type);

//Pushes the log string onto the Lua stack [-0, +1, e]
static void lua_pushLog(lua_State *L, GLuint handle, GLboolean is_program)
{
	//Why write two log printing functions when you can use FUNCTION POINTERS AND TERNARY OPERATORS >:D
	GLboolean (*glIs)(GLuint)                                  = is_program? glIsProgram         : glIsShader;
	void (*glGetiv)(GLuint, GLenum, GLint *)                   = is_program? glGetProgramiv      : glGetShaderiv;
	void (*glGetInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *) = is_program? glGetProgramInfoLog : glGetShaderInfoLog;

	if (glIs(handle)) {
		int log_length = 0, max_length = 0;
		glGetiv(handle, GL_INFO_LOG_LENGTH, &max_length);
		char *info_log = (char *)malloc(max_length);
		glGetInfoLog(handle, max_length, &log_length, info_log);
		if (log_length > 0) {
			lua_pushlstring(L, info_log, log_length);
		}
		free(info_log);
	} else {
		lua_pushstring(L, "Handle ");
		lua_pushinteger(L, handle);
		lua_pushstring(L, is_program ? " is not a program handle." : " is not a shader handle.");
		lua_concat(L, 3);
	}
}

//Returns a shader or nil, error string if there was a problem.
static int lua_shader(lua_State *L, GLenum type)
{
	size_t len = 0;
	const GLchar *string = lua_tolstring(L, 1, &len);
	if (string && len) {
		GLint success = GL_FALSE;
		GLuint *shader = lua_newuserdatauv(L, sizeof(GLuint), 0);
		*shader = 0;
		luaL_setmetatable(L, "tu.Shader");
		*shader = glCreateShader(type);

		glShaderSource(*shader, 1, &string, &(GLint){len});
		glCompileShader(*shader);
		glGetShaderiv(*shader, GL_COMPILE_STATUS, &success);
		if (success) {
			return 1; //Returns the userdata, already on the stack
		}

		lua_pushnil(L);
		lua_pushstring(L, "Unable to compile ");        // 1
		lua_pushstring(L, shader_enum_to_string(type)); // 2
		lua_pushstring(L, " ");                         // 3
		lua_pushinteger(L, *shader);                    // 4
		lua_pushstring(L, "!\n");                       // 5
		lua_pushLog(L, *shader, GL_FALSE);              // 6
		lua_concat(L, 6);
	}
	return 2; //Returns nil, error string
}

static int lua_vertexShader(lua_State *L)
{
	return lua_shader(L, GL_VERTEX_SHADER);
}

static int lua_fragmentShader(lua_State *L)
{
	return lua_shader(L, GL_FRAGMENT_SHADER);
}

static int lua_geometryShader(lua_State *L)
{
	return lua_shader(L, GL_GEOMETRY_SHADER);
}

int lua_shaderProgram(lua_State *L)
{
	GLint success = GL_TRUE;
	int top = lua_gettop(L);

	GLuint *program = lua_newuserdatauv(L, sizeof(GLuint), 0);
	luaL_setmetatable(L, "tu.ShaderProgram");
	*program = glCreateProgram();

	for (int i = 1; i <= top; i++) {
		GLuint *shader = luaL_checkudata(L, i, "tu.Shader");
		glAttachShader(*program, *shader);
	}

	glLinkProgram(*program);
	glGetProgramiv(*program, GL_LINK_STATUS, &success);
	if (!success) {
		lua_pushnil(L);
		lua_pushstring(L, "Unable to link program "); // 1
		lua_pushinteger(L, *program);                 // 2
		lua_pushstring(L, "!\n");                     // 3
		lua_pushLog(L, *program, GL_TRUE);            // 4
		lua_concat(L, 4);
		return 2;
	}

	//ShaderProgram is already on the stack
	return 1;
}

static luaL_Reg lua_opengl[] = {
	{"ActiveTexture", lua_glActiveTexture},
	{"BindBuffer", lua_glBindBuffer},
	{"BindTexture", lua_glBindTexture},
	{"BindVertexArray", lua_glBindVertexArray},
	{"BufferData", lua_glBufferData},
	{"Clear", lua_glClear},
	{"ClearColor", lua_glClearColor},
	{"ClearDepth", lua_glClearDepth},
	{"DeleteBuffers", lua_Buffers__gc},
	{"DepthFunc", lua_glDepthFunc},
	{"Disable", lua_glDisable},
	{"DrawElementsInstanced", lua_glDrawElementsInstanced},
	{"Enable", lua_glEnable},
	{"EnableVertexAttribArray", lua_glEnableVertexAttribArray},
	{"Buffers", lua_Buffers},
	{"VertexArrays", lua_vertexArrays},
	{"GetAttribLocation", lua_glGetAttribLocation},
	{"GetUniformLocation", lua_glGetUniformLocation},
	{"PolygonMode", lua_glPolygonMode},
	{"PrimitiveRestartIndex", lua_glPrimitiveRestartIndex},
	{"UseProgram", lua_glUseProgram},
	{"VertexAttribDivisor", lua_glVertexAttribDivisor},
	{"VertexAttribPointer", lua_glVertexAttribPointer},
	{"Viewport", lua_glViewport},
	{"VertexShader", lua_vertexShader},
	{"FragmentShader", lua_fragmentShader},
	{"GeometryShader", lua_geometryShader},
	{"ShaderProgram", lua_shaderProgram},
	{NULL, NULL}
};

//Create a table of valid values for "glEnable"
//Example use: glEnable(GL_BLEND) in Lua is gl.Enable(gl.BLEND)
#define ENABLEVAL(name) {GL_##name, #name}
static struct {
	GLenum value;
	const char *name;
} lua_glEnableVals[] = {
	ENABLEVAL(BLEND),
	ENABLEVAL(COLOR_LOGIC_OP),
	ENABLEVAL(CULL_FACE),
	ENABLEVAL(DEBUG_OUTPUT),
	ENABLEVAL(DEBUG_OUTPUT_SYNCHRONOUS),
	ENABLEVAL(DEPTH_CLAMP),
	ENABLEVAL(DEPTH_TEST),
	ENABLEVAL(DITHER),
	ENABLEVAL(FRAMEBUFFER_SRGB),
	ENABLEVAL(LINE_SMOOTH),
	ENABLEVAL(MULTISAMPLE),
	ENABLEVAL(POLYGON_OFFSET_FILL),
	ENABLEVAL(POLYGON_OFFSET_LINE),
	ENABLEVAL(POLYGON_OFFSET_POINT),
	ENABLEVAL(POLYGON_SMOOTH),
	ENABLEVAL(PRIMITIVE_RESTART),
	ENABLEVAL(PRIMITIVE_RESTART_FIXED_INDEX),
	ENABLEVAL(RASTERIZER_DISCARD),
	ENABLEVAL(SAMPLE_ALPHA_TO_COVERAGE),
	ENABLEVAL(SAMPLE_ALPHA_TO_ONE),
	ENABLEVAL(SAMPLE_COVERAGE),
	ENABLEVAL(SAMPLE_SHADING),
	ENABLEVAL(SAMPLE_MASK),
	ENABLEVAL(SCISSOR_TEST),
	ENABLEVAL(STENCIL_TEST),
	ENABLEVAL(TEXTURE_CUBE_MAP_SEAMLESS),
	ENABLEVAL(PROGRAM_POINT_SIZE),
};

//Create a table of valid targets for "glBindBuffer"
#define BUFFERTARGET(name) {GL_##name, #name}
static struct {
	GLenum target;
	const char *name;
} lua_glBindBufferTargets[] = {
	BUFFERTARGET(ARRAY_BUFFER),              //Vertex attributes
	BUFFERTARGET(ATOMIC_COUNTER_BUFFER),     //Atomic counter storage
	BUFFERTARGET(COPY_READ_BUFFER),          //Buffer copy source
	BUFFERTARGET(COPY_WRITE_BUFFER),         //Buffer copy destination
	BUFFERTARGET(DISPATCH_INDIRECT_BUFFER),  //Indirect compute dispatch commands
	BUFFERTARGET(DRAW_INDIRECT_BUFFER),      //Indirect command arguments
	BUFFERTARGET(ELEMENT_ARRAY_BUFFER),      //Vertex array indices
	BUFFERTARGET(PIXEL_PACK_BUFFER),         //Pixel read target
	BUFFERTARGET(PIXEL_UNPACK_BUFFER),       //Texture data source
	BUFFERTARGET(QUERY_BUFFER),              //Query result buffer
	BUFFERTARGET(SHADER_STORAGE_BUFFER),     //Read-write storage for shaders
	BUFFERTARGET(TEXTURE_BUFFER),            //Texture data buffer
	BUFFERTARGET(TRANSFORM_FEEDBACK_BUFFER), //Transform feedback buffer
	BUFFERTARGET(UNIFORM_BUFFER),            //Uniform block storage
};

/*
STREAM
The data store contents will be modified once and used at most a few times.
STATIC
The data store contents will be modified once and used many times.
DYNAMIC
The data store contents will be modified repeatedly and used many times.

The nature of access may be one of these:

DRAW
The data store contents are modified by the application, and used as the source for GL drawing and image specification commands.
READ
The data store contents are modified by reading data from the GL, and used to return that data when queried by the application.
COPY
The data store contents are modified by reading data from the GL, and used as the source for GL drawing and image specification commands.
*/
//Create a table of valid usages for "glBindBuffer"
#define BUFFERUSAGE(name) {GL_##name, #name}
static struct {
	GLenum usage;
	const char *name;
} lua_glBindBufferUsages[] = {
	BUFFERUSAGE(STREAM_DRAW),
	BUFFERUSAGE(STREAM_READ),
	BUFFERUSAGE(STREAM_COPY),
	BUFFERUSAGE(STATIC_DRAW),
	BUFFERUSAGE(STATIC_READ),
	BUFFERUSAGE(STATIC_COPY),
	BUFFERUSAGE(DYNAMIC_DRAW),
	BUFFERUSAGE(DYNAMIC_READ),
	BUFFERUSAGE(DYNAMIC_COPY),
};

int luaopen_lua_opengl(lua_State *L)
{
	//Set up garbage-collection functions for Shader and ShaderProgram userdata
	luaL_newmetatable(L, "tu.Shader");
	lua_pushcfunction(L, lua_glDeleteShader);
	lua_setfield(L, -2, "__gc");

	luaL_newmetatable(L, "tu.ShaderProgram");
	lua_pushcfunction(L, lua_glDeleteProgram);
	lua_setfield(L, -2, "__gc");

	luaL_newmetatable(L, "tu.VertexArrays");
	lua_pushcfunction(L, lua_glDeleteVertexArrays);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, lua_VertexArrays__index);
	lua_setfield(L, -2, "__index");

	luaL_newmetatable(L, "tu.Buffers");
	lua_pushcfunction(L, lua_Buffers__gc);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, lua_Buffers__index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, lua_Buffers__len);
	lua_setfield(L, -2, "__len");

	luaL_newmetatable(L, "tu.Buffer");
	lua_pushcfunction(L, lua_Buffer__gc);
	lua_setfield(L, -2, "__gc");

	luaL_newmetatable(L, "tu.EnableValue");
	luaL_newmetatable(L, "tu.BufferTarget");
	luaL_newmetatable(L, "tu.BufferUsage");

	//Create a table with enough room for all funcs and glEnable defines
	size_t len_lua_opengl = sizeof(lua_opengl) / sizeof(lua_opengl[0]);
	size_t len_enablevals = sizeof(lua_glEnableVals) / sizeof(lua_glEnableVals[0]);
	size_t len_buffertargets = sizeof(lua_glBindBufferTargets) / sizeof(lua_glBindBufferTargets[0]);
	size_t len_bufferusages = sizeof(lua_glBindBufferUsages) / sizeof(lua_glBindBufferUsages[0]);
	size_t nrec = len_lua_opengl + len_enablevals + len_buffertargets + len_bufferusages;
	lua_createtable(L, 0, nrec);
	//Assign the functions into the table
	luaL_setfuncs(L, lua_opengl, 0);

	//In the future I could make these be userdata with a type to make it type-safe
	//Assign the "enable values" defines into the table
	for (int i = 0; i < len_enablevals; i++) {
		GLenum *enableval = lua_newuserdatauv(L, sizeof(GLenum), 0);
		*enableval = lua_glEnableVals[i].value;
		luaL_setmetatable(L, "tu.EnableValue");
		lua_setfield(L, -2, lua_glEnableVals[i].name);
	}
	//Assign the "buffer targets" defines into the table
	for (int i = 0; i < len_buffertargets; i++) {
		GLenum *buffertarget = lua_newuserdatauv(L, sizeof(GLenum), 0);
		*buffertarget = lua_glBindBufferTargets[i].target;
		luaL_setmetatable(L, "tu.BufferTarget");
		lua_setfield(L, -2, lua_glBindBufferTargets[i].name);
	}

	//Assign the "buffer usages" defines into the table
	for (int i = 0; i < len_bufferusages; i++) {
		GLenum *bufferusage = lua_newuserdatauv(L, sizeof(GLenum), 0);
		*bufferusage = lua_glBindBufferUsages[i].usage;
		luaL_setmetatable(L, "tu.BufferUsage");
		lua_setfield(L, -2, lua_glBindBufferUsages[i].name);
	}

	return 1;
}