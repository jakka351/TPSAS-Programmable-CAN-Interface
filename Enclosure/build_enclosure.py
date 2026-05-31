#!/usr/bin/env python3
# ============================================================================
#  TP-CAN-2I  --  Rugged Sealed Enclosure : authoritative STL builder
#  (c) 2026 Jack Leighton - Tester Present Specialist Automotive Solutions
#  Designed in Australia.
#
#  Pure-Python parametric CAD (pycsg) -> binary STL.  No OpenSCAD / OCC / C++
#  toolchain required, so the case can be regenerated anywhere Python runs:
#
#      pip install pycsg numpy
#      python build_enclosure.py
#
#  Outputs (millimetres, ready for SLA):
#      STL/TP-CAN-2I_base.stl   - tray (PCB mounts here)
#      STL/TP-CAN-2I_lid.stl    - lid  (printed upside-down: mating face = Z0)
#
#  This script is the MANUFACTURING SOURCE OF TRUTH for the printed parts.
#  TP-CAN-2I_enclosure.scad is kept as an editable OpenSCAD reference.
# ============================================================================
import os, struct, math, time
from csg.core import CSG

# ---- PCB -------------------------------------------------------------------
pcb_x, pcb_y, pcb_t = 70.0, 50.0, 1.6
pcb_clear      = 2.0
pcb_hole_inset = 4.0

# ---- Shell -----------------------------------------------------------------
wall       = 3.0
floor_t    = 3.0
lid_top_t  = 3.0
corner_r   = 4.0
standoff_h = 4.0
head_room  = 16.0
gasket_w   = 1.2          # gasket channel width (in rim top face)
gasket_d   = 1.4          # gasket channel depth
gasket_off = 0.9          # channel outer edge, measured in from outer wall
lip_h      = 4.0          # lid registration-rib (tongue) depth into cavity
fit_gap    = 0.20

# ---- Derived ---------------------------------------------------------------
inner_x = pcb_x + 2*pcb_clear          # 74
inner_y = pcb_y + 2*pcb_clear          # 54
outer_x = inner_x + 2*wall             # 80
outer_y = inner_y + 2*wall             # 60
cavity_h = standoff_h + pcb_t + head_room   # 21.6
base_h   = floor_t + cavity_h               # 24.6 (rim height)

pcb_ox = wall + pcb_clear              # 5
pcb_oy = wall + pcb_clear              # 5
pcb_holes = [
    (pcb_ox + pcb_hole_inset,         pcb_oy + pcb_hole_inset),
    (pcb_ox + pcb_x - pcb_hole_inset, pcb_oy + pcb_hole_inset),
    (pcb_ox + pcb_hole_inset,         pcb_oy + pcb_y - pcb_hole_inset),
    (pcb_ox + pcb_x - pcb_hole_inset, pcb_oy + pcb_y - pcb_hole_inset),
]

# ---- Fasteners / inserts ---------------------------------------------------
m3_insert_d = 4.0
m3_clear_d  = 3.4
m3_head_d   = 6.2
m5_clear_d  = 5.5
eyelet_od   = 10.0
standoff_d  = 6.5

# ---- Corner lid-screw towers ----------------------------------------------
tower_d = 9.0
tower_pos = [(corner_r+1.5,         corner_r+1.5),
             (outer_x-corner_r-1.5, corner_r+1.5),
             (corner_r+1.5,         outer_y-corner_r-1.5),
             (outer_x-corner_r-1.5, outer_y-corner_r-1.5)]
tower_top = base_h                     # flush with rim; lid bottoms here

# ---- Connector (Deutsch DT-12) cutout on +X end wall -----------------------
conn_w = 25.0                          # aperture width  (Y)
conn_h = 15.0                          # aperture height (Z)  (fits under rim)
conn_z = floor_t + 3.0                 # aperture bottom above floor (=6)
conn_flange_screw = 3.3
conn_flange_dx    = 31.0

# ---- USB-C service port on -Y side wall ------------------------------------
usb_w, usb_h = 13.0, 8.0
usb_x = pcb_ox + 6.0                   # 11
usb_z = floor_t + standoff_h + pcb_t   # 8.6 (PCB top plane)

# ---- LED light-pipes (5) in the lid ----------------------------------------
led_d  = 3.2
led_y  = outer_y - 12.0                # 48
led_xs = [26, 31, 36, 41, 46]          # PWR, Wi-Fi, CAN1, CAN2, STATUS

# ---- Mesh resolution -------------------------------------------------------
S_CORNER = 24                          # slices for rounded shell corners
S_HOLE   = 16                          # slices for screw/insert holes
EPS      = 1.0                         # cut-tool overshoot (avoids coplanar BSP)

