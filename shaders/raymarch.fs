#version 330 core
out vec4 FragColor;

uniform vec2 u_Resolution;
uniform float u_Time;
uniform vec3 u_CamPos;
uniform vec3 u_CamTarget;
uniform vec3 u_LightPos;
uniform int u_SceneIndex;
uniform int u_ShadowsEnabled;
uniform int u_AOEnabled;

#define MAX_STEPS 128
#define MAX_DIST 60.0
#define SURF_DIST 0.001

struct Hit {
    float d;
    vec3 color;
    float spec;
    float roughness;
    int matType; // 0: plain, 1: checkerboard
};

// Rotation matrices
mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

// SDF Primitives
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdPlane(vec3 p, float h) {
    return p.y - h;
}

// Operators
Hit opUnion(Hit a, Hit b) {
    if (a.d < b.d) return a;
    return b;
}

Hit opSubtract(Hit a, Hit b) { // b minus a
    Hit res = b;
    res.d = max(-a.d, b.d);
    return res;
}

Hit opIntersect(Hit a, Hit b) {
    Hit res = (a.d > b.d) ? a : b;
    res.d = max(a.d, b.d);
    return res;
}

Hit opSmoothUnion(Hit a, Hit b, float k) {
    float h = clamp(0.5 + 0.5 * (b.d - a.d) / k, 0.0, 1.0);
    float d = mix(b.d, a.d, h) - k * h * (1.0 - h);
    vec3 col = mix(b.color, a.color, h);
    return Hit(d, col, mix(b.spec, a.spec, h), mix(b.roughness, a.roughness, h), 0);
}

Hit mapScene(vec3 p) {
    // Floor plane with procedural checkerboard
    Hit res = Hit(sdPlane(p, -1.0), vec3(0.9), 0.2, 32.0, 1);

    if (u_SceneIndex == 0) {
        // Scene 0: Organic Morphing Metaballs & Floating CSG Objects
        // Central morphing blobs
        vec3 pBlob1 = p - vec3(0.0, 0.8 + 0.3 * sin(u_Time * 2.0), 0.0);
        vec3 pBlob2 = p - vec3(0.8 * cos(u_Time * 1.5), 0.8 + 0.4 * sin(u_Time * 2.5), 0.8 * sin(u_Time * 1.5));
        vec3 pBlob3 = p - vec3(-0.7 * sin(u_Time * 1.8), 0.8 + 0.3 * cos(u_Time * 2.2), 0.7 * cos(u_Time * 1.8));

        Hit b1 = Hit(sdSphere(pBlob1, 0.65), vec3(0.9, 0.25, 0.2), 0.8, 64.0, 0);
        Hit b2 = Hit(sdSphere(pBlob2, 0.45), vec3(0.2, 0.7, 0.9), 0.8, 64.0, 0);
        Hit b3 = Hit(sdSphere(pBlob3, 0.40), vec3(0.9, 0.8, 0.1), 0.8, 64.0, 0);

        Hit blobs = opSmoothUnion(b1, b2, 0.35);
        blobs = opSmoothUnion(blobs, b3, 0.35);
        res = opUnion(res, blobs);

        // Floating rotating carved box (CSG subtraction)
        vec3 pBox = p - vec3(-3.0, 1.0, 1.0);
        pBox.xy = rot(u_Time * 0.8) * pBox.xy;
        pBox.xz = rot(u_Time * 0.6) * pBox.xz;
        Hit outerBox = Hit(sdRoundBox(pBox, vec3(0.7), 0.1), vec3(0.3, 0.85, 0.4), 0.5, 32.0, 0);
        Hit innerSphere = Hit(sdSphere(pBox, 0.85), vec3(0.9, 0.4, 0.1), 0.8, 64.0, 0);
        Hit carvedBox = opSubtract(innerSphere, outerBox);
        res = opUnion(res, carvedBox);

        // Orbiting golden Torus
        vec3 pTorus = p - vec3(3.0, 1.0 + 0.2 * sin(u_Time * 3.0), 1.0);
        pTorus.yz = rot(u_Time * 1.2) * pTorus.yz;
        pTorus.xz = rot(u_Time * 0.9) * pTorus.xz;
        Hit torus = Hit(sdTorus(pTorus, vec2(0.8, 0.22)), vec3(0.95, 0.75, 0.2), 1.0, 128.0, 0);
        res = opUnion(res, torus);

    } else if (u_SceneIndex == 1) {
        // Scene 1: Precision CSG Geometry & Pillars
        // Central drilled block
        vec3 pBlock = p - vec3(0.0, 0.8, 0.0);
        pBlock.xz = rot(u_Time * 0.5) * pBlock.xz;
        Hit mainCube = Hit(sdRoundBox(pBlock, vec3(1.0), 0.15), vec3(0.3, 0.45, 0.85), 0.6, 48.0, 0);

        // Cross drills through the cube using cylinders
        Hit drillX = Hit(sdCylinder(pBlock.yzx, 1.5, 0.45), vec3(1.0), 0.0, 1.0, 0);
        Hit drillY = Hit(sdCylinder(pBlock.xyz, 1.5, 0.45), vec3(1.0), 0.0, 1.0, 0);
        Hit drillZ = Hit(sdCylinder(pBlock.xzy, 1.5, 0.45), vec3(1.0), 0.0, 1.0, 0);

        Hit csgObj = opSubtract(drillX, mainCube);
        csgObj = opSubtract(drillY, csgObj);
        csgObj = opSubtract(drillZ, csgObj);
        res = opUnion(res, csgObj);

        // Surrounding pillars
        for (int i = 0; i < 4; ++i) {
            float angle = float(i) * 1.57079 + u_Time * 0.2;
            vec3 pos = vec3(cos(angle) * 3.5, 0.8, sin(angle) * 3.5);
            Hit pillar = Hit(sdCylinder(p - pos, 1.8, 0.35), vec3(0.8, 0.8, 0.85), 0.4, 32.0, 0);
            res = opUnion(res, pillar);
        }

    } else {
        // Scene 2: Infinite Repetition Hall of Spheres and Columns
        vec3 pRep = p;
        pRep.xz = mod(pRep.xz + 3.0, 6.0) - 3.0;

        Hit repCol = Hit(sdCylinder(pRep - vec3(0.0, 1.5, 0.0), 2.5, 0.4), vec3(0.65, 0.7, 0.8), 0.4, 32.0, 0);
        Hit repSph = Hit(sdSphere(pRep - vec3(0.0, 1.5 + sin(u_Time * 3.0 + p.x + p.z) * 0.5, 0.0), 0.6),
                         vec3(0.9, 0.3, 0.2), 0.9, 96.0, 0);
        Hit repObj = opSmoothUnion(repCol, repSph, 0.2);
        res = opUnion(res, repObj);
    }

    return res;
}

