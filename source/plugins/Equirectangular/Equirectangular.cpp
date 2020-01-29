#include "Equirectangular.h"
using namespace ffglex;
using namespace ffglqs;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< Equirectangular >,// Create method
	"SH02",                  // Plugin unique ID of maximum length 4.
	"Equirectangular",               // Plugin name
	2,                       // API major version number
	1,                       // API minor version number
	1,                       // Plugin major version number
	0,                       // Plugin minor version number
	FF_EFFECT,               // Plugin type
	"",                      // Plugin description
	""                       // About
);

static const char _vertexShaderCode[] = R"(#version 410 core
uniform vec2 MaxUV;

layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;

out vec2 uv;

void main()
{
	gl_Position = vPosition;
	uv = vUV * MaxUV;
}
)";

static const char _fragmentShaderCode[] = R"(#version 410 core
#define PI 3.141592653589793
#define TWPI 6.283185307179586
uniform vec2 MaxUV;
uniform float Time;
uniform sampler2D InputTexture;
uniform float rotate;
uniform float scale;
uniform float zoom;
uniform float perspective;
uniform float look_X;
uniform float look_Y;
uniform float position_X;
uniform float position_Y;

in vec2 uv;
in vec2 vUV;

out vec4 fragColor;

vec2 lookXY = vec2(look_X, look_Y);
vec2 positionXY = vec2(position_X, position_Y);

vec2 _rotate(vec2 v, float angle) {return cos(angle)*v+sin(angle)*vec2(v.y,-v.x);}

void main()
{

    vec2 _uv = uv.xy / MaxUV.xy;
	vec2 _uvc = uv.xy / MaxUV.y - vec2(MaxUV.x/MaxUV.y/2.0, 0.5);
    vec2 v = zoom*_uv + MaxUV.y;
    v.y -= 0.5;

    float th = v.y * PI,
        ph = v.x * TWPI;
    vec3 sp = vec3( sin(ph) * cos(th), sin(th), cos(ph) * cos(th) );
    vec3 pos = vec3( PI, rotate * PI, 0);
    pos *= scale;
	//fragColor = vec4(1.0);
    sp = mix(sp, normalize(vec3(_uvc, 1.0)), perspective);

    sp.yz = _rotate(sp.yz, lookXY.y*TWPI);
    sp.xy = _rotate(sp.xy, lookXY.x*TWPI);

    fragColor = texture(InputTexture, mod(vec2(dot(pos, sp.zxy), dot(pos.yzx, sp.zxy))+positionXY.xy, 1.0));
}
)";

Equirectangular::Equirectangular()
{
	// Input properties
	SetMinInputs( 1 );
	SetMaxInputs( 1 );

	AddParam( Param::Create( "scale", 0.20 ) );
	AddParam( Param::Create( "zoom", 0.5 ) );
	AddParam( Param::Create( "perspective" ) );
	AddParam( Param::Create( "rotate", 0.0 ) );
	AddParam( Param::Create( "look_X" ) );
	AddParam( Param::Create( "look_Y" ) );
	AddParam( Param::Create( "position_X" ) );
	AddParam( Param::Create( "position_Y" ) );
}
Equirectangular::~Equirectangular()
{
}

FFResult Equirectangular::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( _vertexShaderCode, _fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}

	//Use base-class init as success result so that it retains the viewport.
	return CFFGLPlugin::InitGL( vp );
}
FFResult Equirectangular::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	if( pGL->numInputTextures < 1 )
		return FF_FAIL;

	if( pGL->inputTextures[ 0 ] == NULL )
		return FF_FAIL;

	//FFGL requires us to leave the context in a default state on return, so use this scoped binding to help us do that.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	//The shader's sampler is always bound to sampler index 0 so that's where we need to bind the texture.
	//Again, we're using the scoped bindings to help us keep the context in a default state.
	ScopedSamplerActivation activateSampler( 0 );
	Scoped2DTextureBinding textureBinding( pGL->inputTextures[ 0 ]->Handle );

	shader.Set( "inputTexture", 0 );

	//The input texture's dimension might change each frame and so might the content area.
	//We're adopting the texture's maxUV using a uniform because that way we dont have to update our vertex buffer each frame.
	FFGLTexCoords maxCoords = GetMaxGLTexCoords( *pGL->inputTextures[ 0 ] );
	shader.Set( "MaxUV", maxCoords.s, maxCoords.t );

	float timeNow = hostTime / 1000.0;
	shader.Set( "Time", timeNow );

	//This takes care of sending all the parameter that the plugin registered to the shader.
	SendParams( shader );

	quad.Draw();

	return FF_SUCCESS;
}
FFResult Equirectangular::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();

	return FF_SUCCESS;
}
