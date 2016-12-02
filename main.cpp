#include <iostream>
#include <type_traits>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>

// sequence containers
#include <array>
#include <vector>
#include <deque>
#include <forward_list>
#include <list>

// sorted associative containers
#include <set> // incl multiset
#include <map> // incl multimap

// unordered associative containers
#include <unordered_set> // incl unordered_multiset
#include <unordered_map> // incl unordered_multimap


/*
TODO
- reconsider const buffer concept for read buffers... std::ifstream doesnt do that
- initialize buffer with some size

LIMITATIONS
- integral types: int, double, int64_t
- size_t not allowed as type --> no mpiutil fcts, used internally
- needs default ctor available
- no pointers --> user workaround in load/save
- no container adaptors (stack, queue, priority_queue) --> user workaround in load/save exposing underlying container
*/


typedef int64_t  int64_t;


template<typename T>
struct _BufferTraits
{
  typedef std::vector<T> BCType;
  typedef typename BCType::const_iterator BCIterator;
}; 


struct _OBufferIts
{
  typedef std::vector<size_t>     BCSizes;
  typedef BCSizes::const_iterator BCSit;

  BCSit bi_sizes;

  _BufferTraits<int>::BCIterator     bi_int;
  _BufferTraits<int64_t>::BCIterator bi_int64_t;
  _BufferTraits<double>::BCIterator  bi_double;
};


template<typename O>
struct _OBuffer
{
  _OBufferIts::BCSizes bc_sizes;

  typename _BufferTraits<int>::BCType     bc_int;
  typename _BufferTraits<int64_t>::BCType bc_int64_t;
  typename _BufferTraits<double>::BCType  bc_double;

  /// because this just keeps track of the state of deserialization - no changes to the buffer itself
  mutable _OBufferIts its; 
};


namespace _traits
{  
  template<typename T, typename B>
  struct access{};
  
  template<typename B>
  struct access<int, B>
  {
    static typename       _BufferTraits<int>::BCType& buffer(B &b) { return b.bc_int; }
    static const typename _BufferTraits<int>::BCType& buffer(const B &b) { return b.bc_int; }
    static typename       _BufferTraits<int>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_int; }
  };

  template<typename B>
  struct access<int64_t, B>
  {
    static typename       _BufferTraits<int64_t>::BCType& buffer(B &b) { return b.bc_int64_t; }
    static const typename _BufferTraits<int64_t>::BCType& buffer(const B &b) { return b.bc_int64_t; }
    static typename       _BufferTraits<int64_t>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_int64_t; }
  };

  template<typename B>
  struct access<double, B>
  {
    static typename       _BufferTraits<double>::BCType& buffer(B &b) { return b.bc_double; }
    static const typename _BufferTraits<double>::BCType& buffer(const B &b) { return b.bc_double; }
    static typename       _BufferTraits<double>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_double; }
  };

  // specialized for container size buffer only, we don't allow for size_t as data type
  template<typename B>
  struct access<size_t, B>
  {
    static typename       _BufferTraits<size_t>::BCType& buffer(B &b) { return b.bc_sizes; }
    static const typename _BufferTraits<size_t>::BCType& buffer(const B &b) { return b.bc_sizes; }
    static typename       _BufferTraits<size_t>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_sizes; }
  };
}


// convenience functions for traits access - traits won't be accessed
// directly by anything else, only vie this interface. 

template<typename T, typename B> inline
typename _BufferTraits<T>::BCType& buffer(B &b)
{
  return _traits::access<T, B>::buffer(b);
}


template<typename T, typename B> inline
const typename _BufferTraits<T>::BCType& buffer(const B &b)
{
  return _traits::access<T, B>::buffer(b);
}


template<typename T, typename B> inline
typename _BufferTraits<T>::BCIterator& buffer_iterator(const B &b)
{
  return _traits::access<T, B>::buffer_iterator(b);
}

/// from here on, the buffer, its iterators and types should
/// only be interfaced - via the above three functions


