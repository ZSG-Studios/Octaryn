"""CPU quadrature and source-contract checks; not a GPU or image-quality test."""
from pathlib import Path
import math
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / 'octaryn-client/Shaders'
PI = math.pi


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def normalize(v):
    length = math.sqrt(dot(v, v))
    return tuple(x/length for x in v)


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def fresnel(f0, cosine):
    return f0+(1-f0)*(1-max(0, min(1, cosine)))**5


def g1(cosine, a2):
    return 2*cosine/max(cosine+math.sqrt(a2+(1-a2)*cosine*cosine), 1e-6)


def environment(f0, roughness, nv):
    r = [max(0, min(1, roughness))*a+b
         for a, b in zip((-1, -.0275, -.572, .022), (1, .0425, 1.04, -.04))]
    a = min(r[0]*r[0], 2**(-9.28*max(0, min(1, nv))))*r[0]+r[1]
    return max(0, min(1, f0*(-1.04*a+r[2])+1.04*a+r[3]))


def visible_normal(view, alpha, u, v):
    stretched = normalize((alpha*view[0], alpha*view[1], view[2]))
    length2 = stretched[0]**2+stretched[1]**2
    tangent = ((-stretched[1]/math.sqrt(length2), stretched[0]/math.sqrt(length2), 0)
               if length2 > 0 else (1, 0, 0))
    bitangent = cross(stretched, tangent)
    radius, phi = math.sqrt(u), 2*PI*v
    x, y = radius*math.cos(phi), radius*math.sin(phi)
    blend = .5*(1+stretched[2])
    y = (1-blend)*math.sqrt(max(0, 1-x*x))+blend*y
    z = math.sqrt(max(0, 1-x*x-y*y))
    disk = tuple(x*t+y*b+z*s for t, b, s in zip(tangent, bitangent, stretched))
    return normalize((alpha*disk[0], alpha*disk[1], max(0, disk[2])))


def sampled_integral(f0, roughness, nv, count=65536):
    view = (math.sqrt(1-nv*nv), 0, nv)
    alpha = max(roughness*roughness, .0025)
    result = 0
    for i in range(count):
        h = visible_normal(view, alpha, (i+.5)/count, (i*.61803398875) % 1)
        vh = dot(view, h)
        nl = 2*vh*h[2]-nv
        if nl > 0:
            result += fresnel(f0, vh)*g1(nl, alpha*alpha)
    return result/count


def hemisphere_integral(f0, roughness, nv, nz=256, nphi=512):
    """Independent uniform-solid-angle integration of D*G*F/(4*N.V)."""
    view = (math.sqrt(1-nv*nv), 0, nv)
    a2 = max(roughness*roughness, .0025)**2
    result = 0
    for z in range(nz):
        nl = (z+.5)/nz
        radius = math.sqrt(1-nl*nl)
        for p in range(nphi):
            phi = 2*PI*(p+.5)/nphi
            h = normalize((view[0]+radius*math.cos(phi), radius*math.sin(phi), nv+nl))
            denominator = h[2]*h[2]*(a2-1)+1
            distribution = a2/(PI*denominator*denominator)
            result += distribution*g1(nl, a2)*g1(nv, a2)*fresnel(f0, dot(view, h))/(4*nv)
    return result*2*PI/(nz*nphi)


