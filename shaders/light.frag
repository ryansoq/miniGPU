#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 fragColor;
void main() {
    // 用法向量的方向做簡單打光：面朝右上前方越亮
    float b = vNormal.x * 0.30 + vNormal.y * 0.60 + vNormal.z * 0.30 + 0.45;
    fragColor = vec4(b * 0.85, b * 0.82, b * 0.78, 1.0);
}