template<typename O>
struct BufferTraits
{
  typedef _OBuffer<O> Buffer;
}; 


template<typename O> inline
std::unique_ptr<typename BufferTraits<O>::Buffer> make_write_buffer()
{
  std::unique_ptr<typename BufferTraits<O>::Buffer> bPtr(nullptr);
  bPtr.reset(new BufferTraits<O>::Buffer());  
  return bPtr;
}


/// this is the only external link to the type of container used in
/// the buffer - everything else is encapsulated through the stl iterator 
/// api. at the bottom level (integral types), all << operators end here
// these three guys should be the only one that actually access the buffer
// and its iterators

template<typename B, typename D> inline
void push_into_buffer(B &b, D v)
{
  // buffer is a sequence containers
  buffer<D>(b).push_back(v);
}


template<typename B, typename D> inline
void fetch_from_buffer(const B &b, D &v)
{
  auto &it = buffer_iterator<D>(b);
  v = (*it);
  ++it;
}


// next stored size from buffer
template<typename B> inline
size_t fetch_size(B &b)
{
  auto &bit_size = buffer_iterator<size_t>(b);
  const auto csize = *bit_size;
  ++bit_size;
  return csize;
}


template<typename B, typename C> inline
void fetch_size_and_apply(B &b, C &c)
{  
  c.resize(fetch_size(b));
}


/// BUFFER INTEGRAL TYPES
/// for each integral type we support. this results in some code duplication
/// but gives better compilation errors for unsupported types and also makes
/// for a clear bottom of recursion

/// int
template<typename B> inline
void operator << (B &b, int v)
{
  push_into_buffer(b, v);
}
template<typename B> inline
void operator >> (const B &b, int &v)
{
  fetch_from_buffer(b, v);
}


/// int64_t
template<typename B> inline
  void operator << (B &b, int64_t v)
{
  push_into_buffer(b, v);
}
template<typename B> inline
  void operator >> (const B &b, int64_t &v)
{
  fetch_from_buffer(b, v);
}


/// double
template<typename B> inline
void operator << (B &b, double v)
{
  push_into_buffer(b, v);
}
template<typename B> inline
  void operator >> (const B &b, double &v)
{
  fetch_from_buffer(b, v);
}


/// STL CONTAINERS

/// these should be dispatched to from container entry points, require only iterator compliance

template<typename B, typename FwdOutIt> inline
void insert_range(B &b, FwdOutIt first, FwdOutIt last)
{
  while (first != last)
  {
    b << *first;
    ++first;
  }
}


template<typename B, typename FwdOutIt> inline
void insert_range_and_size(B &b, FwdOutIt first, FwdOutIt last)
{
  push_into_buffer(b, static_cast<size_t>(std::distance(first, last)));
  insert_range(b, first, last);
}


template<typename B, typename C> inline
void insert_key_value_range_and_size(B &b, const C &c)
{
  push_into_buffer(b, static_cast<size_t>(c.size()));
  for (const auto &v : c)
  {
    b << v.first;
    b << v.second;
  }
}


template<typename B, typename FwdInIt> inline
void fetch_range(B &b, FwdInIt first, FwdInIt last)
{
  while (first != last)
  {
    b >> *first;
    ++first;
  }
}


// these use insert for when we don't construct the range in advance

template<typename D, typename B, typename C> inline
void fetch_range_using_insertion(B &b, C&c)
{
  const auto size = fetch_size(b);  
  for (size_t i = 0; i < size; ++i)
  {
    D tmp;
    b >> tmp;
    c.insert(tmp);
  }
}

template<typename Dk, typename Dv, typename B, typename C> inline
void fetch_key_value_range_using_insertion(B &b, C&c)
{
  const auto size = fetch_size(b);  
  for (size_t i = 0; i < size; ++i)
  {
    Dk tmpk;
    Dv tmpv;
    b >> tmpk;
    b >> tmpv;
    c.insert(std::make_pair(tmpk, tmpv));
  }
}





