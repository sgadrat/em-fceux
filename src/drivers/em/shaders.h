
static const char* rgb_vert_src =
    "precision highp float;\n"
    DEFINE(NUM_TAPS)
    DEFINE(IDX_W)
    "attribute vec4 a_vert;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv[int(NUM_TAPS)];\n"
    "varying vec2 v_deemp_uv;\n"
    "#define UV_OUT(i_, o_) v_uv[i_] = vec2(uv.x + (o_)/IDX_W, uv.y)\n"
    "void main() {\n"
    "vec2 uv = a_uv;\n"
    "UV_OUT(0,-2.0);\n"
    "UV_OUT(1,-1.0);\n"
    "UV_OUT(2, 0.0);\n"
    "UV_OUT(3, 1.0);\n"
    "UV_OUT(4, 2.0);\n"
    "v_deemp_uv = vec2(uv.y, 0.0);\n"
    "gl_Position = a_vert;\n"
    "}\n";
static const char* rgb_frag_src =
    "precision highp float;\n"
    DEFINE(NUM_SUBPS)
    DEFINE(NUM_TAPS)
    DEFINE(LOOKUP_W)
    DEFINE(IDX_W)
    DEFINE(YW2)
    DEFINE(CW2)
    "uniform sampler2D u_idxTex;\n"
    "uniform sampler2D u_deempTex;\n"
    "uniform sampler2D u_lookupTex;\n"
    "uniform vec3 u_mins;\n"
    "uniform vec3 u_maxs;\n"
    "uniform float u_brightness;\n"
    "uniform float u_contrast;\n"
    "uniform float u_color;\n"
    "uniform float u_rgbppu;\n"
    "uniform float u_gamma;\n"
    "varying vec2 v_uv[int(NUM_TAPS)];\n"
    "varying vec2 v_deemp_uv;\n"
    "const mat3 c_convMat = mat3(\n"
    "    1.0,        1.0,        1.0,       // Y\n"
    "    0.946882,   -0.274788,  -1.108545, // I\n"
    "    0.623557,   -0.635691,  1.709007   // Q\n"
    ");\n"
    "#define P(i_)  p = floor(IDX_W * v_uv[i_])\n"
    "#define U(i_)  (mod(p.x - p.y, 3.0)*NUM_SUBPS*NUM_TAPS + subp*NUM_TAPS + float(i_)) / (LOOKUP_W-1.0)\n"
    "#define V(i_)  ((255.0/511.0) * texture2D(u_idxTex, v_uv[i_]).r + deemp)\n"
    "#define UV(i_) uv = vec2(U(i_), V(i_))\n"
    "#define RESCALE(v_) ((v_) * (u_maxs-u_mins) + u_mins)\n"
    "#define SMP(i_) P(i_); UV(i_); yiq += RESCALE(texture2D(u_lookupTex, uv).rgb)\n"

    "void main(void) {\n"
    "float deemp = 64.0 * (255.0/511.0) * texture2D(u_deempTex, v_deemp_uv).r;\n"
    "float subp = mod(floor(NUM_SUBPS*IDX_W * v_uv[int(NUM_TAPS)/2].x), NUM_SUBPS);\n"
    "vec2 p;\n"
    "vec2 la;\n"
    "vec2 uv;\n"
    "vec3 yiq = vec3(0.0);\n"
    "SMP(0);\n"
    "SMP(1);\n"
    "SMP(2);\n"
    // Snatch in RGB PPU; uv.x is already calculated, so just read from lookup tex with u=1.0.
    "vec3 rgbppu = RESCALE(texture2D(u_lookupTex, vec2(1.0, uv.y)).rgb);\n"
    "SMP(3);\n"
    "SMP(4);\n"
    // Working multiplier for filtered chroma to match PPU is 2/5 (for CW2=12).
    // Is this because color fringing with composite?
    "yiq *= (8.0/2.0) / vec3(YW2, CW2-2.0, CW2-2.0);\n"
    "yiq = mix(yiq, rgbppu, u_rgbppu);\n"
    "yiq.gb *= u_color;\n"
    "vec3 result = clamp(c_convMat * yiq, 0.0, 1.0);\n"
    // Gamma convert RGB from NTSC space to space similar to SRGB.
    "result = pow(result, vec3(u_gamma));\n"
    // NOTE: While this seems to be wrong (after gamma), it works well in practice...?
    "gl_FragColor = vec4(u_contrast * result + u_brightness, 1.0);\n"
    "}\n";

