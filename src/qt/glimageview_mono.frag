# version 400

// normalization range
uniform int u_blacklevel;
uniform int u_whitelevel;

// colormap
uniform float u_cmap_select;
uniform vec3 u_highlight_clipped_under;
uniform vec3 u_highlight_clipped_over;

// textures
uniform sampler2D u_image;
uniform sampler2D u_colormaps;

// texCoords from vertex shader
in vec2 v_texCoord;

// output color
layout(location = 0) out vec4 outputColor;

void main(void)
{
    // Calculate the pixel coordinate for manual sampling
    ivec2 size = textureSize(u_image, 0);
    ivec2 coord = ivec2(v_texCoord * vec2(size));

    // The image is passed in as LUMINANCE format in [0, 1] scale
    // rescale to [0, 65535]
    highp int value_raw = int(texelFetch(u_image, coord, 0).r * 65535.0);

    // Black subtraction and rescale back to [0, 1]
    // (assume u_whitelevel > u_blacklevel)
    highp float value_norm = float(value_raw - u_blacklevel) / float(u_whitelevel - u_blacklevel);
    // Clip to [0.0, 1.0]
    value_norm = clamp(value_norm, 0.0, 1.0);

    // Apply colormap
    if ( value_raw <= u_blacklevel ) {
      outputColor = vec4(u_highlight_clipped_under, 0.5);
    } else if (value_raw >= u_whitelevel ) {
      outputColor = vec4(u_highlight_clipped_over, 0.5);
    } else {
      outputColor = texture(u_colormaps, vec2(value_norm, u_cmap_select));
    }
}
