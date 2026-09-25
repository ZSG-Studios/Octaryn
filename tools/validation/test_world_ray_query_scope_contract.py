"""Source contract for short-lived ray-query traversal state."""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "octaryn-client/Shaders/RayTracing/WorldRayQuery.slang").read_text()

scope_start = SOURCE.index("    {\n      RayQuery<RAY_FLAG_NONE> query;")
scope_end = SOURCE.index("    }\n    // Opaque map triangles", scope_start) + len("    }")
map_publish = SOURCE.index("if(committedTriangle && committedInstance==kMapRayInstanceId)", scope_end)
any_hit = SOURCE.index("if(anyHit)return result;", map_publish)
after_scope = SOURCE.index("world_map_ray_surface(result", any_hit)

assert scope_start < scope_end < map_publish < any_hit < after_scope
assert "committedDistance=query.CommittedRayT()" in SOURCE[scope_start:scope_end]
assert "committedBarycentrics=query.CommittedTriangleBarycentrics()" in SOURCE[
    scope_start:scope_end
]
assert "result.hit=true;result.distance=committedDistance;" in SOURCE[map_publish:any_hit]
assert "if(anyHit)return result;" in SOURCE[any_hit:after_scope]
print("world ray query scope contract: passed")
