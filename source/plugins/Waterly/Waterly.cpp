#include "Waterly.h"
using namespace ffglex;
using namespace ffglqs;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< Waterly >,// Create method
	"SH01",                  // Plugin unique ID of maximum length 4.
	"Waterly",               // Plugin name
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
#define PI 3.14159265358979
uniform vec2 MaxUV;
uniform float Time;
uniform sampler2D InputTexture;
uniform float mode;
uniform float rate;
uniform float scale;
uniform float intensity;

in vec2 uv;

out vec4 fragColor;


float SCALE = 1.0;
float style = 0.0;
float input_scale = 1.0;
vec2 direction = vec2(0.0);
vec2 position = vec2(0.0);

mat3 m = mat3( 0.00,  0.80,  0.60,
              -0.80,  0.36, -0.48,
              -0.60, -0.48,  0.64 );

float hash( float n )
{
    return fract(sin(n)*43758.5453);
}

float noise( in vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);

    f = f*f*(3.0-2.0*f);

    float n = p.x + p.y*57.0 + 113.0*p.z;

    float res = mix(mix(mix( hash(n+  0.0), hash(n+  1.0),f.x),
                        mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
                    mix(mix( hash(n+113.0), hash(n+114.0),f.x),
                        mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
    return res;
}

float fbm( vec3 p )
{
    float f;
    f  = 0.5000*noise( p ); p = m*p*2.02;
    f += 0.2500*noise( p ); p = m*p*2.03;
    f += 0.1250*noise( p ); p = m*p*2.01;
    f += 0.0625*noise( p );
    return f;
}
// --- End of: Created by inigo quilez --------------------
float mynoise ( vec3 p)
{
    return mix(noise(p*scale)*intensity, .5+.5*noise(p*scale)*intensity, style);
}
float myfbm( vec3 p )
{
    float f;
    f  = 0.5000*mynoise( p ); p = m*p*2.02;
    f += 0.2500*mynoise( p ); p = m*p*2.03;
    f += 0.1250*mynoise( p ); p = m*p*2.01;
    f += 0.0625*mynoise( p ); p = m*p*2.05;
    f += 0.0625/2.*mynoise( p ); p = m*p*2.02;
    f += 0.0625/4.*mynoise( p );
    return f;
}
float myfbm2( vec3 p )
{
    float f;
    f  = 1. - 0.5000*mynoise( p ); p = m*p*2.02;
    f *= 1. - 0.2500*mynoise( p ); p = m*p*2.03;
    f *= 1. - 0.1250*mynoise( p ); p = m*p*2.01;
    f *= 1. - 0.0625*mynoise( p ); p = m*p*2.05;
    f *= 1. - 0.0625/2.*mynoise( p ); p = m*p*2.02;
    f *= 1. - 0.0625/4.*mynoise( p );
    return f;
}

mat2 rotate2d(float _angle){
    return mat2(cos(_angle),-sin(_angle),
    sin(_angle),cos(_angle));
}

void main()
{
	vec2 _uv = uv;
    _uv.xy += direction.xy;

    _uv = rotate2d(1*-PI) * _uv;

	vec3 v;
	vec3 p = 4.*vec3(_uv,0.)+(Time)*(.1,.7,1.2)*rate;
	float x;
	if (mode == 1.0) {
		x = myfbm(p);
	} else {
		x = myfbm2(p);
	}

	//v = vec3(x);
	v = (.5+.5*sin(x*vec3(30.,20.,10.)*SCALE))/SCALE;
	float g = 1.;
	//g = pow(length(v),1.);
	//g =  .5*noise(8.*m*m*m*p)+.5; g = 2.*pow(g,3.);
	v *= g;

    vec3 Ti = texture(InputTexture, mod(.02*v.xy+position.xy+uv.xy, 1.0)).rgb;
    vec3 T=Ti;
    //T = Ti+(1.-Ti)*Tf;
	vec3 T1,T2;
	T1 = vec3(0.,0.,0.); T1 *= .5*(T+1.);
	T2 = vec3(1.,1.,1.); //T2 = 1.2*Ti*vec3(1.,.8,.6)-.2;
	v = mix(T1,T2,T);
	fragColor = vec4(v, 1.0);
}
)";

Waterly::Waterly()
{
	// Input properties
	SetMinInputs( 1 );
	SetMaxInputs( 1 );

	AddParam( ParamRange::Create( "rate", 0, ParamRange::Range(-1, 1) ) );
	AddParam( Param::Create( "intensity" ) );
	AddParam( Param::Create( "scale" ) );
	AddParam( ParamOption::Create( "mode", { { "1" }, { "2" } } ) );
}
Waterly::~Waterly()
{
}

FFResult Waterly::InitGL( const FFGLViewportStruct* vp )
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
FFResult Waterly::ProcessOpenGL( ProcessOpenGLStruct* pGL )
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
FFResult Waterly::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();

	return FF_SUCCESS;
}
