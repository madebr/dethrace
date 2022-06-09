#if defined(__EMSCRIPTEN__)
#define SHADER_PREFIX "#version 100\n" "precision mediump float;\n"
#else
#define SHADER_PREFIX "#version 330 core\n"
#endif

const char* vs_2d =
    SHADER_PREFIX
    "attribute vec3 aPos;\n"
    "attribute vec3 aColor;\n"
    "attribute vec2 aTexCoord;\n"
    "\n"
    "varying vec2 TexCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "   TexCoord = aTexCoord;\n"
    "}\0";

const char* fs_2d =
    SHADER_PREFIX
    "uniform sampler2D pixels;\n"
    "uniform sampler2D palette;\n"
    "\n"
    "void main()\n"
    "{\n"
    "  vec4 paletteColor = texture2D(pixels, gl_FragCoord.xy);\n"
    "  int palette_index = int(paletteColor.x);\n"
    "  vec4 texel = texture2D(palette, vec2(palette_index, 0));\n"
    "  gl_FragColor = texel;\n"
    "}\n\0";

const char* vs_3d =
    SHADER_PREFIX
    "attribute vec3 aPos;\n"
    "attribute vec3 aNormal;\n"
    "attribute vec2 aUV;\n"
    "\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "\n"
    "varying vec3 FragPos;\n"
    "varying vec3 Normal;\n"
    "varying vec2 TexCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   FragPos = vec3(model * vec4(aPos, 1.0));\n"
    "   Normal = aNormal;\n"
    "   TexCoord = aUV;\n"
    "   gl_Position = projection * view * vec4(FragPos, 1.0);\n"
    "}\0";

const char* fs_3d =
    SHADER_PREFIX
    "uniform vec3 pixels_transform[2];\n"
    "uniform sampler2D pixels;\n"
    "uniform sampler2D shade_table;\n"
    "uniform int palette_index_override;\n"
    "uniform int light_value;\n"
    "uniform vec4 clip_planes[6];\n"
    "uniform int clip_plane_count;\n"
    "\n"
    "varying vec3 FragPos;\n"
    "varying vec3 Normal;\n"
    "varying vec2 TexCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "  for(int i=0; i<clip_plane_count; i++) {\n"
    "      // calculate signed plane-vertex distance\n"
    "      vec4 v4 = vec4(gl_FragCoord.xyz, 1);\n"
    "      float d = dot(clip_planes[i], v4);\n"
    "      if (d < 0.0) discard;\n"
    "  }\n"
    "    if (palette_index_override == -1) {"
    "       vec3 sample_coord = mat3(pixels_transform[0], pixels_transform[1], vec3(0, 0, 1)) * vec3(TexCoord.xy, 1);\n"
    "       int texel = int(texture2D(pixels, sample_coord.xy).x);\n"
    "       if (light_value == -1) {\n"
    "           gl_FragColor.x = texel;\n"
    "       } else {\n"
    "           gl_FragColor.x = texture2D(shade_table, ivec2(texel, light_value)).x;\n"
    "       }\n"
    "       if (gl_FragColor.x == 0) {\n"
    "           discard;\n"
    "       }\n"
    "    }\n"
    "    else {\n"
    "        gl_FragColor.x = float(palette_index_override);\n"
    "    }\n"
    "}\n\0";
