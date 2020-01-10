template <class T, class T1, class T2,
MY_IF(is_scalar<T>() && is_scalar<T1>() && is_scalar<T2>())>
void Plus(T &v, const T1 &v1, const T2 &v2)
{ v = v1 + v2; }