// Numerical normal computation using tetrahedron method
vec3 calcNormal(vec3 p) {
    const vec2 e = vec2(0.0008, -0.0008);
    return normalize(e.xyy * mapScene(p + e.xyy).d +
                     e.yyx * mapScene(p + e.yyx).d +
                     e.yxy * mapScene(p + e.yxy).d +
                     e.xxx * mapScene(p + e.xxx).d);
}

// Soft shadow calculation
float calcShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    if (u_ShadowsEnabled == 0) return 1.0;
    float res = 1.0;
    float t = mint;
    for (int i = 0; i < 48 && t < maxt; ++i) {
        float h = mapScene(ro + rd * t).d;
        if (h < 0.0005)
            return 0.0;
        res = min(res, k * h / t);
        t += clamp(h, 0.02, 0.3);
    }
    return clamp(res, 0.0, 1.0);
}

// Ambient Occlusion
float calcAO(vec3 p, vec3 n) {
    if (u_AOEnabled == 0) return 1.0;
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; ++i) {
        float h = 0.02 + 0.12 * float(i);
        float d = mapScene(p + h * n).d;
        occ += (h - d) * sca;
        sca *= 0.85;
    }
    return clamp(1.0 - 2.0 * occ, 0.0, 1.0);
}

// Camera Ray Generation
vec3 getRayDirection(vec2 uv, vec3 ro, vec3 ta, float fov) {
    vec3 forward = normalize(ta - ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);
    return normalize(uv.x * right + uv.y * up + forward * fov);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_Resolution.xy) / u_Resolution.y;

    vec3 ro = u_CamPos;
    vec3 rd = getRayDirection(uv, ro, u_CamTarget, 1.5);

    // Sky / Background gradient
    vec3 skyColor = mix(vec3(0.03, 0.05, 0.1), vec3(0.12, 0.16, 0.25), clamp(rd.y + 0.3, 0.0, 1.0));
    vec3 col = skyColor;

    // Ray marching loop
    float t = 0.0;
    Hit hit;
    bool found = false;

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * t;
        hit = mapScene(p);
        if (hit.d < SURF_DIST) {
            found = true;
            break;
        }
        if (t > MAX_DIST) break;
        t += hit.d;
    }

    if (found) {
        vec3 p = ro + rd * t;
        vec3 n = calcNormal(p);

        // Base color
        vec3 albedo = hit.color;
        if (hit.matType == 1) { // Procedural checkerboard floor
            vec2 c = floor(p.xz);
            float check = mod(c.x + c.y, 2.0);
            albedo = mix(vec3(0.12, 0.12, 0.14), vec3(0.85, 0.85, 0.88), check);
        }

        // Lighting vectors
        vec3 lightDir = normalize(u_LightPos - p);
        vec3 viewDir = -rd;
        vec3 halfDir = normalize(lightDir + viewDir);

        // Ambient Occlusion & Soft Shadow
        float ao = calcAO(p, n);
        float shadow = calcShadow(p + n * 0.01, lightDir, 0.02, 25.0, 16.0);

        // Blinn-Phong Shading
        float diff = max(dot(n, lightDir), 0.0);
        float spec = pow(max(dot(n, halfDir), 0.0), hit.roughness) * hit.spec;

        // Ambient and fill sky light
        vec3 ambient = vec3(0.12, 0.14, 0.2) * ao;
        vec3 diffuseLight = vec3(1.0, 0.95, 0.85) * diff * shadow;
        vec3 specularLight = vec3(1.0, 0.95, 0.9) * spec * shadow;

        // Combine lighting
        col = albedo * (ambient + diffuseLight) + specularLight;

        // Subtle distance fog
        float fog = 1.0 - exp(-0.0008 * t * t * t);
        col = mix(col, skyColor, clamp(fog, 0.0, 1.0));
    }

    // Tone mapping and gamma correction
    col = col / (col + vec3(1.0)); // Reinhard
    col = pow(col, vec3(1.0 / 2.2)); // Gamma

    FragColor = vec4(col, 1.0);
}

