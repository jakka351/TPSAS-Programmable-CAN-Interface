#!/usr/bin/env python3
# Quick verification render of the exported STLs (isometric PNG) + topology report.
import struct, os, math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

HERE = os.path.dirname(os.path.abspath(__file__))

def read_stl(p):
    with open(p, "rb") as f:
        f.read(80); n = struct.unpack("<I", f.read(4))[0]
        T = np.empty((n, 3, 3), np.float32)
        for i in range(n):
            f.read(12)
            T[i] = np.array(struct.unpack("<9f", f.read(36)), np.float32).reshape(3, 3)
            f.read(2)
    return T

def topo(T):
    vk = {}; idx = lambda v: vk.setdefault((round(v[0],4),round(v[1],4),round(v[2],4)), len(vk))
    E = {}; F = len(T)
    for tri in T:
        a, b, c = idx(tri[0]), idx(tri[1]), idx(tri[2])
        for u, v in ((a,b),(b,c),(c,a)):
            E[frozenset((u,v))] = E.get(frozenset((u,v)),0)+1
    V = len(vk); Ec = len(E)
    return V, Ec, F, V-Ec+F

def render(T, title, png, elev=26, azim=-52):
    n = np.cross(T[:,1]-T[:,0], T[:,2]-T[:,0])
    ln = np.linalg.norm(n, axis=1, keepdims=True); ln[ln==0] = 1; n = n/ln
    light = np.array([0.4, -0.5, 0.85]); light = light/np.linalg.norm(light)
    sh = 0.35 + 0.65*np.clip(n @ light, 0, 1)
    base = np.array([0.62, 0.71, 0.80])
    cols = np.clip(sh[:,None]*base, 0, 1)
    fig = plt.figure(figsize=(8,6), dpi=120)
    ax = fig.add_subplot(111, projection="3d")
    pc = Poly3DCollection(T, facecolors=cols, edgecolors=(0,0,0,0.06), linewidths=0.15)
    ax.add_collection3d(pc)
    mn = T.reshape(-1,3).min(0); mx = T.reshape(-1,3).max(0)
    ctr = (mn+mx)/2; rng = (mx-mn).max()/2*1.05
    ax.set_xlim(ctr[0]-rng, ctr[0]+rng); ax.set_ylim(ctr[1]-rng, ctr[1]+rng); ax.set_zlim(ctr[2]-rng, ctr[2]+rng)
    ax.set_box_aspect((1,1,1)); ax.view_init(elev=elev, azim=azim)
    ax.set_title(title, fontsize=11); ax.set_axis_off()
    fig.tight_layout(); fig.savefig(png, bbox_inches="tight"); plt.close(fig)

if __name__ == "__main__":
    for name in ("base", "lid"):
        stl = os.path.join(HERE, "STL", f"TP-CAN-2I_{name}.stl")
        T = read_stl(stl)
        V, E, F, eu = topo(T)
        print(f"{name}: V={V} E={E} F={F} euler(V-E+F)={eu}  (closed genus-0 shell => 2)")
        png = os.path.join(HERE, "STL", f"preview_{name}.png")
        render(T, f"TP-CAN-2I  {name}", png)
        print(f"   render -> {os.path.relpath(png, HERE)}")
    # combined assembly-ish view: base + lid lifted
    Tb = read_stl(os.path.join(HERE,"STL","TP-CAN-2I_base.stl"))
    Tl = read_stl(os.path.join(HERE,"STL","TP-CAN-2I_lid.stl")).copy()
    Tl[:,:,2] = -Tl[:,:,2] + 24.6 + 14          # flip lid upright, lift above base
    render(np.concatenate([Tb, Tl]), "TP-CAN-2I  exploded assembly",
           os.path.join(HERE,"STL","preview_assembly.png"))
    print("   render -> STL/preview_assembly.png")
