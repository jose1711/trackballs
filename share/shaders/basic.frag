#version 130

#ifdef GL_ES
precision mediump float;
#endif

uniform int render_stage;

uniform mat4 model_matrix;
uniform int fog_active;
uniform float fog_start;
uniform float fog_end;
uniform vec3 fog_color;
uniform vec3 light_position;
uniform vec3 light_diffuse;
uniform vec3 light_specular;
uniform vec3 light_ambient;
uniform vec3 global_ambient;
uniform float quadratic_attenuation;

uniform sampler2DArray arrtex;
varying vec4 fcolor;
varying vec3 texco;
varying vec3 cpos;
varying vec3 inormal;
varying float flatkey;

void main(void) {
  if (render_stage == 0 && fcolor.w < 0.95) {
    // Drop transparent elements in first stage only.
    // Tolerance is generous since float packing may induce slight errors
    discard;
  }

  // Linear combinations of units need not be units
  vec3 normal;
  if (flatkey >= 1.) {
    normal = normalize(inormal);
  } else {
    normal = normalize(cross(dFdx(cpos), dFdy(cpos)));
  }
  float clamped = max(0, min(8, floor(16. * texco.z + 0.5)));
  vec4 texcolor = fcolor * texture(arrtex, vec3(texco.xy, clamped));

  // Lighting model, not tuned very much
  vec3 light_pos = vec4(model_matrix * vec4(light_position, 1.)).xyz;
  float light_distance = length(light_pos - cpos);
  float strength = 1.0 / (1. + quadratic_attenuation*light_distance*light_distance);
  vec3 L = normalize(light_pos - cpos);
  vec3 E = normalize(-cpos);
  vec3 R = normalize(-reflect(L, normal));
  vec3 Iambient = texcolor.xyz * strength*light_ambient;
  vec3 Idiffuse = texcolor.xyz * strength*light_diffuse * max(dot(normal, L), 0.0);
  Idiffuse = clamp(Idiffuse, 0.0, 1.0);
  vec3 surfcolor = light_ambient + Iambient + Idiffuse;
  surfcolor = clamp(surfcolor, 0.0, 1.0);

  float dist;
  if (fog_active == 0) {
    // Fog not active, skip
    dist = 0.;
  } else {
    // Apply linear fog as in original
    dist = clamp(1.0 - (fog_end - length(cpos)) / (fog_end - fog_start), 0., 1.0);
  }
  // Force override alpha
  gl_FragColor = vec4(mix(surfcolor, fog_color, dist), fcolor.w);
}
