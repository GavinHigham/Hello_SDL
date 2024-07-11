local glla = require 'glla'
local util,gl = require 'lib/util'.safe()
local glsw = util.glsw
local vec3 = glla.vec3
circlegraph = {}

function circlegraph.init()

	local shaders = glsw(io.open('shaders/glsw/atmosphere.glsl', 'r'):read('a'))
	local common = glsw(io.open('shaders/glsw/common.glsl', 'r'):read('a'))
	local vs, log = gl.VertexShader('#version 330\n' .. shaders['vertex.GL33'])
	if log then
		print(log)
	end
	local fs, log = gl.FragmentShader(table.concat({'#version 330',
			shaders['fragment.GL33'], common['utility'], common['noise'],
			common['lighting'], common['debugging']}, '\n'))
	if log then
		print(log)
	end
	local shaderProgram, log = gl.ShaderProgram(vs, fs)
	if log then
		print(log)
	end
	_G.shaderProgram = shaderProgram
	-- print("#shaderProgram = "..#shaderProgram)

	gl.Enable(gl.DEPTH_TEST);
	gl.Enable(gl.PRIMITIVE_RESTART);
	gl.Disable(gl.CULL_FACE);

	-- local a, b = vec3(1, 2, 3), vec3(4, 5, 6)
	-- -- a.xzy = vec3(10, 11, 12)
	-- -- a.zyx = b.zyy
	-- local c = a:normalize()
	-- print(a)
	-- print(c)

	return 0
end

function circlegraph.deinit()
	shaderProgram = nil
	collectgarbage()
end

function circlegraph.resize(width, height)
end

function circlegraph.update(dt)
end

function circlegraph.render()
	gl.ClearColor(0.01, 0.22, 0.23, 1.0);
	gl.Clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT)
	gl.BindVertexArray(vertexArrays[1])
	gl.UseProgram(shaderProgram)

--[[
Nice syntax for setting uniforms would abstract away the type, but still do type checking. Something like:
(Option 1)
shaderProgram.projectionMatrix = projMat
shaderProgram.cameraPos = vec3(3, 4, 5)
shaderProgram.time = 5.4

Could also do:
shaderProgram:uniforms {
	projectionMatrix = projMat,
	cameraPos = vec3(3, 4, 5),
	time = 5.4,
}
shaderProgram:uniforms{time = 3.1}
uniformsTable = shaderProgram:uniforms()
gl.uniforms(shaderProgram){time = 0.0}

Preferred option. It's succinct, and not too confusing if you're familiar with Lua-isms.

(Option 2)
shaderProgram:setUniform("projectionMatrix", projMat)

Sort of Java/Love2D style. Boring, but other options could be built on top of it.

(Option 3)
shaderProgram:uniforms["projectionMatrix"] = projMat
shaderProgram:uniforms.projectionMatrix = projMat
shaderProgram:uniforms = {
	projectionMatrix = projMat,
	cameraPos = vec3(3, 4, 5),
	time = 5.4,
}

This one gets annoying, having to type .uniforms repeatedly, and string args for names are annoying.
The assignment of the table is misleading, because you're not nullifying unreferenced uniforms.
Although, I could make that be an alternative, where if you don't explicitly assign every uniform, it's an error.

--
In all cases, I want to make this step unecessary when I unify shaders with Lua scripting.
]]
	shaderProgram.air_b = vec3(0.00000519673, 0.0000121427, 0.0000296453)

--[[
Next, I want a way to tweak uniforms with minimal fiddling. Something like:
	shaderProgram.time = tweak{name = 'Time', min = 0, max = 10, style = tweakStyles.default}

	{
		.name = "Planet Radius", .x = 5.0, .y = 0, .min = 0.1, .max = 5.0,
		.target = &at->planet_radius, 
		.style = wstyles, .color = {.fill = {187, 187, 187, 255}, .border = {95, 95, 95, 255}, .font = {255, 255, 255}}
	},
]]
end

return circlegraph

--[[
TODO 11/11/2021

I'm going to need to create a linear algebra library for Lua,
or create bindings to the C one I've already created.

I also need to go through the OpenGL header and extract the
values for enums and other value definitions.

Alternatively: Create bindings for a higher-level abstraction
first, then add support for custom render functions later on.

API could be something like, create mesh with particular shader

]]