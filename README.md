# Alahmadi et al.’s pairing-Hamiltonian problems

[GitHub repository](https://github.com/emzeay/pairing-hamiltonian-proofs)

The four questions below come from Section 8 of [Alahmadi et al., *Extending a perfect matching to a Hamiltonian cycle* (2015)](https://dmtcs.episciences.org/articles/2105/download#page=12). A graph is pairing-Hamiltonian if every pairing of its vertices can be completed to a Hamiltonian cycle by adding a disjoint perfect matching of graph edges. The prescribed pairs can be nonedges.

### Question 1 · Rings of cubes

We prove that chains of three-dimensional cubes of any length are pairing-Hamiltonian. The prism theorem gives the four-dimensional chain, and taking a spanning chain in each ring answers both cases. The proof also gives a sharp bound of four added edges per seam in the three-dimensional chain.

- [Computer-assisted proof](q1/proof-computational.pdf), [certificate](q1/certificate.txt.gz), and [verifier](q1/verify.cpp)
- [Geometric proof by cases](q1/proof-geometric.pdf), our current human-checkable argument

### Question 2 · Edges that can be forced

The proposed bound of 100 has a negative answer. Prescribing one half-layer and pairing the remaining vertices across the cut by a cyclic shift makes every edge of the opposite half-layer unavailable as an added edge. In dimension nine, that gives 128 unavailable edges. This also follows from the prescribed-endpoint theorem of [Gregor, Novotný, and Škrekovski (2018)](https://www.combinatorics.org/ojs/index.php/eljc/article/view/v25i2p56).

### Question 3 · Products of two cycles

Gauci and Zerafa showed that the only pairing-Hamiltonian product of two cycles is the product of two 4-cycles. [Gauci and Zerafa, *Perfect Matchings and Hamiltonicity in the Cartesian Product of Cycles*](https://arxiv.org/abs/2005.02913)

### Question 4 · Graphs of girth at least five

We construct pairing-Hamiltonian graphs of girth five on $20m$ vertices for every positive integer $m$. The proof joins copies of a 20-vertex block along seams that create no squares. The gluing argument is in prose, and the finite claim about the block has a computer-assisted proof. We are exploring a direct proof of that finite claim.

- [Computer-assisted proof](q4/proof-computational.pdf), [certificate](q4/certificate.bin.gz), and [verifier](q4/verify.cpp)

Each computer-assisted proof includes commands to replay its certificate from its own folder. The certificates are gzip-compressed text for Q1 and gzip-compressed binary data for Q4.

This repository is a work in progress. The Q1 and Q4 results were developed with AI assistance and have undergone internal checks. They are awaiting independent expert review.
