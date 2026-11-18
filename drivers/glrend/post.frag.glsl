##ifdef GL_ES
#version 300 es
precision highp float;
precision mediump int;
precision lowp usampler2D;
##endif
##ifdef GL_CORE
#version 140
#extension GL_ARB_explicit_attrib_location:require
##endif

in vec3 colour;
in vec2 uv;

uniform sampler2D uSampler;
uniform float uFlipVertically;
uniform float uSharpenAmount;

layout (location = 0) out vec4 mainColour;

/*
 * Mild 4-neighbour unsharp mask (Laplacian sharpen).
 *
 * The kernel weights are (centre 1 + a, N/S/E/W -a/4), which sum to exactly 1,
 * so the pass has unity DC gain: flat regions are copied through unchanged and
 * overall brightness/colour is preserved. The result is clamped to [0, 1] so
 * that edge overshoot (halos/ringing) cannot push pixels out of range.
 *
 * The source is the drawable-sized off-screen colour attachment, presented 1:1
 * through a fullscreen quad with GL_NEAREST + GL_CLAMP_TO_EDGE sampling, so the
 * offsets below land exactly on neighbouring texels and edge fragments clamp
 * against the image border instead of wrapping.
 */
void main()
{
	// Same vertical flip as default.frag; the tap offsets are symmetric so the
	// direction of the flip does not matter for the blur.
	vec2 base = vec2(uv.x, abs(uFlipVertically - uv.y));
	vec2 texel = 1.0 / vec2(textureSize(uSampler, 0));

	vec3 c = texture(uSampler, base).rgb;
	vec3 n = texture(uSampler, base + vec2(0.0, texel.y)).rgb;
	vec3 s = texture(uSampler, base - vec2(0.0, texel.y)).rgb;
	vec3 e = texture(uSampler, base + vec2(texel.x, 0.0)).rgb;
	vec3 w = texture(uSampler, base - vec2(texel.x, 0.0)).rgb;

	vec3 blur = (n + s + e + w) * 0.25;
	vec3 sharp = c + uSharpenAmount * (c - blur);

	mainColour = vec4(clamp(sharp, 0.0, 1.0), 1.0);
}
