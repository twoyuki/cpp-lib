#include <cstddef>
#include <type_traits>
#include <utility>

namespace Detail {

template <typename T>
struct is_explicitly_constructible {
 private:
  static void test(T);

 public:
  static constexpr bool value = !requires() { test({}); };
};

}  // namespace Detail

template <size_t Index, typename Type, bool = std::is_empty_v<Type>>
class TupleLiaf;

template <typename Indices, typename... Types>
struct TupleImpl;

template <typename... Types>
class Tuple;

template <size_t Index, typename... Types>
decltype(auto) get(Tuple<Types...>& tuple);

template <size_t Index, typename... Types>
decltype(auto) get(const Tuple<Types...>& tuple);

template <size_t Index, typename... Types>
decltype(auto) get(Tuple<Types...>&& tuple);

template <size_t Index, typename... Types>
decltype(auto) get(const Tuple<Types...>&& tuple);

namespace Detail {

template <typename Type>
struct IsTupleHelper : public std::false_type {};

template <typename... Types>
struct IsTupleHelper<Tuple<Types...>> : public std::true_type {};

template <typename Type>
concept IsTuple = IsTupleHelper<std::remove_cvref_t<Type>>::value;

}  // namespace Detail

template <size_t Index, typename Type>
class TupleLiaf<Index, Type, false> {
 public:
  TupleLiaf()
    requires std::is_default_constructible_v<Type>
      : value_() {}

  TupleLiaf(const TupleLiaf&) = default;

  TupleLiaf(TupleLiaf&&) = default;

  template <typename Any>
    requires std::is_constructible_v<Type, Any&&>
  TupleLiaf(Any&& value) : value_(std::forward<Any>(value)) {}

  Type& get() { return value_; }

  const Type& get() const { return value_; }

 private:
  Type value_;
};

template <size_t Index, typename Type>
class TupleLiaf<Index, Type, true> : private Type {
 public:
  TupleLiaf() = default;

  TupleLiaf(const TupleLiaf&) = default;

  TupleLiaf(TupleLiaf&&) = default;

  template <typename Any>
    requires std::is_constructible_v<Type, Any&&>
  TupleLiaf(Any&& value) : Type(std::forward<Any>(value)) {}

  Type& get() { return static_cast<Type&>(*this); }

  const Type& get() const { return static_cast<const Type&>(*this); }
};

template <size_t... Indices, typename... Types>
struct TupleImpl<std::index_sequence<Indices...>, Types...>
    : public TupleLiaf<Indices, Types>... {
  TupleImpl() = default;

  TupleImpl(const TupleImpl&) = default;

  TupleImpl(TupleImpl&&) = default;

  template <typename... Args>
    requires std::conjunction_v<std::is_constructible<Types, Args&&>...>
  TupleImpl(Args&&... args)
      : TupleLiaf<Indices, Types>(std::forward<Args>(args))... {}
};

template <typename... Types>
class Tuple {
 public:
  explicit(std::disjunction_v<Detail::is_explicitly_constructible<Types>...>)
      Tuple() = default;

  Tuple(const Tuple&) = default;

  Tuple(Tuple&&) = default;

  explicit(!std::conjunction_v<std::is_convertible<const Types&, Types>...>)
      Tuple(const Types&... args)
    requires std::conjunction_v<std::is_copy_constructible<Types>...>
      : base_(args...) {}

  template <typename... Args>
    requires std::conjunction_v<std::is_constructible<Types, Args&&>...>
  explicit(!std::conjunction_v<std::is_convertible<Args&&, Types>...>)
      Tuple(Args&&... args)
      : base_(std::forward<Args>(args)...) {}

  template <typename... Others>
    requires std::conjunction_v<std::is_constructible<Types, const Others&>...>
  explicit(!std::conjunction_v<std::is_convertible<const Others&, Types>...>)
      Tuple(const Tuple<Others...>& other)
      : Tuple(other, make_index_sequence()) {}

  template <typename... Others>
    requires std::conjunction_v<std::is_constructible<Types, Others&&>...>
  explicit(!std::conjunction_v<std::is_convertible<Others&&, Types>...>)
      Tuple(Tuple<Others...>&& other)
      : Tuple(std::move(other), make_index_sequence()) {}

