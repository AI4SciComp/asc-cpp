! Original test-only ISO_C_BINDING probe; no upstream implementation is copied.
subroutine asc_lapack_fortran_probe(single_values, double_values, &
    integer_bytes, logical_bytes) bind(C)
  use, intrinsic :: iso_c_binding, only: c_float_complex, c_double_complex, c_int
  implicit none
  complex(c_float_complex), intent(inout) :: single_values(2)
  complex(c_double_complex), intent(inout) :: double_values(2)
  integer(c_int), intent(out) :: integer_bytes, logical_bytes
  integer :: ordinary_integer
  logical :: ordinary_logical
  integer_bytes = storage_size(ordinary_integer) / 8
  logical_bytes = storage_size(ordinary_logical) / 8
  single_values(1) = conjg(single_values(1)) + 2
  single_values(2) = -single_values(2)
  double_values(1) = conjg(double_values(1)) + 2
  double_values(2) = -double_values(2)
end subroutine asc_lapack_fortran_probe
