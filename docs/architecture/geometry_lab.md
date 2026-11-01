# Geometry Lab module — parsers, API, viewer

Implements `backend/include/nexus/geometry.hpp` /
`backend/src/geometry/{stl_parser,csv_parser}.cpp`.

## ASCII STL parser

A hand-rolled tokenizer (not `std::istringstream >>` alone) that also
tracks line numbers, because a useful "malformed STL" error needs to say
*where*. It strictly validates the grammar:

```
solid <name>
  facet normal <nx> <ny> <nz>
    outer loop
      vertex <x> <y> <z>   (x3)
    endloop
  endfacet                  (repeated)
endsolid [name]
```

Anything that doesn't match — missing keywords, wrong vertex count, a
non-finite (NaN/Inf) or unparsable number, binary bytes fed in as if
ASCII — is rejected with a line-numbered, specific error rather than
either crashing or silently producing garbage geometry. A `solid`
header immediately followed by `endsolid` (zero triangles) is also
rejected as an empty file, not treated as a valid degenerate case.

Size is capped at 5 MiB by default, checked before parsing starts.

## ANSYS nodal-temperature CSV parser

Requires the header to be *exactly* `node_id,x_m,y_m,z_m,temperature_C`
(reordered or renamed columns are rejected, not silently remapped).
Each data row must have exactly 5 fields, each numeric field must parse
fully (no trailing garbage) and be finite. Errors name the 1-indexed
row (matching what a user sees if they open the file in a spreadsheet).
A header with zero data rows is rejected as an empty dataset.

## Why these two are separate from the thermal solver

The thermal FEA module (`nexus/thermal.hpp`) and this module read and
write superficially similar things (nodes, temperatures) but serve
different purposes: thermal.cpp *computes* a temperature field from a
PDE; geometry's CSV parser only *imports* a nodal temperature field
that's assumed to already exist (synthetic here, real ANSYS output in
general). The brief is explicit that mesh-to-result registration
between the two is out of scope, so they're kept as independent
modules rather than forced into one shared data model.

## API

- `POST /api/geometry/stl/parse` — body: raw ASCII STL text; returns
  the parsed triangles + bounding box, or `{"valid":false,"error":...}`.
- `POST /api/geometry/csv/parse` — body: raw CSV text; returns parsed
  nodes + temperature stats, or `{"valid":false,"error":...}`.
- `GET /api/geometry/sample/stl`, `GET /api/geometry/sample/csv` —
  serve the committed sample files (see `data/geometry/README.md`) so
  the frontend's "Load sample" buttons exercise the exact same parse
  endpoints as a real upload, not a separate code path.

## Frontend viewers

`MeshViewer` and `PointCloudViewer` (`frontend/src/components/`) are
real Three.js scenes built directly from the parsed JSON — not
pre-rendered images. Both support orbit (rotate/pan/zoom via
`OrbitControls`) and a "fit to view" button that recenters the camera
on the actual parsed bounding box. The point cloud additionally
supports click-to-inspect via raycasting, cross-referenced against a
sortable table of all nodes.