# ============================================================================
#  pycsg helpers  (translate/rotate mutate in place -> wrap to return object)
# ============================================================================
def box(x0, y0, z0, dx, dy, dz):
    return CSG.cube(center=[x0+dx/2.0, y0+dy/2.0, z0+dz/2.0],
                    radius=[dx/2.0, dy/2.0, dz/2.0])

def cyl(x, y, z0, z1, r, slices=S_HOLE):
    return CSG.cylinder(start=[x, y, z0], end=[x, y, z1], radius=r, slices=slices)

def moved(obj, d):
    obj.translate(d); return obj

def turned(obj, axis, ang):
    obj.rotate(axis, ang); return obj

def union_all(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out.union(p)
    return out

def rbox(x0, y0, z0, X, Y, Z, r, slices=S_CORNER):
    """Prism [x0,x0+X] x [y0,y0+Y] x [z0,z0+Z] with vertical rounded edges r."""
    r = max(0.01, min(r, X/2.0-0.01, Y/2.0-0.01))
    parts = [box(x0+r, y0, z0, X-2*r, Y, Z),
             box(x0, y0+r, z0, X, Y-2*r, Z)]
    for cx in (x0+r, x0+X-r):
        for cy in (y0+r, y0+Y-r):
            parts.append(cyl(cx, cy, z0, z0+Z, r, slices))
    return union_all(parts)

# ============================================================================
#  Mounting ear (heavy-duty: M5 thru + steel-eyelet anti-crush pocket)
# ============================================================================
def corner_ear(corner, ang):
    ear_reach = 13.0          # corner pivot -> M5 hole centre
    ear_t     = 8.0
    ear_drop  = 0.8           # ears protrude below the floor as contact feet
    boss_r    = (eyelet_od + 4.0) / 2.0      # 7
    # local: connecting slab from inside the shell out to the boss, + boss disc
    slab = box(-2.0, -boss_r, -ear_drop, ear_reach+2.0, 2*boss_r, ear_t+ear_drop)
    boss = cyl(ear_reach, 0.0, -ear_drop, ear_t, boss_r, S_CORNER)
    ear  = slab.union(boss)
    # M5 clearance hole + flush eyelet pocket
    ear  = ear.subtract(cyl(ear_reach, 0.0, -ear_drop-EPS, ear_t+EPS, m5_clear_d/2.0, S_HOLE))
    ear  = ear.subtract(cyl(ear_reach, 0.0, ear_t-3.0, ear_t+EPS, eyelet_od/2.0, S_HOLE))
    ear  = turned(ear, [0, 0, 1], ang)
    ear  = moved(ear, [corner[0], corner[1], 0.0])
    return ear

# ============================================================================
#  BASE (tray)
# ============================================================================
def build_base():
    shell = rbox(0, 0, 0, outer_x, outer_y, base_h, corner_r)

    # hollow cavity (open top: overshoot above rim)
    shell = shell.subtract(
        rbox(wall, wall, floor_t, inner_x, inner_y, cavity_h+EPS, max(0.5, corner_r-wall)))

    # gasket channel in the rim top face (proper ring: outer - inner)
    gz = base_h - gasket_d
    go, gi = gasket_off, gasket_off + gasket_w
    ring = rbox(go, go, gz, outer_x-2*go, outer_y-2*go, gasket_d+EPS, corner_r-go) \
        .subtract(rbox(gi, gi, gz-EPS, outer_x-2*gi, outer_y-2*gi, gasket_d+3*EPS,
                       max(0.4, corner_r-gi)))
    shell = shell.subtract(ring)

    # Deutsch DT connector aperture on +X wall + 2 flange screw holes
    shell = shell.subtract(box(outer_x-wall-EPS, outer_y/2-conn_w/2, conn_z,
                               wall+2*EPS, conn_w, conn_h))
    for s in (-1, 1):
        yc = outer_y/2 + s*conn_flange_dx/2
        shell = shell.subtract(
            CSG.cylinder(start=[outer_x-wall-EPS, yc, conn_z+conn_h/2],
                         end=[outer_x+EPS, yc, conn_z+conn_h/2],
                         radius=conn_flange_screw/2.0, slices=S_HOLE))

    # USB-C service port on -Y wall
    shell = shell.subtract(box(usb_x-usb_w/2, -EPS, usb_z-usb_h/2,
                               usb_w, wall+2*EPS, usb_h))

    # PCB standoffs (M3 heat-set insert pilots) - sunk into the floor so the
    # boss/floor join is a volume overlap (clean union), not a coincident face
    for (px, py) in pcb_holes:
        so = cyl(px, py, floor_t-0.6, floor_t+standoff_h, standoff_d/2.0, S_HOLE)
        so = so.subtract(cyl(px, py, floor_t-0.6-EPS, floor_t+standoff_h+EPS,
                             m3_insert_d/2.0, S_HOLE))
        shell = shell.union(so)

    # Corner lid-screw towers (insert pocket in the top)
    for (px, py) in tower_pos:
        tw = cyl(px, py, floor_t-0.6, tower_top, tower_d/2.0, S_HOLE)
        tw = tw.subtract(cyl(px, py, tower_top-6.0, tower_top+EPS,
                             m3_insert_d/2.0, S_HOLE))
        shell = shell.union(tw)

    # Heavy-duty corner mounting ears
    shell = shell.union(corner_ear((0, 0),             225))
    shell = shell.union(corner_ear((outer_x, 0),       315))
    shell = shell.union(corner_ear((0, outer_y),       135))
    shell = shell.union(corner_ear((outer_x, outer_y),  45))
    return shell

# ============================================================================
#  LID  (own frame: Z0 = mating plane; plate goes +Z, tongue ribs go -Z)
# ============================================================================
def build_lid():
    lid = rbox(0, 0, 0, outer_x, outer_y, lid_top_t, corner_r)

    # four registration ribs (tongue) - run along the edges, clear of corners
    a, b = 12.0, 0.0
    rw   = 2.0                                   # rib thickness
    ro   = wall + fit_gap                        # rib outer face inside the wall
    xlo, xhi = 12.0, outer_x-12.0
    ylo, yhi = 12.0, outer_y-12.0
    ribs = [
        box(xlo, ro,            -lip_h, xhi-xlo, rw, lip_h),          # near -Y
        box(xlo, outer_y-ro-rw, -lip_h, xhi-xlo, rw, lip_h),          # near +Y
        box(ro,            ylo, -lip_h, rw, yhi-ylo, lip_h),          # near -X
        box(outer_x-ro-rw, ylo, -lip_h, rw, yhi-ylo, lip_h),         # near +X
    ]
    lid = union_all([lid] + ribs)

    # corner screw holes (clearance through + head counterbore from top)
    for (px, py) in tower_pos:
        lid = lid.subtract(cyl(px, py, -lip_h-EPS, lid_top_t+EPS, m3_clear_d/2.0, S_HOLE))
        lid = lid.subtract(cyl(px, py, lid_top_t-2.0, lid_top_t+EPS, m3_head_d/2.0, S_HOLE))

    # LED light-pipe holes
    for x in led_xs:
        lid = lid.subtract(cyl(x, led_y, -lip_h-EPS, lid_top_t+EPS, led_d/2.0, S_HOLE))

    # recessed branding-label pocket on the top face (engrave/print insert)
    lid = lid.subtract(box(outer_x/2-26, outer_y/2-7, lid_top_t-0.6, 52, 14, 0.6+EPS))
    return lid

# ============================================================================
#  STL export
# ============================================================================
def _normal(a, b, c):
    ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
    vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
    nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
    L = math.sqrt(nx*nx+ny*ny+nz*nz)
    return (0.0, 0.0, 0.0) if L == 0 else (nx/L, ny/L, nz/L)

def csg_to_triangles(obj):
    verts, polys, _ = obj.toVerticesAndPolygons()
    tris = []
    for poly in polys:
        if len(poly) < 3:
            continue
        for i in range(1, len(poly)-1):
            a, b, c = verts[poly[0]], verts[poly[i]], verts[poly[i+1]]
            tris.append((_normal(a, b, c), a, b, c))
    return tris

def weld_and_clean(tris, grid=2.0, tol=1.0e-3):
    """Weld coincident vertices and split T-junctions so the mesh is 2-manifold.

    pycsg's BSP CSG returns a closed surface but stitches coplanar union/cut
    seams with T-junctions (a long edge on one face meets two short collinear
    edges on the neighbour).  We insert the straddling vertex into the long
    triangle so every edge ends up shared by exactly two faces.
    """
    from collections import defaultdict

    # 1. weld vertices on a 1e-4 grid -> integer ids
    V, idx = [], {}
    def vid(p):
        k = (round(p[0], 4), round(p[1], 4), round(p[2], 4))
        i = idx.get(k)
        if i is None:
            i = len(V); idx[k] = i; V.append(k)
        return i
    F = []
    for _n, a, b, c in tris:
        i, j, k = vid(a), vid(b), vid(c)
        if i != j and j != k and i != k:
            F.append((i, j, k))

    # 2. spatial hash of vertices for collinear straddle queries
    cells = defaultdict(list)
    for vi, p in enumerate(V):
        cells[(int(p[0]//grid), int(p[1]//grid), int(p[2]//grid))].append(vi)

    def candidates(p, q):
        out = set()
        steps = int(max(abs(q[0]-p[0]), abs(q[1]-p[1]), abs(q[2]-p[2]))//grid) + 2
        for s in range(steps+1):
            t = s/steps
            cx = int((p[0]+(q[0]-p[0])*t)//grid)
            cy = int((p[1]+(q[1]-p[1])*t)//grid)
            cz = int((p[2]+(q[2]-p[2])*t)//grid)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        out.update(cells.get((cx+dx, cy+dy, cz+dz), ()))
        return out

    def splits(pi, qi):
        p, q = V[pi], V[qi]
        dx, dy, dz = q[0]-p[0], q[1]-p[1], q[2]-p[2]
        L2 = dx*dx + dy*dy + dz*dz
        if L2 == 0:
            return []
        res = []
        for w in candidates(p, q):
            if w == pi or w == qi:
                continue
            vw = V[w]
            wx, wy, wz = vw[0]-p[0], vw[1]-p[1], vw[2]-p[2]
            t = (wx*dx + wy*dy + wz*dz) / L2
            if t <= 1e-6 or t >= 1.0-1e-6:
                continue
            cxv, cyv, czv = wy*dz-wz*dy, wz*dx-wx*dz, wx*dy-wy*dx
            if (cxv*cxv + cyv*cyv + czv*czv)/L2 > tol*tol:
                continue
            res.append((t, w))
        res.sort()
        return [w for _t, w in res]

    # 3. re-triangulate every face that has straddling vertices on its edges
    newF = []
    for (i, j, k) in F:
        si, sj, sk = splits(i, j), splits(j, k), splits(k, i)
        if not (si or sj or sk):
            newF.append((i, j, k)); continue
        loop = [i] + si + [j] + sj + [k] + sk            # convex -> fan is valid
        for m in range(1, len(loop)-1):
            a, b, c = loop[0], loop[m], loop[m+1]
            if a != b and b != c and a != c:
                newF.append((a, b, c))

    # 4. drop interior coincident faces: triangles sharing all three vertices
    #    cancel in opposite-winding pairs (interior), keep the net majority.
    def okey(f):
        m = f.index(min(f))
        return (f[m], f[(m+1) % 3], f[(m+2) % 3])
    bucket = defaultdict(list)
    for f in newF:
        bucket[frozenset(f)].append(f)
    finalF = []
    for fs in bucket.values():
        if len(fs) == 1:
            finalF.append(fs[0]); continue
        ref = okey(fs[0]); cw = ccw = 0; rcw = rccw = None
        for f in fs:
            if okey(f) == ref:
                cw += 1; rcw = f
            else:
                ccw += 1; rccw = f
        net = cw - ccw
        finalF += [rcw]*net if net > 0 else [rccw]*(-net)

    out = []
    for (i, j, k) in finalF:
        a, b, c = V[i], V[j], V[k]
        out.append((_normal(a, b, c), a, b, c))
    return out

def manifold_stats(tris):
    from collections import Counter
    ec = Counter()
    rnd = lambda v: (round(v[0], 3), round(v[1], 3), round(v[2], 3))
    for _n, a, b, c in tris:
        a, b, c = rnd(a), rnd(b), rnd(c)
        for u, v in ((a, b), (b, c), (c, a)):
            ec[frozenset((u, v))] += 1
    hist = Counter(ec.values())
    return dict(sorted(hist.items()))

def write_stl(path, tris):
    with open(path, 'wb') as f:
        f.write(b'\0'*80)
        f.write(struct.pack('<I', len(tris)))
        for n, a, b, c in tris:
            f.write(struct.pack('<3f', *n))
            f.write(struct.pack('<3f', *a))
            f.write(struct.pack('<3f', *b))
            f.write(struct.pack('<3f', *c))
            f.write(struct.pack('<H', 0))

def bbox(tris):
    xs = [v[0] for _, a, b, c in tris for v in (a, b, c)]
    ys = [v[1] for _, a, b, c in tris for v in (a, b, c)]
    zs = [v[2] for _, a, b, c in tris for v in (a, b, c)]
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

# ============================================================================
if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out  = os.path.join(here, "STL")
    os.makedirs(out, exist_ok=True)

    for name, builder in (("base", build_base), ("lid", build_lid)):
        t0 = time.time()
        print(f"[build] {name} ...", flush=True)
        solid = builder()
        tris  = csg_to_triangles(solid)
        tris  = weld_and_clean(tris)
        path  = os.path.join(out, f"TP-CAN-2I_{name}.stl")
        write_stl(path, tris)
        lo, hi = bbox(tris)
        stats = manifold_stats(tris)
        ok = (set(stats) == {2})
        print(f"  {name}: {len(tris)} triangles  "
              f"bbox X[{lo[0]:.1f},{hi[0]:.1f}] "
              f"Y[{lo[1]:.1f},{hi[1]:.1f}] Z[{lo[2]:.1f},{hi[2]:.1f}]")
        print(f"        edge-histogram {stats}  2-manifold={ok}  "
              f"({time.time()-t0:.1f}s)  -> {os.path.relpath(path, here)}")
    print("done.")