class EnvironmentTests(unittest.TestCase):
    def test_production_formula_linkage(self):
        env = ''.join((SHADERS/'Materials/PbrEnvironment.slang').read_text().split())
        reflections = ''.join((SHADERS/'Hdr/MapReflections.slang').read_text().split())
        direct = ''.join((SHADERS/'Materials/MetallicRoughness.slang').read_text().split())
        for expression in (
            'saturate(roughness)*float4(-1,-.0275,-.572,.022)+float4(1,.0425,1.04,-.04)',
            'min(r.x*r.x,exp2(-9.28*saturate(nv)))*r.x+r.y',
            'float2(-1.04,1.04)*a+r.zw', 'saturate(f0*ab.x+ab.y)',
            'normalize(float3(alpha*view.xy,view.z))',
            'float3(-stretched.y,stretched.x,0)/sqrt(lengthSquared)',
            'cross(stretched,tangent)', 'sqrt(sample.x)', '6.28318530718*sample.y',
            '.5*(1+stretched.z)', '(1-blend)*sqrt(max(0,1-x*x))+blend*y',
            'x*tangent+y*bitangent+sqrt(max(0,1-x*x-y*y))*stretched',
            'normalize(float3(alpha*disk.xy,max(0,disk.z)))'):
            self.assertIn(expression, env)
        for expression in ('pbr_visible_normal(localView,alpha,random)',
                           'count==1?1:pbr_smith_g1(nl,a2)',
                           'radiance*pbr_fresnel(f0,vh)*weight', 'returnsum/count;',
                           'diffuse_environment(direction,sun.xyz,sky)',
                           'roughness<.08?1u:uint(reflectionSampling.x)', 'dot(normal,view)<=0'):
            self.assertIn(expression, reflections)
        self.assertIn('f0+(1-f0)*pow(1-saturate(cosine),5)', direct)
        self.assertIn('pbr_fresnel(lerp(float3(.04),albedo,metallic),vh)', direct)
        self.assertNotIn('pbr_environment_reflectance', direct)

    def test_visible_normals_and_weights(self):
        for nv in (1e-5, .01, .2, .7, 1):
            for roughness in (.001, .08, .3, .7, 1):
                alpha = max(roughness*roughness, .0025)
                view = (math.sqrt(1-nv*nv), 0, nv)
                for i in range(1024):
                    h = visible_normal(view, alpha, (i+.5)/1024, (i*.61803398875) % 1)
                    self.assertAlmostEqual(dot(h, h), 1, places=12)
                    self.assertGreaterEqual(h[2], 0)
                    vh = dot(view, h)
                    self.assertGreater(vh, 0)
                    nl = 2*vh*h[2]-nv
                    if nl > 0:
                        for f0 in (.04, .5, 1):
                            weight = fresnel(f0, vh)*g1(nl, alpha*alpha)
                            self.assertGreaterEqual(weight, 0)
                            self.assertLessEqual(weight, 1+1e-12)

    def test_independent_numerical_integration(self):
        for roughness, nv, f0 in ((.35, .1, .04), (.65, .5, .04), (1, 1, .04),
                                  (.35, .5, .8), (.65, .1, .8), (1, .1, .8),
                                  (.65, .5, .42)):
            sampled = sampled_integral(f0, roughness, nv)
            integrated = hemisphere_integral(f0, roughness, nv)
            analytic = environment(f0, roughness, nv)
            print(f'rough={roughness} nv={nv} f0={f0} VNDF={sampled:.6f} '
                  f'quadrature={integrated:.6f} analytic={analytic:.6f}')
            self.assertAlmostEqual(sampled, integrated, delta=.002)
            # The mobile fit uses an approximate visibility model. Its dielectric
            # diffuse-energy split is useful; it is not exact metallic GGX energy.
            self.assertGreaterEqual(analytic, 0)
            self.assertLessEqual(analytic, 1)
            if f0 == .04:
                self.assertAlmostEqual(analytic, integrated, delta=.04)

    def test_rough_grazing_diffuse_is_not_black(self):
        for roughness in (.5, .8, 1):
            reflectance = environment(.04, roughness, 0)
            self.assertLess(reflectance, .5)
            self.assertGreater(1-reflectance, .5)
        self.assertEqual(fresnel(.04, 0), 1)
        self.assertAlmostEqual(fresnel(.04, 1), .04)
        self.assertAlmostEqual(fresnel(.04, .5), .07)


if __name__ == '__main__':
    unittest.main(verbosity=2)
