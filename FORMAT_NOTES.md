# Format observations and semantic confidence

## Verified against the two supplied files

| Field | Detection/interpretation | Confidence |
|---|---|---|
| Node block | Big-endian `uint32 id + 3 * float64`, 28-byte stride | Strong sample evidence |
| Connectivity | Big-endian arity + indices, arity 2/3/4 | Strong sample evidence |
| Body properties | 13 big-endian doubles; first is mass, next 3 are COM, last 9 form symmetric 3x3 inertia tensor | Strong sample evidence, still producer-specific |
| Mode parameter table | Sequential ID + five doubles | Strong structural detection; meaning of the five columns not asserted |
| Prefix | Raw bytes before node block | Exact preservation only; not assumed to be only a header |
| Modal/physical region | Raw bytes between mode table and connectivity | Exact preservation only; may contain mode shapes and other matrices |
| Units | Not decoded | Requires unit code mapping/schema |
| Generalized mass/stiffness matrices | Not decoded | Requires verified section layout |
| Full modal vectors | Not decoded | Requires verified sparse/dense representation and DOF ordering |

## Why the tool is not labeled a universal semantic decoder

The public product documentation describes what an MNF contains, but a complete byte-level record map is not established by these two examples. A generic scanner can preserve every byte and safely decode known structures, but cannot infer the meaning of every numeric region from values alone.

For full semantic support, add a producer/version profile with documented section IDs, integer widths, byte order, dimensions, DOF ordering, sparse-matrix conventions, and unit enums. Keep the raw block export enabled so unknown data is never lost.
