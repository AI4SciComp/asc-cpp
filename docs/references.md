# Scientific references

Primary specifications and algorithm sources used by ASCCpp include:

- C. L. Lawson, R. J. Hanson, D. R. Kincaid, and F. T. Krogh, “Basic Linear
  Algebra Subprograms for Fortran Usage,” *ACM Transactions on Mathematical
  Software* 5(3), 1979, <https://doi.org/10.1145/355841.355847>.
- J. J. Dongarra, J. Du Croz, S. Hammarling, and R. J. Hanson, “An Extended Set
  of FORTRAN Basic Linear Algebra Subprograms,” *ACM TOMS* 14(1), 1988,
  <https://doi.org/10.1145/42288.42291>.
- I. S. Duff, M. A. Heroux, and R. Pozo, “An Overview of the Sparse Basic
  Linear Algebra Subprograms,” *ACM TOMS* 28(2), 2002,
  <https://doi.org/10.1145/567806.567810>.
- J. K. Salmon et al., “Parallel Random Numbers: As Easy as 1, 2, 3,” SC11,
  2011, <https://doi.org/10.1145/2063384.2063405> (Philox).
- M. E. O'Neill, “PCG: A Family of Simple Fast Space-Efficient Statistically
  Good Algorithms for Random Number Generation,” 2014,
  <https://www.pcg-random.org/paper.html>.
- S. Joe and F. Y. Kuo, “Constructing Sobol Sequences with Better Two-
  Dimensional Projections,” *SIAM Journal on Scientific Computing* 30(5),
  2008, <https://doi.org/10.1137/070709359>.

The exact Joe--Kuo direction-number data and redistribution terms are retained
in [`data/random/`](../data/random/). Algorithm use and rejection decisions are
tracked by the BLAS and Random contracts rather than inferred from citations.
