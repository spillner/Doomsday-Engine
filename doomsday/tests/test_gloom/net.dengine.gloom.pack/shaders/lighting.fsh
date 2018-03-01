#version 330 core

#include "common/gbuffer_in.glsl"
#include "common/lightmodel.glsl"

uniform mat3 uViewToWorldRotate;
uniform samplerCube/*Shadow*/ uShadowMaps[6];

//uniform float uShadowFarPlanes[6]; // isn't vRadius enough?

flat DENG_VAR vec3  vOrigin;     // view space
flat DENG_VAR vec3  vDirection;  // view space
flat DENG_VAR vec3  vIntensity;
flat DENG_VAR float vRadius;
flat DENG_VAR int   vShadowIndex;

void main(void) {
    vec3 pos      = GBuffer_FragViewSpacePos().xyz;
    vec3 normal   = GBuffer_FragViewSpaceNormal();
    vec3 lightVec = normalize(vOrigin - pos);

    out_FragColor = vec4(0.0); // By default no color is received.

    /*if (dot(normal, lightVec) < 0.0) {
        return; // Surface faces away from light.
    }*/

    float lit = 1.0;

    // Check the shadow map.
    if (vShadowIndex >= 0) {
        vec3 vsRay = vOrigin - pos;
        float len = length(vsRay);
        /*lit = texture(uShadowMaps[vShadowIndex], vec4(uViewToWorldRotate * vsRay, len/25.0));*/
        vec3 wRay = uViewToWorldRotate * vsRay;
        wRay.z = -wRay.z;
        out_FragColor = vec4(2.0 * vec3(texture(uShadowMaps[vShadowIndex],
            wRay).r/vRadius), 1.0);
        return;
    }
    if (lit <= 0.001) {
        return;
    }

    vec4 albedo = texelFetch(uGBufferAlbedo, ivec2(gl_FragCoord.xy), 0);
    vec4 specGloss = vec4(0.2);

    Light light = Light(vOrigin, vDirection, vIntensity, vRadius, 1.0);
    SurfacePoint surf = SurfacePoint(pos, normal, albedo.rgb, specGloss);

    out_FragColor = vec4(vec3(lit) /* * Gloom_BlinnPhong(light, surf) */, 0.0);
}