  template <typename First, typename Second>
  Tuple(const std::pair<First, Second>& pair)
      : Tuple(pair.first, pair.second) {}

  template <typename First, typename Second>
  Tuple(std::pair<First, Second>&& pair)
      : Tuple(std::move(pair.first), std::move(pair.second)) {}

  Tuple& operator=(const Tuple& other)
    requires std::conjunction_v<std::is_copy_assignable<Types>...>
  {
    assign(other, make_index_sequence());
    return *this;
  }

  template <typename... Others>
    requires std::conjunction_v<std::is_assignable<Types&, const Others&>...>
  Tuple& operator=(const Tuple<Others...>& other) {
    assign(other, make_index_sequence());
    return *this;
  }

  Tuple& operator=(Tuple&& other)
    requires std::conjunction_v<std::is_move_assignable<Types>...>
  {
    assign(std::move(other), make_index_sequence());
    return *this;
  }

  template <typename... Others>
    requires std::conjunction_v<std::is_assignable<Types&, Others&&>...>
  Tuple& operator=(Tuple<Others...>&& other) {
    assign(std::move(other), make_index_sequence());
    return *this;
  }

  template <size_t Index, typename... Others>
  friend decltype(auto) get(Tuple<Others...>& tuple);

  template <size_t Index, typename... Others>
  friend decltype(auto) get(const Tuple<Others...>& tuple);

  template <size_t Index, typename... Others>
  friend decltype(auto) get(Tuple<Others...>&& tuple);

  template <size_t Index, typename... Others>
  friend decltype(auto) get(const Tuple<Others...>&& tuple);

 private:
  using Base = TupleImpl<std::make_index_sequence<sizeof...(Types)>, Types...>;

  Base base_;

  template <typename... Others, size_t... Indices>
  Tuple(const Tuple<Others...>& tuple, std::index_sequence<Indices...>)
      : Tuple(get<Indices>(tuple)...) {}

  template <typename... Others, size_t... Indices>
  Tuple(Tuple<Others...>&& tuple, std::index_sequence<Indices...>)
      : Tuple(get<Indices>(std::move(tuple))...) {}

  template <typename... Others, size_t... Indices>
  void assign(const Tuple<Others...>& tuple, std::index_sequence<Indices...>) {
    ((get<Indices>(*this) = get<Indices>(tuple)), ...);
  }

  template <typename... Others, size_t... Indices>
  void assign(Tuple<Others...>&& tuple, std::index_sequence<Indices...>) {
    ((get<Indices>(*this) = get<Indices>(std::move(tuple))), ...);
  }

  static auto make_index_sequence() {
    return std::make_index_sequence<sizeof...(Types)>();
  }
};

template <typename First, typename Second>
Tuple(std::pair<First, Second>) -> Tuple<First, Second>;

namespace std {

template <typename... Types>
struct tuple_size<Tuple<Types...>>
    : public integral_constant<size_t, sizeof...(Types)> {};

template <size_t Index, typename Type, typename... Types>
struct tuple_element<Index, Tuple<Type, Types...>> {
  using type = tuple_element_t<Index - 1, Tuple<Types...>>;
};

template <typename Type, typename... Types>
struct tuple_element<0, Tuple<Type, Types...>> {
  using type = Type;
};

}  // namespace std

template <size_t Index, typename... Types>
decltype(auto) get(Tuple<Types...>& tuple) {
  using Type = std::tuple_element_t<Index, Tuple<Types...>>;
  using Base = TupleLiaf<Index, Type>;
  return static_cast<Base&>(tuple.base_).get();
}

template <size_t Index, typename... Types>
decltype(auto) get(const Tuple<Types...>& tuple) {
  using Type = std::tuple_element_t<Index, Tuple<Types...>>;
  using Base = const TupleLiaf<Index, Type>;
  return static_cast<Base&>(tuple.base_).get();
}

