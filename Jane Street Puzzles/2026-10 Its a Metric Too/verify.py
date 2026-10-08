"""Standalone verifier for Jane Street "It's a Metric, Too" (October 2026).

Checks that the state map below is a valid answer:
  * every state is connected and symmetric (some rotation or reflection maps it to itself);
  * capitols are found exactly as the rules define them;
  * shortest-path costs to the nearest capitol match every clue;
then prints the answer (sum of squares of the row sums of state sizes).
"""
import heapq

N = 11
# Solution: one letter per state (capitols not marked here; they are computed).
STATE_MAP = """
AAAABCDDDDD
BBBBBCCCEEE
BFFFFCGCEHE
IFFFFCCCEHE
IIFFFFJCKHH
IFFFFFJJKHL
IIIMFFFJJHH
NNIMOFFFFFH
NIIOOFFFFHH
NNINOPFFFFH
NNNNPPFFFFH
"""
CLUES = {(0,1):8,(0,2):4,(0,5):11,(0,9):5,(1,7):11,(2,2):7,(2,5):1,(2,10):14,
         (3,0):28,(3,3):51,(3,6):1,(3,8):6,(4,1):22,(4,8):4,(4,10):1,(5,3):15,(5,5):10,(5,7):0,
         (6,0):11,(6,2):11,(6,9):9,(7,2):17,(7,4):14,(7,7):10,(7,10):13,(8,0):30,(8,5):6,(8,8):45,
         (9,3):10,(10,1):26,(10,5):0,(10,8):77,(10,9):61}

rows = [r for r in STATE_MAP.split() if r]
states = {}
for r in range(N):
    for c in range(N):
        states.setdefault(rows[r][c], []).append((r, c))

def connected(S):
    S = set(S); stack = [next(iter(S))]; seen = {stack[0]}
    while stack:
        r, c = stack.pop()
        for q in ((r+1,c),(r-1,c),(r,c+1),(r,c-1)):
            if q in S and q not in seen: seen.add(q); stack.append(q)
    return len(seen) == len(S)

def symmetries(S):
    """Fixed-cell sets of every non-identity rotation/reflection mapping S onto itself."""
    S = set(S); out = []
    maps = [(1,0,0,-1),(-1,0,0,1),(-1,0,0,-1),(0,1,1,0),(0,-1,-1,0),(0,1,-1,0),(0,-1,1,0)]
    r0 = min(p[0] for p in S); c0 = min(p[1] for p in S)
    for a, b, c, d in maps:
        img = [(a*r + b*q, c*r + d*q) for r, q in S]
        tr = r0 - min(p[0] for p in img); tc = c0 - min(p[1] for p in img)
        f = lambda p: (a*p[0] + b*p[1] + tr, c*p[0] + d*p[1] + tc)
        if {f(p) for p in S} == S:
            out.append([p for p in S if f(p) == p])
    return out

capitols = set()
for name, S in states.items():
    assert connected(S), f'state {name} is not connected'
    syms = symmetries(S)
    assert syms, f'state {name} is not symmetric'
    if all(len(fx) == 1 for fx in syms):
        capitols.add(syms[0][0])

size = {p: len(S) for S in states.values() for p in S}
dist = {p: float('inf') for p in size}; pq = []
for p in capitols: dist[p] = 0; heapq.heappush(pq, (0, p))
while pq:
    dv, p = heapq.heappop(pq)
    if dv > dist[p]: continue
    r, c = p
    for q in ((r+1,c),(r-1,c),(r,c+1),(r,c-1)):
        if q in size and dv + min(size[p], size[q]) < dist[q]:
            dist[q] = dv + min(size[p], size[q]); heapq.heappush(pq, (dist[q], q))

wrong = {p: (dist[p], v) for p, v in CLUES.items() if dist[p] != v}
print(f'{len(states)} states, capitols at {sorted(capitols)}')
print('all clues match' if not wrong else f'MISMATCHES: {wrong}')
row_sums = [sum(size[(r, c)] for c in range(N)) for r in range(N)]
print('row sums:', row_sums)
print('answer:', sum(v * v for v in row_sums))