static const char* stretch_vert_src =
    DEFINE(IDX_H)
    "precision highp float;\n"
    "attribute vec4 a_vert;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv[2];\n"
    "void main() {\n"
    "vec2 uv = a_uv;\n"
    "v_uv[0] = vec2(uv.x, 1.0 - uv.y);\n"
    "v_uv[1] = vec2(v_uv[0].x, v_uv[0].y - 0.25/IDX_H);\n"
    "gl_Position = a_vert;\n"
    "}\n";
static const char* stretch_frag_src =
    "precision highp float;\n"
    DEFINE(IDX_H)
    DEFINE(M_PI)
    "uniform float u_scanlines;\n"
    "uniform sampler2D u_rgbTex;\n"
    "varying vec2 v_uv[2];\n"
    "void main(void) {\n"
    // Sample adjacent scanlines, linearize color and average to smoothen slightly vertically.
    "vec3 c0 = texture2D(u_rgbTex, v_uv[0]).rgb;\n"
    "vec3 c1 = texture2D(u_rgbTex, v_uv[1]).rgb;\n"
    "vec3 color = 0.5 * (c0*c0 + c1*c1);\n"
    // Use oscillator to mix color with its square to achieve scanlines effect.
    // This formula dims midtones, keeping bright and very dark colors.
    "float scanlines = u_scanlines * (1.0 - abs(sin(M_PI*IDX_H * v_uv[0].y - M_PI*0.125)));\n"
    // Gamma encode color w/ sqrt().
    "gl_FragColor = vec4(sqrt(mix(color, max(2.0*color - 1.0, 0.0), scanlines)), 1.0);\n"
    "}\n";

static const char* screen_vert_src =
    "precision highp float;\n"
    DEFINE(RGB_W)
    DEFINE(M_PI)
    "attribute vec4 a_vert;\n"
    "attribute vec3 a_norm;\n"
    "attribute vec2 a_uv;\n"
    "uniform float u_convergence;\n"
    "uniform mat4 u_mvp;\n"
    "uniform vec2 u_uvScale;\n"
    "varying vec2 v_uv[5];\n"
    "varying vec3 v_color;\n"
    "#define TAP(i_, o_) v_uv[i_] = uv + vec2((o_) / RGB_W, 0.0)\n"
    "void main() {\n"
    "vec2 uv = 0.5 + u_uvScale.xy * (a_uv - 0.5);\n"
    "TAP(0,-4.0);\n"
    "TAP(1,-u_convergence);\n"
    "TAP(2, 0.0);\n"
    "TAP(3, u_convergence);\n"
    "TAP(4, 4.0);\n"
// TODO: tsone: have some light on screen or not?
#if 0
    "vec3 view_pos = vec3(0.0, 0.0, 2.5);\n"
    "vec3 light_pos = vec3(-1.0, 6.0, 3.0);\n"
    "vec3 n = normalize(a_norm);\n"
    "vec3 v = normalize(view_pos - a_vert.xyz);\n"
    "vec3 l = normalize(light_pos - a_vert.xyz);\n"
    "vec3 h = normalize(l + v);\n"
    "float ndotl = max(dot(n, l), 0.0);\n"
    "float ndoth = max(dot(n, h), 0.0);\n"
    "v_color = vec3(0.006*ndotl + 0.04*pow(ndoth, 21.0));\n"
#else
// TODO: tsone: This color must be linearized
    "v_color = vec3(0.0);\n"
#endif
    "gl_Position = u_mvp * a_vert;\n"
    "}\n";
static const char* screen_frag_src =
"precision highp float;\n"
"uniform sampler2D u_stretchTex;\n"
"uniform mat3 u_sharpenKernel;\n"
"varying vec2 v_uv[5];\n"
"varying vec3 v_color;\n"

//
// From 'Improved texture interpolation' by Inigo Quilez (2009):
// http://iquilezles.org/www/articles/texture/texture.htm
//
// Modifies linear interpolation by applying a smoothstep function to
// texture image coordinate fractional part. This causes derivatives of
// the interpolation slope to be zero, avoiding discontinuities.
// Quintic smoothstep makes 1st & 2nd order derivates zero, and regular
// (cubic) smoothstep makes only 1st order derivate zero.
//
// Not only it is useful for normal and bump map texture intepolation,
// it also gives appearance slightly similar to nearest interpolation,
// but smoother. This is useful for surfaces that should appear "pixelated",
// for example LCD and TV screens.
//
"vec3 texture2DSmooth( const sampler2D tex, const vec2 uv, const vec2 texResolution ) {\n"
    "vec2 p = uv * texResolution + 0.5;\n"
    "vec2 i = floor( p );\n"
    "vec2 f = p - i;\n"
    "f = ( ( 6.0 * f - 15.0 ) * f + 10.0 ) * f*f*f;\n" // Quintic smoothstep.
