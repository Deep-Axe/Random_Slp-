# Jane Street puzzle — "It's a Metric, Too" (October 2026)

**Answer: 408951**

## Solution

Each letter is one state; lowercase marks a state's capitol.

```
A A A A B C D D D D D
B B b B B C C C E e E
B F F F F C g C E H E
I F F F F C C C E H E
I I F F F F J C K H H
I F F F F F J j K H l
I i I M F f F J J H H
N N I M O F F F F F H
N I I O O F F F F H H
N N I N O P F F F F H
N N N N P p F F F F H
```

| State | Size | Capitol | | State | Size | Capitol |
|---|---|---|---|---|---|---|
| A | 4 | – | | I | 11 | (6,1) |
| B | 7 | (1,2) | | J | 5 | (5,7) |
| C | 10 | – | | K | 2 | – |
| D | 5 | – | | L | 1 | (5,10) |
| E | 7 | (1,9) | | M | 2 | – |
| F | 37 | (6,5) | | N | 10 | – |
| G | 1 | (2,6) | | O | 4 | – |
| H | 12 | – | | P | 3 | (10,5) |

State sizes per cell:

```
  4   4   4   4   7  10   5   5   5   5   5
  7   7   7   7   7  10  10  10   7   7   7
  7  37  37  37  37  10   1  10   7  12   7
 11  37  37  37  37  10  10  10   7  12   7
 11  11  37  37  37  37   5  10   2  12  12
 11  37  37  37  37  37   5   5   2  12   1
 11  11  11   2  37  37  37   5   5  12  12
 10  10  11   2   4  37  37  37  37  37  12
 10  11  11   4   4  37  37  37  37  12  12
 10  10  11  10   4   3  37  37  37  37  12
 10  10  10  10   3   3  37  37  37  37  12
```

Row sums: 58, 86, 202, 215, 211, 221, 180, 234, 212, 208, 206.
Sum of squares = **408951**.

Run `python3 verify.py` to check it independently. It recomputes every state's symmetries and
capitol from scratch, runs a shortest-path search from the capitols, and confirms all 33 clues.

## How it was found

1. **Hand deductions.**
   - A one-cell state is always a capitol, at distance 0. So every clue of 1 sits next to a singleton.
   - That forces a singleton at (5,10).
   - For neighbouring cells, |d(u) − d(v)| ≤ min(size u, size v). Applied to pairs of clues, this gives minimum state sizes. For example, the 77 and 61 at the bottom force states of at least 16 cells.
2. **Exact case analysis with CP-SAT.** Narrow hypotheses were proven impossible one by one. For example, a chain of exact proofs showed that (10,8) and (10,9) are both in states of 17 or more cells.
3. **Learned "cores".** Candidate giant states were pinned and refuted. Each refutation's unsat core gave a general rule ("these cells can never share a state"). About 80 such rules were learned and fed back into the search.
4. **The formulation that cracked it: a hybrid shape-placement model.**
   - Every symmetric state of up to 10 cells is listed explicitly: 85,975 shapes, each with its size and capitol known exactly.
   - Larger states use 7 flexible slots, each with its own symmetry and capitol encoding.
   - On planted 7×7 test puzzles, explicit placements solved 10/10 in seconds. The earlier cell-label model solved 5/10.
   - On the real grid, the hybrid model found the solution in about 52 minutes on 4 cores.

Uniqueness was not separately checked.