template <size_t Index, typename... Types>
decltype(auto) get(Tuple<Types...>&& tuple) {
  using Type = std::tuple_element_t<Index, Tuple<Types...>>;
  using Base = TupleLiaf<Index, Type>;
  return static_cast<Type&&>(static_cast<Base&&>(tuple.base_).get());
}

template <size_t Index, typename... Types>
decltype(auto) get(const Tuple<Types...>&& tuple) {
  using Type = std::tuple_element_t<Index, Tuple<Types...>>;
  using Base = const TupleLiaf<Index, Type>;
  return static_cast<const Type&&>(static_cast<Base&&>(tuple.base_).get());
}

namespace Detail {

template <typename Type, typename Candidate, typename... Types>
constexpr size_t find_index(size_t current) {
  if constexpr (std::is_same_v<Type, Candidate>) {
    return current;
  } else {
    return find_index<Type, Types...>(current + 1);
  }
}

template <typename Type, typename Tuple>
struct FindOneType;

template <typename Type, typename... Types>
struct FindOneType<Type, Tuple<Types...>> {
  static constexpr size_t value = find_index<Type, Types...>(0);
};

}  // namespace Detail

template <typename Type, Detail::IsTuple TupleType>
decltype(auto) get(TupleType&& tuple) {
  using Clear = std::remove_cvref_t<TupleType>;
  using Detail::FindOneType;
  return get<FindOneType<Type, Clear>::value>(std::forward<TupleType>(tuple));
}

template <typename... Args>
auto makeTuple(Args&&... args) {
  return Tuple<std::decay_t<Args>...>(std::forward<Args>(args)...);
}

template <typename... Args>
auto forwardAsTuple(Args&&... args) {
  return Tuple<Args&&...>(std::forward<Args>(args)...);
}

template <typename... Types>
auto tie(Types&... args) {
  return Tuple<Types&...>(args...);
}

namespace TupleCatImplementation {

template <typename... Tuples>
struct TupleCat;

template <typename... Tuples>
using TupleCatType = typename TupleCat<Tuples...>::Type;

template <>
struct TupleCat<> {
  using Type = Tuple<>;
};

template <typename... Types>
struct TupleCat<Tuple<Types...>> {
  using Type = Tuple<Types...>;
};

template <typename... FirstTypes, typename... SecondTypes, typename... Tuples>
struct TupleCat<Tuple<FirstTypes...>, Tuple<SecondTypes...>, Tuples...> {
  using Type = TupleCatType<Tuple<FirstTypes..., SecondTypes...>, Tuples...>;
};

template <typename... Tuples>
struct MakeFirstIndicesList;

template <>
struct MakeFirstIndicesList<> {
  using Type = std::index_sequence<>;
};

template <typename First, typename... Other>
struct MakeFirstIndicesList<First, Other...> {
  using Type =
      std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<First>>>;
};

template <typename Return, typename Indices, typename... Tuples>
struct TupleConcater;

template <typename Return, size_t... Indices, typename First, typename... Other>
struct TupleConcater<Return, std::index_sequence<Indices...>, First, Other...> {
  template <typename... Args>
  static Return do_first(First&& first, Other&&... other, Args&&... args) {
    using IndicesList = typename MakeFirstIndicesList<Other...>::Type;
    using Next = TupleConcater<Return, IndicesList, Other...>;
    return Next::do_first(std::forward<Other>(other)...,
                          std::forward<Args>(args)...,
                          get<Indices>(std::forward<First>(first))...);
  }
};

template <typename Return>
struct TupleConcater<Return, std::index_sequence<>> {
  template <typename... Args>
  static Return do_first(Args&&... args) {
    return Return(std::forward<Args>(args)...);
  }
};

template <typename... Tuples>
auto tupleCat(Tuples&&... tuples) {
  using Return = TupleCatType<std::remove_cvref_t<Tuples>...>;
  using IndicesList = typename MakeFirstIndicesList<Tuples...>::Type;
  using Concater = TupleConcater<Return, IndicesList, Tuples...>;
  return Concater::do_first(std::forward<Tuples>(tuples)...);
}

}  // namespace TupleCatImplementation

using TupleCatImplementation::tupleCat;