//    "f = ( 3.0 - 2.0 * f ) * f*f;\n" // Cubic smoothstep.
    "p = ( i + f - 0.5 ) / texResolution;\n"
    "return texture2D( tex, p ).rgb;\n"
"}\n"

// TODO: tsone: duplicate code (screen & tv)
//
// Optimized cubic texture interpolation. Uses 4 texture samples for the 4x4 texel area.
//
// http://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl
//
"vec4 cubic( const float v ) {\n"
    "vec4 n = vec4( 1.0, 2.0, 3.0, 4.0 ) - v;\n"
    "vec4 s = n * n * n;\n"
    "float x = s.x;\n"
    "float y = s.y - 4.0 * s.x;\n"
    "float z = s.z - 4.0 * s.y + 6.0 * s.x;\n"
    "float w = 6.0 - x - y - z;\n"
    "return vec4( x, y, z, w );\n"
"}\n"
"vec3 texture2DCubic( const sampler2D tex, in vec2 uv, const vec2 reso ) {\n"
    "uv = uv * reso - 0.5;\n"
    "float fx = fract( uv.x );\n"
    "float fy = fract( uv.y );\n"
    "uv.x -= fx;\n"
    "uv.y -= fy;\n"
    "vec4 xcub = cubic( fx );\n"
    "vec4 ycub = cubic( fy );\n"
    "vec4 c = uv.xxyy + vec4( - 0.5, 1.5, - 0.5, 1.5 );\n"
    "vec4 s = vec4( xcub.xz, ycub.xz ) + vec4( xcub.yw, ycub.yw );\n"
    "vec4 offs = c + vec4( xcub.yw, ycub.yw ) / s;\n"
    "vec3 s0 = texture2D( tex, offs.xz / reso ).rgb;\n"
    "vec3 s1 = texture2D( tex, offs.yz / reso ).rgb;\n"
    "vec3 s2 = texture2D( tex, offs.xw / reso ).rgb;\n"
    "vec3 s3 = texture2D( tex, offs.yw / reso ).rgb;\n"
    "float sx = s.x / (s.x + s.y);\n"
    "float sy = s.z / (s.z + s.w);\n"
    "return mix( mix( s3, s2, sx ), mix( s1, s0, sx ), sy );\n"
"}\n"

// Sample, linearize and sum.
"#define SMP(i_, m_) tmp = texture2D(u_stretchTex, v_uv[i_]).rgb; color += (m_) * tmp*tmp\n"
//"#define SMP(i_, m_) color += (m_) * texture2DSmooth(u_stretchTex, v_uv[i_], vec2(1120.0, 960.0))\n"
"void main(void) {\n"
    "vec3 tmp;\n"
    "vec3 color = vec3(0.0);\n"
    "SMP(0, u_sharpenKernel[0]);\n"
    "SMP(1, vec3(1.0, 0.0, 0.0));\n"
    "SMP(2, u_sharpenKernel[1]);\n"
    "SMP(3, vec3(0.0, 0.0, 1.0));\n"
    "SMP(4, u_sharpenKernel[2]);\n"
    "color = clamp(color, 0.0, 1.0);\n"
    "color += v_color;\n"
    // Gamma encode color w/ sqrt().
    "gl_FragColor = vec4(sqrt(color), 1.0);\n"
"}\n";

static const char* tv_vert_src =
    "precision highp float;\n"
    "attribute vec4 a_vert;\n"
    "attribute vec3 a_norm;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec3 v_color;\n"
    "varying vec3 v_p;\n"
    "varying vec3 v_n;\n"
    "varying vec3 v_v;\n"

// TODO: tsone: testing distances in uvs
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "varying vec2 v_blends;\n"

    "void main() {\n"
        "vec3 view_pos = vec3(0.0, 0.0, 2.5);\n"
        "vec3 light_pos = vec3(-1.0, 6.0, 3.0);\n"
        "vec4 p = u_mvp * a_vert;\n"
        "vec3 n = normalize(a_norm);\n"
        "vec3 v = normalize(view_pos - a_vert.xyz);\n"
        "vec3 l = normalize(light_pos - a_vert.xyz);\n"
        "vec3 h = normalize(l + v);\n"
        "float ndotl = max(dot(n, l), 0.0);\n"
        "float ndoth = max(dot(n, h), 0.0);\n"
        "v_color = vec3(0.000 + 0.004*ndotl + 0.00*pow(ndoth, 19.0));\n"