/// pair
template<typename B, typename D1, typename D2> inline
void operator << (B &b, const std::pair<D1, D2> &c)
{
  b << c.first;
  b << c.second;
}
template<typename B, typename D1, typename D2> inline
void operator >> (const B &b, std::pair<D1, D2> &c)
{
  b >> c.first;
  b >> c.second;
}


/// vector
template<typename B, typename D> inline
void operator << (B &b, const std::vector<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
void operator >> (const B &b, std::vector<D> &c)
{  
  fetch_size_and_apply(b, c);
  fetch_range(b, c.begin(), c.end());
}


/// list
template<typename B, typename D> inline
void operator << (B &b, const std::list<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
void operator >> (const B &b, std::list<D> &c)
{
  fetch_size_and_apply(b, c);
  fetch_range(b, c.begin(), c.end());
}


/// array
template<typename B, typename D, size_t N> inline
void operator << (B &b, const std::array<D, N> &c)
{
  insert_range(b, c.begin(), c.end());
}
template<typename B, typename D, size_t N> inline
void operator >> (const B &b, std::array<D, N> &c)
{
  // fixed width - no resize here
  fetch_range(b, c.begin(), c.end());
}


/// deque
template<typename B, typename D> inline
void operator << (B &b, const std::deque<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
void operator >> (const B &b, std::deque<D> &c)
{
  fetch_size_and_apply(b, c);
  fetch_range(b, c.begin(), c.end());
}


/// forward_list
template<typename B, typename D> inline
  void operator << (B &b, const std::forward_list<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
void operator >> (const B &b, std::forward_list<D> &c)
{
  fetch_size_and_apply(b, c);
  fetch_range(b, c.begin(), c.end());
}


/// set
template<typename B, typename D> inline
void operator << (B &b, const std::set<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
void operator >> (const B &b, std::set<D> &c)
{
  fetch_range_using_insertion<D>(b, c);
}


/// multiset
template<typename B, typename D> inline
void operator << (B &b, const std::multiset<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
void operator >> (const B &b, std::multiset<D> &c)
{
  fetch_range_using_insertion<D>(b, c);
}


/// map
template<typename B, typename Dk, typename Dv> inline
void operator << (B &b, const std::map<Dk, Dv> &c)
{
  insert_key_value_range_and_size(b, c);
}
template<typename B, typename Dk, typename Dv> inline
void operator >> (const B &b, std::map<Dk, Dv> &c)
{
  fetch_key_value_range_using_insertion<Dk, Dv>(b, c);
}

/// multimap
template<typename B, typename Dk, typename Dv> inline
  void operator << (B &b, const std::multimap<Dk, Dv> &c)
{
  insert_key_value_range_and_size(b, c);
}
template<typename B, typename Dk, typename Dv> inline
  void operator >> (const B &b, std::multimap<Dk, Dv> &c)
{
  fetch_key_value_range_using_insertion<Dk, Dv>(b, c);
}


/// unordered set
template<typename B, typename D> inline
  void operator << (B &b, const std::unordered_set<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
  void operator >> (const B &b, std::unordered_set<D> &c)
{
  fetch_range_using_insertion<D>(b, c);
}


/// unordered multiset
template<typename B, typename D> inline
  void operator << (B &b, const std::unordered_multiset<D> &c)
{
  insert_range_and_size(b, c.begin(), c.end());
}
template<typename B, typename D> inline
  void operator >> (const B &b, std::unordered_multiset<D> &c)
{
  fetch_range_using_insertion<D>(b, c);
}


/// unordered map
template<typename B, typename Dk, typename Dv> inline
  void operator << (B &b, const std::unordered_map<Dk, Dv> &c)
{
  insert_key_value_range_and_size(b, c);
}
template<typename B, typename Dk, typename Dv> inline
  void operator >> (const B &b, std::unordered_map<Dk, Dv> &c)
{
  fetch_key_value_range_using_insertion<Dk, Dv>(b, c);
}

/// unordered multimap
template<typename B, typename Dk, typename Dv> inline
  void operator << (B &b, const std::unordered_multimap<Dk, Dv> &c)
{
  insert_key_value_range_and_size(b, c);
}
template<typename B, typename Dk, typename Dv> inline
  void operator >> (const B &b, std::unordered_multimap<Dk, Dv> &c)
{
  fetch_key_value_range_using_insertion<Dk, Dv>(b, c);
}


/// RECURSIVE, TYPE-BASED DISPATCHERS
/// these are used for subtypes that don't have a << op 
/// implemented, but do have user specified load/save
/// methods. this does namespace based lookup


template<typename B, typename T> inline
void operator << (B &b, const T &c)
{
  save(b, c);
}


template<typename B, typename T> inline
void operator >> (const B &b, T &c)
{  
  load(b, c);
}


/// initialize read buffer iterator for provided intergal type
template<typename T, typename B> inline
void initialize_read_buffer_it(const B& b)
{
  buffer_iterator<T>(b) = buffer<T>(b).begin();
}


/// initialize read buffer iterators
template<typename B> inline
void initialize_read_buffer_its(const B& b)
{
  initialize_read_buffer_it<int>(b);
  initialize_read_buffer_it<int64_t>(b);
  initialize_read_buffer_it<double>(b);
  initialize_read_buffer_it<size_t>(b);
}


template<typename O> inline
std::unique_ptr<typename BufferTraits<O>::Buffer> make_read_buffer(std::unique_ptr<typename BufferTraits<O>::Buffer>& b)
{
  std::unique_ptr<typename BufferTraits<O>::Buffer> bPtr(b.release());
  initialize_read_buffer_its(*bPtr);
  return bPtr;
}


template<typename O> inline
void mpi_gather_dummy(const O &oput, O &oget)
{
  auto wb = make_write_buffer<O>();
  save(*wb, oput);

  // exchange buffer arrays here...

  const auto rb = make_read_buffer<O>(wb);
  load(*rb, oget);
}


/////////////////////////////////////////////////////
////                  USER                       ////
/////////////////////////////////////////////////////


struct SomeType
{
  std::vector<std::vector<int>> data;
  std::vector<int> more_data;
  std::pair<int,int> pair_data;
  std::vector<double> double_data;
  std::vector<std::pair<double,double>> double_data_pairs;
  int64_t i;
  std::list<double> double_list;
  std::array<int, 3> int_array;
  std::deque<int64_t> int64_deck;
  std::forward_list<int> int_flist;
  std::set<double> double_set;
  std::multiset<int64_t> multi_set;
  std::multiset<std::pair<int,double>> multi_set_nested_pair;
  std::map<int, std::vector<double>> map_int_vector_double;
  std::multimap<int, std::set<int64_t>> multimap_int_set_int64_t;
  std::set<double> double_uset;
  std::multiset<int64_t> umulti_set;
  std::multiset<std::pair<int,double>> umulti_set_nested_pair;
  std::map<int, std::vector<double>> umap_int_vector_double;
  std::multimap<int, std::set<int64_t>> umultimap_int_set_int64_t;
};


template<typename Buffer> inline
void save(Buffer &b, const SomeType &d)
{
  b << d.data;
  b << d.more_data;
  b << d.pair_data;
  b << d.double_data;
  b << d.double_data_pairs;
  b << d.i;
  b << d.double_list;
  b << d.int_array;
  b << d.int64_deck;
  b << d.int_flist;
  b << d.double_set;
  b << d.multi_set;
  b << d.multi_set_nested_pair;
  b << d.map_int_vector_double;
  b << d.multimap_int_set_int64_t;
  b << d.double_uset;
  b << d.umulti_set;
  b << d.umulti_set_nested_pair;
  b << d.umap_int_vector_double;
  b << d.umultimap_int_set_int64_t;
}


template<typename Buffer> inline
void load(const Buffer &b, SomeType &d)
{
  b >> d.data;
  b >> d.more_data;
  b >> d.pair_data;
  b >> d.double_data;
  b >> d.double_data_pairs;
  b >> d.i;
  b >> d.double_list;
  b >> d.int_array;
  b >> d.int64_deck;
  b >> d.int_flist;
  b >> d.double_set;
  b >> d.multi_set;
  b >> d.multi_set_nested_pair;
  b >> d.map_int_vector_double;
  b >> d.multimap_int_set_int64_t;
  b >> d.double_uset;
  b >> d.umulti_set;
  b >> d.umulti_set_nested_pair;
  b >> d.umap_int_vector_double;
  b >> d.umultimap_int_set_int64_t;
}


struct RecursiveType
{
  int64_t i;
  SomeType st;
};


template<typename Buffer> inline
void save(Buffer &b, const RecursiveType &d)
{
  b << d.i;
  b << d.st;
}


template<typename Buffer> inline
void load(const Buffer &b, RecursiveType &d)
{
  b >> d.i;
  b >> d.st;
}


int main()
{
  // initialize
  SomeType tput;
  tput.data.insert(tput.data.begin(), 10, std::vector<int>(5, 2));
  tput.more_data.insert(tput.more_data.begin(), 18, -3);
  tput.pair_data.first = 8;
  tput.pair_data.second = 3;
  tput.double_data.insert(tput.double_data.begin(), 24, 1.2);
  tput.double_data_pairs.insert(tput.double_data_pairs.begin(), 12, std::make_pair(2.,9.));
  tput.int_array[0] = 12;
  tput.int_array[1] = 8;
  tput.i = 880; 
  tput.int64_deck = std::deque<int64_t>(800, 123456789101112);
  tput.int_flist = std::forward_list<int>(42, -9);
  tput.double_set.insert(1.5);
  tput.double_set.insert(2.5);
  tput.double_set.insert(3.0);
  tput.multi_set.insert(100005000);
  tput.multi_set.insert(67742);
  tput.multi_set.insert(67742);
  tput.multi_set.insert(100005000);
  tput.multi_set.insert(465);
  tput.multi_set_nested_pair.insert(std::make_pair(84, 2.01));
  tput.multi_set_nested_pair.insert(std::make_pair(21, 4.02));
  tput.map_int_vector_double[89] = std::vector<double>(19, 36.0);
  tput.map_int_vector_double[98] = std::vector<double>(91, 63.0);
  std::set<int64_t> set_int64_t;
  set_int64_t.insert(546431);
  set_int64_t.insert(543687);
  set_int64_t.insert(4683687);
  set_int64_t.insert(15864);
  std::set<int64_t> set_int64_t0;
  set_int64_t0.insert(5456431);
  set_int64_t0.insert(5434687);
  set_int64_t0.insert(46863687);
  set_int64_t0.insert(158364);
  tput.multimap_int_set_int64_t.insert(std::make_pair(1, set_int64_t));
  tput.multimap_int_set_int64_t.insert(std::make_pair(1, set_int64_t0));
  tput.multimap_int_set_int64_t.insert(std::make_pair(5, set_int64_t));
  tput.double_uset = tput.double_set;
  tput.umulti_set = tput.multi_set;
  tput.umulti_set_nested_pair = tput.multi_set_nested_pair;
  tput.umap_int_vector_double = tput.map_int_vector_double;
  tput.umultimap_int_set_int64_t = tput.multimap_int_set_int64_t;
  
  // exchange
  SomeType tget;
  mpi_gather_dummy(tput, tget);


  // TESTS
  std::cout << std::boolalpha << (tget.data == tput.data) << "\n";
  std::cout << std::boolalpha << (tget.more_data == tput.more_data) << "\n";
  std::cout << std::boolalpha << (tget.pair_data == tput.pair_data) << "\n";
  std::cout << std::boolalpha << (tget.double_data == tput.double_data) << "\n";
  std::cout << std::boolalpha << (tget.double_data_pairs == tput.double_data_pairs) << "\n";
  std::cout << std::boolalpha << (tget.i == tput.i) << "\n";
  std::cout << std::boolalpha << (tget.double_list == tput.double_list) << "\n";
  std::cout << std::boolalpha << (tget.int_array == tput.int_array) << "\n";
  std::cout << std::boolalpha << (tget.int64_deck == tput.int64_deck) << "\n";
  std::cout << std::boolalpha << (tget.int_flist == tput.int_flist) << "\n";
  std::cout << std::boolalpha << (tget.double_set == tput.double_set) << "\n";
  std::cout << std::boolalpha << (tget.multi_set == tput.multi_set) << "\n";
  std::cout << std::boolalpha << (tget.multi_set_nested_pair == tput.multi_set_nested_pair) << "\n";
  std::cout << std::boolalpha << (tget.map_int_vector_double == tput.map_int_vector_double) << "\n";
  std::cout << std::boolalpha << (tget.multimap_int_set_int64_t == tput.multimap_int_set_int64_t) << "\n";
  std::cout << std::boolalpha << (tget.double_uset == tput.double_uset) << "\n";
  std::cout << std::boolalpha << (tget.umulti_set == tput.umulti_set) << "\n";
  std::cout << std::boolalpha << (tget.umulti_set_nested_pair == tput.umulti_set_nested_pair) << "\n";
  std::cout << std::boolalpha << (tget.umap_int_vector_double == tput.umap_int_vector_double) << "\n";
  std::cout << std::boolalpha << (tget.umultimap_int_set_int64_t == tput.umultimap_int_set_int64_t) << "\n";



  RecursiveType rtput;
  rtput.i = 424242424242424242;
  rtput.st = tput;
  RecursiveType rtget;
  mpi_gather_dummy(rtput, rtget);

  // TESTS
  std::cout << std::boolalpha << (rtget.i == rtput.i) << "\n";
  std::cout << std::boolalpha << (rtget.st.data == rtput.st.data) << "\n";
  std::cout << std::boolalpha << (rtget.st.more_data == rtput.st.more_data) << "\n";
  std::cout << std::boolalpha << (rtget.st.pair_data == rtput.st.pair_data) << "\n";
  std::cout << std::boolalpha << (rtget.st.double_data == rtput.st.double_data) << "\n";
  std::cout << std::boolalpha << (rtget.st.double_data_pairs == rtput.st.double_data_pairs) << "\n";
  std::cout << std::boolalpha << (rtget.st.i == rtput.st.i) << "\n";
  std::cout << std::boolalpha << (rtget.st.double_list == rtput.st.double_list) << "\n";
  std::cout << std::boolalpha << (rtget.st.int_array == rtput.st.int_array) << "\n";
  std::cout << std::boolalpha << (rtget.st.int64_deck == rtput.st.int64_deck) << "\n";
  std::cout << std::boolalpha << (rtget.st.int_flist == rtput.st.int_flist) << "\n";
  std::cout << std::boolalpha << (rtget.st.double_set == rtput.st.double_set) << "\n";
  std::cout << std::boolalpha << (rtget.st.multi_set == rtput.st.multi_set) << "\n";
  std::cout << std::boolalpha << (rtget.st.multi_set_nested_pair == rtput.st.multi_set_nested_pair) << "\n";
  std::cout << std::boolalpha << (rtget.st.map_int_vector_double == rtput.st.map_int_vector_double) << "\n";
  std::cout << std::boolalpha << (rtget.st.multimap_int_set_int64_t == rtput.st.multimap_int_set_int64_t) << "\n";
  std::cout << std::boolalpha << (rtget.st.double_uset == rtput.st.double_uset) << "\n";
  std::cout << std::boolalpha << (rtget.st.umulti_set == rtput.st.umulti_set) << "\n";
  std::cout << std::boolalpha << (rtget.st.umulti_set_nested_pair == rtput.st.umulti_set_nested_pair) << "\n";
  std::cout << std::boolalpha << (rtget.st.umap_int_vector_double == rtput.st.umap_int_vector_double) << "\n";
  std::cout << std::boolalpha << (rtget.st.umultimap_int_set_int64_t == rtput.st.umultimap_int_set_int64_t) << "\n";

  std::cin.get();
  return 0;
}

