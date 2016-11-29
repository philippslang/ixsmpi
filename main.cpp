#include <iostream>
#include <vector>
#include <type_traits>
#include <algorithm>
#include <cstdint>


typedef std::int64_t  int64_t;


template<typename T>
struct _BufferTraits
{
  typedef std::vector<T> BCType;
  typedef typename BCType::const_iterator BCIterator;
}; 


struct _OBufferIts
{
  typedef std::vector<size_t> BCSizes;
  typedef BCSizes::const_iterator BCSit;

  BCSit bi_sizes;
  _BufferTraits<int>::BCIterator bi_int;
  _BufferTraits<int64_t>::BCIterator bi_int64_t;
  _BufferTraits<double>::BCIterator bi_double;
};


template<typename O>
struct _OBuffer
{
  _OBufferIts::BCSizes bc_sizes;
  typename _BufferTraits<int>::BCType bc_int;
  typename _BufferTraits<int64_t>::BCType bc_int64_t;
  typename _BufferTraits<double>::BCType bc_double;

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
    static typename _BufferTraits<int>::BCType& buffer(B &b) { return b.bc_int; }
    static const typename _BufferTraits<int>::BCType& buffer(const B &b) { return b.bc_int; }
    static typename _BufferTraits<int>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_int; }
  };

  template<typename B>
  struct access<int64_t, B>
  {
    static typename _BufferTraits<int64_t>::BCType& buffer(B &b) { return b.bc_int64_t; }
    static const typename _BufferTraits<int64_t>::BCType& buffer(const B &b) { return b.bc_int64_t; }
    static typename _BufferTraits<int64_t>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_int64_t; }
  };

  template<typename B>
  struct access<double, B>
  {
    static typename _BufferTraits<double>::BCType& buffer(B &b) { return b.bc_double; }
    static const typename _BufferTraits<double>::BCType& buffer(const B &b) { return b.bc_double; }
    static typename _BufferTraits<double>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_double; }
  };

  // specialized for container size buffer only, we don't allow for size_t as data type
  template<typename B>
  struct access<size_t, B>
  {
    static typename _BufferTraits<size_t>::BCType& buffer(B &b) { return b.bc_sizes; }
    static const typename _BufferTraits<size_t>::BCType& buffer(const B &b) { return b.bc_sizes; }
    static typename _BufferTraits<size_t>::BCIterator& buffer_iterator(const B &b) { return b.its.bi_sizes; }
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


template<typename T>
struct BufferTraits
{
  typedef _OBuffer<T> Buffer;
}; 


/// this is the only external link to the type of container used in
/// the buffer - everything else is encapsulated through the stl iterator 
/// api. at the bottom level (integral types), all << operators end here
template<typename B, typename D> inline
void push_into_buffer(B &b, D v)
{
  // buffer is a sequence containers
  buffer<D>(b).push_back(v);
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
  auto &it = buffer_iterator<int>(b);
  v = (*it);
  ++it;
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
  auto &it = buffer_iterator<int64_t>(b);
  v = (*it);
  ++it;
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
  auto &it = buffer_iterator<double>(b);
  v = (*it);
  ++it;
}


/// STL CONTAINERS


///pair

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


template<typename B, typename D> inline
void operator << (B &b, const std::vector<D> &c)
{
  push_into_buffer(b, c.size());
  std::for_each(c.begin(), c.end(), [&b](const D& v){ b << v;});
}


template<typename B, typename D> inline
void operator >> (const B &b, std::vector<D> &c)
{  
  auto &bit_size = buffer_iterator<size_t>(b);
  const auto csize = *bit_size;
  ++bit_size;

  c.resize(csize);
  std::for_each(c.begin(), c.end(), [&b](D& v){ b >> v;});
}


/// RECURSIVE, TYPE-BASED DISPATCHERS
/// these are used for subtypes that dont have a << op 
/// implemented, but do have user specified load/save
/// methods. this dos namespace based lookup

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



template<typename B> inline
void init_load(const B& b)
{

  buffer_iterator<int>(b) = buffer<int>(b).begin();
  buffer_iterator<int64_t>(b) = buffer<int64_t>(b).begin(); 
  buffer_iterator<double>(b) = buffer<double>(b).begin(); 
  buffer_iterator<size_t>(b) = buffer<size_t>(b).begin(); 
}


template<typename O> inline
void mpi_gather_dummy(const O &tput, O &tget)
{
  BufferTraits<O>::Buffer b;
  save(b, tput);

  // exchange buffer arrays here...

  init_load(b);
  load(b, tget);
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
}


struct RecursiveType
{
  SomeType st;
};


template<typename Buffer> inline
void save(Buffer &b, const RecursiveType &d)
{
  b << d.st;
}


template<typename Buffer> inline
void load(const Buffer &b, RecursiveType &d)
{
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
  tput.double_data.insert(tput.double_data.begin(), 12, 1.2);
  tput.double_data_pairs.insert(tput.double_data_pairs.begin(), 12, std::make_pair(2.,9.));
  tput.i = 880; 
 
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


  RecursiveType rtput;
  RecursiveType rtget;
  mpi_gather_dummy(rtput, rtget);


  return 0;
}