// TODO: tsone: disable light for now
        "v_color = vec3(0.0);\n"
        "v_n = n;\n"
        "v_v = v;\n"
        "v_p = a_vert.xyz;\n"
        "gl_Position = u_mvp * a_vert;\n"
    
        "float les = length(a_uv);\n"
        "vec2 clay = (les > 0.0) ? a_uv / les : vec2(0.0);\n"
        "vec2 nxy = normalize(v_n.xy);\n"
        "float mixer = max(1.0 - 30.0*les, 0.0);\n"
        "vec2 pool = normalize(mix(clay, nxy, mixer));\n"
        "vec4 tmp = u_mvp * vec4(a_vert.xy + 5.5*vec2(1.0, 7.0/8.0)*(vec2(les) + vec2(0.0014, 0.0019))*pool, a_vert.zw);\n"
        "v_uv = 0.5 + 0.5 * tmp.xy / tmp.w;\n"

        "float w = 51.0 * les;\n"
        "if (w < 1.0) {\n"
            "v_blends.x = w;\n"
            "v_blends.y = 0.0;\n"
        "} else if (w < 4.0) {\n"
            "v_blends.x = 1.0;\n"
            "v_blends.y = 0.33333*w - 0.33333;\n"
        "} else {\n"
            "v_blends.x = 1.0;\n"
            "v_blends.y = 1.0;\n"
        "}\n"
    "}\n";
static const char* tv_frag_src =
    "precision highp float;\n"
    DEFINE(M_PI)
    "uniform sampler2D u_downsample1Tex;\n"
    "uniform sampler2D u_downsample3Tex;\n"
    "uniform sampler2D u_downsample5Tex;\n"
    "varying vec3 v_color;\n"
    "varying vec3 v_p;\n"
    "varying vec3 v_n;\n"
    "varying vec3 v_v;\n"

    "varying vec2 v_uv;\n"
    "varying vec2 v_blends;\n"

// TODO: tsone: duplicate code (screen & tv)
//
// Optimized cubic texture interpolation. Uses 4 texture samples for the 4x4 texel area.
//
// http://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl
//
"vec4 cubic( const float v ) {\n"
    "vec4 n = vec4( 1.0, 2.0, 3.0, 4.0 ) - v;\n"
    "vec4 s = n * n * n;\n"
    "float x = s.x;\n"
    "float y = s.y - 4.0 * s.x;\n"
    "float z = s.z - 4.0 * s.y + 6.0 * s.x;\n"
    "float w = 6.0 - x - y - z;\n"
    "return vec4( x, y, z, w );\n"
"}\n"
"vec3 texture2DCubic( const sampler2D tex, in vec2 uv, const vec2 reso ) {\n"
    "uv = uv * reso - 0.5;\n"
    "float fx = fract( uv.x );\n"
    "float fy = fract( uv.y );\n"
    "uv.x -= fx;\n"
    "uv.y -= fy;\n"
    "vec4 xcub = cubic( fx );\n"
    "vec4 ycub = cubic( fy );\n"
    "vec4 c = uv.xxyy + vec4( - 0.5, 1.5, - 0.5, 1.5 );\n"
    "vec4 s = vec4( xcub.xz, ycub.xz ) + vec4( xcub.yw, ycub.yw );\n"
    "vec4 offs = c + vec4( xcub.yw, ycub.yw ) / s;\n"
    "vec3 s0 = texture2D( tex, offs.xz / reso ).rgb;\n"
    "vec3 s1 = texture2D( tex, offs.yz / reso ).rgb;\n"
    "vec3 s2 = texture2D( tex, offs.xw / reso ).rgb;\n"
    "vec3 s3 = texture2D( tex, offs.yw / reso ).rgb;\n"
    "float sx = s.x / (s.x + s.y);\n"
    "float sy = s.z / (s.z + s.w);\n"
    "return mix( mix( s3, s2, sx ), mix( s1, s0, sx ), sy );\n"
"}\n"

    "const vec2 c_uvMult = vec2(0.892, 0.827);\n"
    "const vec3 c_center = vec3(0.0, 0.000135, -0.04427);\n"
    "const vec3 c_size = vec3(0.95584, 0.703985, 0.04986);\n"
//    "const vec3 c_size = vec3(0.95584-0.011, 0.703985-0.011, 0.04986);\n"

    "void main(void) {\n"
        "vec3 color;\n"

#if 1
    "vec3 d = vec3(-v_uv, 0.0);\n"
    "vec3 c = vec3(v_p.xy + v_uv, v_p.z);\n"
#else
    // calculate closest point 'c' on screen plane
    "vec3 d = v_p - c_center;\n"
    "vec3 q = min(abs(d) - c_size, 0.0) + c_size;\n"
    "vec3 c = c_center + sign(d) * q;\n"
    // point-light shading respective to 'c'
    "d = v_p - c;\n"
#endif
//    "float ndoth = max(dot(n, normalize(normalize(v_v) + l)), 0.0);\n"
//    "vec3 l = normalize(vec3(0.0, 0.0, 0.02) - v_p);\n" // light at "center"
//    "float h = 1.0 + 96.0 * dot(d, d);\n"
//    "float spec = (0.1 * (2.0+6.0)/(2.0*M_PI)) * pow(ndoth, 6.0);\n"
//    "float diff = (0.9/M_PI) * (ndotl/(1.0 + h*h));\n"
    "float diff = 0.2;\n"
    // sample and mix
    "color = v_color;\n"
    "vec2 nuv = 2.0*v_uv - 1.0;\n"
    "float vignette = max(1.0 - length(nuv), 0.0);\n"

    // Sample from downsampled (blurry) textures and linearize.
    "vec3 ds0 = texture2D(u_downsample1Tex, v_uv).rgb;\n"
    "vec3 ds1 = texture2D(u_downsample3Tex, v_uv).rgb;\n"
    "vec3 ds2 = texture2D(u_downsample5Tex, v_uv).rgb;\n"
    "ds0 *= ds0;\n"
    "ds1 *= ds1;\n"
    "ds2 *= ds2;\n"
    // Blend together to mimic glossy reflection and ambient diffuse.
    "color = mix(mix(ds0, ds1, v_blends.x), ds2, v_blends.y);\n"
    "color *= diff;\n"
    // Gamma encode color w/ sqrt().
    "gl_FragColor = vec4(sqrt(color), 1.0);\n"
    "}\n";

// Downsample shader.
static const char* downsample_vert_src =
    "precision highp float;\n"
    "uniform vec2 u_offsets[8];\n"
    "attribute vec4 a_vert;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv[8];\n"

    "void main() {\n"
    "gl_Position = a_vert;\n"
    "for (int i = 0; i < 8; i++) {\n"
        "v_uv[i] = a_uv + u_offsets[i];\n"
    "}\n"
    "}\n";
static const char* downsample_frag_src =
    "precision highp float;\n"
    "uniform sampler2D u_downsampleTex;\n"
    "uniform float u_weights[8];\n"
    "varying vec2 v_uv[8];\n"

    "void main(void) {\n"
    "vec3 result = vec3(0.0);\n"
    "for (int i = 0; i < 8; i++) {\n"
        "vec3 color = texture2D(u_downsampleTex, v_uv[i]).rgb;\n"
        "result += u_weights[i] * color*color;\n"
    "}\n"
    "gl_FragColor = vec4(sqrt(result), 1.0);\n"
    "}\n";

// Combine shader.
static const char* combine_vert_src =
    "precision highp float;\n"
    "attribute vec4 a_vert;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
        "gl_Position = a_vert;\n"
        "v_uv = a_uv;\n"
    "}\n";
static const char* combine_frag_src =
    "precision highp float;\n"
    "uniform sampler2D u_tvTex;\n"
    "uniform sampler2D u_downsample3Tex;\n"
    "uniform sampler2D u_downsample5Tex;\n"
    "uniform float u_glow;\n"
    "varying vec2 v_uv;\n"

    "void main(void) {\n"
        // Sample screen/tv and downsampled (blurry) textures for glow.
        "vec3 color = texture2D(u_tvTex, v_uv).rgb;\n"
        "vec3 ds3 = texture2D(u_downsample3Tex, v_uv).rgb;\n"
        "vec3 ds5 = texture2D(u_downsample5Tex, v_uv).rgb;\n"
        // Linearize color values.
        "color *= color;\n"
        "ds3 *= ds3;\n"
        "ds5 *= ds5;\n"
        // Blend in glow as blurry highlight allowing slight bleeding on higher u_glow values.
        "float g2 = u_glow * u_glow;\n"
        "color = (color + u_glow*ds3 + g2*ds5) / (1.0 + g2);\n"
        // Gamma encode w/ sqrt() to something similar to sRGB space.
        "gl_FragColor = vec4(sqrt(color), 1.0);\n"
    "}\n";
